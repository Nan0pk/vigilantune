#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include "../shared/types.hpp"

namespace vigilantune {

    // 5x5 Multi-dimensional ECU Lookup Table
    struct ECUTable2D {
        TagID x_axis_tag;
        TagID y_axis_tag;
        std::array<double, 5> x_ticks;
        std::array<double, 5> y_ticks;
        std::array<std::array<double, 5>, 5> grid;
        ActuatorID target_actuator;

        // Zero-allocation Bilinear Interpolation
        double lookup(double x, double y) const {
            // Find surrounding bounding indices for X
            size_t i = 0;
            while (i < 3 && x > x_ticks[i + 1]) {
                i++;
            }
            // Find surrounding bounding indices for Y
            size_t j = 0;
            while (j < 3 && y > y_ticks[j + 1]) {
                j++;
            }

            double x0 = x_ticks[i], x1 = x_ticks[i + 1];
            double y0 = y_ticks[j], y1 = y_ticks[j + 1];

            // Normalize delta bounds [0.0, 1.0]
            double dx = (x1 - x0) > 0.0 ? (x - x0) / (x1 - x0) : 0.0;
            double dy = (y1 - y0) > 0.0 ? (y - y0) / (y1 - y0) : 0.0;

            dx = std::clamp(dx, 0.0, 1.0);
            dy = std::clamp(dy, 0.0, 1.0);

            // Fetch the 4 bounding grid vertices
            double q00 = grid[i][j];
            double q10 = grid[i + 1][j];
            double q01 = grid[i][j + 1];
            double q11 = grid[i + 1][j + 1];

            // Interpolate X direction
            double r1 = (1.0 - dx) * q00 + dx * q10;
            double r2 = (1.0 - dx) * q01 + dx * q11;

            // Interpolate Y direction
            return (1.0 - dy) * r1 + dy * r2;
        }
    };

    // Pre-configured default ECU Maps
    class ECUMapRegistry {
    public:
        // EPP Map: CPU Utilization (%) vs CPU Temperature (C) -> Target: EPP Actuator
        static ECUTable2D get_default_epp_map() {
            return ECUTable2D{
                TagID::CPU_Utilization,
                TagID::Thermal_Headroom, // CPU Temperature
                { 0.0, 25.0, 50.0, 75.0, 100.0 }, // CPU Util Ticks
                { 40.0, 55.0, 70.0, 85.0, 100.0 }, // CPU Temp Ticks
                {{
                    { 90.0, 80.0, 60.0, 60.0, 70.0 },  // Util 0%
                    { 80.0, 60.0, 40.0, 50.0, 70.0 },  // Util 25%
                    { 60.0, 40.0, 20.0, 40.0, 60.0 },  // Util 50%
                    { 40.0, 20.0, 0.0,  30.0, 50.0 },  // Util 75%
                    { 20.0, 0.0,  0.0,  20.0, 40.0 }   // Util 100%
                }},
                ActuatorID::EnergyPreference
            };
        }

        // Timer Resolution Map: Thread Queue Length vs GPU Utilization (%) -> Target: Timer Resolution Actuator
        static ECUTable2D get_default_timer_map() {
            return ECUTable2D{
                TagID::Thread_Queue_Length,
                TagID::GPU_Utilization,
                { 0.0, 2.0, 5.0, 12.0, 30.0 }, // Thread Queue Ticks
                { 0.0, 25.0, 50.0, 75.0, 100.0 }, // GPU Util Ticks
                {{
                    { 15.6, 15.6, 10.0, 5.0, 1.0 }, // Queue 0
                    { 15.6, 10.0, 5.0,  2.0, 1.0 }, // Queue 2
                    { 10.0, 5.0,  2.0,  1.0, 0.5 }, // Queue 5
                    { 5.0,  2.0,  1.0,  0.5, 0.5 }, // Queue 12
                    { 1.0,  0.5,  0.5,  0.5, 0.5 }  // Queue 30 (Pin max precision)
                }},
                ActuatorID::TimerCadence
            };
        }

        // Battery / Cooling Policy Map: Battery Percent (%) vs Battery Discharge Rate (mW) -> Target: SystemCooling
        static ECUTable2D get_default_cooling_map() {
            return ECUTable2D{
                TagID::Battery_Percent,
                TagID::Battery_Power_Rate,
                { 100.0, 80.0, 50.0, 30.0, 15.0 }, // Battery % Ticks
                { 0.0, -5000.0, -10000.0, -15000.0, -25000.0 }, // Discharge rate in mW
                {{
                    { 1.0, 1.0, 1.0, 1.0, 0.0 }, // Battery 100% (Active cooling preferred)
                    { 1.0, 1.0, 1.0, 0.0, 0.0 }, // Battery 80%
                    { 1.0, 1.0, 0.0, 0.0, 0.0 }, // Battery 50%
                    { 1.0, 0.0, 0.0, 0.0, 0.0 }, // Battery 30%
                    { 0.0, 0.0, 0.0, 0.0, 0.0 }  // Battery 15% (Passive only to save battery motor)
                }},
                ActuatorID::SystemCooling
            };
        }
    };

} // namespace vigilantune
