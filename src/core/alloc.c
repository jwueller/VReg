#include "core/alloc.h"

VREGDEF void VReg__AssertFailed(void)
{
    if (IsDebuggerPresent())
        __debugbreak();
}

VREGDEF _Ret_maybenull_ void* VReg__Alloc(_In_opt_ void* const pAllocContext, const SIZE_T cb)
{
    (void)pAllocContext;
    if (cb == 0)
        return NULL;

    return VREG_MALLOC(pAllocContext, cb);
}

VREGDEF _Ret_maybenull_ void* VReg__AllocZeroed(_In_opt_ void* const pAllocContext, const SIZE_T cb)
{
    void* const p = VReg__Alloc(pAllocContext, cb);
    if (p)
        ZeroMemory(p, cb);

    return p;
}

VREGDEF _Ret_maybenull_ void* VReg__Realloc(_In_opt_ void* const pAllocContext, _In_opt_ void* const p, const SIZE_T cb)
{
    (void)pAllocContext;
    if (!p)
        return VReg__Alloc(pAllocContext, cb);
    if (cb == 0)
    {
        VREG_FREE(pAllocContext, p);
        return NULL;
    }

    return VREG_REALLOC(pAllocContext, p, cb);
}

VREGDEF void VReg__Free(_In_opt_ void* const pAllocContext, _In_opt_ void* const p)
{
    (void)pAllocContext;
    if (p)
        VREG_FREE(pAllocContext, p);
}
