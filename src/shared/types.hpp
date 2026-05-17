#pragma once
#include <string>
#include <map>
#include <variant>
#include <chrono>
#include <unordered_map>
#include <shared_mutex>

namespace wspa {
    using TagValue = std::variant<double, int, std::string, bool>;

    struct Tag {
        TagValue value;
        std::chrono::system_clock::time_point last_updated;
    };

    class TagDatabase {
    public:
        void set(const std::string& name, const TagValue& value) {
            std::unique_lock lock(m_mutex);
            m_data[name] = { value, std::chrono::system_clock::now() };
        }

        bool get(const std::string& name, Tag& out_tag) const {
            std::shared_lock lock(m_mutex);
            auto it = m_data.find(name);
            if (it != m_data.end()) {
                out_tag = it->second;
                return true;
            }
            return false;
        }

        std::unordered_map<std::string, Tag> get_all() const {
            std::shared_lock lock(m_mutex);
            return m_data;
        }

        size_t size() const {
            std::shared_lock lock(m_mutex);
            return m_data.size();
        }

        bool contains(const std::string& name) const {
            std::shared_lock lock(m_mutex);
            return m_data.count(name) > 0;
        }

    private:
        std::unordered_map<std::string, Tag> m_data;
        mutable std::shared_mutex m_mutex;
    };
}
