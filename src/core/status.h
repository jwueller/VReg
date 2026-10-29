#pragma once

#include <Windows.h>

#include <exception>
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
 *  VReg and terminates, as it would escaping the `noexcept` boundary.
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
        std::terminate();
    }
}
} // namespace VReg
