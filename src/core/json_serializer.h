#ifndef JSON_SERIALIZER_H
#define JSON_SERIALIZER_H

#include "core/model.h"
#include <boost/json.hpp>
#include <string>

/**
 * @brief Serializes an AnalysisResult into a boost::json::value (SRP).
 *
 * The document shape lives here, not in main.cpp, so the CLI entry point stays
 * thin and the output format has exactly one owner. boost::json::object is
 * std::map-keyed, so key order is sorted and the serialized bytes are stable
 * run to run — the analyzer's JSON output must not drift across refactors.
 */
boost::json::value serialize(const AnalysisResult& result, const std::string& input_path);

#endif // JSON_SERIALIZER_H
