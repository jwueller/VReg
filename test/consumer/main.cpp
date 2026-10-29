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
    return (VReg::GetVersion().wMajor == VReg::VREG_VERSION_MAJOR && status == ERROR_INVALID_DATA) ? 0 : 1;
}
