#include "core/path.h"

VREGDEF LSTATUS VReg__SplitPath(
    _In_opt_ void* const pAllocContext,
    const VReg__WStringView path,
    _Inout_ VReg__Array* const pComponents
)
{
    SIZE_T start = 0;
    if (path.cch != 0 && path.pch[0] == L'\\')
        return ERROR_BAD_PATHNAME;

    while (start < path.cch)
    {
        SIZE_T end = start;
        VReg__WStringView component;
        while (end < path.cch && path.pch[end] != L'\\')
            ++end;

        component.pch = path.pch + start;
        component.cch = end - start;
        if (component.cch != 0)
        {
            if (component.cch > VREG__MAX_KEY_NAME_CHARS)
                return ERROR_INVALID_PARAMETER;
            if (!VReg__ArrayAppend(pAllocContext, pComponents, sizeof(component), &component))
                return ERROR_OUTOFMEMORY;
        }

        start = end + 1;
    }

    return ERROR_SUCCESS;
}
