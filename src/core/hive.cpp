#include <VReg/hive.h>

#include "core/hive.h"
#include "core/status.h"

#include <new>

namespace VReg
{
namespace
{
/// The hive's allocation context frees what the hive handed out.
class OwnedWString
{
public:
    explicit OwnedWString(VRegHive* const pHive) noexcept :
        m_pHive(pHive)
    {
    }

    ~OwnedWString()
    {
        VReg__WStringFree(VReg__HiveAllocContext(m_pHive), &m_string);
    }

    OwnedWString(const OwnedWString&) = delete;
    OwnedWString& operator=(const OwnedWString&) = delete;

    [[nodiscard]] VReg__WString* Get() noexcept
    {
        return &m_string;
    }

    [[nodiscard]] std::wstring Str() const
    {
        return {VReg__WStringText(&m_string), m_string.cch};
    }

private:
    VRegHive* m_pHive;
    VReg__WString m_string{};
};

class OwnedBuffer
{
public:
    explicit OwnedBuffer(VRegHive* const pHive) noexcept :
        m_pHive(pHive)
    {
    }

    ~OwnedBuffer()
    {
        VReg__BufferFree(VReg__HiveAllocContext(m_pHive), &m_buffer);
    }

    OwnedBuffer(const OwnedBuffer&) = delete;
    OwnedBuffer& operator=(const OwnedBuffer&) = delete;

    [[nodiscard]] VReg__Buffer* Get() noexcept
    {
        return &m_buffer;
    }

    [[nodiscard]] std::vector<std::byte> Bytes() const
    {
        const auto* const pb = reinterpret_cast<const std::byte*>(m_buffer.pb);
        return {pb, pb + m_buffer.cb};
    }

private:
    VRegHive* m_pHive;
    VReg__Buffer m_buffer{};
};

std::vector<VReg__WStringView> ViewsOf(const KeyPath path)
{
    std::vector<VReg__WStringView> views;
    views.reserve(path.size());
    for (const auto name : path)
        views.push_back({name.data(), name.size()});

    return views;
}

OpenResult ResultOf(const VReg__OpenResult& result) noexcept
{
    return {.pNode = result.pNode, .hLower = result.hLower};
}

std::wstring StringOf(const VReg__WStringView view)
{
    return {view.pch, view.cch};
}

LSTATUS CollectUpper(void* const pContext, const VReg__UpperRecord* const pRecord) noexcept
try
{
    auto& records = *static_cast<std::vector<UpperRecord>*>(pContext);
    const auto* const pb = reinterpret_cast<const std::byte*>(pRecord->pData);
    records.push_back(
        {
            .kind = pRecord->kind == VREG__UPPER_VALUE ? UpperRecord::Kind::Value : UpperRecord::Kind::Key,
            .path = StringOf(pRecord->path),
            .name = StringOf(pRecord->name),
            .dwType = pRecord->dwType,
            .data = {pb, pb + pRecord->cbData},
            .isTombstone = pRecord->isTombstone != FALSE,
            .isOpaque = pRecord->isOpaque != FALSE,
        }
    );
    return ERROR_SUCCESS;
}
catch (...)
{
    return StatusFromException();
}
} // namespace

Hive::Hive()
{
    if (VRegCreateHive(nullptr, &m_pHive) != ERROR_SUCCESS)
        throw std::bad_alloc();
}

Hive::Hive(
    Registry& lower,
    const HKEY hLowerRoot,
    _In_z_ const LPCWSTR pszLowerPath,
    const REGSAM samLowerView
)
{
    const auto status = VRegCreateLayeredHive(
        lower.CObject(),
        hLowerRoot,
        pszLowerPath,
        samLowerView,
        nullptr,
        &m_pHive
    );
    if (status != ERROR_SUCCESS)
        throw std::bad_alloc();
}

Hive::~Hive()
{
    VRegDestroyHive(m_pHive);
}

_Ret_notnull_ VRegHive* Hive::Get() const noexcept
{
    return m_pHive;
}

void Hive::Freeze() noexcept
{
    VRegFreezeHive(m_pHive);
}

bool Hive::IsFrozen() const noexcept
{
    return VRegIsHiveFrozen(m_pHive) != FALSE;
}

_Ret_notnull_ Node* Hive::Root() const noexcept
{
    return VReg__HiveRoot(m_pHive);
}

HKEY Hive::HandleOf(_In_ const Node* const pNode) noexcept
{
    return VReg__NodeHandle(pNode);
}

_Ret_maybenull_ Node* Hive::NodeOf(const HKEY hKey) noexcept
{
    return VReg__NodeOf(hKey);
}

_Ret_notnull_ VRegHive* Hive::HiveOf(_In_ const Node* const pNode) noexcept
{
    return VReg__NodeHive(pNode);
}

LSTATUS Hive::OpenKey(
    _In_ Node* const pParent,
    const KeyPath path,
    const REGSAM samDesired,
    OpenResult& result
) const noexcept
try
{
    const auto views = ViewsOf(path);
    VReg__OpenResult opened{};
    const auto status = VReg__HiveOpenKey(m_pHive, pParent, views.data(), views.size(), samDesired, &opened);
    result = ResultOf(opened);
    return status;
}
catch (...)
{
    return StatusFromException();
}

LSTATUS Hive::CreateKey(
    _Inout_ Node* const pParent,
    const KeyPath path,
    _In_z_ const LPCWSTR pszClassName,
    const DWORD dwOptions,
    const REGSAM samDesired,
    OpenResult& result,
    DWORD& dwDisposition
) noexcept
try
{
    const auto views = ViewsOf(path);
    VReg__OpenResult opened{};
    const auto status = VReg__HiveCreateKey(
        m_pHive,
        pParent,
        views.data(),
        views.size(),
        pszClassName,
        dwOptions,
        samDesired,
        &opened,
        &dwDisposition
    );
    result = ResultOf(opened);
    return status;
}
catch (...)
{
    return StatusFromException();
}

LSTATUS Hive::DeleteKey(
    _Inout_ Node* const pParent,
    const KeyPath path
) noexcept
try
{
    const auto views = ViewsOf(path);
    return VReg__HiveDeleteKey(m_pHive, pParent, views.data(), views.size());
}
catch (...)
{
    return StatusFromException();
}

LSTATUS Hive::DeleteTree(
    _Inout_ Node* const pNode
) noexcept
{
    return VReg__HiveDeleteTree(m_pHive, pNode);
}

LSTATUS Hive::QueryValue(
    _In_ Node* const pNode,
    _In_z_ const LPCWSTR pszName,
    DWORD& dwType,
    DWORD& cbData,
    _Out_opt_ std::vector<std::byte>* const pData
) const noexcept
try
{
    OwnedBuffer data(m_pHive);
    const auto status = VReg__HiveQueryValue(m_pHive, pNode, pszName, &dwType, &cbData, pData ? data.Get() : nullptr);
    if (status == ERROR_SUCCESS && pData)
        *pData = data.Bytes();
    return status;
}
catch (...)
{
    return StatusFromException();
}

LSTATUS Hive::SetValue(
    _Inout_ Node* const pNode,
    _In_z_ const LPCWSTR pszName,
    const DWORD dwType,
    const std::span<const std::byte> data
) noexcept
{
    return VReg__HiveSetValue(
        m_pHive,
        pNode,
        pszName,
        dwType,
        reinterpret_cast<const BYTE*>(data.data()),
        static_cast<DWORD>(data.size())
    );
}

LSTATUS Hive::DeleteValue(
    _Inout_ Node* const pNode,
    _In_z_ const LPCWSTR pszName
) noexcept
{
    return VReg__HiveDeleteValue(m_pHive, pNode, pszName);
}

LSTATUS Hive::SetMuiTextValue(
    _Inout_ Node* const pNode,
    _In_z_ const LPCWSTR pszName,
    _In_z_ const LPCWSTR pszText
) noexcept
{
    return VReg__HiveSetMuiTextValue(m_pHive, pNode, pszName, pszText);
}

LSTATUS Hive::QueryMuiText(
    _In_ Node* const pNode,
    _In_z_ const LPCWSTR pszName,
    std::wstring& text
) const noexcept
try
{
    OwnedWString owned(m_pHive);
    const auto status = VReg__HiveQueryMuiText(m_pHive, pNode, pszName, owned.Get());
    if (status == ERROR_SUCCESS)
        text = owned.Str();
    return status;
}
catch (...)
{
    return StatusFromException();
}

LSTATUS Hive::EnumKey(
    _In_ Node* const pNode,
    const DWORD dwIndex,
    std::wstring& name,
    std::wstring& className,
    FILETIME& lastWrite
) const noexcept
try
{
    OwnedWString ownedName(m_pHive);
    OwnedWString ownedClass(m_pHive);
    const auto status = VReg__HiveEnumKey(m_pHive, pNode, dwIndex, ownedName.Get(), ownedClass.Get(), &lastWrite);
    if (status == ERROR_SUCCESS)
    {
        name = ownedName.Str();
        className = ownedClass.Str();
    }
    return status;
}
catch (...)
{
    return StatusFromException();
}

LSTATUS Hive::EnumValue(
    _In_ Node* const pNode,
    const DWORD dwIndex,
    std::wstring& name,
    DWORD& dwType,
    DWORD& cbData,
    _Out_opt_ std::vector<std::byte>* const pData
) const noexcept
try
{
    OwnedWString ownedName(m_pHive);
    OwnedBuffer data(m_pHive);
    const auto status = VReg__HiveEnumValue(
        m_pHive,
        pNode,
        dwIndex,
        ownedName.Get(),
        &dwType,
        &cbData,
        pData ? data.Get() : nullptr
    );
    if (status == ERROR_SUCCESS)
    {
        name = ownedName.Str();
        if (pData)
            *pData = data.Bytes();
    }
    return status;
}
catch (...)
{
    return StatusFromException();
}

LSTATUS Hive::QueryInfo(
    _In_ Node* const pNode,
    KeyInfo& info
) const noexcept
try
{
    OwnedWString className(m_pHive);
    VReg__KeyInfo cInfo{};
    cInfo.className = *className.Get();
    const auto status = VReg__HiveQueryInfo(m_pHive, pNode, &cInfo);
    *className.Get() = cInfo.className;
    if (status == ERROR_SUCCESS)
    {
        info = {
            .className = className.Str(),
            .cSubKeys = cInfo.cSubKeys,
            .cchMaxSubKey = cInfo.cchMaxSubKey,
            .cchMaxClass = cInfo.cchMaxClass,
            .cValues = cInfo.cValues,
            .cchMaxValueName = cInfo.cchMaxValueName,
            .cbMaxValue = cInfo.cbMaxValue,
            .lastWrite = cInfo.lastWrite,
        };
    }
    return status;
}
catch (...)
{
    return StatusFromException();
}

LSTATUS Hive::CheckKey(
    _In_ Node* const pNode
) const noexcept
{
    return VReg__HiveCheckKey(m_pHive, pNode);
}

LSTATUS Hive::Watch(
    _In_ Node* const pNode,
    const BOOL bWatchSubtree,
    const DWORD dwFilter,
    const HANDLE hEvent
) const noexcept
{
    return VReg__HiveWatch(m_pHive, pNode, bWatchSubtree, dwFilter, hEvent);
}

LSTATUS Hive::SnapshotUpper(std::vector<UpperRecord>& records) const noexcept
try
{
    std::vector<UpperRecord> result;
    const auto status = VReg__HiveSnapshotUpper(m_pHive, &CollectUpper, &result);
    if (status == ERROR_SUCCESS)
        records = std::move(result);
    return status;
}
catch (...)
{
    return StatusFromException();
}

_Ret_maybenull_ VRegRegistry* Hive::Lower() const noexcept
{
    return VReg__HiveLower(m_pHive);
}

LSTATUS Hive::OpenLowerValueKey(
    _In_ Node* const pNode,
    _In_z_ const LPCWSTR pszName,
    HKEY& hLower
) const noexcept
{
    return VReg__HiveOpenLowerValueKey(m_pHive, pNode, pszName, &hLower);
}
} // namespace VReg
