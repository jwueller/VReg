#pragma once

#include <Windows.h>

#include <functional>
#include <utility>

namespace VReg::Test
{
/**
 * @brief Runs a function on a thread of its own; the destructor joins it.
 *
 *  A Win32 thread rather than `std::async` or `std::promise`: under Wine,
 *  whose `msvcp140.dll` predates the current headers, a `std::future` loses
 *  its wakeup or aborts the process.
 */
class Thread
{
public:
    explicit Thread(std::function<void()> function) :
        m_function(std::move(function))
    {
        m_hThread = CreateThread(nullptr, 0, &Run, this, 0, nullptr);
    }

    ~Thread()
    {
        if (m_hThread)
        {
            WaitForSingleObject(m_hThread, INFINITE);
            CloseHandle(m_hThread);
        }
    }

    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

    /// Whether the function returned within @p dwMilliseconds.
    [[nodiscard]] bool Wait(const DWORD dwMilliseconds) const noexcept
    {
        return WaitForSingleObject(m_hThread, dwMilliseconds) == WAIT_OBJECT_0;
    }

private:
    static DWORD WINAPI Run(const LPVOID pParameter)
    {
        static_cast<Thread*>(pParameter)->m_function();
        return 0;
    }

    std::function<void()> m_function;
    HANDLE m_hThread = nullptr;
};
} // namespace VReg::Test
