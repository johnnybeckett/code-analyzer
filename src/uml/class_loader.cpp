#include "uml/class_loader.h"

#include <boost/json.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace uml {

std::vector<std::string> JsonClassLoader::parseFileClasses(const std::string& filename) {
    std::vector<std::string> class_data;
    namespace json = boost::json;

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Warning: Could not open file " << filename << std::endl;
        return class_data;
    }

    boost::system::error_code ec;
    json::value root = json::parse(file, ec);
    if (ec) {
        std::cerr << "Warning: failed to parse " << filename << ": " << ec.message() << std::endl;
        return class_data;
    }

    if (!root.is_object() || !root.as_object().contains("classes")
        || !root.as_object().at("classes").is_array()) {
        std::cerr << "Warning: no 'classes' array in " << filename << std::endl;
        return class_data;
    }

    // Serialize each class object so the browser receives the real data
    for (const auto& cls : root.as_object().at("classes").as_array()) {
        class_data.push_back(json::serialize(cls));
    }

    return class_data;
}

std::vector<std::string> JsonClassLoader::parseFileClassesStrict(const std::string& filename) {
    std::vector<std::string> class_data;
    namespace json = boost::json;

    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("could not open analyzer file: " + filename);
    }

    boost::system::error_code ec;
    json::value root = json::parse(file, ec);
    if (ec) {
        throw std::runtime_error("failed to parse " + filename + ": " + ec.message());
    }

    if (!root.is_object() || !root.as_object().contains("classes")
        || !root.as_object().at("classes").is_array()) {
        throw std::runtime_error("no 'classes' array in " + filename);
    }

    // Serialize each class object so the browser receives the real data
    for (const auto& cls : root.as_object().at("classes").as_array()) {
        class_data.push_back(json::serialize(cls));
    }

    return class_data;
}

std::string JsonClassLoader::getClassesJSON(const std::vector<std::string>& class_data) {
    std::ostringstream json;
    json << "[";

    for (size_t i = 0; i < class_data.size(); ++i) {
        if (i > 0) json << ",";
        json << class_data[i];
    }

    json << "]";
    return json.str();
}

}  // namespace uml
