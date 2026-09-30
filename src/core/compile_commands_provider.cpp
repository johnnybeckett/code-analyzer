#include "core/compile_commands_provider.h"

#include <boost/json.hpp>
#include <fstream>
#include <iostream>

CompileCommandsFileProvider::CompileCommandsFileProvider(std::string path)
    : path_(std::move(path)) {}

std::vector<std::string> CompileCommandsFileProvider::files() const {
    std::vector<std::string> files;

    std::ifstream file(path_);
    if (!file.is_open()) {
        std::cerr << "Unable to open compile_commands file: " << path_ << std::endl;
        return files;
    }

    boost::system::error_code ec;
    boost::json::value root = boost::json::parse(file, ec);
    if (ec) {
        std::cerr << "Error parsing compile_commands.json: " << ec.message() << std::endl;
        return files;
    }
    if (!root.is_array()) {
        std::cerr << "Error: compile_commands.json is not a JSON array" << std::endl;
        return files;
    }

    // Each entry's "file" field names a translation unit. Non-object entries
    // and entries without a usable "file" are skipped, as before
    for (const auto& entry : root.as_array()) {
        if (entry.is_object() && entry.as_object().contains("file") &&
            entry.as_object().at("file").is_string()) {
            std::string file_path;
            file_path = entry.as_object().at("file").as_string();
            if (!file_path.empty()) {
                files.push_back(file_path);
            }
        }
    }

    return files;
}

std::string CompileCommandsFileProvider::banner() const {
    return "Analyzing compile_commands.json: " + path_;
}
