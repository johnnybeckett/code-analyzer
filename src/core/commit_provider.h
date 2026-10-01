#ifndef COMMIT_PROVIDER_H
#define COMMIT_PROVIDER_H

#include <filesystem>
#include <string>
#include <vector>

#include "core/source_file_provider.h"

/**
 * @brief A SourceFileProvider that reads a repo's sources at a commit from git
 *        objects (`git show`), without checking out a working tree.
 *
 * A superproject/submodule layout is detected from the gitlink entries of
 * `git ls-tree` (mode 160000, type "commit"); each submodule's files are read
 * with `git show` against the SHA the parent repo records for it, from the
 * submodule's local git dir — so no checkout of the submodule (or the
 * superproject) is required. Submodules whose objects were never fetched are
 * skipped with a warning; we never network-fetch.
 *
 * Because the IParser pipeline reads files by path, the git-show content is
 * materialized into a staging directory that mirrors the project tree
 * (submodule files under their gitlink path). By default that directory is a
 * fresh temp dir owned by this provider and removed on destruction; an
 * explicit staging directory is used as-is and left in place.
 *
 * Limitations (documented, deliberate): top-level gitlinks only — nested
 * submodules are not descended; and the ref is whatever the caller passes
 * (main splits `repo@ref` on the last '@').
 */
class CommitFileProvider final : public SourceFileProvider {
public:
    CommitFileProvider(std::string repo, std::string ref,
                       std::string staging_override = "");

    ~CommitFileProvider() override;

    /**
     * @brief Materialize the commit's files (superproject + pinned submodules)
     *        and return their staged paths, in enumeration order.
     *
     * Not side-effect-free in the usual sense: this is the lazy point where
     * the staging directory is created and the git-show content is written —
     * hence the mutable members below. A call after a failed ref resolution
     * still yields an empty list.
     */
    std::vector<std::string> files() const override;

    /**
     * @brief One-line banner: "Analyzing commit <ref-or-short-sha> of <repo>".
     *        Prints the resolved short SHA once files() has resolved it,
     *        otherwise the ref as given.
     */
    std::string banner() const override;

private:
    // Resolve the ref to a full commit SHA in this repo; "" on failure (warns).
    std::string resolve_commit() const;

    // `git show <sha>:<show_path>` (under the given git prefix) → staged file
    // at <staging>/<target_rel>. These differ for submodule files: the show
    // path is relative to the submodule's own root, the target keeps the
    // gitlink path so the staging tree mirrors the superproject.
    void materialize(const std::string& git_prefix, const std::string& sha,
                     const std::string& show_path,
                     const std::string& target_rel,
                     std::vector<std::string>& files) const;

    // Resolve the submodule's local git dir and materialize its tree at
    // sub_sha under <staging>/<sub_path>/; warns and returns false if the
    // submodule has no local git dir or the pinned object is absent.
    bool materialize_submodule(const std::string& sub_path,
                               const std::string& sub_sha,
                               std::vector<std::string>& files) const;

    std::string repo_;
    std::string ref_;
    std::string staging_override_;

    // Lazy materialization state: files() (a const interface requirement)
    // creates the staging dir on first use and caches the resolved SHA.
    mutable bool staging_ready_ = false;
    mutable std::filesystem::path staging_;
    mutable bool remove_on_exit_ = false;
    mutable std::string resolved_sha_;
};

#endif // COMMIT_PROVIDER_H
