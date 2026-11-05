#pragma once

#include <string>

// Host matching for Shields (FR-SHD1). Kept free of WebView2 and Win32 so it can be tested on any OS.

// The lowercase host of an absolute URL, or "" when there is none:
// "https://Ad.DoubleClick.net:443/x?y=1" gives "ad.doubleclick.net".
std::wstring HostOf(const std::wstring& url);

// True when `host` is a listed domain or a subdomain of one.
bool IsBlockedHost(const std::wstring& host);
