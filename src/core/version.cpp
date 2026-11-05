#include <VReg/version.h>

#include "core/version_info.h"

namespace VReg
{
FileVersion GetFileVersion() noexcept
{
    return FILE_VERSION;
}

_Ret_z_ LPCSTR GetVersion() noexcept
{
    return VERSION_TEXT;
}
} // namespace VReg
