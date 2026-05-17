#pragma once
#include <map>
#include <string>
#include <vector>
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
        double calculate_stress_score(const TagDatabase& db);
        bool is_dirty(const TagDatabase& db);

        TagDatabase m_last_state;
        double m_last_stress_score;

        std::unique_ptr<InferenceManager> m_inference;
    };
}
