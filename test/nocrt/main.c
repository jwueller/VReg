// A virtual registry over an in-memory hive, round-tripped without a C
// runtime: there is no startup code, so the entry point is ours and the exit
// code is the test result.
#include <vreg.h>

static BOOL RoundTrip(void)
{
    static const WCHAR szPath[] = L"Software\\NoCrt";
    static const WCHAR szName[] = L"Answer";
    VRegHive* pHive = NULL;
    VReg* pRegistry = NULL;
    const DWORD dwExpected = 42;
    DWORD dwValue = 0;
    DWORD cbValue = sizeof(dwValue);
    BOOL isOk = FALSE;

    if (VRegHive_CreateInMemory(&pHive) != ERROR_SUCCESS)
        goto done;
    if (VReg_CreateVirtual(NULL, &pRegistry) != ERROR_SUCCESS)
        goto done;
    if (VReg_Mount(pRegistry, HKEY_CURRENT_USER, szPath, pHive, 0) != ERROR_SUCCESS)
        goto done;
    if (VReg_RegSetKeyValueW(pRegistry, HKEY_CURRENT_USER, szPath, szName, REG_DWORD, &dwExpected, sizeof(dwExpected)) != ERROR_SUCCESS)
        goto done;
    if (VReg_RegGetValueW(pRegistry, HKEY_CURRENT_USER, szPath, szName, RRF_RT_REG_DWORD, NULL, &dwValue, &cbValue) != ERROR_SUCCESS)
        goto done;
    isOk = dwValue == dwExpected && cbValue == sizeof(dwValue);

done:
    VReg_Destroy(pRegistry);
    VRegHive_Destroy(pHive);
    return isOk;
}

void NoCrtMain(void)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    ExitProcess(RoundTrip() ? 0 : 1);
}
