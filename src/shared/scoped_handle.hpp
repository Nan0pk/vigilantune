#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <pdh.h>

namespace wspa {
    // Generic RAII wrapper for Win32 HANDLE types
    class ScopedHandle {
    public:
        explicit ScopedHandle(HANDLE h = INVALID_HANDLE_VALUE) noexcept : m_handle(h) {}
        ~ScopedHandle() { close(); }

        // Non-copyable
        ScopedHandle(const ScopedHandle&) = delete;
        ScopedHandle& operator=(const ScopedHandle&) = delete;

        // Movable
        ScopedHandle(ScopedHandle&& other) noexcept : m_handle(other.m_handle) { other.m_handle = INVALID_HANDLE_VALUE; }
        ScopedHandle& operator=(ScopedHandle&& other) noexcept {
            if (this != &other) { close(); m_handle = other.m_handle; other.m_handle = INVALID_HANDLE_VALUE; }
            return *this;
        }

        HANDLE get() const noexcept { return m_handle; }
        HANDLE release() noexcept { HANDLE h = m_handle; m_handle = INVALID_HANDLE_VALUE; return h; }
        void reset(HANDLE h = INVALID_HANDLE_VALUE) noexcept { close(); m_handle = h; }
        bool valid() const noexcept { return m_handle != INVALID_HANDLE_VALUE && m_handle != NULL; }
        explicit operator bool() const noexcept { return valid(); }

    private:
        void close() noexcept {
            if (valid()) { CloseHandle(m_handle); m_handle = INVALID_HANDLE_VALUE; }
        }
        HANDLE m_handle;
    };

    // RAII wrapper for handles freed with LocalFree (e.g., PowerGetActiveScheme returns)
    template<typename T>
    class ScopedLocalPtr {
    public:
        explicit ScopedLocalPtr(T* ptr = nullptr) noexcept : m_ptr(ptr) {}
        ~ScopedLocalPtr() { if (m_ptr) LocalFree(m_ptr); }
        ScopedLocalPtr(const ScopedLocalPtr&) = delete;
        ScopedLocalPtr& operator=(const ScopedLocalPtr&) = delete;
        ScopedLocalPtr(ScopedLocalPtr&& other) noexcept : m_ptr(other.m_ptr) { other.m_ptr = nullptr; }
        ScopedLocalPtr& operator=(ScopedLocalPtr&& other) noexcept {
            if (this != &other) { if (m_ptr) LocalFree(m_ptr); m_ptr = other.m_ptr; other.m_ptr = nullptr; }
            return *this;
        }
        T* get() const noexcept { return m_ptr; }
        T* release() noexcept { T* p = m_ptr; m_ptr = nullptr; return p; }
        explicit operator bool() const noexcept { return m_ptr != nullptr; }
        T& operator*() const noexcept { return *m_ptr; }
        T* operator->() const noexcept { return m_ptr; }
    private:
        T* m_ptr;
    };

    // RAII wrapper for HWINEVENTHOOK
    class ScopedWinEventHook {
    public:
        explicit ScopedWinEventHook(HWINEVENTHOOK hook = nullptr) noexcept : m_hook(hook) {}
        ~ScopedWinEventHook() { close(); }

        // Non-copyable
        ScopedWinEventHook(const ScopedWinEventHook&) = delete;
        ScopedWinEventHook& operator=(const ScopedWinEventHook&) = delete;

        // Movable
        ScopedWinEventHook(ScopedWinEventHook&& other) noexcept : m_hook(other.m_hook) { other.m_hook = nullptr; }
        ScopedWinEventHook& operator=(ScopedWinEventHook&& other) noexcept {
            if (this != &other) { close(); m_hook = other.m_hook; other.m_hook = nullptr; }
            return *this;
        }

        HWINEVENTHOOK get() const noexcept { return m_hook; }
        void reset(HWINEVENTHOOK hook = nullptr) noexcept { close(); m_hook = hook; }
        bool valid() const noexcept { return m_hook != nullptr; }
        explicit operator bool() const noexcept { return valid(); }

    private:
        void close() noexcept {
            if (valid()) { UnhookWinEvent(m_hook); m_hook = nullptr; }
        }
        HWINEVENTHOOK m_hook;
    };

    // RAII wrapper for PDH_HQUERY
    class ScopedPdhQuery {
    public:
        explicit ScopedPdhQuery(PDH_HQUERY query = nullptr) noexcept : m_query(query) {}
        ~ScopedPdhQuery() { close(); }

        // Non-copyable
        ScopedPdhQuery(const ScopedPdhQuery&) = delete;
        ScopedPdhQuery& operator=(const ScopedPdhQuery&) = delete;

        // Movable
        ScopedPdhQuery(ScopedPdhQuery&& other) noexcept : m_query(other.m_query) { other.m_query = nullptr; }
        ScopedPdhQuery& operator=(ScopedPdhQuery&& other) noexcept {
            if (this != &other) { close(); m_query = other.m_query; other.m_query = nullptr; }
            return *this;
        }

        PDH_HQUERY get() const noexcept { return m_query; }
        void reset(PDH_HQUERY query = nullptr) noexcept { close(); m_query = query; }
        bool valid() const noexcept { return m_query != nullptr; }
        explicit operator bool() const noexcept { return valid(); }

    private:
        void close() noexcept {
            if (valid()) { PdhCloseQuery(m_query); m_query = nullptr; }
        }
        PDH_HQUERY m_query;
    };
} // namespace wspa
