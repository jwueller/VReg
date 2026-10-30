#pragma once

#include <Windows.h>

namespace VReg
{
/**
 * @brief Holds an @ref SRWLOCK exclusively for its lifetime.
 *
 *  The library locks with @ref SRWLOCK rather than @ref std::mutex. Current
 *  MSVC headers give @ref std::mutex a constexpr constructor that only the
 *  newest `msvcp140.dll` understands; Wine's fails the lock and aborts the
 *  process. Building with @ref _DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR was passed
 *  over: it would still tie every consumer to some `msvcp140.dll`.
 */
class ExclusiveLock
{
public:
    explicit ExclusiveLock(SRWLOCK& lock) noexcept :
        m_lock(lock)
    {
        AcquireSRWLockExclusive(&m_lock);
    }

    ~ExclusiveLock()
    {
        ReleaseSRWLockExclusive(&m_lock);
    }

    ExclusiveLock(const ExclusiveLock&) = delete;
    ExclusiveLock& operator=(const ExclusiveLock&) = delete;

private:
    SRWLOCK& m_lock;
};
} // namespace VReg
