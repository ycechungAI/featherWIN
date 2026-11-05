#include "blocklist.h"

#include <algorithm>
#include <cwctype>

namespace {

// Starter list. Feather's full rules (Blocklist.swift) replace this when Shields is ported.
const wchar_t* const kBlockedDomains[] = {
    L"doubleclick.net",
    L"googlesyndication.com",
    L"googleadservices.com",
    L"adservice.google.com",
    L"adnxs.com",
    L"criteo.com",
    L"outbrain.com",
    L"taboola.com",
    L"scorecardresearch.com",
    L"hotjar.com",
};

}  // namespace

std::wstring HostOf(const std::wstring& url) {
    const size_t scheme = url.find(L"://");
    if (scheme == std::wstring::npos) {
        return L"";
    }
    const size_t start = scheme + 3;
    const size_t end = url.find_first_of(L"/?#:", start);
    std::wstring host = url.substr(start, end == std::wstring::npos ? std::wstring::npos : end - start);
    std::transform(host.begin(), host.end(), host.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return host;
}

bool IsBlockedHost(const std::wstring& host) {
    for (const wchar_t* listed : kBlockedDomains) {
        const std::wstring domain = listed;
        if (host == domain) {
            return true;
        }
        const bool subdomain = host.size() > domain.size() &&
                               host.compare(host.size() - domain.size(), domain.size(), domain) == 0 &&
                               host[host.size() - domain.size() - 1] == L'.';
        if (subdomain) {
            return true;
        }
    }
    return false;
}
