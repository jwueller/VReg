// One of two DLLs that each carry their own statically linked copy of VReg,
// as independent modules in one process would.

#include "hook_mount.h"

#ifndef VREG_HOOK_MODULE_NAME
#error "VREG_HOOK_MODULE_NAME names the module in its hive."
#endif

extern "C" __declspec(dllexport) LONG __cdecl HookModuleInstall(const wchar_t* const pszPath)
{
    return VReg::Test::InstallMount(pszPath, VREG_HOOK_MODULE_NAME);
}
