#include <vreg.h>

#include <Shlwapi.h>

int main()
{
    VRegScript* pScript = nullptr;
    const LSTATUS status = VRegScript_Parse("", 0, &pScript, nullptr);
    VRegScript_Destroy(pScript);
#ifdef CONSUMER_WITH_DETOURS
    VRegHooks_Neutralize();
#endif
    const ULONGLONG version = VREG_VERSION;
    const bool isFound =
        static_cast<WORD>(version >> 48) == CONSUMER_VERSION_MAJOR
        && static_cast<WORD>(version >> 32) == CONSUMER_VERSION_MINOR
        && static_cast<WORD>(version >> 16) == CONSUMER_VERSION_PATCH;
    return (isFound && status == ERROR_INVALID_DATA) ? 0 : 1;
}
