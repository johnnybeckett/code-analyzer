#include "server/uml_server.h"
#include "server/review_store.h"
#include "uml/uml_model.h"

#include <arpa/inet.h>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <ifaddrs.h>
#include <memory>
#include <net/if.h>
#include <netinet/in.h>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

/**
 * @brief Print usage information for the UML server
 */
void print_usage(const std::string& program_name) {
    std::cout << "Usage: " << program_name << " [options] <input_json_file>...\n";
    std::cout << "Options:\n";
    std::cout << "  --port <n>                 TCP port to listen on (default 8000)\n";
    std::cout << "  --hide <regex>             Hide classes matching regex pattern\n";
    std::cout << "  -h, --help                 Show this help message\n";
    std::cout << "\n";
    std::cout << "  One file     -> a single combined diagram\n";
    std::cout << "  Two files    -> diff mode, in 'older newer' order:\n";
    std::cout << "                    added (green), removed (red) at the\n";
    std::cout << "                    class, method and member level\n";
    std::cout << "  More than two -> diff uses the first two; the rest ignored\n";
    std::cout << "\n";
    std::cout << "The server serves the UML model page over HTTP (GET) on the\n";
    std::cout << "chosen port, bound to all interfaces.\n";
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " data.json\n";
    std::cout << "  " << program_name << " --port 9000 older.json newer.json\n";
}

/**
 * @brief Best-effort discovery of the first non-loopback IPv4 address.
 *
 * Used only to print a LAN URL on startup. Returns an empty string if none is
 * found; the caller falls back to localhost.
 */
std::string get_lan_ipv4() {
    std::string result;
    struct ifaddrs* ifap = nullptr;
    if (getifaddrs(&ifap) != 0) {
        return result;
    }
    for (struct ifaddrs* ifa = ifap; ifa; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET
            && !(ifa->ifa_flags & IFF_LOOPBACK)) {
            auto* sin = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
            char buf[INET_ADDRSTRLEN] = {0};
            if (inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf))) {
                result = buf;
                break;
            }
        }
    }
    freeifaddrs(ifap);
    return result;
}

}  // namespace

/**
 * @brief Main entry point for the UML model HTTP server
 * @param argc Number of command line arguments
 * @param argv Command line arguments
 * @return Exit status
 */
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Error: No input specified\n";
        print_usage(argv[0]);
        return 1;
    }

    std::vector<std::string> input_files;
    std::vector<std::string> hide_patterns;
    unsigned int port = 8000;  // default

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            const std::string val = argv[++i];
            char* end = nullptr;
            const unsigned long parsed = std::strtoul(val.c_str(), &end, 10);
            if (end == val.c_str() || *end != '\0' || parsed == 0 || parsed >= 65536) {
                std::cerr << "Error: --port must be an integer in 1..65535 (got: "
                          << val << ")\n";
                return 1;
            }
            port = static_cast<unsigned int>(parsed);
        } else if (arg == "--hide" && i + 1 < argc) {
            hide_patterns.push_back(argv[++i]);
        } else if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else {
            input_files.push_back(arg);
        }
    }

    if (input_files.size() > 2) {
        std::cerr << "Warning: diff mode uses the first two files; ignoring the rest\n";
    }

    // Fail fast: build the whole page BEFORE binding a port, so a bad input never
    // leaves a half-configured server listening.
    const uml::UmlModel model(input_files, hide_patterns);
    std::string body;
    try {
        body = model.build_html();
    } catch (const std::exception& e) {
        std::cerr << "Error preparing UML model: " << e.what() << "\n";
        return 1;
    }

    // Preload the allowlisted class source files so `GET /source?path=...` can
    // return them by exact lookup — the server never builds a path from a query.
    std::map<std::string, std::string> sources;
    for (const auto& path : model.source_files()) {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            std::cerr << "Warning: source file not found; /source will 404 for "
                      << path << "\n";
            continue;
        }
        std::ostringstream data;
        data << in.rdbuf();
        sources.emplace(path, data.str());
    }

    // In diff mode, review comments are enabled: they persist to a JSON file
    // next to the baseline input, and the Markdown export is titled after the
    // two inputs. In single-input mode the endpoints stay present but inert
    // (review == nullptr), so the page's diff-mode UI is the only consumer.
    const auto base = [](const std::string& p) {
        const std::size_t i = p.find_last_of('/');
        return (i == std::string::npos) ? p : p.substr(i + 1);
    };
    std::shared_ptr<server::ReviewStore> review;
    std::string title;
    if (model.is_diff()) {
        const std::string rpath = input_files[0] + ".review.json";
        review = std::make_shared<server::ReviewStore>(rpath);  // loads if present
        title = base(input_files[0]) + " -> " + base(input_files[1]);
        std::cout << "Review comments: " << rpath << "\n";
    } else {
        title = base(input_files[0]);
    }

    server::UmlServer srv(std::move(body), std::move(sources),
                          static_cast<std::uint16_t>(port),
                          std::move(review), std::move(title));
    const unsigned int bound_port = srv.local_port();

    // Clean shutdown on Ctrl-C / SIGTERM.
    boost::asio::signal_set signals(srv.io(), SIGINT, SIGTERM);
    signals.async_wait([&srv](boost::system::error_code, int) { srv.stop(); });

    std::cout << "Serving the UML model on port " << bound_port << " (all interfaces).\n";
    const std::string lan = get_lan_ipv4();
    if (!lan.empty()) {
        std::cout << "  Local:    http://localhost:" << bound_port << "/\n";
        std::cout << "  Network:  http://" << lan << ":" << bound_port << "/\n";
    } else {
        std::cout << "  Open:     http://localhost:" << bound_port << "/\n";
    }
    std::cout << "Press Ctrl-C to stop.\n";
    std::cout.flush();

    srv.run();
    std::cout << "\nUmlServer stopped.\n";
    return 0;
}
