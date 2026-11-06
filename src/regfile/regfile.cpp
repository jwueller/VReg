#include <VReg/regfile.h>

#include "core/status.h"

#include <new>

namespace VReg::RegFile
{
namespace
{
/// Frees a C script when it goes out of scope.
class OwnedScript
{
public:
    OwnedScript() noexcept = default;

    ~OwnedScript()
    {
        VRegDestroyScript(m_pScript);
    }

    OwnedScript(const OwnedScript&) = delete;
    OwnedScript& operator=(const OwnedScript&) = delete;

    [[nodiscard]] VRegScript** Receive() noexcept
    {
        return &m_pScript;
    }

    [[nodiscard]] VRegScript* Get() const noexcept
    {
        return m_pScript;
    }

private:
    VRegScript* m_pScript = nullptr;
};

std::wstring StringOf(const LPCWSTR psz)
{
    return psz ? std::wstring(psz) : std::wstring();
}

/// The C++ copy of a C script's operations.
Script ScriptOf(const VRegScript* const pScript)
{
    Script script;
    std::size_t cOperations = 0;
    const auto* const pOperations = VRegGetScriptOperations(pScript, &cOperations);
    script.operations.reserve(cOperations);
    for (std::size_t i = 0; i < cOperations; ++i)
    {
        const auto& operation = pOperations[i];
        const auto* const pb = reinterpret_cast<const std::byte*>(operation.pData);
        script.operations.push_back(
            {
                .kind = static_cast<OperationKind>(operation.kind),
                .hRoot = operation.hRoot,
                .path = StringOf(operation.pszPath),
                .name = StringOf(operation.pszName),
                .dwType = operation.dwType,
                .data = {pb, pb + operation.cbData},
                .line = operation.line,
            }
        );
    }
    return script;
}

ParseError ErrorOf(const VRegParseError& error) noexcept
{
    return {.line = error.line, .column = error.column, .pszMessage = error.pszMessage};
}

/// The bytes a script lays out as, copied out of the C allocation.
LSTATUS BytesOf(const VRegScript* const pScript, const Format format, std::vector<std::byte>& file)
{
    void* pBytes = nullptr;
    SIZE_T cbBytes = 0;
    const auto status = VRegWriteScript(pScript, static_cast<VRegFormat>(format), &pBytes, &cbBytes);
    if (status != ERROR_SUCCESS)
        return status;

    try
    {
        const auto* const pb = static_cast<const std::byte*>(pBytes);
        file.assign(pb, pb + cbBytes);
    }
    catch (...)
    {
        VRegFreeScriptBytes(pScript, pBytes);
        throw;
    }

    VRegFreeScriptBytes(pScript, pBytes);
    return ERROR_SUCCESS;
}
} // namespace

LSTATUS Parse(
    const std::span<const std::byte> file,
    Script& script,
    _Out_opt_ ParseError* const pError
) noexcept
try
{
    OwnedScript parsed;
    VRegParseError error{};
    const auto status = VRegParseScript(file.data(), file.size(), nullptr, parsed.Receive(), &error);
    if (pError)
        *pError = ErrorOf(error);
    if (status != ERROR_SUCCESS)
        return status;

    script = ScriptOf(parsed.Get());
    return ERROR_SUCCESS;
}
catch (...)
{
    return StatusFromException();
}

LSTATUS ParseFile(
    const std::filesystem::path& path,
    Script& script,
    _Out_opt_ ParseError* const pError
) noexcept
try
{
    OwnedScript parsed;
    VRegParseError error{};
    const auto status = VRegParseScriptFile(path.c_str(), nullptr, parsed.Receive(), &error);
    if (pError)
        *pError = ErrorOf(error);
    if (status != ERROR_SUCCESS)
        return status;

    script = ScriptOf(parsed.Get());
    return ERROR_SUCCESS;
}
catch (...)
{
    return StatusFromException();
}

_Success_(return == ERROR_SUCCESS)
LSTATUS Apply(
    const Script& script,
    Registry& registry,
    _On_failure_(_Out_opt_) std::size_t* const pFailed
) noexcept
try
{
    std::vector<VRegOperation> operations;
    operations.reserve(script.operations.size());
    for (const auto& operation : script.operations)
    {
        // A NUL inside a path or name would cut it short as a C string, which
        // the C++ shape can hold and the parser never produces.
        if (
            operation.path.find(L'\0') != std::wstring::npos
            || operation.name.find(L'\0') != std::wstring::npos
            || operation.data.size() > (std::numeric_limits<DWORD>::max)()
        )
        {
            if (pFailed)
                *pFailed = static_cast<std::size_t>(&operation - script.operations.data());
            return ERROR_INVALID_PARAMETER;
        }

        operations.push_back(
            {
                .kind = static_cast<VRegOperationKind>(operation.kind),
                .hRoot = operation.hRoot,
                .pszPath = operation.path.c_str(),
                .pszName = operation.name.c_str(),
                .dwType = operation.dwType,
                .pData = reinterpret_cast<const BYTE*>(operation.data.data()),
                .cbData = static_cast<DWORD>(operation.data.size()),
                .line = operation.line,
            }
        );
    }

    SIZE_T failed = 0;
    const auto status = VRegApplyOperations(operations.data(), operations.size(), registry.CObject(), &failed);
    if (status != ERROR_SUCCESS && pFailed)
        *pFailed = failed;
    return status;
}
catch (...)
{
    return StatusFromException();
}

LSTATUS ExportSnapshot(
    Registry& registry,
    const HKEY hRoot,
    _In_z_ const LPCWSTR pszPath,
    const Format format,
    std::vector<std::byte>& file
) noexcept
try
{
    OwnedScript script;
    const auto status = VRegSnapshotScript(registry.CObject(), hRoot, pszPath, nullptr, script.Receive());
    if (status != ERROR_SUCCESS)
        return status;

    return BytesOf(script.Get(), format, file);
}
catch (...)
{
    return StatusFromException();
}

LSTATUS ExportSnapshotToFile(
    Registry& registry,
    const HKEY hRoot,
    _In_z_ const LPCWSTR pszPath,
    const Format format,
    const std::filesystem::path& fileName
) noexcept
try
{
    OwnedScript script;
    const auto status = VRegSnapshotScript(registry.CObject(), hRoot, pszPath, nullptr, script.Receive());
    if (status != ERROR_SUCCESS)
        return status;

    return VRegWriteScriptFile(script.Get(), static_cast<VRegFormat>(format), fileName.c_str());
}
catch (...)
{
    return StatusFromException();
}

LSTATUS ExportDelta(
    const Hive& hive,
    const HKEY hRoot,
    _In_z_ const LPCWSTR pszPath,
    const Format format,
    std::vector<std::byte>& file
) noexcept
try
{
    OwnedScript script;
    const auto status = VRegDeltaScript(hive.Get(), hRoot, pszPath, nullptr, script.Receive());
    if (status != ERROR_SUCCESS)
        return status;

    return BytesOf(script.Get(), format, file);
}
catch (...)
{
    return StatusFromException();
}

LSTATUS ExportDeltaToFile(
    const Hive& hive,
    const HKEY hRoot,
    _In_z_ const LPCWSTR pszPath,
    const Format format,
    const std::filesystem::path& fileName
) noexcept
try
{
    OwnedScript script;
    const auto status = VRegDeltaScript(hive.Get(), hRoot, pszPath, nullptr, script.Receive());
    if (status != ERROR_SUCCESS)
        return status;

    return VRegWriteScriptFile(script.Get(), static_cast<VRegFormat>(format), fileName.c_str());
}
catch (...)
{
    return StatusFromException();
}
} // namespace VReg::RegFile
