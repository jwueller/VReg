# The core library links without a C runtime. The program supplies the three
# functions the compiler calls for CopyMemory and structure copies (see
# support.c) and nothing else, so a C runtime function the library starts to
# call fails the link as an unresolved external.
add_executable(VReg_NoCrt
    main.c
    support.c
    vreg_impl.c
)

# Deliberately not VReg::VReg: a consumer of the single header has nothing
# but the file. The hooks stay out: Detours has C runtime needs of its own,
# and the claim is the core's.
target_include_directories(VReg_NoCrt PRIVATE
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(VReg_NoCrt PRIVATE
    VReg_BuildSettings
    kernel32
    advapi32
)

# CMake's default import libraries (user32, shell32, ole32 and others) would
# hide what the program really needs. /NODEFAULTLIB also drops the advapi32
# that vreg.h asks for in a pragma, so it is listed above.
set(CMAKE_C_STANDARD_LIBRARIES "")

# The project's normal flags, less what makes the compiler call into the C
# runtime, which a consumer without one switches off as well:
# - /GS puts a security cookie and handler in the program (__security_cookie,
#   __security_check_cookie, __GSHandlerCheck); /sdl implies it.
# - /RTC1 (Debug) calls _RTC_InitBase, _RTC_Shutdown and _RTC_CheckStackVars.
# - clang's loop idiom recognition turns the library's string length loops
#   into strlen and wcslen calls at -O2; -fno-builtin stops it.
# - MSVC for ARM64 compiles the Interlocked* intrinsics into calls to C
#   runtime functions that pick the instructions at run time
#   (/forceInterlockedFunctions, known to cl 19.44 and later).
# Not switched off: stack probes (__chkstk) and 64-bit arithmetic helpers
# (_allmul), which the library's frames and arithmetic never call for.
string(REPLACE "/RTC1" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")

target_compile_options(VReg_NoCrt PRIVATE
    /GS-
    $<$<C_COMPILER_ID:Clang>:/clang:-fno-builtin>
)

if(CMAKE_C_COMPILER_ID STREQUAL "MSVC"
        AND CMAKE_C_COMPILER_ARCHITECTURE_ID STREQUAL "ARM64"
        AND CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 19.44)
    target_compile_options(VReg_NoCrt PRIVATE
        /forceInterlockedFunctions-
    )
endif()

target_link_options(VReg_NoCrt PRIVATE
    /NODEFAULTLIB
    /ENTRY:NoCrtMain
    /SUBSYSTEM:CONSOLE
)

# No test dialogs: the program has no CRT to raise them, and its entry point
# turns off the system's.
add_test(NAME nocrt COMMAND VReg_NoCrt)

set_tests_properties(nocrt PROPERTIES
    TIMEOUT 120
)
