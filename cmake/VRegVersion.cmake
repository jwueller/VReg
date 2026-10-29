# The version of the sources at a directory, computed the same way when CMake
# configures (include this file, then call vreg_compute_version) and on every
# build (run it with -P, which writes the header GetFileVersion() and
# GetVersion() report from, so a commit made since configuring cannot
# leave them stale).
#
# vcpkg.json owns MAJOR.MINOR.PATCH: the version under development, which its
# commit becomes once tagged v<version>. TWEAK counts the commits in the
# history, so the four-part number orders every build. The string is semver:
# the tagged commit built unchanged is the release (0.3.0+<commit>); any other
# build of a git checkout is a preview counting the commits since the last v*
# tag (0.3.0-preview.5+<commit>, -dirty after the commit for local changes to
# tracked files), so previews sort below their release. Outside a checkout, as
# from a release archive, TWEAK is 0 and the string the version alone, unless
# a commit is given for the build metadata.

# Sets VREG_VERSION (the full semver string) and VREG_VERSION_MAJOR, _MINOR,
# _PATCH and _TWEAK (its numbers) in the caller's scope for the sources at
# SOURCE_DIR; COMMIT, when not empty, replaces what git would say.
function(vreg_compute_version sourceDir commit)
    file(READ "${sourceDir}/vcpkg.json" manifest)
    string(JSON version GET "${manifest}" version-semver)
    if(NOT version MATCHES "^([0-9]+)[.]([0-9]+)[.]([0-9]+)$")
        message(FATAL_ERROR "vcpkg.json's version-semver is not MAJOR.MINOR.PATCH: ${version}")
    endif()
    set(major "${CMAKE_MATCH_1}")
    set(minor "${CMAKE_MATCH_2}")
    set(patch "${CMAKE_MATCH_3}")
    set(tweak 0)
    set(text "${version}")
    set(metadata "${commit}")

    # Only a checkout of these sources counts: unpacked from an archive into
    # some other repository (vcpkg's buildtrees inside a vcpkg clone, say),
    # git would answer for that repository instead.
    find_package(Git QUIET)
    set(isCheckout 1)
    if(NOT commit AND GIT_FOUND)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" rev-parse --show-toplevel --is-shallow-repository
            WORKING_DIRECTORY "${sourceDir}"
            OUTPUT_VARIABLE answers
            RESULT_VARIABLE isCheckout
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(isCheckout EQUAL 0)
            string(REPLACE "\n" ";" answers "${answers}")
            list(GET answers 0 topLevel)
            list(GET answers 1 shallow)
            file(REAL_PATH "${topLevel}" topLevel)
            file(REAL_PATH "${sourceDir}" sourceRoot)
            string(TOLOWER "${topLevel}" topLevel)
            string(TOLOWER "${sourceRoot}" sourceRoot)
            if(NOT topLevel STREQUAL sourceRoot)
                set(isCheckout 1)
            endif()
        endif()
    endif()
    if(NOT commit AND GIT_FOUND AND isCheckout EQUAL 0)
        if(shallow STREQUAL "true")
            message(FATAL_ERROR "The checkout is shallow, so its commits cannot be counted for the version: "
                                "fetch the whole history (git fetch --unshallow --tags).")
        endif()

        execute_process(
            COMMAND "${GIT_EXECUTABLE}" rev-list --count HEAD
            WORKING_DIRECTORY "${sourceDir}"
            OUTPUT_VARIABLE tweak
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(tweak GREATER 65535)
            message(FATAL_ERROR "The commit count ${tweak} does not fit the 16-bit field of a file version.")
        endif()

        execute_process(
            COMMAND "${GIT_EXECUTABLE}" describe --tags --match v[0-9]* --long --abbrev=40 --dirty
            WORKING_DIRECTORY "${sourceDir}"
            OUTPUT_VARIABLE described
            RESULT_VARIABLE describedResult
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(describedResult EQUAL 0 AND described MATCHES "^v([0-9.]+)-([0-9]+)-g([0-9a-f]+)(-dirty)?$")
            set(tag "${CMAKE_MATCH_1}")
            set(distance "${CMAKE_MATCH_2}")
            set(metadata "${CMAKE_MATCH_3}${CMAKE_MATCH_4}")
            if(tag STREQUAL version AND distance GREATER 0)
                message(FATAL_ERROR "v${version} is released and ${distance} commits follow it: "
                                    "raise the version in vcpkg.json to the next one.")
            endif()
            if(NOT (tag STREQUAL version AND metadata MATCHES "^[0-9a-f]+$"))
                string(APPEND text "-preview.${distance}")
            endif()
        else()
            # No version tag yet: the commit is known, the distance is not.
            execute_process(
                COMMAND "${GIT_EXECUTABLE}" describe --always --abbrev=40 --dirty --exclude=*
                WORKING_DIRECTORY "${sourceDir}"
                OUTPUT_VARIABLE metadata
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            string(APPEND text "-preview")
        endif()
    endif()

    if(metadata)
        string(APPEND text "+${metadata}")
    endif()

    set(VREG_VERSION_MAJOR "${major}" PARENT_SCOPE)
    set(VREG_VERSION_MINOR "${minor}" PARENT_SCOPE)
    set(VREG_VERSION_PATCH "${patch}" PARENT_SCOPE)
    set(VREG_VERSION_TWEAK "${tweak}" PARENT_SCOPE)
    set(VREG_VERSION "${text}" PARENT_SCOPE)
endfunction()

# Run with -P: writes OUTPUT for the sources at SOURCE_DIR, rewriting it only
# when its text changes, so only what includes it rebuilds.
if(CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)
    vreg_compute_version("${SOURCE_DIR}" "${COMMIT}")
    set(header "#pragma once\n\n")
    foreach(part IN ITEMS MAJOR MINOR PATCH TWEAK)
        string(APPEND header "#define VREG_VERSION_${part} ${VREG_VERSION_${part}}\n")
    endforeach()
    string(APPEND header "#define VREG_VERSION \"${VREG_VERSION}\"\n")
    if(EXISTS "${OUTPUT}")
        file(READ "${OUTPUT}" old)
    endif()
    if(NOT old STREQUAL header)
        file(WRITE "${OUTPUT}" "${header}")
    endif()
endif()
