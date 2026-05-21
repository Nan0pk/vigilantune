#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif

// Prevent Windows.h ERROR macro from colliding with our log level enum
#ifdef ERROR
#undef ERROR
#endif

#include <windows.h>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <mutex>
#include <iostream>
#include <string>
#include <memory>

// Re-define ERROR if it was removed, but under a safe alias for user code
// Windows headers define ERROR as 0; we avoid that symbol entirely in our enum.

namespace nanoloop::log {

    // ── Log Levels ──────────────────────────────────────────────────────
    enum class Level : int {
        TRACE     = 0,
        DEBUG     = 1,
        INFO      = 2,
        WARN      = 3,
        ERROR_LVL = 4,   // Avoids collision with Windows ERROR macro
        FATAL     = 5
    };

    // Compile-time minimum log level (override with -DNANOLOOP_LOG_LEVEL=N)
#ifndef NANOLOOP_LOG_LEVEL
#define NANOLOOP_LOG_LEVEL 2  // Default: INFO
#endif

    // ── Helpers ─────────────────────────────────────────────────────────
    inline const char* levelToString(Level lvl) noexcept {
        switch (lvl) {
            case Level::TRACE:     return "TRACE";
            case Level::DEBUG:     return "DEBUG";
            case Level::INFO:      return "INFO ";
            case Level::WARN:      return "WARN ";
            case Level::ERROR_LVL: return "ERROR";
            case Level::FATAL:     return "FATAL";
        }
        return "?????";
    }

    inline WORD levelToColor(Level lvl) noexcept {
        switch (lvl) {
            case Level::TRACE:     return FOREGROUND_INTENSITY;                                          // Dark grey
            case Level::DEBUG:     return FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;     // Cyan
            case Level::INFO:      return FOREGROUND_GREEN | FOREGROUND_INTENSITY;                       // Green
            case Level::WARN:      return FOREGROUND_RED   | FOREGROUND_GREEN | FOREGROUND_INTENSITY;    // Yellow
            case Level::ERROR_LVL: return FOREGROUND_RED   | FOREGROUND_INTENSITY;                       // Red
            case Level::FATAL:     return FOREGROUND_RED   | FOREGROUND_BLUE  | FOREGROUND_INTENSITY;    // Magenta
        }
        return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    }

    // ── Singleton Logger State ──────────────────────────────────────────
    struct LoggerState {
        std::mutex              mtx;
        std::unique_ptr<std::ofstream> fileStream;
        Level                   runtimeLevel = static_cast<Level>(NANOLOOP_LOG_LEVEL);

        static LoggerState& instance() {
            static LoggerState s;
            return s;
        }

        // Optional: open a log file for mirrored output
        void initFile(const std::string& path) {
            std::lock_guard<std::mutex> lock(mtx);
            fileStream = std::make_unique<std::ofstream>(path, std::ios::app);
        }

        // Optional: change runtime level (must be >= compile-time level)
        void setLevel(Level lvl) {
            std::lock_guard<std::mutex> lock(mtx);
            runtimeLevel = lvl;
        }
    };

    // ── Timestamp ───────────────────────────────────────────────────────
    inline std::string timestamp() {
        using namespace std::chrono;
        auto now  = system_clock::now();
        auto tt   = system_clock::to_time_t(now);
        std::tm   tm_buf{};
        localtime_s(&tm_buf, &tt);

        std::ostringstream oss;
        oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S");
        return oss.str();
    }

    // ── LogStream (RAII – flushes message on destruction) ───────────────
    class LogStream {
    public:
        LogStream(Level lvl, const char* component)
            : m_level(lvl), m_component(component), m_active(true) {}

        // Move-only
        LogStream(LogStream&& o) noexcept
            : m_level(o.m_level)
            , m_component(o.m_component)
            , m_ss(std::move(o.m_ss))
            , m_active(o.m_active) {
            o.m_active = false;
        }

        LogStream(const LogStream&)            = delete;
        LogStream& operator=(const LogStream&) = delete;
        LogStream& operator=(LogStream&&)      = delete;

        ~LogStream() {
            if (!m_active) return;
            flush();
        }

        template<typename T>
        LogStream& operator<<(const T& val) {
            m_ss << val;
            return *this;
        }

    private:
        void flush() {
            auto& state = LoggerState::instance();
            std::lock_guard<std::mutex> lock(state.mtx);

            // Build formatted line: [timestamp] [LEVEL] [Component] message
            std::string ts  = timestamp();
            std::string msg = m_ss.str();

            std::ostringstream line;
            line << "[" << ts << "] "
                 << "[" << levelToString(m_level) << "] "
                 << "[" << m_component << "] "
                 << msg;

            std::string formatted = line.str();

            // ── Console output with color ──
            HANDLE hConsole = GetStdHandle(STD_ERROR_HANDLE);
            CONSOLE_SCREEN_BUFFER_INFO csbi{};
            WORD originalAttrs = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
            if (GetConsoleScreenBufferInfo(hConsole, &csbi)) {
                originalAttrs = csbi.wAttributes;
            }

            SetConsoleTextAttribute(hConsole, levelToColor(m_level));
            std::cerr << formatted << std::endl;
            SetConsoleTextAttribute(hConsole, originalAttrs);

            // ── File output (if configured) ──
            if (state.fileStream && state.fileStream->is_open()) {
                *state.fileStream << formatted << std::endl;
            }
        }

        Level              m_level;
        const char*        m_component;
        std::ostringstream m_ss;
        bool               m_active;
    };

    // ── Factory ─────────────────────────────────────────────────────────
    inline LogStream log(Level lvl, const char* component) {
        return LogStream(lvl, component);
    }

} // namespace nanoloop::log

// ── Public Macros ───────────────────────────────────────────────────────
// Each macro is compiled away if the level is below the compile-time minimum.
// Usage:  LOG_INFO("MyComponent", "value = " << 42);

#define NANOLOOP_LOG_IMPL(lvl, comp, msg)                                   \
    do {                                                                     \
        auto& nanoloop_log_state_ = ::nanoloop::log::LoggerState::instance(); \
        if (static_cast<int>(lvl) >= static_cast<int>(nanoloop_log_state_.runtimeLevel)) { \
            ::nanoloop::log::LogStream(lvl, comp) << msg;                   \
        }                                                                    \
    } while (0)

#if NANOLOOP_LOG_LEVEL <= 0
#define LOG_TRACE(comp, msg) NANOLOOP_LOG_IMPL(::nanoloop::log::Level::TRACE, comp, msg)
#else
#define LOG_TRACE(comp, msg) ((void)0)
#endif

#if NANOLOOP_LOG_LEVEL <= 1
#define LOG_DEBUG(comp, msg) NANOLOOP_LOG_IMPL(::nanoloop::log::Level::DEBUG, comp, msg)
#else
#define LOG_DEBUG(comp, msg) ((void)0)
#endif

#if NANOLOOP_LOG_LEVEL <= 2
#define LOG_INFO(comp, msg)  NANOLOOP_LOG_IMPL(::nanoloop::log::Level::INFO, comp, msg)
#else
#define LOG_INFO(comp, msg)  ((void)0)
#endif

#if NANOLOOP_LOG_LEVEL <= 3
#define LOG_WARN(comp, msg)  NANOLOOP_LOG_IMPL(::nanoloop::log::Level::WARN, comp, msg)
#else
#define LOG_WARN(comp, msg)  ((void)0)
#endif

#if NANOLOOP_LOG_LEVEL <= 4
#define LOG_ERROR(comp, msg) NANOLOOP_LOG_IMPL(::nanoloop::log::Level::ERROR_LVL, comp, msg)
#else
#define LOG_ERROR(comp, msg) ((void)0)
#endif

#if NANOLOOP_LOG_LEVEL <= 5
#define LOG_FATAL(comp, msg) NANOLOOP_LOG_IMPL(::nanoloop::log::Level::FATAL, comp, msg)
#else
#define LOG_FATAL(comp, msg) ((void)0)
#endif
