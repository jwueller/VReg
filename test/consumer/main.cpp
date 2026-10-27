#include <VReg/VReg.h>

int main()
{
    return VReg::GetVersion().major == VReg::kVersionMajor ? 0 : 1;
}
