# Run with -P: writes the single-header form of VReg (vreg.h) into OUTPUT_DIR
# for the sources at SOURCE_DIR, rewriting it only when its text changes. COMMIT and STRICT mean what they do for
# cmake/VRegVersion.cmake. The tree under include/ and src/ stays the source;
# nothing generated is edited.

include(${CMAKE_CURRENT_LIST_DIR}/VRegVersion.cmake)

# Text travels through global properties and list splitting, which both treat
# semicolons, brackets and backslashes specially; they are swapped for tokens
# and restored on write.
function(vreg_escape var)
    string(REPLACE "\\" "@VREG_BACKSLASH@" ${var} "${${var}}")
    string(REPLACE ";" "@VREG_SEMI@" ${var} "${${var}}")
    string(REPLACE "[" "@VREG_LBRACKET@" ${var} "${${var}}")
    string(REPLACE "]" "@VREG_RBRACKET@" ${var} "${${var}}")
    set(${var} "${${var}}" PARENT_SCOPE)
endfunction()

function(vreg_unescape var)
    string(REPLACE "@VREG_SEMI@" ";" ${var} "${${var}}")
    string(REPLACE "@VREG_LBRACKET@" "[" ${var} "${${var}}")
    string(REPLACE "@VREG_RBRACKET@" "]" ${var} "${${var}}")
    string(REPLACE "@VREG_BACKSLASH@" "\\" ${var} "${${var}}")
    set(${var} "${${var}}" PARENT_SCOPE)
endfunction()

function(vreg_put text)
    vreg_escape(text)
    set_property(GLOBAL APPEND_STRING PROPERTY VREG_OUT "${text}")
endfunction()

# Emits a source file where it is first included. Every file is emitted once
# across all output; `#pragma once` goes, since the output has explicit guards.
function(vreg_inline path)
    get_property(done GLOBAL PROPERTY VREG_EMITTED)
    if(path IN_LIST done)
        return()
    endif()
    set_property(GLOBAL APPEND PROPERTY VREG_EMITTED "${path}")

    file(RELATIVE_PATH rel "${SOURCE_DIR}" "${path}")
    get_filename_component(dir "${path}" DIRECTORY)
    vreg_put("// --- ${rel} ---\n")

    file(READ "${path}" text)
    string(REPLACE "\r\n" "\n" text "${text}")
    vreg_escape(text)
    string(REPLACE "\n" ";L" text "L${text}")
    set(lines ${text})
    list(GET lines -1 last)
    if(last STREQUAL "L")
        list(POP_BACK lines)
    endif()

    foreach(item IN LISTS lines)
        string(SUBSTRING "${item}" 1 -1 line)
        if(line STREQUAL "#pragma once")
            continue()
        endif()

        if(line MATCHES "^#include[ \t]*([<\"])([^>\"]*)[>\"]")
            set(open "${CMAKE_MATCH_1}")
            set(name "${CMAKE_MATCH_2}")
            if(open STREQUAL "<" AND name MATCHES "^VReg/")
                vreg_inline("${SOURCE_DIR}/include/${name}")
                continue()
            elseif(open STREQUAL "\"")
                if(name STREQUAL "version_info.h")
                    continue()
                elseif(EXISTS "${dir}/${name}")
                    vreg_inline("${dir}/${name}")
                elseif(EXISTS "${SOURCE_DIR}/src/${name}")
                    vreg_inline("${SOURCE_DIR}/src/${name}")
                else()
                    message(FATAL_ERROR "${rel}: cannot resolve ${line}")
                endif()
                continue()
            endif()
        endif()

        # The text is already escaped, so it goes in as it is.
        set_property(GLOBAL APPEND_STRING PROPERTY VREG_OUT "${line}\n")
    endforeach()
    vreg_put("\n")
endfunction()

function(vreg_write name)
    get_property(text GLOBAL PROPERTY VREG_OUT)
    vreg_unescape(text)
    set(file "${OUTPUT_DIR}/${name}")
    if(EXISTS "${file}")
        file(READ "${file}" old)
    endif()
    if(NOT old STREQUAL text)
        file(WRITE "${file}" "${text}")
    endif()
endfunction()

vreg_compute_version("${SOURCE_DIR}" "${COMMIT}" "${STRICT}")

# MIT wants its notice in every copy, and a single header is copied on its own.
file(READ "${SOURCE_DIR}/LICENSE" license)
string(REPLACE "\r\n" "\n" license "${license}")
string(STRIP "${license}" license)
string(REPLACE "\n" "\n/// " license "/// ${license}")
string(REPLACE "/// \n" "///\n" license "${license}")

# Agent's choice: a BOM, since the license is UTF-8 and MSVC reads a file
# without one in the consumer's code page, where /WX turns a misread comment
# (C4819) into an error. Transliterating the license was passed over: it is
# the copyright holder's name.
string(ASCII 239 187 191 bom)

set_property(GLOBAL PROPERTY VREG_EMITTED "")
set_property(GLOBAL PROPERTY VREG_OUT "")

# Agent's choice: the hooks are opt-in through a macro named like the CMake
# option, rather than through a second header, so one file is all a consumer
# copies.
vreg_put("${bom}/// @file vreg.h
/// @brief A virtual Windows registry for C++, as an overlay over the real one,
///  or held entirely in memory.
/// @version ${VREG_VERSION}
/// @note Generated from VReg's sources: do not edit.
///
/// @details Include it from any file. In exactly one `.cpp` file per module,
///  define `VREG_IMPLEMENTATION` before including it to compile the library
///  there; it links `advapi32` by itself.
///
///  Defining `VREG_WITH_DETOURS` wherever it is included adds
///  @ref VReg::Detours. That needs Microsoft Detours: `<detours/detours.h>`
///  on the include path, and its library linked.
///
/// @copyright
/// @verbatim
${license}
/// @endverbatim

// The guards are explicit instead of `#pragma once` because the header is
// entered again in the file that defines VREG_IMPLEMENTATION after an
// earlier include without it; the hooks have guards of their own for the
// same reason.

#ifndef VREG_H_INCLUDED
#define VREG_H_INCLUDED

")
vreg_inline("${SOURCE_DIR}/include/VReg/core.h")
vreg_inline("${SOURCE_DIR}/include/VReg/regfile.h")
vreg_put("#endif // VREG_H_INCLUDED

#ifdef VREG_WITH_DETOURS
#ifndef VREG_DETOURS_H_INCLUDED
#define VREG_DETOURS_H_INCLUDED

")
vreg_inline("${SOURCE_DIR}/include/VReg/detours.h")
vreg_put("#endif // VREG_DETOURS_H_INCLUDED
#endif // VREG_WITH_DETOURS

#ifdef VREG_IMPLEMENTATION
#ifndef VREG_IMPLEMENTATION_INCLUDED
#define VREG_IMPLEMENTATION_INCLUDED

#pragma comment(lib, \"advapi32\")

")
foreach(part IN ITEMS MAJOR MINOR PATCH TWEAK)
    vreg_put("#define VREG_VERSION_${part} ${VREG_VERSION_${part}}\n")
endforeach()
vreg_put("#define VREG_VERSION \"${VREG_VERSION}\"\n\n")

# The sources in lexical order, core before regfile.
file(GLOB core_sources "${SOURCE_DIR}/src/core/*.cpp")
file(GLOB regfile_sources "${SOURCE_DIR}/src/regfile/*.cpp")
list(SORT core_sources)
list(SORT regfile_sources)
foreach(source IN LISTS core_sources regfile_sources)
    vreg_inline("${source}")
endforeach()
vreg_put("#undef VREG_VERSION_MAJOR
#undef VREG_VERSION_MINOR
#undef VREG_VERSION_PATCH
#undef VREG_VERSION_TWEAK
#undef VREG_VERSION

#endif // VREG_IMPLEMENTATION_INCLUDED

// The hooks use the private headers the implementation above brought in.
#ifdef VREG_WITH_DETOURS
#ifndef VREG_DETOURS_IMPLEMENTATION_INCLUDED
#define VREG_DETOURS_IMPLEMENTATION_INCLUDED

")
vreg_inline("${SOURCE_DIR}/src/detours/hooks.cpp")
vreg_put("#endif // VREG_DETOURS_IMPLEMENTATION_INCLUDED
#endif // VREG_WITH_DETOURS
#endif // VREG_IMPLEMENTATION
")
vreg_write(vreg.h)
