#include "core/value_type.h"

VREGDEF BOOL VReg__IsStringType(const DWORD dwType)
{
    return dwType == REG_SZ || dwType == REG_EXPAND_SZ || dwType == REG_MULTI_SZ;
}
