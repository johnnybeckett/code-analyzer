#include "core/config.h"

#include <boost/json.hpp>
#include <fstream>
#include <sstream>

namespace {

boost::json::array to_json_array(const std::vector<std::string>& values) {
    boost::json::array arr;
    for (const auto& v : values) {
        arr.emplace_back(v);
    }
    return arr;
}

/**
 * @brief Read a string-array option out of a configuration document.
 *
 * A missing key is not an error: the caller's default is kept. A present key
 * that is not a JSON array of strings is a malformed document.
 */
bool read_string_array(const boost::json::value& doc, const char* key,
                       std::vector<std::string>& out) {
    if (!doc.is_object()) return false;
    const auto& obj = doc.as_object();
    auto it = obj.find(key);
    if (it == obj.end()) return true;  // absent: keep the caller's default
    if (!it->value().is_array()) return false;
    out.clear();
    for (const auto& el : it->value().as_array()) {
        if (!el.is_string()) return false;
        out.emplace_back(el.as_string());
    }
    return true;
}

} // namespace

std::optional<Config> Config::load(const std::string& path) {
    std::ifstream in(path);
    if (!in) return std::nullopt;
    std::ostringstream buf;
    buf << in.rdbuf();

    boost::json::value doc;
    try {
        doc = boost::json::parse(buf.str());
    } catch (const std::exception&) {
        return std::nullopt;  // malformed document
    }

    Config config;
    if (!read_string_array(doc, "source_extensions", config.source_extensions) ||
        !read_string_array(doc, "skip_dirs", config.skip_dirs)) {
        return std::nullopt;
    }
    return config;
}

std::optional<std::string> Config::save(const std::string& path) const {
    boost::json::object doc;
    doc["source_extensions"] = to_json_array(source_extensions);
    doc["skip_dirs"] = to_json_array(skip_dirs);
    const std::string text = boost::json::serialize(doc);

    std::ofstream out(path);
    if (!out) return std::nullopt;
    out << text;
    out.close();
    if (!out) return std::nullopt;
    return text;
}
