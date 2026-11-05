#pragma once

#include <string>

// What the address bar means (FR-NAV1, FR-NAV2): a web address, a bare domain, or a search.
// Search goes to DuckDuckGo until FR-NAV2 adds the engine setting. Pure, so it runs on any OS.
std::wstring ToUrl(const std::wstring& input);
