#pragma once
#include <string>
#include <array>
#include <atomic>
#include <chrono>
#include <intrin.h>

namespace nanoloop {}
namespace vigilantune {}

namespace vigilantune {
    enum class TagID : size_t {
        CPU_Utilization = 0,
        Thread_Queue_Length,
        Thermal_Headroom, // CPU Temperature
        GPU_Utilization,
        Disk_Utilization,
        CPU_Frequency_Avg,
        CPU_Thermal_Limit,
        Timer_Resolution_100ns,
        Timer_Pollution,
        Foreground_App_Hash,
        GPU_Temperature,
        Battery_Percent,
        Battery_Power_Rate,
        Network_Throughput,
        Memory_Utilization,
        MAX_TAGS
    };

    enum class ActuatorID : size_t {
        PerformanceBoost = 0,
        ProcessorFloor,
        EnergyPreference,
        SystemCooling,
        TimerCadence,
        MAX_ACTUATORS
    };

    struct ActuationSet {
        std::array<double, static_cast<size_t>(ActuatorID::MAX_ACTUATORS)> values;
        std::array<bool, static_cast<size_t>(ActuatorID::MAX_ACTUATORS)> has_value;

        ActuationSet() {
            values.fill(0.0);
            has_value.fill(false);
        }

        void set(ActuatorID id, double val) {
            size_t idx = static_cast<size_t>(id);
            values[idx] = val;
            has_value[idx] = true;
        }

        bool empty() const {
            for (bool b : has_value) {
                if (b) return false;
            }
            return true;
        }
    };

    struct TagSnapshot {
        std::array<double, static_cast<size_t>(TagID::MAX_TAGS)> values;
        std::array<uint64_t, static_cast<size_t>(TagID::MAX_TAGS)> timestamps;

        bool is_dirty(const TagSnapshot& other, double epsilon = 1.0) const {
            for (size_t i = 0; i < values.size(); ++i) {
                if (std::abs(values[i] - other.values[i]) > epsilon) return true;
            }
            return false;
        }
    };

    // Lock-Free Real-Time Tag Database (Architecture #2)
    // Pre-allocated array of atomic doubles using C++20 std::atomic<double>
    class TagDatabase {
    public:
        TagDatabase() {
            for (size_t i = 0; i < static_cast<size_t>(TagID::MAX_TAGS); ++i) {
                m_values[i].store(0.0, std::memory_order_relaxed);
                m_timestamps[i].store(0, std::memory_order_relaxed);
            }
        }

        void set(TagID tag, double value) {
            size_t idx = static_cast<size_t>(tag);
            m_values[idx].store(value, std::memory_order_release);
            uint64_t now = __rdtsc();
            m_timestamps[idx].store(now, std::memory_order_release);
        }

        double get(TagID tag) const {
            return m_values[static_cast<size_t>(tag)].load(std::memory_order_acquire);
        }

        TagSnapshot get_snapshot() const {
            TagSnapshot snapshot;
            for (size_t i = 0; i < static_cast<size_t>(TagID::MAX_TAGS); ++i) {
                snapshot.values[i] = m_values[i].load(std::memory_order_acquire);
                snapshot.timestamps[i] = m_timestamps[i].load(std::memory_order_acquire);
            }
            return snapshot;
        }

    private:
        std::array<std::atomic<double>, static_cast<size_t>(TagID::MAX_TAGS)> m_values;
        std::array<std::atomic<uint64_t>, static_cast<size_t>(TagID::MAX_TAGS)> m_timestamps;
    };
} // namespace vigilantune
