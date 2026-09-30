#include <gtest/gtest.h>
#include "core/config.h"
#include "core/parser_registry.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <vector>

namespace {

std::string tmp_path(const std::string& name) {
    auto dir = std::filesystem::temp_directory_path() / "config_test";
    std::filesystem::create_directories(dir);
    return (dir / name).string();
}

} // namespace

TEST(ConfigTest, DefaultsCoverStandardExtensionsAndSkipDirs) {
    Config config;
    // The full standard set, including the easy-to-drop ones
    EXPECT_NE(std::find(config.source_extensions.begin(), config.source_extensions.end(), ".cpp"),
              config.source_extensions.end());
    EXPECT_NE(std::find(config.source_extensions.begin(), config.source_extensions.end(), ".hxx"),
              config.source_extensions.end());
    EXPECT_NE(std::find(config.source_extensions.begin(), config.source_extensions.end(), ".tcc"),
              config.source_extensions.end());
    EXPECT_NE(std::find(config.source_extensions.begin(), config.source_extensions.end(), ".cs"),
              config.source_extensions.end());
    EXPECT_NE(std::find(config.source_extensions.begin(), config.source_extensions.end(), ".py"),
              config.source_extensions.end());
    EXPECT_NE(std::find(config.skip_dirs.begin(), config.skip_dirs.end(), ".git"),
              config.skip_dirs.end());
    EXPECT_NE(std::find(config.skip_dirs.begin(), config.skip_dirs.end(), "build"),
              config.skip_dirs.end());
}

TEST(ConfigTest, SaveThenLoadRoundTrips) {
    Config config;
    config.skip_dirs = {".git", "vendor"};
    const auto path = tmp_path("roundtrip.json");

    const auto saved = config.save(path);
    ASSERT_NE(saved, std::nullopt);

    const auto loaded = Config::load(path);
    ASSERT_NE(loaded, std::nullopt);
    EXPECT_EQ(loaded->skip_dirs, (std::vector<std::string>{".git", "vendor"}));
    EXPECT_EQ(loaded->source_extensions, config.source_extensions);
}

TEST(ConfigTest, LoadMissingFileIsNullopt) {
    EXPECT_EQ(Config::load("/nonexistent/no_such_config.json"), std::nullopt);
}

TEST(ConfigTest, LoadInvalidJsonIsNullopt) {
    const auto path = tmp_path("not_json.json");
    std::ofstream(path) << "this is not json";
    EXPECT_EQ(Config::load(path), std::nullopt);
}

TEST(ConfigTest, LoadRejectsWrongTypes) {
    const auto path = tmp_path("bad_type.json");
    std::ofstream(path) << R"({"skip_dirs": "not-an-array"})";
    EXPECT_EQ(Config::load(path), std::nullopt);

    const auto path2 = tmp_path("bad_element.json");
    std::ofstream(path2) << R"({"source_extensions": [".cpp", 42]})";
    EXPECT_EQ(Config::load(path2), std::nullopt);
}

TEST(ConfigTest, PartialDocumentKeepsDefaultsForMissingKeys) {
    const auto path = tmp_path("partial.json");
    std::ofstream(path) << R"({"skip_dirs": ["out"]})";

    const auto loaded = Config::load(path);
    ASSERT_NE(loaded, std::nullopt);
    EXPECT_EQ(loaded->skip_dirs, (std::vector<std::string>{"out"}));
    // source_extensions absent from the document: the default set survives
    EXPECT_EQ(loaded->source_extensions, Config().source_extensions);
}

TEST(ConfigTest, ConfiguredExtensionsRestrictTheRegistry) {
    Config config;
    config.source_extensions = {".cs"};
    const ParserRegistry registry = ParserRegistry::standard(config.source_extensions);

    EXPECT_TRUE(registry.has_parser(".cs"));
    EXPECT_FALSE(registry.has_parser(".cpp"));
    EXPECT_FALSE(registry.has_parser(".py"));
    // An extension no parser handles is ignored, not an error
    EXPECT_FALSE(registry.has_parser(".unknown"));
}
