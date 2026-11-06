#include "core/export_table.h"

/// Makes every module linking VReg import advapi32, so the loader maps it
/// before VReg runs and resolution finds it without loading anything. The
/// address is taken in a function body: an initializer would need the import
/// thunk's address, which C does not guarantee to be constant.
static void* volatile g_pfnImportAdvapi32;

/// Ordinal comparison of two export names: negative, zero or positive.
static int VReg__CompareExportNames(_In_z_ const char* pszLeft, _In_z_ const char* pszRight)
{
    while (*pszLeft != '\0' && *pszLeft == *pszRight)
    {
        ++pszLeft;
        ++pszRight;
    }

    return (int)(unsigned char)*pszLeft - (int)(unsigned char)*pszRight;
}

_Ret_maybenull_ void* VReg__FindExport(_In_opt_ const HMODULE hModule, _In_z_ const char* const pszName)
{
    const BYTE* pBase;
    const IMAGE_DOS_HEADER* pDos;
    const IMAGE_NT_HEADERS* pNt;
    const IMAGE_DATA_DIRECTORY* pDirectory;
    const IMAGE_EXPORT_DIRECTORY* pExports;
    const DWORD* pNames;
    const WORD* pOrdinals;
    const DWORD* pFunctions;
    DWORD low;
    DWORD high;

    if (!hModule)
        return NULL;

    pBase = (const BYTE*)hModule;
    pDos = (const IMAGE_DOS_HEADER*)pBase;
    pNt = (const IMAGE_NT_HEADERS*)(pBase + pDos->e_lfanew);
    pDirectory = &pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (pDirectory->Size == 0)
        return NULL;

    pExports = (const IMAGE_EXPORT_DIRECTORY*)(pBase + pDirectory->VirtualAddress);
    pNames = (const DWORD*)(pBase + pExports->AddressOfNames);
    pOrdinals = (const WORD*)(pBase + pExports->AddressOfNameOrdinals);
    pFunctions = (const DWORD*)(pBase + pExports->AddressOfFunctions);

    // The name table is sorted by ordinal string comparison; the loader
    // binary-searches it the same way.
    low = 0;
    high = pExports->NumberOfNames;
    while (low < high)
    {
        const DWORD middle = low + (high - low) / 2;
        const int order = VReg__CompareExportNames(pszName, (const char*)(pBase + pNames[middle]));
        if (order == 0)
        {
            const DWORD rva = pFunctions[pOrdinals[middle]];

            // An address inside the export directory is a forwarder string.
            if (rva >= pDirectory->VirtualAddress && rva < pDirectory->VirtualAddress + pDirectory->Size)
                return NULL;

            return (BYTE*)hModule + rva;
        }

        if (order < 0)
            high = middle;
        else
            low = middle + 1;
    }

    return NULL;
}

_Ret_maybenull_ void* VReg__FindRegistryExport(_In_z_ const char* const pszName)
{
    void* const pFunction = VReg__FindExport(GetModuleHandleW(L"KernelBase.dll"), pszName);
    g_pfnImportAdvapi32 = (void*)&RegOpenKeyW;
    if (pFunction)
        return pFunction;

    return VReg__FindExport(GetModuleHandleW(L"advapi32.dll"), pszName);
}
