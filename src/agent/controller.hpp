#pragma once
#include <map>
#include <string>
#include <vector>
#include "../shared/types.hpp"

namespace wspa {
    class Controller {
    public:
        Controller();
        ~Controller();

        // Evaluates the current state and returns relative changes
        std::map<std::string, double> evaluate(const TagDatabase& db);

    private:
        double calculate_stress_score(const TagDatabase& db);
        bool is_dirty(const TagDatabase& db);

        TagDatabase m_last_state;
        double m_last_stress_score;
    };
}
