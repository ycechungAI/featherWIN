// Regression tests for featherWIN pure logic (blocklist, URL tools, bookmarks).
// Runs on any OS; CMake target: logic_test. Goal: broad coverage similar in spirit to Feather's regression suite.
#include <cstdio>
#include <memory>
#include <string>

#include "blocklist.h"
#include "bookmarks.h"
#include "url.h"

namespace {

int failures = 0;
int checks = 0;

#define CHECK(condition)                                                     \
    do {                                                                     \
        ++checks;                                                            \
        if (!(condition)) {                                                  \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

void TestHostOf() {
    // Basic extraction/lowercasing
    CHECK(HostOf(L"https://Ad.DoubleClick.net:443/x?y=1") == L"ad.doubleclick.net");
    CHECK(HostOf(L"http://example.com") == L"example.com");
    CHECK(HostOf(L"https://EXAMPLE.COM/Path") == L"example.com");
    CHECK(HostOf(L"HTTP://MiXeD.CaSe.Org") == L"mixed.case.org");
    // Ports, paths, query, fragment
    CHECK(HostOf(L"https://example.com:8080/path") == L"example.com");
    CHECK(HostOf(L"https://example.com/path/to/page") == L"example.com");
    CHECK(HostOf(L"https://example.com?query=1") == L"example.com");
    CHECK(HostOf(L"https://example.com#frag") == L"example.com");
    CHECK(HostOf(L"https://example.com:8443?x=1#y") == L"example.com");
    CHECK(HostOf(L"http://sub.domain.example.co.uk:80/a?b#c") == L"sub.domain.example.co.uk");
    // No host / non-http schemes handled by parser shape
    CHECK(HostOf(L"data:text/plain,hi") == L"");
    CHECK(HostOf(L"about:blank") == L"");
    CHECK(HostOf(L"javascript:void(0)") == L"");
    CHECK(HostOf(L"mailto:test@example.com") == L"");
    CHECK(HostOf(L"file:///C:/test.html") == L"");
    CHECK(HostOf(L"") == L"");
    CHECK(HostOf(L"not a url") == L"");
    CHECK(HostOf(L"https://") == L"");
    // Trailing/edge forms
    CHECK(HostOf(L"https://localhost/") == L"localhost");
    CHECK(HostOf(L"https://127.0.0.1:3000/") == L"127.0.0.1");
    CHECK(HostOf(L"https://[::1]/") == L"["); // current impl stops at ':' in host parsing; regression documents behavior
    CHECK(HostOf(L"ftp://files.example.com/pub") == L"files.example.com");
}

void TestIsBlockedHost() {
    // All starter blocked domains
    CHECK(IsBlockedHost(L"doubleclick.net"));
    CHECK(IsBlockedHost(L"googlesyndication.com"));
    CHECK(IsBlockedHost(L"googleadservices.com"));
    CHECK(IsBlockedHost(L"adservice.google.com"));
    CHECK(IsBlockedHost(L"adnxs.com"));
    CHECK(IsBlockedHost(L"criteo.com"));
    CHECK(IsBlockedHost(L"outbrain.com"));
    CHECK(IsBlockedHost(L"taboola.com"));
    CHECK(IsBlockedHost(L"scorecardresearch.com"));
    CHECK(IsBlockedHost(L"hotjar.com"));
    // Subdomains
    CHECK(IsBlockedHost(L"ad.doubleclick.net"));
    CHECK(IsBlockedHost(L"stats.g.doubleclick.net"));
    CHECK(IsBlockedHost(L"pagead2.googlesyndication.com"));
    CHECK(IsBlockedHost(L"static.hotjar.com"));
    CHECK(IsBlockedHost(L"x.y.z.taboola.com"));
    // Lookalikes / must not block
    CHECK(!IsBlockedHost(L"notdoubleclick.net"));
    CHECK(!IsBlockedHost(L"example.com"));
    CHECK(!IsBlockedHost(L"google.com"));
    CHECK(!IsBlockedHost(L"adservice.google.org"));
    CHECK(!IsBlockedHost(L"myhotjar.com"));
    CHECK(!IsBlockedHost(L"criteo.com.evil.test"));
    CHECK(!IsBlockedHost(L""));
    CHECK(!IsBlockedHost(L"doubleclick.net.fake"));
    CHECK(!IsBlockedHost(L"prefixdoubleclick.net"));
    CHECK(!IsBlockedHost(L"localhost"));
}

void TestToUrl() {
    // Already absolute URLs preserved (trimmed)
    CHECK(ToUrl(L"  https://a.b/c  ") == L"https://a.b/c");
    CHECK(ToUrl(L"http://plain.test/") == L"http://plain.test/");
    CHECK(ToUrl(L"https://example.com") == L"https://example.com");
    CHECK(ToUrl(L"HTTP://UPPER.TEST") == L"https://HTTP://UPPER.TEST"); // case-sensitive prefix check => treated as domain-like input; documents current behavior
    CHECK(ToUrl(L"\t\n https://trim.test \r") == L"https://trim.test");
    // Bare domains -> https
    CHECK(ToUrl(L"example.com") == L"https://example.com");
    CHECK(ToUrl(L"sub.example.org/path") == L"https://sub.example.org/path");
    CHECK(ToUrl(L"127.0.0.1") == L"https://127.0.0.1");
    CHECK(ToUrl(L"a.b.c.d") == L"https://a.b.c.d");
    CHECK(ToUrl(L"test.io") == L"https://test.io");
    CHECK(ToUrl(L"EXAMPLE.COM") == L"https://EXAMPLE.COM");
    // Searches (spaces, or no dot)
    CHECK(ToUrl(L"cats and dogs") == L"https://duckduckgo.com/?q=cats%20and%20dogs");
    CHECK(ToUrl(L"a+b") == L"https://duckduckgo.com/?q=a%2Bb");
    CHECK(ToUrl(L"caf\u00e9 menu") == L"https://duckduckgo.com/?q=caf%C3%A9%20menu");
    CHECK(ToUrl(L"localhost") == L"https://duckduckgo.com/?q=localhost"); // no dot => search by design
    CHECK(ToUrl(L"hello") == L"https://duckduckgo.com/?q=hello");
    CHECK(ToUrl(L"one two three") == L"https://duckduckgo.com/?q=one%20two%20three");
    CHECK(ToUrl(L"c++") == L"https://duckduckgo.com/?q=c%2B%2B");
    CHECK(ToUrl(L"what?") == L"https://duckduckgo.com/?q=what%3F");
    CHECK(ToUrl(L"100%") == L"https://duckduckgo.com/?q=100%25");
    CHECK(ToUrl(L"a&b") == L"https://duckduckgo.com/?q=a%26b");
    CHECK(ToUrl(L"a=b") == L"https://duckduckgo.com/?q=a%3Db");
    CHECK(ToUrl(L"a#b") == L"https://duckduckgo.com/?q=a%23b");
    CHECK(ToUrl(L"  spaced  ") == L"https://duckduckgo.com/?q=spaced");
    CHECK(ToUrl(L"日本語") == L"https://duckduckgo.com/?q=%E6%97%A5%E6%9C%AC%E8%AA%9E");
    CHECK(ToUrl(L"") == L"https://duckduckgo.com/?q=");
    CHECK(ToUrl(L"   ") == L"https://duckduckgo.com/?q=");
    CHECK(ToUrl(L"https://") == L"https://");
    CHECK(ToUrl(L"http://") == L"http://");
    CHECK(ToUrl(L"foo/bar") == L"https://duckduckgo.com/?q=foo%2Fbar"); // no dot, no space => search
}

void TestBookmarksBasic() {
    BookmarkTree tree;
    CHECK(tree.empty());
    CHECK(!IsBookmarked(tree, L"https://example.com/"));
    CHECK(AddBookmark(tree, L"Example", L"https://example.com/"));
    CHECK(tree.size() == 1);
    CHECK(tree[0]->title == L"Example");
    CHECK(tree[0]->url == L"https://example.com/");
    CHECK(tree[0]->type == BookmarkNode::Bookmark);
    CHECK(IsBookmarked(tree, L"https://example.com/"));
    CHECK(!IsBookmarked(tree, L"https://other.com/"));
    CHECK(!AddBookmark(tree, L"Again", L"https://example.com/")); // duplicate
    CHECK(tree.size() == 1);
    CHECK(!AddBookmark(tree, L"No URL", L""));
    CHECK(AddBookmark(tree, L"", L"https://bare.test/")); // title fallback
    CHECK(tree.size() == 2);
    CHECK(tree[1]->title == L"https://bare.test/");
    CHECK(AddBookmark(tree, L"Third", L"https://third.test/"));
    CHECK(tree.size() == 3);

    BookmarkNode* found = FindBookmarkNode(tree, L"https://example.com/");
    CHECK(found != nullptr);
    CHECK(found->title == L"Example");
    CHECK(FindBookmarkNode(tree, L"https://missing.test/") == nullptr);
    const BookmarkTree& ctree = tree;
    CHECK(FindBookmarkNode(ctree, L"https://third.test/") != nullptr);

    BookmarkNode* folder = AddFolder(tree, L"My Folder");
    CHECK(folder != nullptr);
    CHECK(folder->type == BookmarkNode::Folder);
    CHECK(folder->title == L"My Folder");
    CHECK(tree.size() == 4);
    BookmarkNode* folder2 = AddFolder(tree, L"");
    CHECK(folder2 != nullptr);
    CHECK(folder2->title == L"New Folder");

    CHECK(RemoveNode(tree, folder2));
    CHECK(tree.size() == 4);
    CHECK(!RemoveNode(tree, folder2)); // already removed
}

void TestBookmarksMoveAndCycles() {
    BookmarkTree tree;
    AddBookmark(tree, L"A", L"https://a.test/");
    AddBookmark(tree, L"B", L"https://b.test/");
    AddBookmark(tree, L"C", L"https://c.test/");
    BookmarkNode* a = tree[0].get(); BookmarkNode* b = tree[1].get(); BookmarkNode* c = tree[2].get();
    // reorder at root
    MoveNode(tree, c, nullptr, a);
    CHECK(tree[0].get() == c);
    CHECK(tree[1].get() == a);
    CHECK(tree[2].get() == b);
    // move into folder
    BookmarkNode* f = AddFolder(tree, L"F");
    MoveNode(tree, a, f, nullptr);
    CHECK(f->children.size() == 1);
    CHECK(f->children[0].get() == a);
    CHECK(tree.size() == 3); // c,b,f
    // cycle protection: move folder into itself/descendant
    BookmarkNode* sub = AddFolder(f->children, L"Sub");
    (void)sub;
    MoveNode(tree, f, f, nullptr); // into itself
    CHECK(tree.size() == 3); // unchanged (f still at root)
    MoveNode(tree, f, sub, nullptr); // into descendant
    CHECK(tree.size() == 3);

    // remove nested node
    CHECK(RemoveNode(tree, a));
    CHECK(f->children.empty() || (f->children.size() == 1 && f->children[0].get() == sub));

    // null/missing safety
    MoveNode(tree, nullptr, nullptr, nullptr);
    CHECK(true);
}

static void CollectUrls(const BookmarkTree& tree, std::wstring& out) {
    for (const auto& n : tree) {
        if (!n) continue;
        if (n->type == BookmarkNode::Bookmark) { out += n->url; out += L"|"; }
        else CollectUrls(n->children, out);
    }
}

void TestBookmarksSerializeRegression() {
    // Regression (2026-11-05): nested folder deserialization double-pop on 'E' misplaced items.
    BookmarkTree tree;
    BookmarkNode* f1 = AddFolder(tree, L"F1");
    auto b1 = std::make_unique<BookmarkNode>(); b1->type=BookmarkNode::Bookmark; b1->title=L"A"; b1->url=L"https://a.test/"; f1->children.push_back(std::move(b1));
    auto f2 = std::make_unique<BookmarkNode>(); f2->type=BookmarkNode::Folder; f2->title=L"F2"; BookmarkNode* f2p=f2.get(); f1->children.push_back(std::move(f2));
    auto b2 = std::make_unique<BookmarkNode>(); b2->type=BookmarkNode::Bookmark; b2->title=L"B"; b2->url=L"https://b.test/"; f2p->children.push_back(std::move(b2));
    AddBookmark(tree, L"Root", L"https://root.test/");

    std::wstring s = SerializeBookmarks(tree);
    CHECK(s.find(L"F\tF1") != std::wstring::npos);
    CHECK(s.find(L"F\tF2") != std::wstring::npos);
    CHECK(s.find(L"https://b.test/") != std::wstring::npos);

    BookmarkTree rt = DeserializeBookmarks(s);
    CHECK(rt.size() == 2); // F1 and Root
    CHECK(rt[0]->type == BookmarkNode::Folder);
    CHECK(rt[0]->title == L"F1");
    CHECK(rt[0]->children.size() == 2); // A and F2
    CHECK(rt[0]->children[0]->url == L"https://a.test/");
    CHECK(rt[0]->children[1]->type == BookmarkNode::Folder);
    CHECK(rt[0]->children[1]->title == L"F2");
    CHECK(rt[0]->children[1]->children.size() == 1);
    CHECK(rt[0]->children[1]->children[0]->url == L"https://b.test/");
    CHECK(rt[1]->url == L"https://root.test/");

    // Round-trip stability
    BookmarkTree rt2 = DeserializeBookmarks(SerializeBookmarks(rt));
    std::wstring u1,u2; CollectUrls(rt,u1); CollectUrls(rt2,u2);
    CHECK(u1 == u2);
    CHECK(u1.find(L"https://a.test/") != std::wstring::npos);
    CHECK(u1.find(L"https://b.test/") != std::wstring::npos);
    CHECK(u1.find(L"https://root.test/") != std::wstring::npos);

    // Empty/malformed tolerance
    CHECK(DeserializeBookmarks(L"").empty());
    CHECK(DeserializeBookmarks(L"\n\n\r\n").empty());
    BookmarkTree m = DeserializeBookmarks(L"B\tOnlyTitle\n"); // no second tab: url becomes empty; node still created by current impl
    CHECK(m.size() == 1);
    CHECK(m[0]->url.empty());
    CHECK(m[0]->title == L"OnlyTitle");
    BookmarkTree ok = DeserializeBookmarks(L"B\tTitle\thttps://x.test/\n");
    CHECK(ok.size() == 1);
    CHECK(ok[0]->url == L"https://x.test/");

    // Deep nesting
    std::wstring deep = L"F\t1\n  F\t2\n    F\t3\n      B\tD\thttps://d.test/\n    E\n  E\nE\nB\tR\thttps://r.test/\n";
    BookmarkTree dt = DeserializeBookmarks(deep);
    CHECK(dt.size() == 2);
    CHECK(dt[0]->type == BookmarkNode::Folder);
    CHECK(dt[0]->children.size() == 1);
    CHECK(dt[0]->children[0]->children.size() == 1);
    CHECK(dt[0]->children[0]->children[0]->children.size() == 1);
    CHECK(dt[0]->children[0]->children[0]->children[0]->url == L"https://d.test/");
    CHECK(dt[1]->url == L"https://r.test/");
}

void TestNetscapeBookmarks() {
    // Basic parse
    const std::wstring html = L"<!DOCTYPE NETSCAPE-Bookmark-file-1>\n"
        L"<DL><p>\n"
        L"    <DT><H3>My Folder</H3>\n"
        L"    <DL><p>\n"
        L"        <DT><A HREF=\"https://example.com/\">Example</A>\n"
        L"    </DL><p>\n"
        L"    <DT><A HREF=\"https://test.com/\">Test</A>\n"
        L"</DL><p>\n";
    BookmarkTree imported = ParseNetscapeBookmarks(html);
    CHECK(imported.size() == 2);
    CHECK(imported[0]->type == BookmarkNode::Folder);
    CHECK(imported[0]->title == L"My Folder");
    CHECK(imported[0]->children.size() == 1);
    CHECK(imported[0]->children[0]->type == BookmarkNode::Bookmark);
    CHECK(imported[0]->children[0]->title == L"Example");
    CHECK(imported[0]->children[0]->url == L"https://example.com/");
    CHECK(imported[1]->type == BookmarkNode::Bookmark);
    CHECK(imported[1]->title == L"Test");
    CHECK(imported[1]->url == L"https://test.com/");

    std::wstring exported = SerializeNetscapeBookmarks(imported);
    CHECK(exported.find(L"<!DOCTYPE NETSCAPE-Bookmark-file-1>") != std::wstring::npos);
    CHECK(exported.find(L"<TITLE>Bookmarks</TITLE>") != std::wstring::npos);
    CHECK(exported.find(L"<H3>My Folder</H3>") != std::wstring::npos);
    CHECK(exported.find(L"HREF=\"https://example.com/\"") != std::wstring::npos);
    CHECK(exported.find(L">Test</A>") != std::wstring::npos);

    // Case-insensitive tags / attributes
    const std::wstring html2 = L"<dl><dt><h3>F</h3><dl><dt><a href='https://q.test/'>Q</a></dl></dl>";
    BookmarkTree t2 = ParseNetscapeBookmarks(html2);
    CHECK(t2.size() == 1);
    CHECK(t2[0]->type == BookmarkNode::Folder);
    CHECK(t2[0]->title == L"F");
    CHECK(t2[0]->children.size() == 1);
    CHECK(t2[0]->children[0]->url == L"https://q.test/");

    // HTML entities (title unescaped; href attribute value is taken as-is by current parser)
    const std::wstring html3 = L"<DL><DT><A HREF=\"https://e.test/?a=1&amp;b=2\">A &amp; B</A></DL>";
    BookmarkTree t3 = ParseNetscapeBookmarks(html3);
    CHECK(t3.size() == 1);
    CHECK(t3[0]->url == L"https://e.test/?a=1&amp;b=2");
    CHECK(t3[0]->title == L"A & B");

    // Empty / no bookmarks
    CHECK(ParseNetscapeBookmarks(L"").empty());
    CHECK(ParseNetscapeBookmarks(L"<DL><p></DL>").empty());
    CHECK(ParseNetscapeBookmarks(L"not html").empty());

    // Export empty
    BookmarkTree empty;
    std::wstring ex = SerializeNetscapeBookmarks(empty);
    CHECK(ex.find(L"<DL><p>") != std::wstring::npos);
    CHECK(ex.find(L"</DL><p>") != std::wstring::npos);
}

}  // namespace

int main() {
    TestHostOf();
    TestIsBlockedHost();
    TestToUrl();
    TestBookmarksBasic();
    TestBookmarksMoveAndCycles();
    TestBookmarksSerializeRegression();
    TestNetscapeBookmarks();
    if (failures == 0) {
        std::printf("all logic tests passed (%d checks)\n", checks);
        return 0;
    }
    std::printf("%d/%d logic test check(s) failed\n", failures, checks);
    return 1;
}
