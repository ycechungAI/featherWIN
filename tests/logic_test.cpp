// Tests for the pure logic (blocklist, address bar). Runs on any OS; see run_logic_tests.sh.
#include <cstdio>

#include "blocklist.h"
#include "url.h"

namespace {

int failures = 0;

#define CHECK(condition)                                                  \
    do {                                                                  \
        if (!(condition)) {                                               \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            ++failures;                                                   \
        }                                                                 \
    } while (0)

void TestHostOf() {
    CHECK(HostOf(L"https://Ad.DoubleClick.net:443/x?y=1") == L"ad.doubleclick.net");
    CHECK(HostOf(L"http://example.com") == L"example.com");
    CHECK(HostOf(L"data:text/plain,hi") == L"");
}

void TestIsBlockedHost() {
    CHECK(IsBlockedHost(L"doubleclick.net"));
    CHECK(IsBlockedHost(L"ad.doubleclick.net"));
    CHECK(!IsBlockedHost(L"notdoubleclick.net"));
    CHECK(!IsBlockedHost(L"example.com"));
    CHECK(!IsBlockedHost(L""));
}

void TestToUrl() {
    CHECK(ToUrl(L"example.com") == L"https://example.com");
    CHECK(ToUrl(L"  https://a.b/c  ") == L"https://a.b/c");
    CHECK(ToUrl(L"http://plain.test/") == L"http://plain.test/");
    CHECK(ToUrl(L"cats and dogs") == L"https://duckduckgo.com/?q=cats%20and%20dogs");
    CHECK(ToUrl(L"a+b") == L"https://duckduckgo.com/?q=a%2Bb");
    CHECK(ToUrl(L"caf\u00e9 menu") == L"https://duckduckgo.com/?q=caf%C3%A9%20menu");
}

}  // namespace

int main() {
    TestHostOf();
    TestIsBlockedHost();
    TestToUrl();
    if (failures == 0) {
        std::printf("all logic tests passed\n");
        return 0;
    }
    std::printf("%d logic test(s) failed\n", failures);
    return 1;
}
