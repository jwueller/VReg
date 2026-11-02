#pragma once

#include <Windows.h>

#include <new>
#include <stdexcept>

namespace VReg
{
/**
 * @brief The status a `noexcept` entry point answers for the exception
 *  being handled; call it only from a `catch (...)` block.
 *
 *  A failed allocation answers @ref ERROR_OUTOFMEMORY, as the native
 *  registry does when it runs out of memory. Any other exception is a bug in
 *  VReg and answers @ref ERROR_INTERNAL_ERROR: the call fails, but the
 *  process it runs in, often not VReg's own, keeps running. A debug build
 *  breaks into an attached debugger first.
 *
 *  Every entry point that can allocate is a function-try-block ending in
 *  `catch (...) { return StatusFromException(); }`; clang-tidy's
 *  `bugprone-exception-escape` finds one that is not.
 */
inline LSTATUS StatusFromException() noexcept
{
    try
    {
        throw;
    }
    catch (const std::bad_alloc&)
    {
        return ERROR_OUTOFMEMORY;
    }
    catch (const std::length_error&)
    {
        return ERROR_OUTOFMEMORY;
    }
    catch (...)
    {
#ifndef NDEBUG
        if (IsDebuggerPresent())
            __debugbreak();
#endif

        return ERROR_INTERNAL_ERROR;
    }
}

/// A status a failing path answers, which the analyzer cannot prove nonzero
/// when it was stored by another function.
_Post_satisfies_(return != ERROR_SUCCESS)
inline LSTATUS FailureStatus(const LSTATUS status) noexcept
{
    _Analysis_assume_(status != ERROR_SUCCESS);
    return status;
}
} // namespace VReg
