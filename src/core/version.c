#include <VReg/version.h>

#include "core/version_info.h"

VREGDEF ULONGLONG WINAPI VRegGetFileVersion(void)
{
    return VREG__FILE_VERSION;
}

VREGDEF _Ret_z_ LPCSTR WINAPI VRegGetVersion(void)
{
    return VREG__VERSION_TEXT;
}
