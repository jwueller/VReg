#pragma once

#include <VReg/api.h>

#include "core/buffer.h"

#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/// The longest key name component the registry accepts, in characters.
#define VREG__MAX_KEY_NAME_CHARS ((SIZE_T)255)

/// Splits @p path at backslashes as the native registry does, appending the
/// components (views into @p path) to @p pComponents, an array of
/// @ref VReg__WStringView: empty components are skipped, a trailing backslash
/// is ignored, a leading one answers @ref ERROR_BAD_PATHNAME and a component
/// over @ref VREG__MAX_KEY_NAME_CHARS @ref ERROR_INVALID_PARAMETER.
VREGDEF LSTATUS VReg__SplitPath(_In_opt_ void* pAllocContext, VReg__WStringView path, _Inout_ VReg__Array* pComponents);

#ifdef __cplusplus
} // extern "C"
#endif
