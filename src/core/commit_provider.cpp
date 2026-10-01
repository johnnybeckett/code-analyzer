#include "core/commit_provider.h"

#include <atomic>
#include <fstream>
#include <iostream>
#include <unistd.h>

#if defined(__has_include)
#if __has_include(<unistd.h>)
#define COMMIT_PROVIDER_POSIX
#endif
#endif

#ifdef COMMIT_PROVIDER_POSIX
#include <sys/wait.h>
#endif

namespace fs = std::filesystem;

namespace {

// Uniqueness for the auto-created staging dir within this process
std::atomic<unsigned long> next_staging_{0};

// Shell-quote a string for interpolation into a `sh -c` command line:
// single-quote it, escaping any embedded single quotes as '\''.
std::string shell_quote(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    out += "'";
    return out;
}

// Run a command via popen, capture its stdout, and report the exit status.
// git's stderr (if any) passes through to our stderr — we add our own
// diagnosis from the return value.
bool run_capture(const std::string& cmd, std::string& out) {
    out.clear();
    std::FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return false;

    char buf[4096];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, pipe)) > 0) {
        out.append(buf, n);
    }
    int status = pclose(pipe);
    if (status != 0) return false;
    return true;
}

void trim_trailing_newline(std::string& s) {
    if (!s.empty() && s.back() == '\n') s.pop_back();
}

// One entry of `git ls-tree -r -z`: "<mode> <type> <obj-sha>\t<path>"
struct TreeEntry {
    std::string type;  // "blob", "tree", or "commit" (gitlink)
    std::string obj;   // object id (file SHA / submodule commit SHA)
    std::string path;  // repo-relative path
};

// Parse the NUL-separated `ls-tree -r -z` output; malformed entries are
// dropped rather than aborting the walk.
std::vector<TreeEntry> parse_ls_tree(const std::string& raw) {
    std::vector<TreeEntry> entries;
    std::size_t start = 0;
    while (start <= raw.size()) {
        std::size_t nul = raw.find('\0', start);
        if (nul == std::string::npos && start >= raw.size()) break;
        std::string entry = (nul == std::string::npos)
            ? raw.substr(start)
            : raw.substr(start, nul - start);
        if (nul == std::string::npos) break;
        start = nul + 1;
        if (entry.empty()) continue;

        std::size_t tab = entry.find('\t');
        if (tab == std::string::npos) continue;
        const std::string meta = entry.substr(0, tab);

        std::size_t sp1 = meta.find(' ');
        std::size_t sp2 = (sp1 == std::string::npos) ? std::string::npos
                                                      : meta.find(' ', sp1 + 1);
        if (sp1 == std::string::npos || sp2 == std::string::npos) continue;

        TreeEntry e;
        e.type = meta.substr(sp1 + 1, sp2 - sp1 - 1);
        e.obj = meta.substr(sp2 + 1);
        e.path = entry.substr(tab + 1);
        entries.push_back(std::move(e));
    }
    return entries;
}

} // namespace

CommitFileProvider::CommitFileProvider(std::string repo, std::string ref,
                                       std::string staging_override)
    : repo_(std::move(repo)),
      ref_(ref.empty() ? std::string("HEAD") : std::move(ref)),
      staging_override_(std::move(staging_override)) {}

CommitFileProvider::~CommitFileProvider() {
    if (remove_on_exit_) {
        std::error_code ec;
        std::filesystem::remove_all(staging_, ec);
    }
}

std::string CommitFileProvider::resolve_commit() const {
    std::string sha;
    // The ^{commit} peel suffix must be part of the SAME rev argument — as a
    // separate token it is not a valid revspec and rev-parse fails.
    const std::string cmd = "git -C " + shell_quote(repo_) +
                            " rev-parse --verify --quiet " +
                            shell_quote(ref_ + "^{commit}");
    if (!run_capture(cmd, sha)) {
        std::cerr << "Warning: could not resolve ref \"" << ref_ << "\" in "
                  << repo_ << "; no files to analyze\n";
        return "";
    }
    trim_trailing_newline(sha);
    if (sha.empty()) {
        std::cerr << "Warning: git rev-parse returned no commit for \"" << ref_
                  << "\" in " << repo_ << "\n";
        return "";
    }
    resolved_sha_ = sha;
    return sha;
}

void CommitFileProvider::materialize(const std::string& git_prefix,
                                     const std::string& sha,
                                     const std::string& show_path,
                                     const std::string& target_rel,
                                     std::vector<std::string>& files) const {
    const fs::path target = staging_ / target_rel;
    std::error_code ec;
    fs::create_directories(target.parent_path(), ec);

    const std::string cmd = "git " + git_prefix + " show " + sha + ":" +
                            shell_quote(show_path);
    std::string content;
    if (!run_capture(cmd, content)) {
        std::cerr << "Warning: could not read " << show_path
                  << " via git show; skipped\n";
        return;
    }

    std::ofstream out(target);
    if (!out.is_open()) {
        std::cerr << "Warning: could not write staged file " << target
                  << "\n";
        return;
    }
    out << content;
    out.close();
    files.push_back(target.string());
}

bool CommitFileProvider::materialize_submodule(const std::string& sub_path,
                                               const std::string& sub_sha,
                                               std::vector<std::string>& files) const {
    // The submodule's objects must already be in a local git dir; we never
    // network-fetch. Candidate locations, in order:
    //   1. the worktree's git dir (<repo>/<sub_path>/.git — dir or gitfile),
    //      resolved through git so a gitfile indirection is handled;
    //   2. the superproject's modules dir (<repo>/.git/modules/<sub_path>),
    //      which exists even when the submodule worktree was never checked out.
    std::vector<std::string> git_dirs;

    const fs::path worktree = fs::path(repo_) / sub_path;
    if (fs::exists(worktree / ".git")) {
        std::string git_dir;
        const std::string cmd = "git -C " + shell_quote(worktree.string()) +
                                " rev-parse --absolute-git-dir";
        if (run_capture(cmd, git_dir)) {
            trim_trailing_newline(git_dir);
            if (!git_dir.empty()) git_dirs.push_back(git_dir);
        }
    }

    const fs::path modules = fs::path(repo_) / ".git" / "modules" / sub_path;
    if (fs::is_directory(modules)) git_dirs.push_back(modules.string());

    for (const std::string& git_dir : git_dirs) {
        const std::string prefix = "--git-dir " + shell_quote(git_dir);

        std::string probe;
        if (!run_capture("git " + prefix + " cat-file -e " + sub_sha, probe)) {
            // Pinned SHA not in this git dir — try the next candidate
            continue;
        }

        std::string raw;
        if (!run_capture("git " + prefix + " ls-tree -r -z " + sub_sha, raw)) {
            std::cerr << "Warning: could not list submodule " << sub_path
                      << " at " << sub_sha << "\n";
            return false;
        }

        bool any = false;
        for (const TreeEntry& e : parse_ls_tree(raw)) {
            if (e.type != "blob") continue;  // nested gitlinks: out of scope
            // Show path is submodule-root-relative; the staged target keeps
            // the gitlink path so the staging tree mirrors the superproject.
            materialize(prefix, sub_sha, e.path, sub_path + "/" + e.path, files);
            any = true;
        }
        return any;
    }

    std::cerr << "Warning: submodule " << sub_path
              << " has no local git dir (never initialized or fetched); skipped\n";
    return false;
}

std::vector<std::string> CommitFileProvider::files() const {
    std::vector<std::string> files;

    if (repo_.empty()) {
        std::cerr << "Error: the commit source needs a repository path"
                  << std::endl;
        return files;
    }

    // Resolve the ref first: an unresolvable ref degrades to an empty list
    // (and creates no staging dir), exactly like a missing compile database.
    const std::string sha = resolve_commit();
    if (sha.empty()) return files;

    if (!staging_ready_) {
        if (!staging_override_.empty()) {
            std::error_code ec;
            fs::create_directories(staging_override_, ec);
            if (ec) {
                std::cerr << "Warning: could not create staging directory "
                          << staging_override_ << "\n";
                return files;
            }
            staging_ = staging_override_;
            remove_on_exit_ = false;  // the caller owns this directory
        } else {
            staging_ = fs::temp_directory_path() /
                       ("code_analyzer_commit_" +
                        std::to_string(static_cast<unsigned long>(::getpid())) +
                        "_" + std::to_string(++next_staging_));
            std::error_code ec;
            fs::create_directories(staging_, ec);
            if (ec) {
                std::cerr << "Warning: could not create staging directory "
                          << staging_ << "\n";
                return files;
            }
            remove_on_exit_ = true;  // we created it, we clean it up
        }
        staging_ready_ = true;
    }

    // The superproject's tree at the resolved SHA. `ls-tree -r` lists files
    // recursively but reports a submodule only as a single gitlink entry
    // (type "commit", mode 160000) — that is our submodule signal.
    const std::string super_prefix = "-C " + shell_quote(repo_);
    std::string raw;
    if (!run_capture("git " + super_prefix + " ls-tree -r -z " + sha, raw)) {
        std::cerr << "Warning: could not list the tree of " << repo_ << " at "
                  << sha << "\n";
        return files;
    }

    for (const TreeEntry& e : parse_ls_tree(raw)) {
        if (e.type == "blob") {
            materialize(super_prefix, sha, e.path, e.path, files);
        } else if (e.type == "commit") {
            // e.obj is the commit SHA the parent repo pins for this submodule
            materialize_submodule(e.path, e.obj, files);
        }
    }

    return files;
}

std::string CommitFileProvider::banner() const {
    const std::string shown =
        resolved_sha_.empty() ? ref_ : resolved_sha_.substr(0, 7);
    return "Analyzing commit " + shown + " of " + repo_;
}
