#include <VReg/VReg.h>
#include <VReg/regfile.h>

int main()
{
    VReg::RegFile::Script script;
    const auto status = VReg::RegFile::Parse({}, script, nullptr);
    return (VReg::GetVersion().major == VReg::kVersionMajor && status == ERROR_INVALID_DATA) ? 0 : 1;
}
