#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <map>
#include <memory>

/**
 * @brief Configuration manager using singleton pattern
 */
class Config {
public:
    static Config& getInstance();

    /**
     * @brief Set a configuration value
     * @param key Configuration key
     * @param value Configuration value
     */
    void set(const std::string& key, const std::string& value);

    /**
     * @brief Get a configuration value
     * @param key Configuration key
     * @return Configuration value or empty string if not found
     */
    std::string get(const std::string& key) const;

    /**
     * @brief Load configuration from file
     * @param config_file Path to configuration file
     */
    void loadFromFile(const std::string& config_file);

    /**
     * @brief Save configuration to file
     * @param config_file Path to configuration file
     */
    void saveToFile(const std::string& config_file);

private:
    Config() = default;
    std::map<std::string, std::string> config_map_;
};

#endif // CONFIG_H