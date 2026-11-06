#include <VReg/core.h>
#include <VReg/regfile.h>

#ifdef CONSUMER_WITH_DETOURS
#include <VReg/detours.h>
#endif

#include <Shlwapi.h>

int main()
{
    VRegScript* pScript = nullptr;
    const LSTATUS status = VRegParseScript("", 0, nullptr, &pScript, nullptr);
    VRegDestroyScript(pScript);
#ifdef CONSUMER_WITH_DETOURS
    VRegNeutralizeHooks();
#endif
    const ULONGLONG version = VRegGetFileVersion();
    const bool isFound =
        static_cast<WORD>(version >> 48) == CONSUMER_VERSION_MAJOR
        && static_cast<WORD>(version >> 32) == CONSUMER_VERSION_MINOR
        && static_cast<WORD>(version >> 16) == CONSUMER_VERSION_PATCH;
    return (isFound && status == ERROR_INVALID_DATA) ? 0 : 1;
}
