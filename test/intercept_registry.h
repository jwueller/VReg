#pragma once

#include "forwarding_registry.h"

#include <vreg.h>

#include <Windows.h>

#include <cstring>
#include <functional>
#include <map>
#include <optional>
#include <string>

namespace VRegTest
{
/// A registry that forwards to another one, can fail chosen calls with chosen
/// statuses and reports every call it receives before forwarding it, except
/// `RegCloseKey`: a close is cleanup, never reported and never failed.
class InterceptRegistry final : public ForwardingRegistry<InterceptRegistry>
{
public:
    explicit InterceptRegistry(VReg* const pInner) :
        m_pInner(pInner)
    {
    }

    /// Calls named here (`RegOpenKeyExW`, ...) answer the status instead of
    /// being forwarded.
    std::map<std::string, LSTATUS> failures;
    std::function<void(const std::string&)> onCall;

private:
    friend class ForwardingRegistry<InterceptRegistry>;

    template <class Call>
    static LSTATUS Forward(
        VReg* const pThis,
        const char* const pszName,
        const HKEY hKey,
        Call&& call
    )
    {
        auto& self = Self(pThis);
        if (std::strcmp(pszName, "RegCloseKey") != 0)
        {
            if (const auto status = self.Enter(pszName))
                return *status;
        }
        return call(self.m_pInner, hKey);
    }

    std::optional<LSTATUS> Enter(const char* const pszName) const
    {
        if (onCall)
            onCall(pszName);

        const auto it = failures.find(pszName);
        if (it == failures.end())
            return std::nullopt;

        return it->second;
    }

    VReg* m_pInner;
};
} // namespace VRegTest
