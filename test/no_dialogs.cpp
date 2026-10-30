// Test processes run unattended (ctest, CI), where a modal dialog stalls the
// run until someone clicks it. Debug CRT assertions, abort() and crashes are
// reported on stderr instead, and the process fails.

#include <Windows.h>

#include <crtdbg.h>
#include <cstdlib>
#include <initializer_list>

namespace
{
bool DisableDialogs() noexcept
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    for (const int type : {_CRT_WARN, _CRT_ERROR, _CRT_ASSERT})
    {
        (void)type; // the _CrtSet* macros expand to nothing in release builds
        _CrtSetReportMode(type,_CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
        _CrtSetReportFile(type, _CRTDBG_FILE_STDERR);
    }
    return true;
}

// A named function, not a lambda: clang-tidy looks into a lambda's body and
// takes the C APIs there, which are not declared noexcept, for throwing.
[[maybe_unused]] const bool g_dialogsDisabled = DisableDialogs();
} // namespace
