#pragma once
#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <functional>

namespace nanoloop {

    // Lightweight INI file parser — header-only, zero external dependencies.
    // Supports sections, key=value pairs, and comments (';' and '#').
    class ConfigLoader {
    public:
        // Load configuration from an INI file. Returns true if the file was found and parsed.
        bool load(const std::string& filepath) {
            std::ifstream file(filepath);
            if (!file.is_open()) return false;

            std::string line;
            std::string current_section;

            while (std::getline(file, line)) {
                line = trim(line);

                // Skip empty lines and comments
                if (line.empty() || line[0] == ';' || line[0] == '#') continue;

                // Section header
                if (line[0] == '[') {
                    auto end = line.find(']');
                    if (end != std::string::npos) {
                        current_section = line.substr(1, end - 1);
                        std::transform(current_section.begin(), current_section.end(),
                                       current_section.begin(), ::tolower);
                    }
                    continue;
                }

                // Key = Value
                auto eq = line.find('=');
                if (eq != std::string::npos) {
                    std::string key = trim(line.substr(0, eq));
                    std::string value = trim(line.substr(eq + 1));

                    // Strip inline comments
                    auto comment_pos = value.find(';');
                    if (comment_pos != std::string::npos) {
                        value = trim(value.substr(0, comment_pos));
                    }

                    std::transform(key.begin(), key.end(), key.begin(), ::tolower);

                    std::string full_key = current_section.empty() ? key : (current_section + "." + key);
                    m_values[full_key] = value;
                }
            }

            m_loaded = true;
            return true;
        }

        bool is_loaded() const { return m_loaded; }

        // Get a string value with fallback default
        std::string get_string(const std::string& key, const std::string& default_val = "") const {
            std::string lower_key = key;
            std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(), ::tolower);
            auto it = m_values.find(lower_key);
            return (it != m_values.end()) ? it->second : default_val;
        }

        // Get an integer value with fallback default
        int get_int(const std::string& key, int default_val = 0) const {
            std::string val = get_string(key);
            if (val.empty()) return default_val;
            try { return std::stoi(val); }
            catch (...) { return default_val; }
        }

        // Get a double value with fallback default
        double get_double(const std::string& key, double default_val = 0.0) const {
            std::string val = get_string(key);
            if (val.empty()) return default_val;
            try { return std::stod(val); }
            catch (...) { return default_val; }
        }

        // Get a boolean value with fallback default
        // Accepts: true/false, yes/no, 1/0, on/off
        bool get_bool(const std::string& key, bool default_val = false) const {
            std::string val = get_string(key);
            if (val.empty()) return default_val;
            std::transform(val.begin(), val.end(), val.begin(), ::tolower);
            return (val == "true" || val == "yes" || val == "1" || val == "on");
        }

        // Get a comma-separated list as a vector of strings
        std::vector<std::string> get_list(const std::string& key) const {
            std::vector<std::string> result;
            std::string val = get_string(key);
            if (val.empty()) return result;

            std::stringstream ss(val);
            std::string item;
            while (std::getline(ss, item, ',')) {
                item = trim(item);
                if (!item.empty()) {
                    result.push_back(item);
                }
            }
            return result;
        }

        // Get a comma-separated list as a vector of doubles
        std::vector<double> get_double_list(const std::string& key) const {
            std::vector<double> result;
            std::string val = get_string(key);
            if (val.empty()) return result;

            std::stringstream ss(val);
            std::string item;
            while (std::getline(ss, item, ',')) {
                item = trim(item);
                if (!item.empty()) {
                    try { result.push_back(std::stod(item)); }
                    catch (...) { /* ignore invalid items */ }
                }
            }
            return result;
        }

    private:
        static std::string trim(const std::string& s) {
            auto start = s.find_first_not_of(" \t\r\n");
            if (start == std::string::npos) return "";
            auto end = s.find_last_not_of(" \t\r\n");
            return s.substr(start, end - start + 1);
        }

        std::unordered_map<std::string, std::string> m_values;
        bool m_loaded = false;
    };

} // namespace nanoloop
