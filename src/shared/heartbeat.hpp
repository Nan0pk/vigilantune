#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <atomic>
#include <cstdint>
#include <string>

namespace nanoloop {

    // Shared memory layout for Agent <-> Watchdog heartbeat IPC.
    // The agent writes a monotonically increasing tick counter every control loop.
    // The watchdog reads it to detect agent hangs (not just crashes).
    struct HeartbeatData {
        std::atomic<uint64_t> tick_counter;   // Monotonically increasing
        std::atomic<uint64_t> last_tick_time; // Timestamp in ms since epoch
        std::atomic<uint32_t> agent_pid;      // Agent process ID
    };

    // Agent-side heartbeat writer
    class HeartbeatWriter {
    public:
        HeartbeatWriter() : m_mapping(NULL), m_data(nullptr) {
            m_mapping = CreateFileMappingA(
                INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                0, sizeof(HeartbeatData), "Local\\Nanoloop_Heartbeat");
            
            if (m_mapping) {
                m_data = (HeartbeatData*)MapViewOfFile(
                    m_mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(HeartbeatData));
                
                if (m_data) {
                    m_data->tick_counter.store(0, std::memory_order_release);
                    m_data->last_tick_time.store(0, std::memory_order_release);
                    m_data->agent_pid.store(GetCurrentProcessId(), std::memory_order_release);
                }
            }
        }

        ~HeartbeatWriter() {
            if (m_data) UnmapViewOfFile(m_data);
            if (m_mapping) CloseHandle(m_mapping);
        }

        // Call this at the end of each control loop iteration
        void tick() {
            if (!m_data) return;
            m_data->tick_counter.fetch_add(1, std::memory_order_release);
            auto now = GetTickCount64();
            m_data->last_tick_time.store(now, std::memory_order_release);
        }

        bool valid() const { return m_data != nullptr; }

        HeartbeatWriter(const HeartbeatWriter&) = delete;
        HeartbeatWriter& operator=(const HeartbeatWriter&) = delete;

    private:
        HANDLE m_mapping;
        HeartbeatData* m_data;
    };

    // Watchdog-side heartbeat reader
    class HeartbeatReader {
    public:
        HeartbeatReader() : m_mapping(NULL), m_data(nullptr) {
            m_mapping = OpenFileMappingA(FILE_MAP_READ, FALSE, "Local\\Nanoloop_Heartbeat");
            if (m_mapping) {
                m_data = (HeartbeatData*)MapViewOfFile(
                    m_mapping, FILE_MAP_READ, 0, 0, sizeof(HeartbeatData));
            }
        }

        ~HeartbeatReader() {
            if (m_data) UnmapViewOfFile(m_data);
            if (m_mapping) CloseHandle(m_mapping);
        }

        bool valid() const { return m_data != nullptr; }

        // Returns true if the agent's heartbeat has advanced since the last check
        bool is_alive(uint64_t timeout_ms = 5000) const {
            if (!m_data) return false;
            
            uint64_t last_time = m_data->last_tick_time.load(std::memory_order_acquire);
            if (last_time == 0) return true; // Agent hasn't started ticking yet
            
            uint64_t now = GetTickCount64();
            return (now - last_time) < timeout_ms;
        }

        uint64_t get_tick_count() const {
            if (!m_data) return 0;
            return m_data->tick_counter.load(std::memory_order_acquire);
        }

        uint32_t get_agent_pid() const {
            if (!m_data) return 0;
            return m_data->agent_pid.load(std::memory_order_acquire);
        }

        HeartbeatReader(const HeartbeatReader&) = delete;
        HeartbeatReader& operator=(const HeartbeatReader&) = delete;

    private:
        HANDLE m_mapping;
        HeartbeatData* m_data;
    };

} // namespace nanoloop
