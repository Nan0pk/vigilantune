#pragma once
#include <map>
#include <string>
#include <vector>
#include <unordered_map>
#include "../shared/types.hpp"

#include "inference.hpp"

namespace wspa {
    struct ControlResult {
        std::map<std::string, double> adjustments;
        double stress_score;
    };

    class Controller {
    public:
        Controller();
        ~Controller();

        // Evaluates the current state and returns relative changes + stress score
        ControlResult evaluate(const TagDatabase& db);

    private:
        double calculate_stress_score(const std::unordered_map<std::string, Tag>& db);
        bool is_dirty(const std::unordered_map<std::string, Tag>& db);

        std::unordered_map<std::string, Tag> m_last_state;
        double m_last_stress_score;

        std::unique_ptr<InferenceManager> m_inference;
    };
}
