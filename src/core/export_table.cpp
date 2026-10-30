#include "export_table.h"

#include <cstring>

namespace VReg
{
void* FindExport(const HMODULE hModule, const char* const pszName) noexcept
{
    if (!hModule)
        return nullptr;

    const auto* const pBase = reinterpret_cast<const BYTE*>(hModule);
    const auto* const pDos = reinterpret_cast<const IMAGE_DOS_HEADER*>(pBase);
    const auto* const pNt = reinterpret_cast<const IMAGE_NT_HEADERS*>(
        pBase + pDos->e_lfanew
    );
    const auto& directory =
        pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (directory.Size == 0)
        return nullptr;

    const auto* const pExports =
        reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(
            pBase + directory.VirtualAddress
        );
    const auto* const pNames = reinterpret_cast<const DWORD*>(
        pBase + pExports->AddressOfNames
    );
    const auto* const pOrdinals = reinterpret_cast<const WORD*>(
        pBase + pExports->AddressOfNameOrdinals
    );
    const auto* const pFunctions = reinterpret_cast<const DWORD*>(
        pBase + pExports->AddressOfFunctions
    );

    // The name table is sorted by ordinal string comparison; the loader
    // binary-searches it the same way.
    DWORD low = 0;
    DWORD high = pExports->NumberOfNames;
    while (low < high)
    {
        const auto middle = low + (high - low) / 2;
        const auto order = std::strcmp(
            pszName,
            reinterpret_cast<const char*>(pBase + pNames[middle])
        );
        if (order == 0)
        {
            const auto rva = pFunctions[pOrdinals[middle]];
            // An address inside the export directory is a forwarder string.
            if (
                rva >= directory.VirtualAddress
                && rva < directory.VirtualAddress + directory.Size
            )
                return nullptr;

            return reinterpret_cast<BYTE*>(hModule) + rva;
        }

        if (order < 0)
            high = middle;
        else
            low = middle + 1;
    }

    return nullptr;
}
} // namespace VReg
