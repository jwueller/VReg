// The statuses entry points answer for exceptions they catch.

#include "core/status.h"

#include <catch2/catch_test_macros.hpp>

#include <new>
#include <stdexcept>
#include <string>

using namespace VReg;

namespace
{
template <typename Exception>
LSTATUS StatusFor(const Exception& exception) noexcept
try
{
    throw exception;
}
catch (...)
{
    return StatusFromException();
}
} // namespace

TEST_CASE("A failed allocation answers out of memory")
{
    CHECK(StatusFor(std::bad_alloc()) == ERROR_OUTOFMEMORY);
    CHECK(StatusFor(std::length_error("length")) == ERROR_OUTOFMEMORY);
}

TEST_CASE("Any other exception answers an internal error")
{
    CHECK(StatusFor(std::runtime_error("bug")) == ERROR_INTERNAL_ERROR);
    CHECK(StatusFor(std::out_of_range("bug")) == ERROR_INTERNAL_ERROR);
    CHECK(StatusFor(42) == ERROR_INTERNAL_ERROR);
}
