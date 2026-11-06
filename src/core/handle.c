#include <VReg/handle.h>

VREGDEF BOOL WINAPI VRegIsVirtualHKey(_In_opt_ const HKEY hKey)
{
    const UINT_PTR value = (UINT_PTR)hKey;
    return value != 0 && value <= VREG_VIRTUAL_HKEY_BASE && (value & 1) != 0;
}

VREGDEF BOOL WINAPI VRegIsPredefinedHKey(_In_opt_ const HKEY hKey)
{
    const UINT_PTR value = (UINT_PTR)hKey;
    return value >= (UINT_PTR)HKEY_CLASSES_ROOT && value <= (UINT_PTR)HKEY_CURRENT_USER_LOCAL_SETTINGS;
}
