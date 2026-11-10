// The size arithmetic of the private buffers: a request whose byte size would
// wrap a SIZE_T answers FALSE and leaves the instance as it was.

#include <vreg.h>

#include <catch2/catch_test_macros.hpp>

#include <string_view>

namespace
{
constexpr SIZE_T MAX_SIZE = ~SIZE_T{0};
} // namespace

TEST_CASE("A buffer refuses an append whose size would wrap")
{
    VRegInternalBuffer buffer = {0, 0, nullptr};
    REQUIRE(VRegInternalBuffer_Assign(&buffer, "abcd", 4));
    const auto cbCapacity = buffer.cbCapacity;
    const auto pb = buffer.pb;

    CHECK_FALSE(VRegInternalBuffer_Append(&buffer, "x", MAX_SIZE));
    CHECK_FALSE(VRegInternalBuffer_Append(&buffer, "x", MAX_SIZE - 3));
    CHECK(buffer.cb == 4);
    CHECK(buffer.cbCapacity == cbCapacity);
    CHECK(buffer.pb == pb);

    VRegInternalBuffer_Free(&buffer);
}

TEST_CASE("A string refuses a size whose byte count would wrap")
{
    VRegInternalWString text = {0, 0, nullptr};
    REQUIRE(VRegInternalWString_Assign(&text, L"abcd", 4));
    const auto cchCapacity = text.cchCapacity;
    const auto psz = text.psz;

    CHECK_FALSE(VRegInternalWString_Reserve(&text, MAX_SIZE));
    CHECK_FALSE(VRegInternalWString_Reserve(&text, MAX_SIZE / sizeof(wchar_t)));
    CHECK_FALSE(VRegInternalWString_Resize(&text, MAX_SIZE));
    CHECK_FALSE(VRegInternalWString_Resize(&text, MAX_SIZE / sizeof(wchar_t)));
    CHECK_FALSE(VRegInternalWString_Append(&text, L"x", MAX_SIZE));
    CHECK_FALSE(VRegInternalWString_Append(&text, L"x", MAX_SIZE - 3));
    CHECK(text.cch == 4);
    CHECK(text.cchCapacity == cchCapacity);
    CHECK(text.psz == psz);
    CHECK(std::wstring_view(text.psz) == L"abcd");

    VRegInternalWString_Free(&text);
}

TEST_CASE("An array refuses a capacity whose byte size would wrap")
{
    VRegInternalArray array = {nullptr, 0, 0};
    const int element = 7;
    REQUIRE(VRegInternalArray_Append(&array, sizeof(element), &element));
    const auto capacity = array.capacity;
    const auto p = array.p;

    CHECK_FALSE(VRegInternalArray_Reserve(&array, sizeof(element), MAX_SIZE));
    CHECK_FALSE(VRegInternalArray_Reserve(&array, sizeof(element), MAX_SIZE / sizeof(element) + 1));
    CHECK(array.count == 1);
    CHECK(array.capacity == capacity);
    CHECK(array.p == p);

    VRegInternalArray_Free(&array);
}
