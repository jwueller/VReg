#include <VReg/core.h>
#include <VReg/regfile.h>

#ifdef CONSUMER_WITH_DETOURS
#include <VReg/detours.h>
#endif

int main()
{
    VReg::RegFile::Script script;
    const auto status = VReg::RegFile::Parse({}, script, nullptr);
#ifdef CONSUMER_WITH_DETOURS
    VReg::Detours::Neutralize();
#endif
    const auto version = VReg::GetVersion();
    const bool isFound = version.wMajor == CONSUMER_VERSION_MAJOR && version.wMinor == CONSUMER_VERSION_MINOR
                         && version.wPatch == CONSUMER_VERSION_PATCH;
    return (isFound && status == ERROR_INVALID_DATA) ? 0 : 1;
}
