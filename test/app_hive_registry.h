#pragma once

#include "forwarding_registry.h"
#include "objects.h"

#include <vreg.h>

#include <Windows.h>

namespace VRegTest
{
/// The native registry with `HKEY_CURRENT_USER` standing for the root of an
/// app hive, so a `.reg` file written for the current user applies to a
/// private hive.
class AppHiveRegistry final : public ForwardingRegistry<AppHiveRegistry>
{
public:
    explicit AppHiveRegistry(const HKEY hRoot) :
        m_native(MakeNativeRegistry()),
        m_hRoot(hRoot)
    {
    }

private:
    friend class ForwardingRegistry<AppHiveRegistry>;

    template <class Call>
    static LSTATUS Forward(
        VReg* const pThis,
        const char* const pszName,
        const HKEY hKey,
        Call&& call
    )
    {
        (void)pszName;
        auto& self = Self(pThis);
        return call(self.m_native, hKey == HKEY_CURRENT_USER ? self.m_hRoot : hKey);
    }

    OwnedRegistry m_native;
    HKEY m_hRoot;
};
} // namespace VRegTest
