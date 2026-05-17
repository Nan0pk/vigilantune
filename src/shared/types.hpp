#pragma once
#include <string>
#include <map>
#include <variant>
#include <chrono>

namespace wspa {
    using TagValue = std::variant<double, int, std::string, bool>;

    struct Tag {
        TagValue value;
        std::chrono::system_clock::time_point last_updated;
    };

    using TagDatabase = std::map<std::string, Tag>;
}
