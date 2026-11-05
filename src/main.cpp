// featherWIN browser window (WebView2). FR-TAB1/2, FR-NAV1, FR-BM1/2/5, FR-SHD1.
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <wrl/client.h>
#include <wrl/event.h>
#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>

#include <algorithm>
#include <cwctype>
#include <ctime>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "blocklist.h"
#include "bookmarks.h"
#include "resource.h"
#include "url.h"

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;

namespace {

constexpr wchar_t kWindowClass[] = L"featherWIN";
constexpr wchar_t kHomePage[] = L"https://duckduckgo.com";
constexpr int kMenuBarHeight = 26;
constexpr int kTabStripTop = kMenuBarHeight;
constexpr int kTabStripHeight = 34;
constexpr int kToolbarTop = kTabStripTop + kTabStripHeight;
constexpr int kToolbarHeight = 40;
constexpr int kBookmarkRowHeight = 24;
constexpr int kBookmarkBarRows = 2;
constexpr int kBookmarkBarHeight = kBookmarkBarRows * kBookmarkRowHeight + 6;
constexpr int kFindBarHeight = 30;
bool g_findVisible = false;
inline int FindBarTop() { return kToolbarTop + kToolbarHeight; }
inline int FindBarVisibleHeight() { return g_findVisible ? kFindBarHeight : 0; }
inline int BookmarkBarTop() { return kToolbarTop + kToolbarHeight + FindBarVisibleHeight(); }
inline int ChromeHeight() { return BookmarkBarTop() + kBookmarkBarHeight; }
constexpr int kTabMinWidth = 60;
constexpr int kTabMaxWidth = 200;
constexpr int kNewTabWidth = 34;
constexpr int kCloseSize = 16;
constexpr int kBookmarkMaxWidth = 180;
constexpr int kBookmarkGap = 4;
constexpr UINT kMsgShortcut = WM_APP + 1;
constexpr UINT_PTR kTimerFolderClose = 2;
constexpr int kMaxFolderLevels = 5;

enum ControlId { kBackButton = 101, kForwardButton, kReloadButton, kStarButton, kShieldButton, kPipButton, kMediaButton, kAddressBar, kFindEdit = 201, kFindNext, kFindPrev, kFindClose };
enum MenuId { kMenuNone = -1, kMenuFile = 0, kMenuBookmarks = 1, kMenuHistory = 2, kMenuExtensions = 3, kMenuSessions = 4, kMenuTabGroups = 5, kMenuGear = 6 };

struct Tab {
    int id = 0;
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> webview;
    std::wstring title = L"New Tab";
    std::wstring url;
    DWORD lastActiveTick = 0;
    bool suspended = false;
    bool pinned = false;
    int groupId = -1; // FR-GRP*
};

struct TabGroup { // FR-GRP1-4
    int id = 0;
    std::wstring name;
    COLORREF color = RGB(0, 120, 212);
    bool collapsed = false;
};
TabGroup* FindTabGroup(int id);
void SaveTabGroups();
void LoadTabGroups();

struct BookmarkChip {
    RECT rect{};
    BookmarkNode* node = nullptr;
};

struct BookmarkLayout {
    std::vector<BookmarkChip> visible;
    std::vector<BookmarkNode*> overflow;
    bool hasOverflow = false;
    RECT overflowRect{};
};

HWND g_main = nullptr;
HWND g_back = nullptr, g_forward = nullptr, g_reload = nullptr, g_star = nullptr, g_shield = nullptr, g_pip = nullptr, g_media = nullptr, g_address = nullptr;
WNDPROC g_addressProc = nullptr;
WNDPROC g_buttonProc = nullptr;
HWND g_hoverButton = nullptr;
HFONT g_iconFont = nullptr;
HFONT g_uiFont = nullptr;
HFONT g_smallFont = nullptr;
HBRUSH g_whiteBrush = nullptr;
constexpr COLORREF kThemeBg = RGB(246, 247, 249);
constexpr COLORREF kThemeBorder = RGB(210, 214, 220);

ComPtr<ICoreWebView2Environment> g_env;
std::vector<std::unique_ptr<Tab>> g_tabs;
int g_nextTabId = 1;
int g_activeId = 0;
std::vector<TabGroup> g_tabGroups; // FR-GRP1-4
int g_nextGroupId = 1;

BookmarkTree g_bookmarks;
std::wstring g_bookmarkPath;
std::wstring g_dataFolder;
int g_hoverMenu = kMenuNone;
bool g_shieldsEnabled = true;
std::vector<std::wstring> g_shieldExceptions;  // hosts where Shields are off (FR-SHD2)
std::wstring g_shieldExceptionsPath;
std::wstring g_extensionsFolder;
std::wstring g_historyPath;
std::wstring g_pinnedPath;
std::wstring g_sessionPath;
std::wstring g_settingsPath;
std::wstring g_tabGroupsPath; // FR-GRP1: saved in tab-groups.json (simple JSON)
std::wstring g_mediaExtSlot3;  // media dropdown position 3: extension chosen in settings
bool g_extensionsLoaded = false;
bool g_navStripping = false;
HWND g_findEdit = nullptr;
HWND g_findNext = nullptr;
HWND g_findPrev = nullptr;
HWND g_findClose = nullptr;
WNDPROC g_findEditProc = nullptr;
struct ClosedTab { std::wstring url; std::wstring title; };
std::vector<ClosedTab> g_closedTabs;  // FR-NAV4
struct HistoryItem { std::wstring url; std::wstring title; __int64 ts = 0; };
std::vector<HistoryItem> g_history;  // FR-HIS3 (local)
struct DownloadItem { std::wstring file; std::wstring url; std::wstring state; };
std::vector<DownloadItem> g_downloads;  // FR-DL1 (last 50)
struct ExtInfo { std::wstring id; std::wstring name; std::wstring path; std::wstring popup; std::wstring options; bool enabled = true; };
std::vector<ExtInfo> g_extensions;

HWND g_folderPopups[kMaxFolderLevels] = {};
BookmarkNode* g_folderPopupNodes[kMaxFolderLevels] = {};
std::vector<BookmarkNode*> g_folderPopupItems[kMaxFolderLevels];
bool g_folderPopupOverflow[kMaxFolderLevels] = {};
int g_hoverPopupItem = -1;

// Bar drag state
bool g_barDragging = false;
BookmarkNode* g_barDragNode = nullptr;
POINT g_barDragStart{};
BookmarkNode* g_barDropTarget = nullptr;

// Popup drag state
HWND g_dragPopup = nullptr;
int g_dragPopupIndex = -1;
bool g_dragPopupActive = false;
POINT g_dragPopupStart{};
bool g_menuTracking = false;

std::wstring TakeString(LPWSTR text) {
    std::wstring value = text ? text : L"";
    CoTaskMemFree(text);
    return value;
}

std::wstring GetText(HWND hwnd) {
    const int len = GetWindowTextLengthW(hwnd);
    std::wstring text(static_cast<size_t>(len), L'\0');
    GetWindowTextW(hwnd, text.data(), len + 1);
    return text;
}

bool IsCtrlDown() { return (GetKeyState(VK_CONTROL) & 0x8000) != 0; }
bool IsShiftDown() { return (GetKeyState(VK_SHIFT) & 0x8000) != 0; }
bool IsShortcutKey(UINT vk) {
    return (vk >= '1' && vk <= '9') || vk == 'D' || vk == 'F' || vk == 'G' || vk == 'H' || vk == 'J' || vk == 'L' || vk == 'R' || vk == 'T' || vk == 'W' || vk == VK_TAB;
}

Tab* FindTab(int id) {
    for (auto& tab : g_tabs) {
        if (tab->id == id) return tab.get();
    }
    return nullptr;
}

int IndexOf(int id) {
    for (size_t i = 0; i < g_tabs.size(); ++i) {
        if (g_tabs[i]->id == id) return static_cast<int>(i);
    }
    return -1;
}

Tab* ActiveTab() { return FindTab(g_activeId); }

RECT ContentBounds() {
    RECT bounds{};
    GetClientRect(g_main, &bounds);
    bounds.top = ChromeHeight();
    return bounds;
}

constexpr int kPinnedTabWidth = 32;

int PinnedTabCount() {
    int n = 0;
    for (const auto& t : g_tabs) if (t && t->pinned) ++n;
    return n;
}

int NormalTabWidth() {
    RECT client{};
    GetClientRect(g_main, &client);
    const int pinned = PinnedTabCount();
    const int normal = std::max(0, static_cast<int>(g_tabs.size()) - pinned);
    const int avail = std::max(60, static_cast<int>(client.right) - kNewTabWidth - pinned * kPinnedTabWidth);
    if (normal <= 0) return kTabMinWidth;
    return std::clamp(avail / normal, kTabMinWidth, kTabMaxWidth);
}

RECT TabRect(int index) {
    if (index < 0 || index >= static_cast<int>(g_tabs.size())) return {};
    int x = 0;
    const int normalW = NormalTabWidth();
    for (int i = 0; i < index; ++i) {
        x += (g_tabs[static_cast<size_t>(i)] && g_tabs[static_cast<size_t>(i)]->pinned) ? kPinnedTabWidth : normalW;
    }
    const bool pinned = g_tabs[static_cast<size_t>(index)] && g_tabs[static_cast<size_t>(index)]->pinned;
    const int w = pinned ? kPinnedTabWidth : normalW;
    return RECT{x, kTabStripTop + 2, x + w, kTabStripTop + kTabStripHeight};
}

void SortTabsPinnedLeft() {
    std::stable_sort(g_tabs.begin(), g_tabs.end(), [](const std::unique_ptr<Tab>& a, const std::unique_ptr<Tab>& b) {
        const bool ap = a && a->pinned;
        const bool bp = b && b->pinned;
        return ap && !bp;
    });
}

RECT CloseRect(const RECT& tab, bool pinned) {
    if (pinned) return {0,0,0,0};
    const int top = tab.top + (tab.bottom - tab.top - kCloseSize) / 2;
    return RECT{tab.right - kCloseSize - 6, top, tab.right - 6, top + kCloseSize};
}

RECT ButtonRect(int index) {
    const int top = kToolbarTop + 4;
    return RECT{8 + index * 42, top, 8 + index * 42 + 34, top + 32};
}

RECT AddressPillRect() {
    RECT client{};
    GetClientRect(g_main, &client);
    const int rightEdge = std::max(300, static_cast<int>(client.right) - 8);
    HDC dc = GetDC(g_main);
    HGDIOBJ old = SelectObject(dc, g_uiFont ? g_uiFont : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
    SIZE sz{};
    const std::wstring sample(50, L'W');
    GetTextExtentPoint32W(dc, sample.c_str(), 50, &sz);
    SelectObject(dc, old);
    ReleaseDC(g_main, dc);
    const int minWidth = static_cast<int>(sz.cx) * 2 / 5 + 36;
    const int maxWidth = static_cast<int>(sz.cx) + 36;
    const int left = 8 + 7 * 42 + 8;
    const int width = std::clamp(rightEdge - left, minWidth, maxWidth);
    return RECT{left, kToolbarTop + 4, left + width, kToolbarTop + 36};
}

void FillRoundedRect(HDC dc, const RECT& rect, int radius, COLORREF fill, COLORREF border) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void InvalidateStrip() {
    RECT c{};
    GetClientRect(g_main, &c);
    RECT s{0, kTabStripTop, c.right, kTabStripTop + kTabStripHeight};
    InvalidateRect(g_main, &s, FALSE);
}

void InvalidateBookmarkBar() {
    RECT c{};
    GetClientRect(g_main, &c);
    RECT b{0, BookmarkBarTop(), c.right, BookmarkBarTop() + kBookmarkBarHeight};
    InvalidateRect(g_main, &b, FALSE);
}

void InvalidateMenuBar() {
    RECT c{};
    GetClientRect(g_main, &c);
    RECT b{0, 0, c.right, kMenuBarHeight};
    InvalidateRect(g_main, &b, FALSE);
}

std::string ToUtf8(const std::wstring& text) {
    if (text.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string bytes(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), bytes.data(), size, nullptr, nullptr);
    return bytes;
}

std::wstring FromUtf8(const std::string& bytes) {
    if (bytes.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, bytes.c_str(), static_cast<int>(bytes.size()), nullptr, 0);
    std::wstring text(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, bytes.c_str(), static_cast<int>(bytes.size()), text.data(), size);
    return text;
}

void SaveHistory() {  // FR-HIS3: local only
    if (g_historyPath.empty()) return;
    std::string out;
    const size_t maxItems = 500;
    const size_t start = g_history.size() > maxItems ? g_history.size() - maxItems : 0;
    for (size_t i = start; i < g_history.size(); ++i) {
        const auto& h = g_history[i];
        out += std::to_string(h.ts);
        out += '\t';
        // escape tabs/newlines minimally
        std::wstring url = h.url, title = h.title;
        for (auto& c : url) if (c == L'\t' || c == L'\r' || c == L'\n') c = L' ';
        for (auto& c : title) if (c == L'\t' || c == L'\r' || c == L'\n') c = L' ';
        out += ToUtf8(url);
        out += '\t';
        out += ToUtf8(title);
        out += '\n';
    }
    HANDLE file = CreateFileW(g_historyPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(file, out.data(), static_cast<DWORD>(out.size()), &written, nullptr);
    CloseHandle(file);
}

void LoadHistory() {  // FR-HIS3
    g_history.clear();
    if (g_historyPath.empty()) return;
    HANDLE file = CreateFileW(g_historyPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    LARGE_INTEGER size{};
    std::string bytes;
    if (GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart < (2 << 20)) {
        bytes.resize(static_cast<size_t>(size.QuadPart));
        DWORD read = 0;
        ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr);
        bytes.resize(read);
    }
    CloseHandle(file);
    size_t start = 0;
    const __int64 now = static_cast<__int64>(_time64(nullptr));
    const __int64 keepSec = 7 * 24 * 3600;
    while (start < bytes.size()) {
        size_t end = bytes.find('\n', start);
        if (end == std::string::npos) end = bytes.size();
        std::string line = bytes.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t t1 = line.find('\t');
        size_t t2 = (t1 == std::string::npos) ? std::string::npos : line.find('\t', t1 + 1);
        if (t1 != std::string::npos && t2 != std::string::npos) {
            HistoryItem h;
            h.ts = _strtoi64(line.substr(0, t1).c_str(), nullptr, 10);
            h.url = FromUtf8(line.substr(t1 + 1, t2 - t1 - 1));
            h.title = FromUtf8(line.substr(t2 + 1));
            if (!h.url.empty() && (now - h.ts) <= keepSec) g_history.push_back(std::move(h));
        }
        start = end + 1;
    }
}

void RecordHistory(const std::wstring& url, const std::wstring& title) {  // FR-HIS3
    if (url.empty() || url == L"about:blank" || url.rfind(L"edge://", 0) == 0) return;
    HistoryItem h;
    h.url = url;
    h.title = title.empty() ? url : title;
    h.ts = static_cast<__int64>(_time64(nullptr));
    if (!g_history.empty() && g_history.back().url == h.url) {
        g_history.back().title = h.title;
        g_history.back().ts = h.ts;
    } else {
        g_history.push_back(std::move(h));
    }
    if (g_history.size() > 1000) g_history.erase(g_history.begin(), g_history.begin() + (g_history.size() - 1000));
    SaveHistory();
}

void SaveBookmarks() {
    if (g_bookmarkPath.empty()) return;
    const std::string bytes = ToUtf8(SerializeBookmarks(g_bookmarks));
    HANDLE file = CreateFileW(g_bookmarkPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr);
    CloseHandle(file);
}

bool IsYouTubeHost(const std::wstring& h) {
    return h == L"youtube.com" || h == L"www.youtube.com" || h == L"m.youtube.com" ||
           h == L"music.youtube.com" || h == L"youtu.be" ||
           (h.size() > 12 && h.compare(h.size() - 12, 12, L".youtube.com") == 0) ||
           (h.size() > 9 && h.compare(h.size() - 9, 9, L".youtu.be") == 0);
}

void SaveShieldExceptions() {
    if (g_shieldExceptionsPath.empty()) return;
    std::wstring text;
    for (const auto& h : g_shieldExceptions) text += h + L"\n";
    const std::string bytes = ToUtf8(text);
    HANDLE file = CreateFileW(g_shieldExceptionsPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr);
    CloseHandle(file);
}

void LoadShieldExceptions() {
    g_shieldExceptions.clear();
    if (g_shieldExceptionsPath.empty()) return;
    HANDLE file = CreateFileW(g_shieldExceptionsPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    LARGE_INTEGER size{};
    std::string bytes;
    if (GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart < (1 << 20)) {
        bytes.resize(static_cast<size_t>(size.QuadPart));
        DWORD read = 0;
        ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr);
        bytes.resize(read);
    }
    CloseHandle(file);
    std::wstring text = FromUtf8(bytes);
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find(L'\n', start);
        if (end == std::wstring::npos) end = text.size();
        std::wstring line = text.substr(start, end - start);
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        if (!line.empty()) g_shieldExceptions.push_back(line);
        start = end + 1;
    }
}

bool IsShieldsOffForHost(const std::wstring& host) {
    if (host.empty()) return true;
    if (IsYouTubeHost(host)) return true;  // user: don't block on YouTube
    for (const auto& ex : g_shieldExceptions) {
        if (host == ex) return true;
        if (host.size() > ex.size() + 1 && host.compare(host.size() - ex.size() - 1, ex.size() + 1, L"." + ex) == 0) return true;
    }
    return false;
}

std::wstring CurrentPageHost() {
    Tab* tab = ActiveTab();
    return tab ? HostOf(tab->url) : L"";
}

void ToggleShieldsForCurrentSite() {
    const std::wstring host = CurrentPageHost();
    if (host.empty()) return;
    if (IsYouTubeHost(host)) {
        MessageBoxW(g_main, L"Shields are always off on YouTube (to avoid ad-block detection).", L"Shields", MB_OK | MB_ICONINFORMATION);
        return;
    }
    auto it = std::find(g_shieldExceptions.begin(), g_shieldExceptions.end(), host);
    if (it == g_shieldExceptions.end()) g_shieldExceptions.push_back(host);
    else g_shieldExceptions.erase(it);
    SaveShieldExceptions();
    if (g_shield) InvalidateRect(g_shield, nullptr, FALSE);
    Tab* tab = ActiveTab();
    if (tab && tab->webview) tab->webview->Reload();
}

void ShowMediaMenu();  // forward (defined below)
void ShowDownloadsWindow();
void OpenSettingsTab();
void SaveSession();
void LoadHistory();

void LoadSettings() {
    g_mediaExtSlot3.clear();
    if (g_settingsPath.empty()) return;
    HANDLE file = CreateFileW(g_settingsPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    LARGE_INTEGER size{};
    std::string bytes;
    if (GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart < (64 << 10)) {
        bytes.resize(static_cast<size_t>(size.QuadPart));
        DWORD read = 0;
        ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr);
        bytes.resize(read);
    }
    CloseHandle(file);
    // simple: media_ext_slot3=<utf8 text>
    std::string key = "media_ext_slot3=";
    size_t p = bytes.find(key);
    if (p != std::string::npos) {
        size_t e = bytes.find('\n', p);
        std::string v = bytes.substr(p + key.size(), e == std::string::npos ? std::wstring::npos : e - (p + key.size()));
        if (!v.empty() && v.back() == '\r') v.pop_back();
        g_mediaExtSlot3 = FromUtf8(v);
    }
}

void SaveSettings() {
    if (g_settingsPath.empty()) return;
    std::string out = "media_ext_slot3=" + ToUtf8(g_mediaExtSlot3) + "\n";
    HANDLE file = CreateFileW(g_settingsPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(file, out.data(), static_cast<DWORD>(out.size()), &written, nullptr);
    CloseHandle(file);
}

void ToggleSubtitlesAuto() {
    Tab* tab = ActiveTab();
    if (!tab || !tab->webview) return;
    // Toggle/enable text tracks on the first suitable video (main doc + same-origin frames/shadow roots).
    const wchar_t* js = LR"JS((()=>{try{
      if(!document.pictureInPictureElement){} // no-op, keep script shape simple
      const seen=new Set(), vids=[], roots=[document];
      const addRoot=(r)=>{ if(!r||seen.has(r)) return; seen.add(r); try{ r.querySelectorAll&&r.querySelectorAll('video').forEach(v=>vids.push(v)); }catch(e){}
        try{ r.querySelectorAll&&r.querySelectorAll('*').forEach(el=>{ if(el.shadowRoot) addRoot(el.shadowRoot); }); }catch(e){}
        try{ r.querySelectorAll&&r.querySelectorAll('iframe').forEach(f=>{ try{ const d=f.contentDocument; if(d) addRoot(d); }catch(e){} }); }catch(e){}
      };
      addRoot(document);
      const v=vids.find(x=>!x.paused)||vids[0];
      if(!v){ alert('No video found for subtitles.'); return; }
      const tracks=v.textTracks ? Array.from(v.textTracks) : [];
      if(!tracks.length){ alert('No subtitle/caption tracks found on this video. Use "Load subtitle file..." for a local .vtt/.srt file.'); return; }
      const showing=tracks.findIndex(t=>t.mode==='showing');
      tracks.forEach(t=>t.mode='disabled');
      const next=tracks[(showing+1) % tracks.length];
      if(next) next.mode='showing';
    }catch(e){ try{alert('Subtitles failed: '+(e&&e.message?e.message:e));}catch(_){}}})())JS";
    tab->webview->ExecuteScript(js, nullptr);
}

void TurnSubtitlesOff() {
    Tab* tab = ActiveTab();
    if (!tab || !tab->webview) return;
    const wchar_t* js = LR"JS((()=>{try{
      const vids=[...document.querySelectorAll('video')];
      vids.forEach(v=>{ if(v.textTracks) Array.from(v.textTracks).forEach(t=>t.mode='disabled'); });
      document.querySelectorAll('track[data-featherwin-sub]').forEach(t=>t.remove());
    }catch(e){}})())JS";
    tab->webview->ExecuteScript(js, nullptr);
}

static std::wstring SrtToVttIfNeeded(const std::wstring& text, const std::wstring& path) {
    std::wstring lower = path;
    for (auto& c : lower) c = towlower(c);
    if (lower.size() >= 4 && lower.compare(lower.size() - 4, 4, L".vtt") == 0) return text;
    if (!(lower.size() >= 4 && lower.compare(lower.size() - 4, 4, L".srt") == 0)) return text; // best effort
    std::wstring out = L"WEBVTT\n\n";
    // Minimal SRT->VTT: replace timestamp commas with dots.
    for (size_t i = 0; i < text.size(); ++i) {
        wchar_t c = text[i];
        if (c == L',' && i >= 2 && i + 3 < text.size() && iswdigit(text[i-1]) && iswdigit(text[i+1]) && iswdigit(text[i+2]) && iswdigit(text[i+3])) out += L'.';
        else out += c;
    }
    return out;
}

std::wstring JsEscapeSingleQuoted(const std::wstring& s);

void LoadSubtitleFile() {
    Tab* tab = ActiveTab();
    if (!tab || !tab->webview) return;
    wchar_t fileName[MAX_PATH] = L"";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_main;
    ofn.lpstrFilter = L"Subtitle Files (*.vtt;*.srt)\0*.vtt;*.srt\0WebVTT (*.vtt)\0*.vtt\0SubRip (*.srt)\0*.srt\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = L"Load Subtitle File";
    if (!GetOpenFileNameW(&ofn)) return;
    HANDLE file = CreateFileW(fileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) { MessageBoxW(g_main, L"Could not read subtitle file.", L"Subtitles", MB_OK | MB_ICONERROR); return; }
    LARGE_INTEGER size{};
    std::string bytes;
    if (GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart < (4 << 20)) {
        bytes.resize(static_cast<size_t>(size.QuadPart));
        DWORD read = 0;
        ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr);
        bytes.resize(read);
    }
    CloseHandle(file);
    if (bytes.empty()) { MessageBoxW(g_main, L"Subtitle file is empty or too large.", L"Subtitles", MB_OK | MB_ICONERROR); return; }
    // Handle UTF-8 BOM
    if (bytes.size() >= 3 && (unsigned char)bytes[0]==0xEF && (unsigned char)bytes[1]==0xBB && (unsigned char)bytes[2]==0xBF) bytes = bytes.substr(3);
    std::wstring text = FromUtf8(bytes);
    text = SrtToVttIfNeeded(text, fileName);
    std::wstring esc = JsEscapeSingleQuoted(text); // reused for JS single-quoted string context? JsEscapeSingleQuoted escapes for single quotes/newlines
    // Build JS: create blob URL and attach track to active video.
    std::wstring js = L"(async()=>{try{"
        L"const vtt='" + esc + L"';"
        L"const vids=[...document.querySelectorAll('video')]; const v=vids.find(x=>!x.paused)||vids[0];"
        L"if(!v){alert('No video found for subtitles.');return;}"
        L"const blob=new Blob([vtt],{type:'text/vtt'}); const url=URL.createObjectURL(blob);"
        L"document.querySelectorAll('track[data-featherwin-sub]').forEach(t=>t.remove());"
        L"const tr=document.createElement('track'); tr.kind='subtitles'; tr.label='Loaded'; tr.srclang='en'; tr.src=url; tr.default=true; tr.setAttribute('data-featherwin-sub','1');"
        L"v.appendChild(tr); tr.track.mode='showing';"
        L"}catch(e){try{alert('Load subtitles failed: '+(e&&e.message?e.message:e));}catch(_){}}})();";
    tab->webview->ExecuteScript(js.c_str(), nullptr);
}

void ShowMediaMenu() {  // vertical dropdown beside PiP: 1) Download 2) Subtitles 3) first extension (settings)
    RECT r = ButtonRect(6);
    POINT pt{r.left, r.bottom};
    ClientToScreen(g_main, &pt);
    HMENU menu = CreatePopupMenu();
    HMENU subSub = CreatePopupMenu();
    AppendMenuW(subSub, MF_STRING, 21, L"Toggle / Auto");
    AppendMenuW(subSub, MF_STRING, 22, L"Load subtitle file (.vtt/.srt)..."); 
    AppendMenuW(subSub, MF_STRING, 23, L"Turn off");
    AppendMenuW(menu, MF_STRING, 1, L"Downloads");                 // position 1
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)subSub, L"Subtitles");  // position 2
    std::wstring slot3 = g_mediaExtSlot3.empty() ? L"Extension (set in Settings)" : g_mediaExtSlot3;
    AppendMenuW(menu, MF_STRING, 3, slot3.c_str());               // position 3 (chosen in settings)
    g_menuTracking = true;
    const int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_VERPOSANIMATION, pt.x, pt.y, 0, g_main, nullptr);
    g_menuTracking = false;
    DestroyMenu(menu); // destroys submenus too
    switch (cmd) {
        case 1: ShowDownloadsWindow(); break;
        case 21: ToggleSubtitlesAuto(); break;
        case 22: LoadSubtitleFile(); break;
        case 23: TurnSubtitlesOff(); break;
        case 3:
            if (g_mediaExtSlot3.empty()) {
                OpenSettingsTab();
            } else {
                // Open extensions folder / settings context; invoking a specific extension action requires extension ID.
                if (!g_extensionsFolder.empty()) ShellExecuteW(nullptr, L"open", g_extensionsFolder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                MessageBoxW(g_main, (L"Extension slot 3: " + g_mediaExtSlot3 + L"\n\nSet or change it in Settings. Direct extension-button invocation depends on WebView2 extension APIs/IDs.").c_str(), L"Extension", MB_OK | MB_ICONINFORMATION);
            }
            break;
    }
}

void ShowShieldsMenu() {
    const std::wstring host = CurrentPageHost();
    const bool siteOff = IsShieldsOffForHost(host);
    const bool youtube = IsYouTubeHost(host);
    HMENU menu = CreatePopupMenu();
    std::wstring status = L"Shields: " + std::wstring(g_shieldsEnabled ? L"On" : L"Off (global)");
    AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, status.c_str());
    if (!host.empty()) {
        std::wstring siteStatus = youtube ? L"This site: YouTube (always allowed)"
                                          : (siteOff ? L"Ads allowed on this site" : L"Ads blocked on this site");
        AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, siteStatus.c_str());
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    UINT siteFlags = MF_STRING;
    if (host.empty() || youtube) siteFlags |= MF_GRAYED;
    AppendMenuW(menu, siteFlags, 30, siteOff ? L"Block ads on this website" : L"Allow ads on this website");
    AppendMenuW(menu, MF_STRING | (g_shieldsEnabled ? MF_CHECKED : MF_UNCHECKED), 31, L"Shields enabled (all sites)");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 32, L"Manage site exceptions...");
    RECT r = ButtonRect(4);  // shield button index
    POINT pt{r.left, r.bottom};
    ClientToScreen(g_main, &pt);
    g_menuTracking = true;
    const int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, g_main, nullptr);
    g_menuTracking = false;
    DestroyMenu(menu);
    switch (cmd) {
        case 30:
            ToggleShieldsForCurrentSite();
            break;
        case 31:
            g_shieldsEnabled = !g_shieldsEnabled;
            if (g_shield) InvalidateRect(g_shield, nullptr, FALSE);
            { Tab* t = ActiveTab(); if (t && t->webview) t->webview->Reload(); }
            break;
        case 32:
            if (!g_shieldExceptionsPath.empty()) ShellExecuteW(nullptr, L"open", g_shieldExceptionsPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            break;
    }
}

std::wstring StripTrackingParams(const std::wstring& url) {
    const size_t q = url.find(L'?');
    if (q == std::wstring::npos) return url;
    const size_t hash = url.find(L'#', q);
    std::wstring base = url.substr(0, q + 1);
    std::wstring query = url.substr(q + 1, hash == std::wstring::npos ? std::wstring::npos : hash - (q + 1));
    std::wstring frag = hash == std::wstring::npos ? L"" : url.substr(hash);
    auto isTracking = [](const std::wstring& key) {
        if (key.rfind(L"utm_", 0) == 0) return true;
        return key == L"fbclid" || key == L"gclid" || key == L"gclsrc" || key == L"dclid" ||
               key == L"msclkid" || key == L"mc_cid" || key == L"mc_eid" || key == L"yclid" || key == L"_hsenc" || key == L"_hsmi";
    };
    std::wstring out;
    size_t p = 0;
    while (p < query.size()) {
        size_t amp = query.find(L'&', p);
        if (amp == std::wstring::npos) amp = query.size();
        std::wstring part = query.substr(p, amp - p);
        size_t eq = part.find(L'=');
        std::wstring key = eq == std::wstring::npos ? part : part.substr(0, eq);
        if (!isTracking(key)) {
            if (!out.empty()) out += L"&";
            out += part;
        }
        p = amp + 1;
    }
    if (out.empty()) return url.substr(0, q) + frag;
    return base + out + frag;
}

void LoadBookmarks() {
    if (g_bookmarkPath.empty()) return;
    HANDLE file = CreateFileW(g_bookmarkPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    LARGE_INTEGER size{};
    std::string bytes;
    if (GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart < (1 << 20)) {
        bytes.resize(static_cast<size_t>(size.QuadPart));
        DWORD read = 0;
        ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr);
        bytes.resize(read);
    }
    CloseHandle(file);
    g_bookmarks = DeserializeBookmarks(FromUtf8(bytes));
}

HWND CreateChild(HWND parent, const wchar_t* cls, const wchar_t* text, DWORD style, DWORD exStyle, int id);
void LayoutChrome();
void LayoutContent();
void FocusActive();
void HideFindBar();
void FindInPage(bool forward);

std::wstring JsEscapeSingleQuoted(const std::wstring& s) {
    std::wstring out;
    for (wchar_t c : s) {
        if (c == L'\\') out += L"\\\\";
        else if (c == L'\'') out += L"\\'";
        else if (c == L'\r') {}
        else if (c == L'\n') out += L"\\n";
        else out += c;
    }
    return out;
}

void LayoutFindBar() {
    if (!g_findVisible) return;
    RECT c{};
    GetClientRect(g_main, &c);
    const int top = FindBarTop() + 2;
    const int h = kFindBarHeight - 4;
    int x = c.right - 8;
    if (g_findClose) { MoveWindow(g_findClose, x - 28, top, 28, h, TRUE); x -= 32; }
    if (g_findNext)  { MoveWindow(g_findNext,  x - 28, top, 28, h, TRUE); x -= 30; }
    if (g_findPrev)  { MoveWindow(g_findPrev,  x - 28, top, 28, h, TRUE); x -= 32; }
    const int editW = std::clamp(x - 8, 120, 280);
    if (g_findEdit)  { MoveWindow(g_findEdit, x - editW, top, editW, h, TRUE); }
}

void EnsureFindBar() {
    if (g_findEdit) return;
    g_findEdit = CreateChild(g_main, L"EDIT", L"", ES_AUTOHSCROLL, WS_EX_CLIENTEDGE, kFindEdit);
    g_findPrev = CreateChild(g_main, L"BUTTON", L"\u25B2", BS_PUSHBUTTON, 0, kFindPrev);
    g_findNext = CreateChild(g_main, L"BUTTON", L"\u25BC", BS_PUSHBUTTON, 0, kFindNext);
    g_findClose = CreateChild(g_main, L"BUTTON", L"\u00D7", BS_PUSHBUTTON, 0, kFindClose);
    for (HWND c : {g_findEdit, g_findPrev, g_findNext, g_findClose}) {
        SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);
        ShowWindow(c, SW_HIDE);
    }
    g_findEditProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(g_findEdit, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(+[](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
        if (msg == WM_KEYDOWN) {
            if (wp == VK_ESCAPE) { HideFindBar(); return 0; }
            if (wp == VK_RETURN) { FindInPage(!IsShiftDown()); return 0; }
            if (IsCtrlDown() && (wp == 'G' || wp == 'F')) { PostMessageW(g_main, kMsgShortcut, wp, IsShiftDown() ? 1 : 0); return 0; }
        }
        if (msg == WM_CHAR && (wp == VK_ESCAPE || wp == VK_RETURN)) return 0;
        return CallWindowProcW(g_findEditProc, hwnd, msg, wp, lp);
    })));
}

void ShowFindBar() {  // FR-NAV3
    EnsureFindBar();
    if (!g_findVisible) {
        g_findVisible = true;
        LayoutChrome();
        LayoutContent();
        InvalidateRect(g_main, nullptr, FALSE);
    }
    for (HWND c : {g_findEdit, g_findPrev, g_findNext, g_findClose}) if (c) ShowWindow(c, SW_SHOW);
    LayoutFindBar();
    if (g_findEdit) { SetFocus(g_findEdit); SendMessageW(g_findEdit, EM_SETSEL, 0, -1); }
}

void HideFindBar() {  // FR-NAV3
    if (!g_findVisible && !g_findEdit) return;
    g_findVisible = false;
    for (HWND c : {g_findEdit, g_findPrev, g_findNext, g_findClose}) if (c) ShowWindow(c, SW_HIDE);
    Tab* t = ActiveTab();
    if (t && t->webview) t->webview->ExecuteScript(L"window.getSelection().removeAllRanges();", nullptr);
    LayoutChrome();
    LayoutContent();
    InvalidateRect(g_main, nullptr, FALSE);
    FocusActive();
}

void FindInPage(bool forward) {  // FR-NAV3
    Tab* t = ActiveTab();
    if (!t || !t->webview || !g_findEdit) return;
    std::wstring text = GetText(g_findEdit);
    if (text.empty()) return;
    std::wstring esc = JsEscapeSingleQuoted(text);
    // window.find(string, caseSensitive, backwards, wrapAround, wholeWord, searchInFrames, showDialog)
    std::wstring js = L"window.find('" + esc + L"', false, " + (forward ? L"false" : L"true") + L", true, false, false, false);";
    t->webview->ExecuteScript(js.c_str(), nullptr);
}

void LayoutChrome() {
    if (!g_main) return;
    HWND btns[] = {g_back, g_forward, g_reload, g_star, g_shield, g_pip, g_media};
    for (int i = 0; i < 7; ++i) {
        RECT r = ButtonRect(i);
        if (btns[i]) MoveWindow(btns[i], r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);
    }
    RECT pill = AddressPillRect();
    if (g_address) {
        MoveWindow(g_address, static_cast<int>(pill.left + 10), static_cast<int>(pill.top + 7),
                   std::max(0, static_cast<int>(pill.right - pill.left - 20)),
                   static_cast<int>(pill.bottom - pill.top - 14), TRUE);
    }
    if (g_findEdit) {
        for (HWND c : {g_findEdit, g_findPrev, g_findNext, g_findClose}) if (c) ShowWindow(c, g_findVisible ? SW_SHOW : SW_HIDE);
        LayoutFindBar();
    }
}

void LayoutContent() {
    RECT bounds = ContentBounds();
    for (auto& tab : g_tabs) {
        if (tab->controller) tab->controller->put_Bounds(bounds);
    }
}

void RefreshChrome() {
    Tab* tab = ActiveTab();
    BOOL canBack = FALSE;
    BOOL canForward = FALSE;
    if (tab && tab->webview) {
        tab->webview->get_CanGoBack(&canBack);
        tab->webview->get_CanGoForward(&canForward);
    }
    EnableWindow(g_back, canBack);
    EnableWindow(g_forward, canForward);
    EnableWindow(g_reload, tab && tab->webview);
    EnableWindow(g_star, tab && !tab->url.empty());
    EnableWindow(g_shield, TRUE);
    if (g_star) InvalidateRect(g_star, nullptr, FALSE);
    if (g_shield) InvalidateRect(g_shield, nullptr, FALSE);
    if (tab && GetFocus() != g_address && g_address) {
        SetWindowTextW(g_address, tab->url.c_str());
    }
}

void CloseFolderPopupsFrom(int level);
void CloseAllFolderPopups() { CloseFolderPopupsFrom(0); }

void CloseFolderPopupsFrom(int level) {
    if (level < 0) level = 0;
    for (int i = kMaxFolderLevels - 1; i >= level; --i) {
        if (g_folderPopups[i] && IsWindow(g_folderPopups[i])) {
            DestroyWindow(g_folderPopups[i]);
        }
        g_folderPopups[i] = nullptr;
        g_folderPopupNodes[i] = nullptr;
        g_folderPopupItems[i].clear();
        g_folderPopupOverflow[i] = false;
    }
    g_hoverPopupItem = -1;
}

int GetPopupLevel(HWND hwnd) {
    for (int i = 0; i < kMaxFolderLevels; ++i) {
        if (g_folderPopups[i] == hwnd) return i;
    }
    return -1;
}

BookmarkNode* GetPopupNode(HWND hwnd) {
    const int level = GetPopupLevel(hwnd);
    return level >= 0 ? g_folderPopupNodes[level] : nullptr;
}

std::vector<BookmarkNode*>* GetPopupItems(HWND hwnd) {
    const int level = GetPopupLevel(hwnd);
    return level >= 0 ? &g_folderPopupItems[level] : nullptr;
}

BookmarkTree* GetPopupContainer(HWND hwnd) {
    const int level = GetPopupLevel(hwnd);
    if (level < 0) return nullptr;
    if (g_folderPopupOverflow[level]) return &g_bookmarks;
    if (g_folderPopupNodes[level] && g_folderPopupNodes[level]->type == BookmarkNode::Folder) return &g_folderPopupNodes[level]->children;
    return &g_bookmarks;
}

void ArmFolderCloseTimer() {
    if (g_main) SetTimer(g_main, kTimerFolderClose, 400, nullptr);
}

bool IsPointOverFolderUI(POINT screenPt) {
    HWND hw = WindowFromPoint(screenPt);
    if (!hw) return false;
    if (hw == g_main || IsChild(g_main, hw)) {
        POINT c = screenPt;
        ScreenToClient(g_main, &c);
        if (c.y >= BookmarkBarTop() && c.y < BookmarkBarTop() + kBookmarkBarHeight) return true;
    }
    for (int i = 0; i < kMaxFolderLevels; ++i) {
        if (g_folderPopups[i] && (hw == g_folderPopups[i] || IsChild(g_folderPopups[i], hw))) return true;
    }
    return false;
}

void FocusActive() {
    Tab* tab = ActiveTab();
    if (tab && tab->controller) {
        tab->controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
    }
}

void TryResumeTab(Tab& tab) {  // FR-TAB6
    if (!tab.suspended || !tab.webview) return;
    ComPtr<ICoreWebView2_3> wv3;
    if (SUCCEEDED(tab.webview.As(&wv3)) && wv3) {
        wv3->Resume();
        tab.suspended = false;
    } else {
        tab.suspended = false;
        if (!tab.url.empty()) tab.webview->Navigate(tab.url.c_str());
    }
}

void ActivateTab(int id) {
    if (!FindTab(id)) return;
    g_activeId = id;
    for (auto& tab : g_tabs) {
        if (tab->id == id) {
            tab->lastActiveTick = GetTickCount();
            TryResumeTab(*tab);
        }
        if (tab->controller) tab->controller->put_IsVisible(tab->id == id);
    }
    LayoutContent();
    if (GetFocus() != g_address) FocusActive();
    RefreshChrome();
    InvalidateStrip();
}

void NewTab(const std::wstring& url, bool activate, bool pinned = false, const std::wstring& knownTitle = L"");
void NewTabForNewWindow(ComPtr<ICoreWebView2NewWindowRequestedEventArgs> args);
void CloseTab(int id);
void ReopenClosedTab();

void RequestSelfPiP() {  // FR-VID1: try page/self Picture-in-Picture first
    Tab* tab = ActiveTab();
    if (!tab || !tab->webview) return;
    const wchar_t* js = LR"JS((async()=>{try{
        if (document.pictureInPictureElement) { await document.exitPictureInPicture(); return; }
        const seen=new Set(), vids=[], roots=[document];
        const addRoot=(r)=>{ if(!r||seen.has(r)) return; seen.add(r); try{ r.querySelectorAll&&r.querySelectorAll('video').forEach(v=>vids.push(v)); }catch(e){}
          try{ r.querySelectorAll&&r.querySelectorAll('*').forEach(el=>{ if(el.shadowRoot) addRoot(el.shadowRoot); }); }catch(e){}
          try{ r.querySelectorAll&&r.querySelectorAll('iframe').forEach(f=>{ try{ const d=f.contentDocument; if(d) addRoot(d); }catch(e){} }); }catch(e){}
        };
        addRoot(document);
        const score=(v)=>{ let s=0; try{ const r=v.getBoundingClientRect(); s+=Math.min(4000,(r.width||0)*(r.height||0)/100); if(r.width>40&&r.height>30) s+=50;
          if(!v.paused) s+=200; if(v.readyState>=2) s+=80; if(v.currentTime>0) s+=40; if(v.currentSrc||v.src) s+=20; }catch(e){} return s; };
        const candidates=vids.filter(v=>{ try{ return v && (v.readyState>0||v.currentSrc||v.src||v.getAttribute('src')); }catch(e){ return !!v; } })
          .sort((a,b)=>score(b)-score(a));
        let v=candidates.find(x=>!x.paused) || candidates[0];
        if(!v){
          const hasCrossOriginIframe=[...document.querySelectorAll('iframe')].some(f=>{ try{ return !f.contentDocument; }catch(e){ return true; } });
          alert(hasCrossOriginIframe ? 'No accessible video found. The video may be inside a cross-origin player (e.g. Dailymotion/embed). Try the player\'s PiP control or a PiP extension.' : 'No video found on this page.');
          return;
        }
        try{ v.removeAttribute('disablepictureinpicture'); v.disablePictureInPicture=false; }catch(e){}
        if(v.requestPictureInPicture) await v.requestPictureInPicture();
        else alert('Picture in Picture is not available for this video.');
    }catch(e){console.error(e); try{alert('Picture in Picture failed: '+(e&&e.message?e.message:e));}catch(_){}}})();)JS";
    tab->webview->ExecuteScript(js, nullptr);
}

void ParseExtensionManifestPages(const std::wstring& extDir, std::wstring& popup, std::wstring& options);

void AddBrowserExtensionFromPath(ICoreWebView2Profile7* profile7, const std::wstring& extDir, bool notify) {
    if (!profile7 || extDir.empty()) return;
    std::wstring popup, options;
    ParseExtensionManifestPages(extDir, popup, options);
    profile7->AddBrowserExtension(
        extDir.c_str(),
        Callback<ICoreWebView2ProfileAddBrowserExtensionCompletedHandler>(
            [extDir, notify, popup, options](HRESULT errorCode, ICoreWebView2BrowserExtension* extension) -> HRESULT {
                if (FAILED(errorCode) || !extension) {
                    if (notify) {
                        wchar_t hr[32];
                        swprintf_s(hr, L"0x%08X", static_cast<unsigned>(errorCode));
                        MessageBoxW(g_main, (L"Could not load extension (HRESULT " + std::wstring(hr) + L"):\n" + extDir +
                            L"\n\nCommon causes: unsupported manifest/permissions for WebView2, invalid extension folder, or extension APIs not available. Use an unpacked folder that contains manifest.json.").c_str(), L"Extensions", MB_OK | MB_ICONWARNING);
                    }
                    return S_OK;
                }
                LPWSTR idRaw = nullptr;
                LPWSTR nameRaw = nullptr;
                extension->get_Id(&idRaw);
                extension->get_Name(&nameRaw);
                std::wstring id = TakeString(idRaw);
                std::wstring n = TakeString(nameRaw);
                if (!id.empty()) {
                    auto it = std::find_if(g_extensions.begin(), g_extensions.end(), [&](const ExtInfo& e){ return e.id == id; });
                    ExtInfo info; info.id = id; info.name = n.empty() ? extDir : n; info.path = extDir; info.popup = popup; info.options = options; info.enabled = true;
                    if (it == g_extensions.end()) g_extensions.push_back(std::move(info)); else *it = std::move(info);
                }
                if (notify) MessageBoxW(g_main, (L"Loaded extension: " + (n.empty() ? extDir : n) + L"\nID: " + id + L"\n\nIt will also load on restart if kept in the extensions folder. WebView2 has no toolbar action UI; use Extensions menu to open popup/options when available.").c_str(), L"Extensions", MB_OK | MB_ICONINFORMATION);
                return S_OK;
            }).Get());
}

void LoadDefaultExtensions(ICoreWebView2* webview) {
    if (g_extensionsLoaded || !webview || g_extensionsFolder.empty()) return;
    g_extensionsLoaded = true;
    ComPtr<ICoreWebView2_13> webview13;
    if (FAILED(webview->QueryInterface(IID_PPV_ARGS(&webview13))) || !webview13) return;
    ComPtr<ICoreWebView2Profile> profile;
    if (FAILED(webview13->get_Profile(&profile)) || !profile) return;
    ComPtr<ICoreWebView2Profile7> profile7;
    if (FAILED(profile.As(&profile7)) || !profile7) return;

    const std::wstring pattern = g_extensionsFolder + L"\\*";
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        const std::wstring extDir = g_extensionsFolder + L"\\" + fd.cFileName;
        const std::wstring manifest = extDir + L"\\manifest.json";
        if (GetFileAttributesW(manifest.c_str()) == INVALID_FILE_ATTRIBUTES) continue;
        AddBrowserExtensionFromPath(profile7.Get(), extDir, false);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

void AddUnpackedExtensionDialog() {
    if (g_extensionsFolder.empty()) return;
    SHCreateDirectoryExW(nullptr, g_extensionsFolder.c_str(), nullptr);
    // Pick an unpacked extension folder (must contain manifest.json). Chrome Web Store/.crx install is not supported by WebView2.
    wchar_t path[MAX_PATH] = L"";
    BROWSEINFOW bi{};
    bi.hwndOwner = g_main;
    bi.lpszTitle = L"Select an UNPACKED extension folder (the folder that directly contains manifest.json)";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_UAHINT;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return;
    if (!SHGetPathFromIDListW(pidl, path)) { CoTaskMemFree(pidl); return; }
    CoTaskMemFree(pidl);
    std::wstring src = path;
    // Common mistake: user selected a parent folder; try a single manifest.json subfolder.
    if (GetFileAttributesW((src + L"\\manifest.json").c_str()) == INVALID_FILE_ATTRIBUTES) {
        std::wstring probe = src + L"\\manifest.json";
        // no direct child manifest - keep src as-is and report clearly
    }
    if (GetFileAttributesW((src + L"\\manifest.json").c_str()) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(g_main, L"Selected folder does not directly contain manifest.json.\n\nChoose the UNPACKED extension folder (the one that contains manifest.json). For many GitHub extensions, select the 'src' or extension subfolder, not the repo root.\n\nNot supported: Chrome Web Store install, .crx files. Extract/download an unpacked folder first.", L"Add Extension", MB_OK | MB_ICONWARNING);
        return;
    }
    // Copy into extensions folder for persistence (copy folder into g_extensionsFolder).
    std::wstring base = src;
    size_t slash = base.find_last_of(L"\\/");
    if (slash != std::wstring::npos) base = base.substr(slash + 1);
    if (base.empty()) base = L"extension";
    std::wstring dest = g_extensionsFolder + L"\\" + base;
    // If source is already that destination, load directly.
    if (_wcsicmp(dest.c_str(), src.c_str()) == 0) {
        dest = src;
    } else {
        // Ensure unique target folder name by copying with a unique base if needed.
        std::wstring uniqueBase = base;
        std::wstring finalDest = g_extensionsFolder + L"\\" + uniqueBase;
        for (int i = 2; GetFileAttributesW(finalDest.c_str()) != INVALID_FILE_ATTRIBUTES; ++i) {
            uniqueBase = base + L"_" + std::to_wstring(i);
            finalDest = g_extensionsFolder + L"\\" + uniqueBase;
        }
        std::wstring from = src;
        from.push_back(L'\0');
        std::wstring to = g_extensionsFolder; // copy folder INTO extensions folder
        to.push_back(L'\0');
        SHFILEOPSTRUCTW op{};
        op.hwnd = g_main;
        op.wFunc = FO_COPY;
        op.pFrom = from.c_str();
        op.pTo = to.c_str();
        op.fFlags = FOF_NOCONFIRMATION | FOF_NOCONFIRMMKDIR | FOF_SILENT;
        if (SHFileOperationW(&op) != 0 || op.fAnyOperationsAborted) {
            // Fallback: try loading from original location (won't persist in extensions folder unless already there)
            dest = src;
            MessageBoxW(g_main, L"Could not copy the extension into featherWIN's extensions folder. I'll try loading it from the selected location (it may not persist on restart if moved).", L"Add Extension", MB_OK | MB_ICONWARNING);
        } else {
            dest = g_extensionsFolder + L"\\" + base;
            if (GetFileAttributesW(dest.c_str()) == INVALID_FILE_ATTRIBUTES) {
                // if folder was copied with a different effective name, fall back to search? simplest: use finalDest logic isn't applicable post-copy; try expected
                dest = finalDest;
            }
            if (GetFileAttributesW(dest.c_str()) == INVALID_FILE_ATTRIBUTES) dest = src;
        }
    }
    // Load now via active tab profile.
    Tab* t = ActiveTab();
    if (t && t->webview) {
        ComPtr<ICoreWebView2_13> webview13;
        if (SUCCEEDED(t->webview.As(&webview13)) && webview13) {
            ComPtr<ICoreWebView2Profile> profile;
            ComPtr<ICoreWebView2Profile7> profile7;
            if (SUCCEEDED(webview13->get_Profile(&profile)) && profile && SUCCEEDED(profile.As(&profile7)) && profile7) {
                AddBrowserExtensionFromPath(profile7.Get(), dest, true);
                return;
            }
        }
    }
}

bool RunProcessAndWait(const std::wstring& cmdLine, DWORD timeoutMs = 120000) {
    STARTUPINFOW si{}; si.cb = sizeof(si); si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};
    std::wstring cmd = cmdLine;
    std::vector<wchar_t> buf(cmd.begin(), cmd.end()); buf.push_back(L'\0');
    if (!CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) return false;
    DWORD wait = WaitForSingleObject(pi.hProcess, timeoutMs);
    DWORD code = 1;
    if (wait == WAIT_OBJECT_0) GetExitCodeProcess(pi.hProcess, &code);
    else TerminateProcess(pi.hProcess, 1);
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return wait == WAIT_OBJECT_0 && code == 0;
}

std::wstring PsQuote(const std::wstring& s) {
    std::wstring o = L"'";
    for (wchar_t c : s) { if (c == L'\'') o += L"''"; else o += c; }
    o += L"'";
    return o;
}

bool ExtractZipWithPowerShell(const std::wstring& zipPath, const std::wstring& destDir) {
    SHCreateDirectoryExW(nullptr, destDir.c_str(), nullptr);
    // Use -Command with & { ... } to reduce quoting issues.
    std::wstring script = L"& { Expand-Archive -LiteralPath " + PsQuote(zipPath) + L" -DestinationPath " + PsQuote(destDir) + L" -Force -ErrorAction Stop }";
    std::wstring cmd = L"powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command " + script;
    return RunProcessAndWait(cmd);
}

std::wstring FindManifestDir(const std::wstring& root) {
    if (GetFileAttributesW((root + L"\\manifest.json").c_str()) != INVALID_FILE_ATTRIBUTES) return root;
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW((root + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return L"";
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        std::wstring sub = root + L"\\" + fd.cFileName;
        if (GetFileAttributesW((sub + L"\\manifest.json").c_str()) != INVALID_FILE_ATTRIBUTES) { FindClose(h); return sub; }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    // One more level (common in archives)
    h = FindFirstFileW((root + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return L"";
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        std::wstring sub = root + L"\\" + fd.cFileName;
        WIN32_FIND_DATAW fd2{};
        HANDLE h2 = FindFirstFileW((sub + L"\\*").c_str(), &fd2);
        if (h2 != INVALID_HANDLE_VALUE) {
            do {
                if (!(fd2.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
                if (wcscmp(fd2.cFileName, L".") == 0 || wcscmp(fd2.cFileName, L"..") == 0) continue;
                std::wstring sub2 = sub + L"\\" + fd2.cFileName;
                if (GetFileAttributesW((sub2 + L"\\manifest.json").c_str()) != INVALID_FILE_ATTRIBUTES) { FindClose(h2); FindClose(h); return sub2; }
            } while (FindNextFileW(h2, &fd2));
            FindClose(h2);
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return L"";
}

static unsigned ReadU32LE(const std::vector<unsigned char>& b, size_t off) {
    if (off + 4 > b.size()) return 0;
    return static_cast<unsigned>(b[off]) | (static_cast<unsigned>(b[off + 1]) << 8) |
           (static_cast<unsigned>(b[off + 2]) << 16) | (static_cast<unsigned>(b[off + 3]) << 24);
}

bool ExtractCrxToZip(const std::wstring& crxPath, std::wstring& outZipPath) {
    HANDLE file = CreateFileW(crxPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    std::vector<unsigned char> bytes;
    if (GetFileSizeEx(file, &size) && size.QuadPart > 16 && size.QuadPart < (128 << 20)) {
        bytes.resize(static_cast<size_t>(size.QuadPart));
        DWORD read = 0;
        ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr);
        bytes.resize(read);
    }
    CloseHandle(file);
    if (bytes.size() < 16) return false;
    // CRX2/CRX3: "Cr24" magic + version + header fields, followed by ZIP payload.
    // WebView2 loads unpacked folders, not .crx packages; this is a best-effort local unpack.
    size_t zipOff = static_cast<size_t>(-1);
    if (bytes.size() >= 12 && bytes[0] == 'C' && bytes[1] == 'r' && bytes[2] == '2' && bytes[3] == '4') {
        const unsigned ver = ReadU32LE(bytes, 4);
        if (ver == 2 && bytes.size() >= 16) {
            const unsigned pkLen = ReadU32LE(bytes, 8);
            const unsigned sigLen = ReadU32LE(bytes, 12);
            const size_t off = 16ull + pkLen + sigLen;
            if (off < bytes.size()) zipOff = off;
        } else if (ver == 3 && bytes.size() >= 12) {
            const unsigned hdrLen = ReadU32LE(bytes, 8);
            const size_t off = 12ull + hdrLen;
            if (off < bytes.size()) zipOff = off;
        }
    }
    // Fallback: locate ZIP local file header signature PK\x03\x04
    if (zipOff == static_cast<size_t>(-1) || zipOff >= bytes.size() ||
        !(bytes[zipOff] == 'P' && bytes[zipOff + 1] == 'K' && bytes[zipOff + 2] == 3 && bytes[zipOff + 3] == 4)) {
        zipOff = static_cast<size_t>(-1);
        for (size_t i = 0; i + 4 <= bytes.size(); ++i) {
            if (bytes[i] == 'P' && bytes[i+1] == 'K' && bytes[i+2] == 3 && bytes[i+3] == 4) { zipOff = i; break; }
        }
    }
    if (zipOff == static_cast<size_t>(-1) || zipOff >= bytes.size()) return false;
    wchar_t tempPath[MAX_PATH] = L"";
    wchar_t tempFile[MAX_PATH] = L"";
    GetTempPathW(MAX_PATH, tempPath);
    GetTempFileNameW(tempPath, L"fwx", 0, tempFile);
    outZipPath = std::wstring(tempFile) + L".zip";
    DeleteFileW(tempFile);
    HANDLE out = CreateFileW(outZipPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (out == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    WriteFile(out, bytes.data() + zipOff, static_cast<DWORD>(bytes.size() - zipOff), &written, nullptr);
    CloseHandle(out);
    return written > 0;
}

void InstallExtensionFromArchiveDialog() {
    if (g_extensionsFolder.empty()) {
        MessageBoxW(g_main, L"Extensions folder is not available.", L"Extensions", MB_OK | MB_ICONWARNING);
        return;
    }
    wchar_t fileName[MAX_PATH] = L"";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_main;
    ofn.lpstrFilter = L"Extension Archives (*.zip;*.crx)\\0*.zip;*.crx\\0ZIP (*.zip)\\0*.zip\\0CRX (*.crx)\\0*.crx\\0All Files (*.*)\\0*.*\\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = L"Install Extension from ZIP/CRX (unpacked)";
    if (!GetOpenFileNameW(&ofn)) return;
    SHCreateDirectoryExW(nullptr, g_extensionsFolder.c_str(), nullptr);
    std::wstring src = fileName;
    std::wstring lower = src;
    for (auto& c : lower) c = towlower(c);
    const bool isCrx = lower.size() >= 4 && lower.compare(lower.size() - 4, 4, L".crx") == 0;
    // Destination folder name
    std::wstring base = src.substr(src.find_last_of(L"\\/") + 1);
    size_t dot = base.find_last_of(L'.'); if (dot != std::wstring::npos) base = base.substr(0, dot);
    for (auto& c : base) if (wcschr(L"<>:\"/\\|?*", c)) c = L'_';
    if (base.empty()) base = L"extension";
    std::wstring dest = g_extensionsFolder + L"\\" + base;
    for (int i = 1; GetFileAttributesW(dest.c_str()) != INVALID_FILE_ATTRIBUTES && i < 1000; ++i) dest = g_extensionsFolder + L"\\" + base + L"-" + std::to_wstring(i);
    std::wstring zipToExtract = src;
    std::wstring tempZip;
    if (isCrx) {
        if (!ExtractCrxToZip(src, tempZip)) {
            MessageBoxW(g_main, L"Could not read this .crx file. Chrome Web Store .crx packages may be incompatible with WebView2. Try downloading the extension's unpacked/source ZIP from the developer (e.g. GitHub Releases) and use Install from ZIP.", L"Extensions", MB_OK | MB_ICONWARNING);
            return;
        }
        zipToExtract = tempZip;
    }
    if (!ExtractZipWithPowerShell(zipToExtract, dest)) {
        if (!tempZip.empty()) DeleteFileW(tempZip.c_str());
        MessageBoxW(g_main, L"Could not extract the archive. Make sure it's a valid ZIP/unpacked extension archive. PowerShell Expand-Archive is required (built into Windows).", L"Extensions", MB_OK | MB_ICONWARNING);
        return;
    }
    if (!tempZip.empty()) DeleteFileW(tempZip.c_str());
    std::wstring manifestDir = FindManifestDir(dest);
    if (manifestDir.empty()) {
        MessageBoxW(g_main, (L"Extracted, but no manifest.json was found in the archive:\n" + dest + L"\n\nSelect the correct unpacked extension ZIP (it must contain manifest.json).").c_str(), L"Extensions", MB_OK | MB_ICONWARNING);
        return;
    }
    // Load into the active profile if available
    Tab* at = ActiveTab();
    ComPtr<ICoreWebView2_13> wv13; ComPtr<ICoreWebView2Profile> profile; ComPtr<ICoreWebView2Profile7> profile7;
    if (at && at->webview) at->webview.As(&wv13);
    if (wv13) wv13->get_Profile(&profile);
    if (profile) profile.As(&profile7);
    if (profile7) {
        AddBrowserExtensionFromPath(profile7.Get(), manifestDir, true);
    } else {
        MessageBoxW(g_main, (L"Extracted extension to:\n" + manifestDir + L"\n\nRestart featherWIN to load it (active profile API unavailable).").c_str(), L"Extensions", MB_OK | MB_ICONINFORMATION);
    }
}

void SetupTab(Tab& tab) {
    const int id = tab.id;
    if (tab.webview) LoadDefaultExtensions(tab.webview.Get());
    EventRegistrationToken token{};
    tab.webview->add_SourceChanged(
        Callback<ICoreWebView2SourceChangedEventHandler>(
            [id](ICoreWebView2* sender, ICoreWebView2SourceChangedEventArgs*) -> HRESULT {
                Tab* current = FindTab(id);
                if (!current) return S_OK;
                LPWSTR source = nullptr;
                sender->get_Source(&source);
                current->url = TakeString(source);
                if (id == g_activeId) RefreshChrome();
                return S_OK;
            }).Get(),
        &token);
    tab.webview->add_DocumentTitleChanged(
        Callback<ICoreWebView2DocumentTitleChangedEventHandler>(
            [id](ICoreWebView2* sender, IUnknown*) -> HRESULT {
                Tab* current = FindTab(id);
                if (!current) return S_OK;
                LPWSTR title = nullptr;
                sender->get_DocumentTitle(&title);
                current->title = TakeString(title);
                InvalidateStrip();
                return S_OK;
            }).Get(),
        &token);
    tab.webview->add_NewWindowRequested(
        Callback<ICoreWebView2NewWindowRequestedEventHandler>(
            [](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
                // Provide a real WebView2 target so window.open()/OAuth popups (e.g. pCloud Google login) keep opener access.
                NewTabForNewWindow(args);
                return S_OK;
            }).Get(),
        &token);
    tab.controller->add_AcceleratorKeyPressed(
        Callback<ICoreWebView2AcceleratorKeyPressedEventHandler>(
            [](ICoreWebView2Controller*, ICoreWebView2AcceleratorKeyPressedEventArgs* args) -> HRESULT {
                COREWEBVIEW2_KEY_EVENT_KIND kind = COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN;
                UINT vk = 0;
                args->get_KeyEventKind(&kind);
                args->get_VirtualKey(&vk);
                const bool down = kind == COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN || kind == COREWEBVIEW2_KEY_EVENT_KIND_SYSTEM_KEY_DOWN;
                if (down && IsCtrlDown() && IsShortcutKey(vk)) {
                    args->put_Handled(TRUE);
                    PostMessageW(g_main, kMsgShortcut, vk, IsShiftDown() ? 1 : 0);
                }
                return S_OK;
            }).Get(),
        &token);
    // FR-SHD1: basic request filter. YouTube is exempt (ads detection) per user request.
    tab.webview->AddWebResourceRequestedFilter(L"*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
    EventRegistrationToken wr{};
    tab.webview->add_WebResourceRequested(
        Callback<ICoreWebView2WebResourceRequestedEventHandler>(
            [](ICoreWebView2* sender, ICoreWebView2WebResourceRequestedEventArgs* args) -> HRESULT {
                if (!g_shieldsEnabled) return S_OK;  // FR-SHD3 global
                if (sender) {
                    LPWSTR src = nullptr;
                    sender->get_Source(&src);
                    const std::wstring pageHost = HostOf(TakeString(src));
                    if (IsShieldsOffForHost(pageHost)) return S_OK;  // FR-SHD2 per-site + YouTube exempt
                }
                ComPtr<ICoreWebView2WebResourceRequest> request;
                args->get_Request(&request);
                if (!request) return S_OK;
                LPWSTR uri = nullptr;
                request->get_Uri(&uri);
                const std::wstring url = TakeString(uri);
                const std::wstring host = HostOf(url);
                if (!host.empty() && IsBlockedHost(host)) {
                    args->put_Response(nullptr);
                }
                return S_OK;
            }).Get(),
        &wr);
    tab.webview->add_NavigationStarting(
        Callback<ICoreWebView2NavigationStartingEventHandler>(
            [](ICoreWebView2* /*sender*/, ICoreWebView2NavigationStartingEventArgs* /*args*/) -> HRESULT {
                CloseAllFolderPopups();
                // FR-SHD6 tracking-param stripping disabled here: cancel+navigate can break OAuth/POST/login flows
                // (e.g. Google login for pCloud). Revisit with a non-breaking approach.
                return S_OK;
            }).Get(),
        &token);
    tab.webview->AddScriptToExecuteOnDocumentCreated(L"document.addEventListener('mousedown',function(){try{window.chrome.webview.postMessage('pageclick')}catch(e){}});", nullptr);
    tab.webview->add_WebMessageReceived(
        Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [](ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                LPWSTR msg = nullptr;
                args->TryGetWebMessageAsString(&msg);
                std::wstring m = TakeString(msg);
                if (m == L"pageclick") {
                    CloseAllFolderPopups();
                } else if (m.rfind(L"nav|", 0) == 0 && sender) {
                    std::wstring url = m.substr(4);
                    if (!url.empty()) sender->Navigate(url.c_str());
                } else if (m.rfind(L"dlopen|", 0) == 0 || m.rfind(L"dlshow|", 0) == 0) {
                    const bool show = m.rfind(L"dlshow|", 0) == 0;
                    const size_t idx = static_cast<size_t>(_wtoi(m.substr(7).c_str()));
                    if (idx < g_downloads.size()) {
                        const auto& d = g_downloads[idx];
                        std::wstring path = d.file;
                        if (show) {
                            if (!path.empty() && GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
                                std::wstring arg = L"/select,\"" + path + L"\"";
                                ShellExecuteW(nullptr, L"open", L"explorer.exe", arg.c_str(), nullptr, SW_SHOWNORMAL);
                            } else {
                                PWSTR downloads = nullptr;
                                if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &downloads))) {
                                    std::wstring folder = TakeString(downloads);
                                    ShellExecuteW(nullptr, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                                }
                            }
                        } else {
                            if (!path.empty() && GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
                                ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                            } else if (!d.url.empty()) {
                                ShellExecuteW(nullptr, L"open", d.url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                            }
                        }
                    }
                }
                return S_OK;
            }).Get(),
        &token);
    tab.webview->add_NavigationCompleted(
        Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [id](ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                BOOL ok = FALSE;
                if (args) args->get_IsSuccess(&ok);
                if (!ok || !sender) return S_OK;
                Tab* current = FindTab(id);
                if (!current) return S_OK;
                LPWSTR uri = nullptr;
                sender->get_Source(&uri);
                std::wstring url = TakeString(uri);
                if (!url.empty()) RecordHistory(url, current->title);  // FR-HIS3
                return S_OK;
            }).Get(),
        &token);
    ComPtr<ICoreWebView2_4> wv4;
    if (SUCCEEDED(tab.webview.As(&wv4)) && wv4) {
        wv4->add_DownloadStarting(
            Callback<ICoreWebView2DownloadStartingEventHandler>(
                [](ICoreWebView2*, ICoreWebView2DownloadStartingEventArgs* args) -> HRESULT {
                    if (!args) return S_OK;
                    // FR-DL2: default to user's Downloads folder.
                    LPWSTR resultPath = nullptr;
                    args->get_ResultFilePath(&resultPath);
                    std::wstring path = TakeString(resultPath);
                    std::wstring name = path;
                    const size_t slash = name.find_last_of(L"\\/");
                    if (slash != std::wstring::npos) name = name.substr(slash + 1);
                    ComPtr<ICoreWebView2DownloadOperation> op;
                    args->get_DownloadOperation(&op);
                    std::wstring uri;
                    if (op) {
                        LPWSTR u = nullptr;
                        op->get_Uri(&u);
                        uri = TakeString(u);
                    }
                    if (name.empty() && !uri.empty()) {
                        name = uri;
                        const size_t s = name.find_last_of(L"/\\?");
                        if (s != std::wstring::npos) name = name.substr(s + 1);
                        const size_t q = name.find(L'?');
                        if (q != std::wstring::npos) name = name.substr(0, q);
                        if (name.empty()) name = L"download";
                    }
                    // Sanitize filename for Windows paths
                    for (auto& c : name) {
                        if (c == L'<' || c == L'>' || c == L':' || c == L'"' || c == L'|' || c == L'?' || c == L'*' || c == L'/' || c == L'\\') c = L'_';
                        if (c < 32) c = L'_';
                    }
                    while (!name.empty() && (name.back() == L'.' || name.back() == L' ')) name.pop_back();
                    if (name.empty()) name = L"download";
                    if (!name.empty()) {
                        PWSTR downloads = nullptr;
                        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &downloads))) {
                            std::wstring folder = TakeString(downloads);
                            std::wstring candidate = folder + L"\\" + name;
                            // FR-DL2: avoid "download error" from collisions by uniquifying.
                            const DWORD attr = GetFileAttributesW(candidate.c_str());
                            if (attr != INVALID_FILE_ATTRIBUTES) {
                                std::wstring base = name, ext;
                                const size_t dot = name.find_last_of(L'.');
                                if (dot != std::wstring::npos && dot > 0) { base = name.substr(0, dot); ext = name.substr(dot); }
                                for (int i = 1; i < 1000; ++i) {
                                    candidate = folder + L"\\" + base + L" (" + std::to_wstring(i) + L")" + ext;
                                    if (GetFileAttributesW(candidate.c_str()) == INVALID_FILE_ATTRIBUTES) break;
                                }
                            }
                            path = candidate;
                            args->put_ResultFilePath(path.c_str());
                            // Chrome extension files: WebView2 cannot install from Chrome Web Store/.crx.
                            // Save succeeds; tell user how to load unpacked/ZIP if relevant.
                            std::wstring lower = name;
                            for (auto& c : lower) c = towlower(c);
                            if (lower.size() >= 4 && lower.compare(lower.size() - 4, 4, L".crx") == 0) {
                                g_downloads.push_back({path, uri, L"saved (.crx not installable in WebView2)"});
                                if (g_downloads.size() > 50) g_downloads.erase(g_downloads.begin());
                                PostMessageW(g_main, WM_APP + 4, 0, 0); // info hint (async; avoid modal in download callback)
                                return S_OK;
                            }
                        }
                    }
                    // Track (FR-DL1 minimal)
                    DownloadItem di;
                    di.file = path.empty() ? name : path;
                    di.url = uri;
                    di.state = L"started";
                    g_downloads.push_back(std::move(di));
                    if (g_downloads.size() > 50) g_downloads.erase(g_downloads.begin());
                    return S_OK;
                }).Get(),
            &token);
    }
}

void NewTab(const std::wstring& url, bool activate, bool pinned, const std::wstring& knownTitle) {
    if (!g_env) return;
    auto tab = std::make_unique<Tab>();
    tab->id = g_nextTabId++;
    tab->url = url;
    tab->pinned = pinned;
    if (!knownTitle.empty()) tab->title = knownTitle;
    const int id = tab->id;
    g_tabs.push_back(std::move(tab));
    g_env->CreateCoreWebView2Controller(
        g_main,
        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [id, activate](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                Tab* created = FindTab(id);
                if (!created) {
                    if (controller) controller->Close();
                    return S_OK;
                }
                if (FAILED(result) || !controller) {
                    CloseTab(id);
                    return S_OK;
                }
                created->controller = controller;
                controller->get_CoreWebView2(&created->webview);
                controller->put_IsVisible(FALSE);
                controller->put_Bounds(ContentBounds());
                SetupTab(*created);
                if (created->webview) created->webview->Navigate(created->url.c_str());
                if (activate || id == g_activeId) ActivateTab(id);
                else { SaveSession(); InvalidateStrip(); }
                return S_OK;
            }).Get());
}

void NewTabForNewWindow(ComPtr<ICoreWebView2NewWindowRequestedEventArgs> args) {
    if (!g_env || !args) return;
    ComPtr<ICoreWebView2Deferral> deferral;
    if (FAILED(args->GetDeferral(&deferral)) || !deferral) {
        LPWSTR uri = nullptr;
        args->get_Uri(&uri);
        args->put_Handled(TRUE);
        NewTab(TakeString(uri), true);
        return;
    }
    LPWSTR uri = nullptr;
    args->get_Uri(&uri);
    const std::wstring url = TakeString(uri);
    auto tab = std::make_unique<Tab>();
    tab->id = g_nextTabId++;
    tab->url = url;
    const int id = tab->id;
    g_tabs.push_back(std::move(tab));
    g_env->CreateCoreWebView2Controller(
        g_main,
        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [id, args, deferral](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                Tab* created = FindTab(id);
                if (!created) {
                    if (controller) controller->Close();
                    args->put_Handled(TRUE);
                    deferral->Complete();
                    return S_OK;
                }
                if (FAILED(result) || !controller) {
                    CloseTab(id);
                    args->put_Handled(TRUE);
                    deferral->Complete();
                    return S_OK;
                }
                created->controller = controller;
                controller->get_CoreWebView2(&created->webview);
                controller->put_IsVisible(FALSE);
                controller->put_Bounds(ContentBounds());
                SetupTab(*created);
                // Provide target for window.open/opener; WebView2 will perform the requested navigation.
                args->put_NewWindow(created->webview.Get());
                args->put_Handled(TRUE);
                deferral->Complete();
                ActivateTab(id);
                return S_OK;
            }).Get());
}

void CloseTab(int id) {
    const int index = IndexOf(id);
    if (index < 0) return;
    const bool wasActive = id == g_activeId;
    // FR-NAV4: remember closed tab (up to 20)
    Tab* closing = g_tabs[static_cast<size_t>(index)].get();
    if (closing && !closing->url.empty() && closing->url != L"about:blank") {
        g_closedTabs.push_back(ClosedTab{closing->url, closing->title});
        if (g_closedTabs.size() > 20) g_closedTabs.erase(g_closedTabs.begin());
    }
    if (g_tabs[static_cast<size_t>(index)]->controller) {
        g_tabs[static_cast<size_t>(index)]->controller->Close();
    }
    g_tabs.erase(g_tabs.begin() + index);
    if (g_tabs.empty()) {
        DestroyWindow(g_main);
        return;
    }
    if (wasActive) {
        const size_t next = std::min(static_cast<size_t>(index), g_tabs.size() - 1);
        ActivateTab(g_tabs[next]->id);
    } else {
        InvalidateStrip();
    }
}

void NavigateActive(const std::wstring& text) {
    Tab* tab = ActiveTab();
    if (!tab || !tab->webview || text.find_first_not_of(L" \t") == std::wstring::npos) return;
    tab->webview->Navigate(ToUrl(text).c_str());
    FocusActive();
}

void ToggleBookmark() {
    Tab* tab = ActiveTab();
    if (!tab || tab->url.empty() || tab->url.rfind(L"http", 0) != 0) return;
    BookmarkNode* existing = FindBookmarkNode(g_bookmarks, tab->url);
    if (existing) RemoveNode(g_bookmarks, existing);
    else AddBookmark(g_bookmarks, tab->title, tab->url);
    SaveBookmarks();
    InvalidateBookmarkBar();
    RefreshChrome();
}

BookmarkLayout GetBookmarkLayout() {
    BookmarkLayout layout;
    RECT client{};
    GetClientRect(g_main, &client);
    const int rightLimit = std::max(40, static_cast<int>(client.right) - 8);
    const int overflowWidth = 30;
    const int top = BookmarkBarTop() + 2;
    HDC dc = GetDC(g_main);
    HGDIOBJ old = SelectObject(dc, g_uiFont ? g_uiFont : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
    auto chipWidth = [&](BookmarkNode* node) -> int {
        SIZE sz{};
        if (node) GetTextExtentPoint32W(dc, node->title.c_str(), static_cast<int>(node->title.size()), &sz);
        int width = std::min(kBookmarkMaxWidth, static_cast<int>(sz.cx) + 28);
        if (node && node->type == BookmarkNode::Folder) width = std::min(kBookmarkMaxWidth, static_cast<int>(sz.cx) + 40);
        return width;
    };
    // Try to fit in 2 rows without overflow button.
    bool fits = true;
    {
        int row = 0, x = 6;
        for (size_t i = 0; i < g_bookmarks.size(); ++i) {
            int w = chipWidth(g_bookmarks[i].get());
            if (x + w > rightLimit) {
                row++; x = 6;
                if (row >= kBookmarkBarRows || x + w > rightLimit) { fits = false; break; }
            }
            x += w + kBookmarkGap;
        }
    }
    int row = 0, x = 6;
    const int reserve = fits ? 0 : overflowWidth;
    for (size_t i = 0; i < g_bookmarks.size(); ++i) {
        BookmarkNode* node = g_bookmarks[i].get();
        int w = chipWidth(node);
        if (x + w > rightLimit - (row == kBookmarkBarRows - 1 ? reserve : 0)) {
            row++; x = 6;
        }
        if (row >= kBookmarkBarRows || x + w > rightLimit - (row == kBookmarkBarRows - 1 ? reserve : 0)) {
            layout.hasOverflow = true;
            for (size_t j = i; j < g_bookmarks.size(); ++j) layout.overflow.push_back(g_bookmarks[j].get());
            break;
        }
        const int y = top + row * kBookmarkRowHeight;
        layout.visible.push_back(BookmarkChip{RECT{x, y, x + w, y + kBookmarkRowHeight - 2}, node});
        x += w + kBookmarkGap;
    }
    if (layout.hasOverflow) {
        const int ox = std::max(6, rightLimit - overflowWidth);
        const int oy = top + (kBookmarkBarRows - 1) * kBookmarkRowHeight;
        layout.overflowRect = RECT{ox, oy, ox + overflowWidth, oy + kBookmarkRowHeight - 2};
    }
    SelectObject(dc, old);
    ReleaseDC(g_main, dc);
    return layout;
}

std::vector<BookmarkChip> BookmarkChips() { return GetBookmarkLayout().visible; }

BookmarkNode* BookmarkAt(int x, int y) {
    if (y < BookmarkBarTop() || y >= BookmarkBarTop() + kBookmarkBarHeight) return nullptr;
    BookmarkLayout layout = GetBookmarkLayout();
    if (layout.hasOverflow && x >= layout.overflowRect.left && x < layout.overflowRect.right) return nullptr;
    for (const auto& chip : layout.visible) {
        if (x >= chip.rect.left && x < chip.rect.right) return chip.node;
    }
    return nullptr;
}

bool OverflowAt(int x, int y) {
    if (y < BookmarkBarTop() || y >= BookmarkBarTop() + kBookmarkBarHeight) return false;
    BookmarkLayout layout = GetBookmarkLayout();
    return layout.hasOverflow && x >= layout.overflowRect.left && x < layout.overflowRect.right;
}

void ShowOverflowMenu() {
    BookmarkLayout layout = GetBookmarkLayout();
    if (!layout.hasOverflow || layout.overflow.empty()) return;
    HMENU menu = CreatePopupMenu();
    std::vector<BookmarkNode*> nodes;
    std::function<void(HMENU, const std::vector<BookmarkNode*>&)> build = [&](HMENU m, const std::vector<BookmarkNode*>& items) {
        for (BookmarkNode* n : items) {
            if (n->type == BookmarkNode::Folder) {
                HMENU sub = CreatePopupMenu();
                std::vector<BookmarkNode*> children;
                for (auto& c : n->children) children.push_back(c.get());
                build(sub, children);
                AppendMenuW(m, MF_POPUP, reinterpret_cast<UINT_PTR>(sub), n->title.c_str());
            } else {
                nodes.push_back(n);
                AppendMenuW(m, MF_STRING, 2000 + static_cast<UINT>(nodes.size()), n->title.c_str());
            }
        }
    };
    std::vector<BookmarkNode*> over = layout.overflow;
    build(menu, over);
    POINT pt{layout.overflowRect.left, layout.overflowRect.bottom};
    ClientToScreen(g_main, &pt);
    g_menuTracking = true;
    const int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, g_main, nullptr);
    g_menuTracking = false;
    DestroyMenu(menu);
    if (cmd >= 2001) {
        const size_t idx = static_cast<size_t>(cmd - 2001);
        if (idx < nodes.size() && nodes[idx] && nodes[idx]->type == BookmarkNode::Bookmark) {
            Tab* tab = ActiveTab();
            if (tab && tab->webview) tab->webview->Navigate(nodes[idx]->url.c_str());
        }
    }
}

void PaintTabStrip(HDC dc) {
    RECT c{};
    GetClientRect(g_main, &c);
    RECT strip{0, kTabStripTop, c.right, kTabStripTop + kTabStripHeight};
    HBRUSH bg = CreateSolidBrush(kThemeBg);
    FillRect(dc, &strip, bg);
    DeleteObject(bg);
    HGDIOBJ old = SelectObject(dc, g_uiFont ? g_uiFont : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
    SetBkMode(dc, TRANSPARENT);
    int stripEnd = 0;
    for (size_t i = 0; i < g_tabs.size(); ++i) {
        Tab& tab = *g_tabs[i];
        RECT r = TabRect(static_cast<int>(i));
        stripEnd = std::max(stripEnd, static_cast<int>(r.right));
        HBRUSH tabBrush = CreateSolidBrush(tab.id == g_activeId ? RGB(255, 255, 255) : (tab.pinned ? RGB(232, 236, 242) : RGB(224, 227, 232)));
        FillRect(dc, &r, tabBrush);
        DeleteObject(tabBrush);
        if (tab.groupId > 0) { // FR-GRP3 (minimal visual): group color marker
            if (TabGroup* g = FindTabGroup(tab.groupId)) {
                RECT mark{r.left, r.bottom - 3, r.right, r.bottom};
                HBRUSH gb = CreateSolidBrush(g->color); FillRect(dc, &mark, gb); DeleteObject(gb);
            }
        }
        if (tab.pinned) {
            std::wstring one = L"\u2022";
            if (!tab.title.empty()) { one = std::wstring(1, towupper(tab.title[0])); }
            else if (!tab.url.empty()) { const auto h = HostOf(tab.url); if (!h.empty()) one = std::wstring(1, towupper(h[0])); }
            RECT label = r;
            DrawTextW(dc, one.c_str(), -1, &label, DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);
        } else {
            RECT label{r.left + 10, r.top, r.right - kCloseSize - 12, r.bottom};
            DrawTextW(dc, tab.title.c_str(), -1, &label, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
            RECT cr = CloseRect(r, false);
            DrawTextW(dc, L"\u00D7", -1, &cr, DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);
        }
    }
    RECT plus{stripEnd, kTabStripTop + 2, std::min(static_cast<int>(c.right), stripEnd + kNewTabWidth), kTabStripTop + kTabStripHeight};
    DrawTextW(dc, L"+", -1, &plus, DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);
    SelectObject(dc, old);
}

void PaintBookmarkBar(HDC dc) {
    RECT c{};
    GetClientRect(g_main, &c);
    RECT bar{0, BookmarkBarTop(), c.right, BookmarkBarTop() + kBookmarkBarHeight};
    HBRUSH bg = CreateSolidBrush(kThemeBg);
    FillRect(dc, &bar, bg);
    DeleteObject(bg);
    HGDIOBJ old = SelectObject(dc, g_uiFont ? g_uiFont : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
    SetBkMode(dc, TRANSPARENT);
    if (g_bookmarks.empty()) {
        SetTextColor(dc, RGB(120, 124, 130));
        RECT hint{8, BookmarkBarTop(), c.right - 8, BookmarkBarTop() + kBookmarkBarHeight};
        DrawTextW(dc, L"Star a page (Ctrl+D) to keep it here", -1, &hint, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    }
    BookmarkLayout layout = GetBookmarkLayout();
    for (const auto& chip : layout.visible) {
        COLORREF fill = RGB(255, 255, 255);
        if (chip.node && chip.node->type == BookmarkNode::Folder) fill = RGB(222, 228, 238);  // primary folder darker than bookmarks
        if (g_barDragging && chip.node == g_barDropTarget && chip.node && chip.node != g_barDragNode) {
            fill = RGB(200, 205, 215);  // drop target highlight
        }
        FillRoundedRect(dc, chip.rect, 10, fill, fill);
        SetTextColor(dc, RGB(32, 33, 36));
        RECT label = chip.rect;
        label.left += 8;
        if (chip.node && chip.node->type == BookmarkNode::Folder) {
            DrawTextW(dc, L"\u25BE", -1, &label, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
            label.left += 14;
        }
        label.right -= 6;
        DrawTextW(dc, chip.node ? chip.node->title.c_str() : L"", -1, &label, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
    if (layout.hasOverflow) {
        FillRoundedRect(dc, layout.overflowRect, 10, RGB(232, 235, 239), kThemeBorder);
        SetTextColor(dc, RGB(58, 62, 66));
        DrawTextW(dc, L"\u00BB", -1, &layout.overflowRect, DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);
    }
    SelectObject(dc, old);
}

RECT MenuItemRect(int index) {
    const wchar_t* labels[] = {L"File", L"Bookmarks", L"History", L"Extensions", L"Sessions", L"Tab Groups"};
    constexpr int kLeftMenuCount = 6;
    HDC dc = GetDC(g_main);
    HGDIOBJ old = SelectObject(dc, g_smallFont ? g_smallFont : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
    int x = 6;
    RECT result{};
    for (int i = 0; i < kLeftMenuCount; ++i) {
        SIZE sz{};
        GetTextExtentPoint32W(dc, labels[i], static_cast<int>(wcslen(labels[i])), &sz);
        const int w = sz.cx + 12;
        if (i == index) result = RECT{x, 2, x + w, kMenuBarHeight - 2};
        x += w + 2;
    }
    if (index == kMenuGear) {
        RECT c{};
        GetClientRect(g_main, &c);
        // Gear near right end (extensions/settings area).
        result = RECT{c.right - 28, 2, c.right - 6, kMenuBarHeight - 2};
    }
    SelectObject(dc, old);
    ReleaseDC(g_main, dc);
    return result;
}

void PaintMenuBar(HDC dc) {
    RECT c{};
    GetClientRect(g_main, &c);
    RECT bar{0, 0, c.right, kMenuBarHeight};
    HBRUSH bg = CreateSolidBrush(kThemeBg);
    FillRect(dc, &bar, bg);
    DeleteObject(bg);
    const wchar_t* labels[] = {L"File", L"Bookmarks", L"History", L"Extensions", L"Sessions", L"Tab Groups"};
    constexpr int kLeftMenuCount = 6;
    HGDIOBJ old = SelectObject(dc, g_smallFont ? g_smallFont : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(48, 52, 56));
    for (int i = 0; i < kLeftMenuCount; ++i) {
        RECT r = MenuItemRect(i);
        if (g_hoverMenu == i) FillRoundedRect(dc, r, 6, RGB(232, 235, 239), RGB(232, 235, 239));
        DrawTextW(dc, labels[i], -1, &r, DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);
    }
    SelectObject(dc, old);
    RECT gear = MenuItemRect(kMenuGear);
    if (g_hoverMenu == kMenuGear) FillRoundedRect(dc, gear, 6, RGB(232, 235, 239), RGB(232, 235, 239));
    HGDIOBJ iconOld = SelectObject(dc, g_iconFont ? g_iconFont : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
    SetTextColor(dc, RGB(58, 62, 66));
    DrawTextW(dc, L"\uE713", -1, &gear, DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);
    SelectObject(dc, iconOld);
}

int HitTestMenu(int x, int y) {
    if (y < 0 || y >= kMenuBarHeight) return kMenuNone;
    for (int i = 0; i < 6; ++i) {  // left menus
        RECT r = MenuItemRect(i);
        if (x >= r.left && x < r.right) return i;
    }
    RECT gear = MenuItemRect(kMenuGear);
    if (x >= gear.left && x < gear.right) return kMenuGear;
    return kMenuNone;
}

void PaintToolbarButton(const DRAWITEMSTRUCT& item) {
    const bool disabled = (item.itemState & ODS_DISABLED) != 0;
    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    const bool hot = g_hoverButton == item.hwndItem;
    COLORREF fill = kThemeBg;
    if (pressed) fill = RGB(216, 220, 225);
    else if (hot && !disabled) fill = RGB(232, 235, 239);
    FillRoundedRect(item.hDC, item.rcItem, 14, fill, fill);
    const wchar_t* glyph = nullptr;
    bool marked = false;
    switch (item.CtlID) {
        case kBackButton: glyph = L"\uE72B"; break;
        case kForwardButton: glyph = L"\uE72A"; break;
        case kReloadButton: glyph = L"\uE72C"; break;
        case kStarButton: {
            Tab* tab = ActiveTab();
            marked = tab && IsBookmarked(g_bookmarks, tab->url);
            glyph = marked ? L"\uE735" : L"\uE734";
            break;
        }
        case kShieldButton: {
            const std::wstring host = CurrentPageHost();
            const bool off = !g_shieldsEnabled || IsShieldsOffForHost(host);
            marked = !off;
            glyph = L"\uE72E";
            // FR-SHD2: show a slash indicator when shields are off for this site/global.
            // (slash drawn after glyph below)
            break;
        }
        case kPipButton:
            glyph = L"\uE714";  // FR-VID1: video / Picture in Picture
            break;
        case kMediaButton:
            glyph = L"\uE896";  // Download / media menu (grouped dropdown beside PiP)
            break;
        default:
            return;
    }
    COLORREF color = RGB(58, 62, 66);
    if (disabled) color = RGB(176, 180, 186);
    else if (marked && item.CtlID == kStarButton) color = RGB(238, 170, 32);
    else if (marked && item.CtlID == kShieldButton) color = RGB(46, 164, 79);
    else if (!marked && item.CtlID == kShieldButton) color = RGB(176, 80, 80);
    HGDIOBJ old = SelectObject(item.hDC, g_iconFont ? g_iconFont : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, color);
    RECT text = item.rcItem;
    DrawTextW(item.hDC, glyph, -1, &text, DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);
    if (item.CtlID == kShieldButton) {
        const std::wstring host = CurrentPageHost();
        const bool off = !g_shieldsEnabled || IsShieldsOffForHost(host);
        if (off) {
            HPEN slash = CreatePen(PS_SOLID, 2, RGB(176, 80, 80));
            HGDIOBJ op = SelectObject(item.hDC, slash);
            MoveToEx(item.hDC, item.rcItem.left + 8, item.rcItem.bottom - 8, nullptr);
            LineTo(item.hDC, item.rcItem.right - 8, item.rcItem.top + 8);
            SelectObject(item.hDC, op);
            DeleteObject(slash);
        }
    }
    SelectObject(item.hDC, old);
}

void HandleShortcut(UINT vk, bool shift);
void OpenBookmarksFolder() {
    if (!g_dataFolder.empty()) ShellExecuteW(nullptr, L"open", g_dataFolder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void DoImportBookmarks() {
    wchar_t fileName[MAX_PATH] = L"";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_main;
    ofn.lpstrFilter = L"HTML Bookmarks (*.html;*.htm)\0*.html;*.htm\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = L"Import Bookmarks";
    if (!GetOpenFileNameW(&ofn)) return;
    HANDLE file = CreateFileW(fileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    LARGE_INTEGER size{};
    std::string bytes;
    if (GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart < (1 << 20)) {
        bytes.resize(static_cast<size_t>(size.QuadPart));
        DWORD read = 0;
        ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr);
        bytes.resize(read);
    }
    CloseHandle(file);
    BookmarkTree imported = ParseNetscapeBookmarks(FromUtf8(bytes));
    for (auto& node : imported) g_bookmarks.push_back(std::move(node));
    SaveBookmarks();
    InvalidateBookmarkBar();
    MessageBoxW(g_main, L"Bookmarks imported.", L"Import Bookmarks", MB_OK | MB_ICONINFORMATION);
}

void DoExportBookmarks() {
    wchar_t fileName[MAX_PATH] = L"bookmarks.html";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_main;
    ofn.lpstrFilter = L"HTML Bookmarks (*.html)\0*.html\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = L"Export Bookmarks";
    ofn.lpstrDefExt = L"html";
    if (!GetSaveFileNameW(&ofn)) return;
    const std::string bytes = ToUtf8(SerializeNetscapeBookmarks(g_bookmarks));
    HANDLE file = CreateFileW(fileName, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr);
    CloseHandle(file);
    MessageBoxW(g_main, L"Bookmarks exported.", L"Export Bookmarks", MB_OK | MB_ICONINFORMATION);
}

// Simple input dialog (Add/Rename folder/bookmark)
HWND g_inputDlg = nullptr;
HWND g_inputEdit = nullptr;
bool g_inputOk = false;
std::wstring g_inputValue;

LRESULT CALLBACK InputDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_COMMAND:
            if (LOWORD(wp) == IDOK) {
                g_inputValue = GetText(g_inputEdit);
                g_inputOk = true;
                DestroyWindow(hwnd);
                return 0;
            }
            if (LOWORD(wp) == IDCANCEL) {
                g_inputOk = false;
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        case WM_CLOSE:
            g_inputOk = false;
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

std::wstring InputBox(const std::wstring& title, const std::wstring& prompt, const std::wstring& defaultText) {
    g_inputOk = false;
    g_inputValue = defaultText;
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = InputDlgProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"FeatherInputDlg";
        RegisterClassExW(&wc);
        registered = true;
    }
    g_inputDlg = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, L"FeatherInputDlg", title.c_str(),
                                 WS_POPUP | WS_CAPTION | WS_SYSMENU, 0, 0, 360, 130, g_main, nullptr, GetModuleHandleW(nullptr), nullptr);
    RECT pr{}, dr{};
    GetWindowRect(g_main, &pr);
    GetWindowRect(g_inputDlg, &dr);
    SetWindowPos(g_inputDlg, HWND_TOPMOST,
                 pr.left + (pr.right - pr.left - (dr.right - dr.left)) / 2,
                 pr.top + (pr.bottom - pr.top - (dr.bottom - dr.top)) / 2,
                 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
    CreateWindowExW(0, L"STATIC", prompt.c_str(), WS_CHILD | WS_VISIBLE, 12, 10, 330, 18, g_inputDlg, nullptr, GetModuleHandleW(nullptr), nullptr);
    g_inputEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", defaultText.c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 12, 32, 330, 24, g_inputDlg, nullptr, GetModuleHandleW(nullptr), nullptr);
    CreateWindowExW(0, L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 182, 66, 75, 24, g_inputDlg, reinterpret_cast<HMENU>(IDOK), GetModuleHandleW(nullptr), nullptr);
    CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 266, 66, 75, 24, g_inputDlg, reinterpret_cast<HMENU>(IDCANCEL), GetModuleHandleW(nullptr), nullptr);
    SendMessageW(g_inputEdit, EM_SETSEL, 0, -1);
    SetFocus(g_inputEdit);
    EnableWindow(g_main, FALSE);
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.hwnd == g_inputDlg || IsChild(g_inputDlg, msg.hwnd)) {
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) { g_inputValue = GetText(g_inputEdit); g_inputOk = true; break; }
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) { g_inputOk = false; break; }
            if (!IsDialogMessageW(g_inputDlg, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
            if (!IsWindow(g_inputDlg)) break;
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        if (!IsWindow(g_inputDlg)) break;
    }
    if (IsWindow(g_inputDlg)) DestroyWindow(g_inputDlg);
    EnableWindow(g_main, TRUE);
    SetForegroundWindow(g_main);
    if (g_inputOk && !g_inputValue.empty()) return g_inputValue;
    return L"";
}

void ShowFolderPopup(BookmarkNode* folder, RECT anchor, int level);

std::string HtmlEscape(const std::wstring& s) {
    std::string o = ToUtf8(s);
    std::string r;
    for (char c : o) {
        switch (c) {
            case '&': r += "&amp;"; break;
            case '<': r += "&lt;"; break;
            case '>': r += "&gt;"; break;
            case '"': r += "&quot;"; break;
            case '\'': r += "&#39;"; break;
            default: r += c; break;
        }
    }
    return r;
}

std::wstring ExtractJsonStringField(const std::string& json, const std::string& key) {
    // Minimal/heuristic JSON string field extractor (no external deps).
    std::string needle = "\"" + key + "\"";
    size_t p = json.find(needle);
    if (p == std::string::npos) return L"";
    p = json.find(':', p + needle.size());
    if (p == std::string::npos) return L"";
    p++;
    while (p < json.size() && (json[p] == ' ' || json[p] == '\t' || json[p] == '\r' || json[p] == '\n')) p++;
    if (p >= json.size() || json[p] != '"') return L"";
    p++;
    std::string out;
    while (p < json.size()) {
        char c = json[p++];
        if (c == '"') break;
        if (c == '\\' && p < json.size()) {
            char e = json[p++];
            if (e == '"' || e == '\\' || e == '/') out += e;
            else if (e == 'n') out += '\n';
            else if (e == 't') out += '\t';
            else out += e;
        } else out += c;
    }
    return FromUtf8(out);
}

void ParseExtensionManifestPages(const std::wstring& extDir, std::wstring& popup, std::wstring& options) {
    popup.clear(); options.clear();
    HANDLE file = CreateFileW((extDir + L"\\manifest.json").c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    LARGE_INTEGER size{};
    std::string bytes;
    if (GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart < (512 << 10)) {
        bytes.resize(static_cast<size_t>(size.QuadPart));
        DWORD read = 0;
        ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr);
        bytes.resize(read);
    }
    CloseHandle(file);
    if (bytes.size() >= 3 && (unsigned char)bytes[0]==0xEF && (unsigned char)bytes[1]==0xBB && (unsigned char)bytes[2]==0xBF) bytes = bytes.substr(3);
    // Try common keys. Heuristic: last/default_popup match may be inside action/browser_action; good enough for typical manifests.
    popup = ExtractJsonStringField(bytes, "default_popup");
    options = ExtractJsonStringField(bytes, "options_page");
    if (options.empty()) options = ExtractJsonStringField(bytes, "page"); // options_ui.page sometimes appears; heuristic
}

void NewTabWithHtml(const std::wstring& title, const std::wstring& html, bool activate) {  // for internal pages (history/downloads/settings)
    if (!g_env) return;
    auto tab = std::make_unique<Tab>();
    tab->id = g_nextTabId++;
    tab->url = L"about:blank";
    tab->title = title.empty() ? L"Tab" : title;
    const int id = tab->id;
    g_tabs.push_back(std::move(tab));
    if (activate) g_activeId = id;
    g_env->CreateCoreWebView2Controller(
        g_main,
        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [id, html, activate](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                Tab* created = FindTab(id);
                if (!created || FAILED(result) || !controller) {
                    if (controller) controller->Close();
                    if (created) CloseTab(id);
                    return S_OK;
                }
                created->controller = controller;
                controller->get_CoreWebView2(&created->webview);
                controller->put_IsVisible(FALSE);
                controller->put_Bounds(ContentBounds());
                SetupTab(*created);
                created->webview->NavigateToString(html.c_str());
                if (activate || id == g_activeId) ActivateTab(id);
                return S_OK;
            }).Get());
}

void OpenDownloadsTab() {  // FR-DL1: open downloads as a tab list
    std::string body = "<!doctype html><html><head><meta charset='utf-8'><title>Downloads</title>"
        "<style>body{font:14px/1.4 Segoe UI,system-ui,sans-serif;margin:20px;background:#f6f7f9;color:#222}"
        "h1{font-size:20px;margin:0 0 12px} .item{background:#fff;border:1px solid #d8dbe0;border-radius:8px;padding:10px 12px;margin:8px 0}"
        ".f{font-weight:600;word-break:break-all} .u{font-size:12px;color:#555;word-break:break-all;margin-top:4px} .s{font-size:12px;color:#666;margin-top:4px}"
        "button{margin-right:8px;margin-top:8px;padding:4px 10px;border:1px solid #c8ccd2;border-radius:6px;background:#f3f4f6;cursor:pointer}"
        ".empty{color:#666}</style></head><body><h1>Downloads</h1><p style='color:#555;font-size:12px'>Files are saved to your Downloads folder (FR-DL2). List shows recent downloads (last 50).</p>";
    if (g_downloads.empty()) {
        body += "<div class='empty'>No downloads yet.</div>";
    } else {
        for (size_t i = g_downloads.size(); i > 0; --i) {
            const size_t idx = i - 1;
            const auto& d = g_downloads[idx];
            const std::wstring file = d.file.empty() ? d.url : d.file;
            body += "<div class='item'><div class='f'>" + HtmlEscape(file) + "</div>";
            if (!d.url.empty() && d.url != file) body += "<div class='u'>" + HtmlEscape(d.url) + "</div>";
            body += "<div class='s'>Status: " + HtmlEscape(d.state) + "</div>";
            body += "<button data-a='open' data-i='" + std::to_string(idx) + "'>Open</button>";
            body += "<button data-a='show' data-i='" + std::to_string(idx) + "'>Show in folder</button></div>";
        }
    }
    body += "<script>document.addEventListener('click',e=>{const b=e.target.closest('button[data-a]');if(!b)return;"
            "try{chrome.webview.postMessage('dl'+b.getAttribute('data-a')+'|'+b.getAttribute('data-i'))}catch(_){}});</script></body></html>";
    NewTabWithHtml(L"Downloads", FromUtf8(body), true);
}

void ShowDownloadsWindow() { OpenDownloadsTab(); }  // compatibility

void OpenHistoryTab() {  // FR-HIS1: show history in a tab (not popup)
    std::string body = "<!doctype html><html><head><meta charset='utf-8'><title>History</title>"
        "<style>body{font:14px/1.4 Segoe UI,system-ui,sans-serif;margin:20px;background:#f6f7f9;color:#222}"
        "h1{font-size:20px;margin:0 0 12px} .item{background:#fff;border:1px solid #d8dbe0;border-radius:8px;padding:10px 12px;margin:8px 0}"
        ".t{font-weight:600;cursor:pointer;color:#1a5fb4;text-decoration:none;display:block} .u{font-size:12px;color:#666;word-break:break-all;margin-top:4px}"
        ".empty{color:#666}</style></head><body><h1>History</h1>";
    if (g_history.empty()) {
        body += "<div class='empty'>No history yet.</div>";
    } else {
        for (size_t i = g_history.size(); i > 0; --i) {
            const auto& h = g_history[i - 1];
            std::string u = HtmlEscape(h.url);
            std::string t = HtmlEscape(h.title.empty() ? h.url : h.title);
            body += "<div class='item'><a class='t' href='#' data-url='" + u + "'>" + t + "</a><div class='u'>" + u + "</div></div>";
            if (i + 200 < g_history.size()) break; // cap render
        }
    }
    body += "<script>document.addEventListener('click',e=>{const a=e.target.closest('a[data-url]');if(!a)return;e.preventDefault();"
            "try{chrome.webview.postMessage('nav|'+a.getAttribute('data-url'))}catch(_){location.href=a.getAttribute('data-url')}});</script></body></html>";
    NewTabWithHtml(L"History", FromUtf8(body), true);
}

void ShowHistoryWindow() { OpenHistoryTab(); }  // compatibility for existing call sites

void ClearHistoryPrompt() {  // FR-HIS2 (minimal)
    const int r = MessageBoxW(g_main, L"Clear all browsing history stored by featherWIN?", L"Clear History", MB_YESNO | MB_ICONQUESTION);
    if (r == IDYES) {
        g_history.clear();
        SaveHistory();
    }
}

std::wstring SessionsFolderPath() {
    if (g_dataFolder.empty()) return L"";
    std::wstring p = g_dataFolder + L"\\sessions";
    SHCreateDirectoryExW(nullptr, p.c_str(), nullptr);
    return p;
}

std::wstring SanitizeSessionName(std::wstring name) {
    for (auto& c : name) if (wcschr(L"<>:\"/\\|?*\t\r\n", c) || c < 32) c = L'_';
    while (!name.empty() && (name.back() == L'.' || name.back() == L' ')) name.pop_back();
    if (name.empty()) name = L"Session";
    if (name.size() > 80) name.resize(80);
    return name;
}

std::string SerializeCurrentWindowSession(int activeIndexOverride = -999) {
    std::string out;
    const int active = (activeIndexOverride == -999) ? static_cast<int>(IndexOf(g_activeId)) : activeIndexOverride;
    out += std::to_string(active);
    out += "\n";
    for (const auto& tab : g_tabs) {
        if (!tab || tab->url.empty() || tab->url == L"about:blank") continue;
        if (tab->url.rfind(L"data:", 0) == 0) continue; // skip internal/generated pages
        out += tab->pinned ? "1\t" : "0\t";
        out += std::to_string(tab->groupId);
        out += '\t';
        std::wstring url = tab->url, title = tab->title;
        for (auto& c : url) if (c == L'\t' || c == L'\r' || c == L'\n') c = L' ';
        for (auto& c : title) if (c == L'\t' || c == L'\r' || c == L'\n') c = L' ';
        out += ToUtf8(url); out += '\t'; out += ToUtf8(title); out += '\n';
    }
    return out;
}

void SaveNamedSessionDialog() { // FR-SES1 (basic)
    const std::wstring name = InputBox(L"Save Session", L"Session name (saves this window's tabs):", L"");
    if (name.empty()) return;
    const std::wstring folder = SessionsFolderPath();
    if (folder.empty()) return;
    const std::wstring safe = SanitizeSessionName(name);
    const std::wstring path = folder + L"\\" + safe + L".txt";
    const std::string out = SerializeCurrentWindowSession();
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) { MessageBoxW(g_main, L"Could not save session.", L"Sessions", MB_OK | MB_ICONWARNING); return; }
    DWORD written = 0; WriteFile(file, out.data(), static_cast<DWORD>(out.size()), &written, nullptr); CloseHandle(file);
    MessageBoxW(g_main, (L"Saved session: " + safe).c_str(), L"Sessions", MB_OK | MB_ICONINFORMATION);
}

std::vector<std::wstring> ListNamedSessions() {
    std::vector<std::wstring> names;
    const std::wstring folder = SessionsFolderPath();
    if (folder.empty()) return names;
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW((folder + L"\\*.txt").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return names;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        std::wstring n = fd.cFileName;
        if (n.size() >= 4) {
            std::wstring tail = n.substr(n.size() - 4);
            for (auto& c : tail) c = towlower(c);
            if (tail == L".txt") n.resize(n.size() - 4);
        }
        if (!n.empty()) names.push_back(n);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(names.begin(), names.end());
    return names;
}

struct SessionTab { bool pinned = false; int groupId = -1; std::wstring url; std::wstring title; int activeIndex = -1; };

std::vector<SessionTab> LoadSessionDataFromPath(const std::wstring& path) {
    std::vector<SessionTab> tabs;
    if (path.empty()) return tabs;
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return tabs;
    LARGE_INTEGER size{}; std::string bytes;
    if (GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart < (1 << 20)) {
        bytes.resize(static_cast<size_t>(size.QuadPart)); DWORD read = 0;
        ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr); bytes.resize(read);
    }
    CloseHandle(file);
    int active = -1; size_t start = 0; bool first = true;
    while (start < bytes.size()) {
        size_t end = bytes.find('\n', start); if (end == std::string::npos) end = bytes.size();
        std::string line = bytes.substr(start, end - start); if (!line.empty() && line.back() == '\r') line.pop_back(); start = end + 1;
        if (first) { first = false; active = atoi(line.c_str()); continue; }
        if (line.empty()) continue;
        size_t t1 = line.find('\t'); if (t1 == std::string::npos) continue;
        size_t t2 = line.find('\t', t1 + 1);
        size_t t3 = (t2 == std::string::npos) ? std::string::npos : line.find('\t', t2 + 1);
        SessionTab st; st.pinned = (line[0] == '1'); st.activeIndex = active;
        if (t3 != std::string::npos) {
            // New format: pinned \t groupId \t url \t title
            st.groupId = atoi(line.substr(t1 + 1, t2 - t1 - 1).c_str());
            st.url = FromUtf8(line.substr(t2 + 1, t3 - t2 - 1));
            st.title = FromUtf8(line.substr(t3 + 1));
        } else {
            // Old format: pinned \t url \t title
            st.groupId = -1;
            st.url = FromUtf8(t2 == std::string::npos ? line.substr(t1 + 1) : line.substr(t1 + 1, t2 - t1 - 1));
            st.title = FromUtf8(t2 == std::string::npos ? std::string() : line.substr(t2 + 1));
        }
        if (!st.url.empty()) tabs.push_back(std::move(st));
    }
    return tabs;
}

void RestoreNamedSessionByName(const std::wstring& name, bool replaceWindow) {
    // FR-SES1: restore into frontmost window (this window). "New window" not supported (single-window app).
    const std::wstring folder = SessionsFolderPath();
    if (folder.empty()) return;
    const std::wstring path = folder + L"\\" + name + L".txt";
    auto tabs = LoadSessionDataFromPath(path);
    if (tabs.empty()) { MessageBoxW(g_main, L"Could not load that session (empty or missing).", L"Sessions", MB_OK | MB_ICONWARNING); return; }
    std::vector<int> oldIds;
    if (replaceWindow) {
        oldIds.reserve(g_tabs.size());
        for (const auto& t : g_tabs) if (t) oldIds.push_back(t->id);
    }
    std::vector<int> newIds;
    newIds.reserve(tabs.size());
    for (const auto& st : tabs) {
        NewTab(st.url, false, st.pinned, st.title);
        if (!g_tabs.empty()) { g_tabs.back()->groupId = st.groupId; newIds.push_back(g_tabs.back()->id); }
    }
    if (replaceWindow) {
        for (int id : oldIds) {
            if (std::find(newIds.begin(), newIds.end(), id) != newIds.end()) continue;
            if (IndexOf(id) < 0) continue;
            if (g_tabs.size() <= 1) break; // CloseTab destroys window when empty
            CloseTab(id);
        }
    }
    if (!g_tabs.empty()) {
        // Activate a restored tab (prefer first non-pinned restored, else last)
        int activateId = g_tabs.back()->id;
        for (size_t i = 0; i < g_tabs.size(); ++i) {
            if (std::find(newIds.begin(), newIds.end(), g_tabs[i]->id) != newIds.end() && !g_tabs[i]->pinned) { activateId = g_tabs[i]->id; break; }
        }
        if (std::find(newIds.begin(), newIds.end(), activateId) == newIds.end() && !newIds.empty()) activateId = newIds.front();
        if (IndexOf(activateId) >= 0) ActivateTab(activateId);
    }
    SaveSession(); SaveTabGroups(); InvalidateStrip(); LayoutChrome(); InvalidateBookmarkBar();
}

void RenameNamedSessionByName(const std::wstring& name) { // FR-SES1
    const std::wstring folder = SessionsFolderPath();
    if (folder.empty()) return;
    const std::wstring newName = InputBox(L"Rename Session", L"New session name:", name);
    if (newName.empty() || newName == name) return;
    const std::wstring safe = SanitizeSessionName(newName);
    const std::wstring src = folder + L"\\" + name + L".txt";
    const std::wstring dst = folder + L"\\" + safe + L".txt";
    if (lstrcmpiW(src.c_str(), dst.c_str()) != 0 && GetFileAttributesW(dst.c_str()) != INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(g_main, L"A session with that name already exists.", L"Sessions", MB_OK | MB_ICONWARNING);
        return;
    }
    if (!MoveFileW(src.c_str(), dst.c_str())) {
        MessageBoxW(g_main, L"Could not rename session.", L"Sessions", MB_OK | MB_ICONWARNING);
    }
}

// ---- Tab Groups (FR-GRP1-4, minimal) ----
TabGroup* FindTabGroup(int id) {
    for (auto& g : g_tabGroups) if (g.id == id) return &g;
    return nullptr;
}

COLORREF TabGroupPalette(size_t i) {
    static const COLORREF c[] = { RGB(0,120,212), RGB(16,124,16), RGB(209,52,56), RGB(136,23,152), RGB(202,80,16), RGB(0,130,114), RGB(118,118,118) };
    return c[i % (sizeof(c)/sizeof(c[0]))];
}

std::string JsonEscapeUtf8(const std::wstring& s) {
    std::string in = ToUtf8(s), out;
    for (unsigned char c : in) {
        if (c == '\\') out += "\\\\";
        else if (c == '"') out += "\\\"";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if (c < 0x20) { char b[8]; sprintf_s(b, "\\u%04x", c); out += b; }
        else out += static_cast<char>(c);
    }
    return out;
}

void SaveTabGroups() { // FR-GRP1
    if (g_tabGroupsPath.empty()) return;
    std::string j = "{\n  \"nextGroupId\": " + std::to_string(g_nextGroupId) + ",\n  \"groups\": [\n";
    for (size_t i = 0; i < g_tabGroups.size(); ++i) {
        const auto& g = g_tabGroups[i];
        j += "    {\"id\":" + std::to_string(g.id) + ",\"name\":\"" + JsonEscapeUtf8(g.name) +
             "\",\"color\":" + std::to_string(static_cast<unsigned>(g.color & 0xFFFFFF)) +
             ",\"collapsed\":" + (g.collapsed ? "true" : "false") + ",\"tabs\":[";
        bool first = true;
        for (const auto& t : g_tabs) {
            if (t && t->groupId == g.id) { if (!first) j += ","; j += std::to_string(t->id); first = false; }
        }
        j += "]}"; if (i + 1 < g_tabGroups.size()) j += ","; j += "\n";
    }
    j += "  ]\n}\n";
    HANDLE f = CreateFileW(g_tabGroupsPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return;
    DWORD w=0; WriteFile(f, j.data(), static_cast<DWORD>(j.size()), &w, nullptr); CloseHandle(f);
}

static int JsonFindInt(const std::string& s, const std::string& key, int def=0) {
    size_t p = s.find("\""+key+"\""); if (p==std::string::npos) return def;
    p = s.find(':', p); if (p==std::string::npos) return def; return atoi(s.c_str()+p+1);
}
static bool JsonFindBool(const std::string& s, const std::string& key, bool def=false) {
    size_t p = s.find("\""+key+"\""); if (p==std::string::npos) return def;
    p = s.find(':', p); if (p==std::string::npos) return def; while (++p<s.size() && (s[p]==' '||s[p]=='\t')){}; 
    return p<s.size() && s.compare(p,4,"true")==0 ? true : (p<s.size() && s.compare(p,5,"false")==0 ? false : def);
}
static std::wstring JsonFindStrW(const std::string& s, const std::string& key) { return ExtractJsonStringField(s, key); }

void LoadTabGroups() { // FR-GRP1 (best-effort simple JSON)
    g_tabGroups.clear();
    if (g_tabGroupsPath.empty()) return;
    HANDLE f = CreateFileW(g_tabGroupsPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return;
    LARGE_INTEGER sz{}; std::string b; if (GetFileSizeEx(f,&sz) && sz.QuadPart>0 && sz.QuadPart<(1<<20)) { b.resize((size_t)sz.QuadPart); DWORD r=0; ReadFile(f,b.data(),(DWORD)b.size(),&r,nullptr); b.resize(r);} CloseHandle(f);
    g_nextGroupId = std::max(1, JsonFindInt(b, "nextGroupId", 1));
    // Parse group objects naively (sufficient for our own saved format)
    size_t p = 0;
    while ((p = b.find("{\"id\":", p)) != std::string::npos) {
        size_t e = b.find("}", p); if (e==std::string::npos) break;
        std::string obj = b.substr(p, e-p+1);
        TabGroup g; g.id = JsonFindInt(obj, "id", 0); g.name = JsonFindStrW(obj, "name"); g.color = static_cast<COLORREF>(JsonFindInt(obj, "color", RGB(0,120,212))); g.collapsed = JsonFindBool(obj, "collapsed", false);
        if (g.id > 0) { if (g.name.empty()) g.name = L"Group"; g_tabGroups.push_back(std::move(g)); g_nextGroupId = std::max(g_nextGroupId, g_tabGroups.back().id + 1); }
        p = e+1;
    }
    // Tab membership from saved JSON is best restored via session/persistence; keep runtime membership intact if already set.
}

void CreateTabGroupFromTabs(const std::wstring& name, const std::vector<int>& tabIds) { // FR-GRP1
    if (tabIds.empty()) return;
    TabGroup g; g.id = g_nextGroupId++; g.name = name.empty() ? L"Group" : name; g.color = TabGroupPalette(g_tabGroups.size()); g.collapsed = false;
    const int gid = g.id;
    g_tabGroups.push_back(std::move(g));
    for (int id : tabIds) { Tab* t = FindTab(id); if (t && !t->pinned) t->groupId = gid; } // FR-GRP4: groups follow tabs; pinned not grouped in this minimal impl
    SaveTabGroups(); InvalidateStrip(); InvalidateBookmarkBar();
}

void RemoveTabFromGroup(int tabId) {
    Tab* t = FindTab(tabId); if (!t) return; t->groupId = -1; SaveTabGroups(); InvalidateStrip();
}

void DeleteTabGroup(int groupId, bool closeTabs) { // FR-GRP2 (partial)
    for (auto& t : g_tabs) if (t && t->groupId == groupId) { if (!closeTabs) t->groupId = -1; }
    if (closeTabs) {
        std::vector<int> ids; for (auto& t : g_tabs) if (t && t->groupId == groupId) ids.push_back(t->id);
        for (int id : ids) if (g_tabs.size() > 1 && IndexOf(id) >= 0) CloseTab(id);
        for (auto& t : g_tabs) if (t) if (t->groupId == groupId) t->groupId = -1;
    }
    g_tabGroups.erase(std::remove_if(g_tabGroups.begin(), g_tabGroups.end(), [groupId](const TabGroup& g){return g.id==groupId;}), g_tabGroups.end());
    SaveTabGroups(); InvalidateStrip(); InvalidateBookmarkBar();
}

void UngroupTabGroup(int groupId) { DeleteTabGroup(groupId, false); }

void DeleteNamedSessionByName(const std::wstring& name) {
    const std::wstring folder = SessionsFolderPath();
    if (folder.empty()) return;
    const std::wstring path = folder + L"\\" + name + L".txt";
    // Never hard-delete user files (AGENTS.md): send to Recycle Bin.
    std::wstring from = path; from.push_back(L'\0');
    SHFILEOPSTRUCTW op{}; op.wFunc = FO_DELETE; op.pFrom = from.c_str(); op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT;
    SHFileOperationW(&op);
}

void SaveSession() {  // session restore (like Feather) + pinned tabs
    if (g_sessionPath.empty()) return;
    std::string out = SerializeCurrentWindowSession();
    HANDLE file = CreateFileW(g_sessionPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(file, out.data(), static_cast<DWORD>(out.size()), &written, nullptr);
    CloseHandle(file);
}

std::vector<SessionTab> LoadSessionData() {
    return LoadSessionDataFromPath(g_sessionPath);
}

void TogglePinActiveTab() {
    Tab* t = ActiveTab();
    if (!t) return;
    t->pinned = !t->pinned;
    SortTabsPinnedLeft();
    SaveSession();
    InvalidateStrip();
}

void OpenSettingsTab() {  // FR-SET1 (minimal)
    std::string body = "<!doctype html><html><head><meta charset='utf-8'><title>Settings</title>"
        "<style>body{font:14px/1.4 Segoe UI,system-ui,sans-serif;margin:20px;background:#f6f7f9;color:#222;max-width:720px}"
        "h1{font-size:20px;margin:0 0 12px} .card{background:#fff;border:1px solid #d8dbe0;border-radius:8px;padding:12px;margin:10px 0}"
        "code{background:#eef1f4;padding:1px 4px;border-radius:4px} li{margin:6px 0}</style></head><body><h1>Settings</h1>"
        "<div class='card'><b>featherWIN</b> (WebView2). Privacy-first: no telemetry, no accounts, no background processes. Settings are minimal in this build.</div>"
        "<div class='card'><b>Available now</b><ul>"
        "<li>Shields: toolbar shield icon / menu (global, per-site, YouTube exempt). Exceptions file: <code>%APPDATA%\\featherWIN\\shields-exceptions.txt</code></li>"
        "<li>Extensions: unpacked folders in <code>%APPDATA%\\featherWIN\\extensions\\</code> (each with <code>manifest.json</code>), loaded on restart.</li>"
        "<li>Bookmarks: <code>%APPDATA%\\featherWIN\\bookmarks.txt</code> (Netscape import/export in Bookmarks menu).</li>"
        "<li>History/Downloads: local; session (tabs/pinned) saved on close and restored on start.</li>"
        "</ul></div>"
        "<div class='card'><b>Note</b>: WebView2 runtime telemetry/update behavior should be verified against privacy goals before release (C-PRIV).</div>"
        "</body></html>";
    NewTabWithHtml(L"Settings", FromUtf8(body), true);
}

void ShowMenu(int which) {
    CloseAllFolderPopups();
    HMENU menu = CreatePopupMenu();
    if (which == kMenuFile) {
        AppendMenuW(menu, MF_STRING, 1, L"New Tab\tCtrl+T");
        AppendMenuW(menu, MF_STRING, 2, L"Close Tab\tCtrl+W");
        AppendMenuW(menu, MF_STRING | (g_closedTabs.empty() ? MF_GRAYED : MF_ENABLED), 6, L"Reopen Closed Tab\tCtrl+Shift+T");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, 4, L"Picture in Picture");
        AppendMenuW(menu, MF_STRING, 5, L"Open Extensions Folder...");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, 3, L"Exit");
    } else if (which == kMenuBookmarks) {
        Tab* tab = ActiveTab();
        const bool bookmarked = tab && IsBookmarked(g_bookmarks, tab->url);
        AppendMenuW(menu, MF_STRING, 10, bookmarked ? L"Remove Bookmark\tCtrl+D" : L"Bookmark This Page\tCtrl+D");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, 12, L"Add New Folder");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, 14, L"Import Bookmarks...");
        AppendMenuW(menu, MF_STRING, 15, L"Export Bookmarks...");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, 13, L"Open Bookmarks Folder");
    } else if (which == kMenuHistory) {
        Tab* at = ActiveTab();
        const bool pinned = at && at->pinned;
        AppendMenuW(menu, MF_STRING, 30, L"Downloads\tCtrl+J");
        AppendMenuW(menu, MF_STRING, 31, L"Show All History\tCtrl+H");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING | (at ? MF_ENABLED : MF_GRAYED), 33, pinned ? L"Unpin This Tab" : L"Pin This Tab");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, 32, L"Clear History...");
    } else if (which == kMenuExtensions) {
        AppendMenuW(menu, MF_STRING, 28, L"Add Unpacked Extension...");
        AppendMenuW(menu, MF_STRING, 29, L"Install Extension from ZIP/CRX...");
        AppendMenuW(menu, MF_STRING, 25, L"Open Extensions Folder...");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        if (g_extensions.empty()) {
            AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, L"No extensions loaded yet");
        } else {
            AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, L"Loaded extensions (open popup/options):");
            for (size_t i = 0; i < g_extensions.size() && i < 30; ++i) {
                const auto& ex = g_extensions[i];
                HMENU sub = CreatePopupMenu();
                AppendMenuW(sub, MF_STRING, 5000 + static_cast<UINT>(i) * 10 + 1, L"Open Popup");
                AppendMenuW(sub, MF_STRING, 5000 + static_cast<UINT>(i) * 10 + 2, L"Open Options");
                AppendMenuW(sub, MF_STRING, 5000 + static_cast<UINT>(i) * 10 + 3, L"Open Extension Page");
                AppendMenuW(menu, MF_POPUP, (UINT_PTR)sub, ex.name.c_str());
            }
        }
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, L"Firefox extensions are not supported (WebView2 is Chromium). Chrome Web Store install not supported.");
    } else if (which == kMenuSessions) { // FR-SES1
        AppendMenuW(menu, MF_STRING, 40, L"Save Window as Session...");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        const auto sessions = ListNamedSessions();
        HMENU restoreSub = CreatePopupMenu();
        HMENU replaceSub = CreatePopupMenu();
        HMENU renameSub = CreatePopupMenu();
        HMENU deleteSub = CreatePopupMenu();
        if (sessions.empty()) {
            AppendMenuW(restoreSub, MF_STRING | MF_GRAYED, 0, L"(no saved sessions)");
            AppendMenuW(replaceSub, MF_STRING | MF_GRAYED, 0, L"(no saved sessions)");
            AppendMenuW(renameSub, MF_STRING | MF_GRAYED, 0, L"(no saved sessions)");
            AppendMenuW(deleteSub, MF_STRING | MF_GRAYED, 0, L"(no saved sessions)");
        } else {
            for (size_t i = 0; i < sessions.size() && i < 50; ++i) {
                AppendMenuW(restoreSub, MF_STRING, 6000 + static_cast<UINT>(i), sessions[i].c_str());
                AppendMenuW(replaceSub, MF_STRING, 6200 + static_cast<UINT>(i), sessions[i].c_str());
                AppendMenuW(renameSub, MF_STRING, 6300 + static_cast<UINT>(i), sessions[i].c_str());
                AppendMenuW(deleteSub, MF_STRING, 6100 + static_cast<UINT>(i), sessions[i].c_str());
            }
        }
        AppendMenuW(menu, MF_POPUP, (UINT_PTR)restoreSub, L"Restore Session (Add to This Window)");
        AppendMenuW(menu, MF_POPUP, (UINT_PTR)replaceSub, L"Replace This Window with Session");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_POPUP, (UINT_PTR)renameSub, L"Rename Session");
        AppendMenuW(menu, MF_POPUP, (UINT_PTR)deleteSub, L"Delete Session");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, 41, L"Open Sessions Folder...");
        AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, L"New window restore not supported yet (single-window app).");
    } else if (which == kMenuTabGroups) { // FR-GRP1-4 (minimal)
        Tab* active = ActiveTab();
        const bool canGroupActive = active && !active->pinned;
        AppendMenuW(menu, MF_STRING, 50, L"New Group from Current Tab...");
        AppendMenuW(menu, MF_STRING, 51, L"New Group from Window's Tabs...");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        UINT addFlags = MF_POPUP;
        if (!canGroupActive || g_tabGroups.empty()) addFlags |= MF_GRAYED;
        HMENU addSub = CreatePopupMenu();
        if (g_tabGroups.empty()) AppendMenuW(addSub, MF_STRING | MF_GRAYED, 0, L"(no groups)");
        else for (size_t i = 0; i < g_tabGroups.size() && i < 30; ++i) AppendMenuW(addSub, MF_STRING, 7400 + static_cast<UINT>(i), g_tabGroups[i].name.empty() ? L"Group" : g_tabGroups[i].name.c_str());
        AppendMenuW(menu, addFlags, (UINT_PTR)addSub, L"Add Current Tab to Group");
        AppendMenuW(menu, MF_STRING | ((canGroupActive && active->groupId > 0) ? MF_ENABLED : MF_GRAYED), 52, L"Remove Current Tab from Group");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        if (g_tabGroups.empty()) {
            AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, L"No tab groups");
        } else {
            for (size_t i = 0; i < g_tabGroups.size() && i < 30; ++i) {
                const auto& g = g_tabGroups[i];
                HMENU sub = CreatePopupMenu();
                AppendMenuW(sub, MF_STRING, 7000 + static_cast<UINT>(i) * 10 + 1, L"Rename Group...");
                AppendMenuW(sub, MF_STRING, 7000 + static_cast<UINT>(i) * 10 + 2, L"Change Colour");
                AppendMenuW(sub, MF_STRING, 7000 + static_cast<UINT>(i) * 10 + 3, g.collapsed ? L"Expand Group" : L"Collapse Group");
                AppendMenuW(sub, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(sub, MF_STRING, 7000 + static_cast<UINT>(i) * 10 + 4, L"Ungroup Tabs");
                AppendMenuW(sub, MF_STRING, 7000 + static_cast<UINT>(i) * 10 + 5, L"Close Group Tabs");
                std::wstring label = g.name;
                if (label.empty()) label = L"Group";
                AppendMenuW(menu, MF_POPUP, (UINT_PTR)sub, label.c_str());
            }
        }
    } else {  // gear/settings
        const std::wstring host = CurrentPageHost();
        const bool siteOff = IsShieldsOffForHost(host);
        AppendMenuW(menu, MF_STRING, 26, L"Settings...");
        AppendMenuW(menu, MF_STRING, 27, L"Set Media Menu Extension (slot 3)...");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING | (g_shieldsEnabled ? MF_CHECKED : MF_UNCHECKED), 22, L"Shields Enabled (Global)");
        UINT siteFlags = MF_STRING;
        if (host.empty() || IsYouTubeHost(host)) siteFlags |= MF_GRAYED;
        AppendMenuW(menu, siteFlags, 23, siteOff ? L"Enable Shields for This Site" : L"Disable Shields for This Site");
        AppendMenuW(menu, MF_STRING, 24, L"Manage Shield Exceptions...");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, 25, L"Open Extensions Folder...");
        AppendMenuW(menu, MF_STRING, 20, L"Open Bookmarks Folder");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, 21, L"About featherWIN");
    }
    RECT item = MenuItemRect(which);
    POINT pt{item.left, item.bottom};
    ClientToScreen(g_main, &pt);
    g_menuTracking = true;
    const int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, g_main, nullptr);
    g_menuTracking = false;
    DestroyMenu(menu);
    if (cmd >= 7400 && cmd < 7500) { // FR-GRP: add current tab to existing group
        const int gi = cmd - 7400;
        Tab* t = ActiveTab();
        if (t && !t->pinned && gi >= 0 && gi < static_cast<int>(g_tabGroups.size())) {
            t->groupId = g_tabGroups[static_cast<size_t>(gi)].id;
            SaveTabGroups(); SaveSession(); InvalidateStrip();
        }
        return;
    }
    if (cmd >= 7000 && cmd < 7400) { // FR-GRP2/3 manage tab groups
        const int gi = (cmd - 7000) / 10;
        const int action = (cmd - 7000) % 10;
        if (gi >= 0 && gi < static_cast<int>(g_tabGroups.size())) {
            TabGroup& g = g_tabGroups[static_cast<size_t>(gi)];
            const int gid = g.id;
            if (action == 1) {
                std::wstring n = InputBox(L"Rename Tab Group", L"Group name:", g.name);
                if (!n.empty()) { g.name = n; SaveTabGroups(); InvalidateStrip(); InvalidateBookmarkBar(); }
            } else if (action == 2) {
                // cycle color
                size_t base = 0; for (size_t i=0;i<7;i++) if (TabGroupPalette(i)==g.color) { base=i; break; }
                g.color = TabGroupPalette(base + 1); SaveTabGroups(); InvalidateStrip(); InvalidateBookmarkBar();
            } else if (action == 3) {
                g.collapsed = !g.collapsed; SaveTabGroups(); InvalidateStrip();
            } else if (action == 4) {
                UngroupTabGroup(gid);
            } else if (action == 5) {
                DeleteTabGroup(gid, true);
            }
        }
        return;
    }
    if (cmd >= 6000 && cmd < 6400) { // FR-SES1 named sessions
        const auto sessions = ListNamedSessions();
        int idx = -1; bool replace = false; bool doDelete = false; bool doRename = false;
        if (cmd >= 6300) { idx = cmd - 6300; doRename = true; }
        else if (cmd >= 6200) { idx = cmd - 6200; replace = true; }
        else if (cmd >= 6100) { idx = cmd - 6100; doDelete = true; }
        else { idx = cmd - 6000; }
        if (idx >= 0 && idx < static_cast<int>(sessions.size())) {
            const auto& s = sessions[static_cast<size_t>(idx)];
            if (doRename) RenameNamedSessionByName(s);
            else if (doDelete) DeleteNamedSessionByName(s);
            else RestoreNamedSessionByName(s, replace);
        }
        return;
    }
    if (cmd >= 5000) {
        const int extIndex = (cmd - 5000) / 10;
        const int action = (cmd - 5000) % 10;
        if (extIndex >= 0 && extIndex < static_cast<int>(g_extensions.size())) {
            const auto& ex = g_extensions[static_cast<size_t>(extIndex)];
            std::wstring page;
            if (action == 1) page = ex.popup.empty() ? L"popup.html" : ex.popup;
            else if (action == 2) page = ex.options.empty() ? L"options.html" : ex.options;
            else page = ex.popup.empty() ? (ex.options.empty() ? L"manifest.json" : ex.options) : ex.popup;
            if (!ex.id.empty() && !page.empty()) {
                while (!page.empty() && (page[0] == L'/' || page[0] == L'\\')) page.erase(page.begin());
                NewTab(L"chrome-extension://" + ex.id + L"/" + page, true);  // popup spike pattern
            }
        }
        return;
    }
    switch (cmd) {
        case 1: NewTab(kHomePage, true); break;
        case 2: CloseTab(g_activeId); break;
        case 3: DestroyWindow(g_main); break;
        case 4: RequestSelfPiP(); break;
        case 6: ReopenClosedTab(); break;
        case 5:
            if (!g_extensionsFolder.empty()) {
                SHCreateDirectoryExW(nullptr, g_extensionsFolder.c_str(), nullptr);
                ShellExecuteW(nullptr, L"open", g_extensionsFolder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                MessageBoxW(g_main, L"Place unpacked extension folders (each with manifest.json) here.\nDefault targets: Bitwarden, a Picture-in-Picture extension.\nExtensions load on next app start.", L"Extensions", MB_OK | MB_ICONINFORMATION);
            }
            break;
        case 10: ToggleBookmark(); break;
        case 12: {
            std::wstring name = InputBox(L"New Folder", L"Folder name:", L"New Folder");
            if (!name.empty()) { AddFolder(g_bookmarks, name); SaveBookmarks(); InvalidateBookmarkBar(); }
            break;
        }
        case 13: case 20: OpenBookmarksFolder(); break;
        case 14: DoImportBookmarks(); break;
        case 15: DoExportBookmarks(); break;
        case 21: MessageBoxW(g_main, L"featherWIN\nA tiny Windows browser (WebView2).", L"About featherWIN", MB_OK | MB_ICONINFORMATION); break;
        case 22:
            g_shieldsEnabled = !g_shieldsEnabled;
            if (g_shield) InvalidateRect(g_shield, nullptr, FALSE);
            { Tab* t = ActiveTab(); if (t && t->webview) t->webview->Reload(); }
            break;
        case 23:
            ToggleShieldsForCurrentSite();
            break;
        case 24:
            if (!g_shieldExceptionsPath.empty()) {
                // Open for manual management (lightweight; FR-SHD2 "Manage site exceptions").
                ShellExecuteW(nullptr, L"open", g_shieldExceptionsPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            } else if (!g_dataFolder.empty()) {
                OpenBookmarksFolder();
            }
            break;
        case 25:
            if (!g_extensionsFolder.empty()) {
                SHCreateDirectoryExW(nullptr, g_extensionsFolder.c_str(), nullptr);
                ShellExecuteW(nullptr, L"open", g_extensionsFolder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                MessageBoxW(g_main, L"Place unpacked extension folders (each with manifest.json) here, e.g. Bitwarden or a PiP extension. Restart featherWIN to load them.", L"Extensions", MB_OK | MB_ICONINFORMATION);
            }
            break;
        case 30: ShowDownloadsWindow(); break;
        case 31: ShowHistoryWindow(); break;
        case 26: OpenSettingsTab(); break;
        case 27: {
            std::wstring name = InputBox(L"Media Menu Slot 3", L"Extension name/folder for slot 3 (shown beside PiP > dropdown):", g_mediaExtSlot3);
            if (!name.empty() || !g_mediaExtSlot3.empty()) { g_mediaExtSlot3 = name; SaveSettings(); }
            break;
        }
        case 28: AddUnpackedExtensionDialog(); break;
        case 29: InstallExtensionFromArchiveDialog(); break;
        case 40: SaveNamedSessionDialog(); break;
        case 41: {
            const std::wstring folder = SessionsFolderPath();
            if (!folder.empty()) ShellExecuteW(nullptr, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            break;
        }
        case 50: { // FR-GRP1: new group from current tab
            Tab* t = ActiveTab();
            if (!t || t->pinned) { MessageBoxW(g_main, L"Select a normal (non-pinned) tab first.", L"Tab Groups", MB_OK | MB_ICONINFORMATION); break; }
            std::wstring name = InputBox(L"New Tab Group", L"Group name:", HostOf(t->url).empty() ? L"Group" : HostOf(t->url).substr(0, 20));
            if (name.empty()) break;
            CreateTabGroupFromTabs(name, {t->id});
            break;
        }
        case 51: { // FR-GRP1: new group from window's tabs
            std::wstring name = InputBox(L"New Tab Group", L"Group name (all non-pinned tabs):", L"Window Tabs");
            if (name.empty()) break;
            std::vector<int> ids; for (auto& tab : g_tabs) if (tab && !tab->pinned) ids.push_back(tab->id);
            if (ids.empty()) { MessageBoxW(g_main, L"No tabs to group.", L"Tab Groups", MB_OK | MB_ICONINFORMATION); break; }
            CreateTabGroupFromTabs(name, ids);
            break;
        }
        case 52: { // FR-GRP: remove current tab from group
            Tab* t = ActiveTab();
            if (t && t->groupId > 0) { t->groupId = -1; SaveTabGroups(); SaveSession(); InvalidateStrip(); }
            break;
        }
        case 32: ClearHistoryPrompt(); break;
        case 33: TogglePinActiveTab(); break;
    }
}

// Folder popup window (up to 5 levels) — supports hover submenus and drag/drop.
LRESULT CALLBACK FolderPopupWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    BookmarkNode* node = GetPopupNode(hwnd);
    std::vector<BookmarkNode*>* items = GetPopupItems(hwnd);
    BookmarkTree* container = GetPopupContainer(hwnd);
    const int level = GetPopupLevel(hwnd);
    auto itemRect = [](int i) -> RECT { return RECT{2, 4 + i * 24, 218, 4 + (i + 1) * 24}; };
    auto countItems = [&]() -> int { return items ? static_cast<int>(items->size()) : 0; };
    auto hitItem = [&](int y) -> int {
        const int row = (y - 4) / 24;
        if (row >= 0 && row < countItems()) return row;
        return -1;
    };
    auto getItem = [&](int i) -> BookmarkNode* {
        if (!items || i < 0 || i >= static_cast<int>(items->size())) return nullptr;
        return (*items)[static_cast<size_t>(i)];
    };
    auto refreshItems = [&]() {
        if (!items) return;
        items->clear();
        if (g_folderPopupOverflow[level]) {
            BookmarkLayout layout = GetBookmarkLayout();
            *items = layout.overflow;
        } else if (node && node->type == BookmarkNode::Folder) {
            for (auto& c : node->children) items->push_back(c.get());
        }
    };
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(hwnd, &ps);
            if (items) {
                HGDIOBJ old = SelectObject(dc, g_uiFont ? g_uiFont : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
                SetBkMode(dc, TRANSPARENT);
                for (size_t i = 0; i < items->size(); ++i) {
                    BookmarkNode* child = (*items)[i];
                    RECT r = itemRect(static_cast<int>(i));
                    COLORREF bg = RGB(255, 255, 255);
                    if (child && child->type == BookmarkNode::Folder) {
                        bg = (level == 0) ? RGB(214, 221, 234) : RGB(198, 208, 226);
                    }
                    if (static_cast<int>(i) == g_hoverPopupItem) bg = RGB(232, 235, 239);
                    HBRUSH br = CreateSolidBrush(bg);
                    FillRect(dc, &r, br);
                    DeleteObject(br);
                    SetTextColor(dc, RGB(32, 33, 36));
                    RECT label = r;
                    label.left += 8;
                    label.right -= 22;
                    DrawTextW(dc, child ? child->title.c_str() : L"", -1, &label, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
                    if (child && child->type == BookmarkNode::Folder) {
                        RECT ar = r;
                        ar.left = ar.right - 18;
                        DrawTextW(dc, L"\u25B6", -1, &ar, DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);
                    }
                }
                SelectObject(dc, old);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_MOUSEMOVE: {
            const int x = GET_X_LPARAM(lp);
            const int y = GET_Y_LPARAM(lp);
            const int item = hitItem(y);
            if (item != g_hoverPopupItem) {
                g_hoverPopupItem = item;
                InvalidateRect(hwnd, nullptr, FALSE);
                BookmarkNode* child = getItem(item);
                if (item >= 0 && child && child->type == BookmarkNode::Folder && level + 1 < kMaxFolderLevels) {
                    RECT wr{};
                    GetWindowRect(hwnd, &wr);
                    RECT anchor{wr.right - 2, wr.top + 4 + item * 24, wr.right + 220, wr.top + 4 + (item + 1) * 24};
                    ShowFolderPopup(child, anchor, level + 1);
                } else if (item >= 0) {
                    CloseFolderPopupsFrom(level + 1);
                } else {
                    CloseFolderPopupsFrom(level + 1);
                }
            }
            if (g_dragPopup == hwnd && g_dragPopupIndex >= 0 && (GetKeyState(VK_LBUTTON) & 0x8000)) {
                if (!g_dragPopupActive && (abs(x - g_dragPopupStart.x) > 4 || abs(y - g_dragPopupStart.y) > 4)) {
                    g_dragPopupActive = true;
                }
            }
            TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, hwnd, 0};
            TrackMouseEvent(&tme);
            ArmFolderCloseTimer();
            return 0;
        }
        case WM_MOUSELEAVE:
            ArmFolderCloseTimer();
            return 0;
        case WM_LBUTTONDOWN: {
            SetForegroundWindow(g_main);
            const int item = hitItem(GET_Y_LPARAM(lp));
            if (item >= 0) {
                g_dragPopup = hwnd;
                g_dragPopupIndex = item;
                g_dragPopupActive = false;
                g_dragPopupStart = POINT{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
                SetCapture(hwnd);
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            const int x = GET_X_LPARAM(lp);
            const int y = GET_Y_LPARAM(lp);
            if (g_dragPopup == hwnd && g_dragPopupIndex >= 0 && items && container) {
                ReleaseCapture();
                POINT sp{x, y};
                ClientToScreen(hwnd, &sp);
                BookmarkNode* dragged = getItem(g_dragPopupIndex);
                if (g_dragPopupActive && dragged) {
                    POINT mc = sp;
                    ScreenToClient(g_main, &mc);
                    bool moved = false;
                    // Drop onto bookmark bar / folder chip
                    if (mc.y >= BookmarkBarTop() && mc.y < BookmarkBarTop() + kBookmarkBarHeight) {
                        BookmarkNode* target = BookmarkAt(mc.x, mc.y);
                        if (target && target->type == BookmarkNode::Folder && target != dragged) {
                            MoveNode(g_bookmarks, dragged, target, nullptr);
                        } else {
                            MoveNode(g_bookmarks, dragged, nullptr, target && target != dragged ? target : nullptr);
                        }
                        moved = true;
                    } else {
                        // Drop onto a folder popup item (move into subfolder) or reorder in same popup
                        HWND over = WindowFromPoint(sp);
                        int overLevel = GetPopupLevel(over);
                        if (overLevel >= 0) {
                            std::vector<BookmarkNode*>* overItems = GetPopupItems(over);
                            BookmarkTree* overContainer = GetPopupContainer(over);
                            POINT oc = sp;
                            ScreenToClient(over, &oc);
                            const int overItem = (oc.y - 4) / 24;
                            if (over == hwnd && overContainer == container && overItem >= 0 && overItem < static_cast<int>(container->size()) && overItem != g_dragPopupIndex) {
                                auto movedNode = std::move((*container)[static_cast<size_t>(g_dragPopupIndex)]);
                                container->erase(container->begin() + g_dragPopupIndex);
                                int ins = overItem;
                                if (ins > g_dragPopupIndex) --ins;
                                container->insert(container->begin() + ins, std::move(movedNode));
                                refreshItems();
                                moved = true;
                            } else if (overItems && overItem >= 0 && overItem < static_cast<int>(overItems->size())) {
                                BookmarkNode* target = (*overItems)[static_cast<size_t>(overItem)];
                                if (target && target->type == BookmarkNode::Folder && target != dragged) {
                                    MoveNode(g_bookmarks, dragged, target, nullptr);
                                    moved = true;
                                }
                            }
                        }
                    }
                    if (moved) { SaveBookmarks(); InvalidateBookmarkBar(); refreshItems(); InvalidateRect(hwnd, nullptr, FALSE); }
                    CloseAllFolderPopups();
                } else {
                    // Click
                    if (dragged && dragged->type == BookmarkNode::Bookmark) {
                        Tab* tab = ActiveTab();
                        if (tab && tab->webview) tab->webview->Navigate(dragged->url.c_str());
                        CloseAllFolderPopups();
                    }
                }
                g_dragPopup = nullptr;
                g_dragPopupIndex = -1;
                g_dragPopupActive = false;
                return 0;
            }
            return 0;
        }
        case WM_RBUTTONDOWN: {
            KillTimer(g_main, kTimerFolderClose);
            const int item = hitItem(GET_Y_LPARAM(lp));
            BookmarkNode* n = getItem(item);
            if (item >= 0 && n) {
                HMENU menu = CreatePopupMenu();
                // Order: Add New Folder, Rename, Delete (as requested). Bookmark extras first.
                if (n->type == BookmarkNode::Bookmark) {
                    AppendMenuW(menu, MF_STRING, 1, L"Open in New Tab");
                    AppendMenuW(menu, MF_STRING, 2, L"Copy URL");
                    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
                }
                AppendMenuW(menu, MF_STRING, 5, L"Add New Folder");
                AppendMenuW(menu, MF_STRING, 3, L"Rename");
                AppendMenuW(menu, MF_STRING, 4, L"Delete");
                POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
                ClientToScreen(hwnd, &pt);
                g_menuTracking = true;
                const int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
                g_menuTracking = false;
                DestroyMenu(menu);
                if (cmd == 1 && n->type == BookmarkNode::Bookmark) NewTab(n->url, true);
                else if (cmd == 2 && n->type == BookmarkNode::Bookmark) {
                    if (OpenClipboard(hwnd)) {
                        EmptyClipboard();
                        const size_t bytes = (n->url.size() + 1) * sizeof(wchar_t);
                        HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, bytes);
                        if (h) {
                            void* p = GlobalLock(h);
                            if (p) { memcpy(p, n->url.c_str(), bytes); GlobalUnlock(h); SetClipboardData(CF_UNICODETEXT, h); }
                        }
                        CloseClipboard();
                    }
                } else if (cmd == 3) {
                    std::wstring name = InputBox(L"Rename", L"New name:", n->title);
                    if (!name.empty()) { n->title = name; SaveBookmarks(); refreshItems(); InvalidateRect(hwnd, nullptr, FALSE); InvalidateBookmarkBar(); }
                } else if (cmd == 4) {
                    RemoveNode(g_bookmarks, n); SaveBookmarks(); CloseAllFolderPopups(); InvalidateBookmarkBar(); RefreshChrome();
                } else if (cmd == 5) {
                    BookmarkTree* targetChildren = (n->type == BookmarkNode::Folder) ? &n->children : (container ? container : &g_bookmarks);
                    std::wstring name = InputBox(L"New Folder", L"Folder name:", L"New Folder");
                    if (!name.empty()) { AddFolder(*targetChildren, name); SaveBookmarks(); refreshItems(); InvalidateRect(hwnd, nullptr, FALSE); InvalidateBookmarkBar(); }
                }
            }
            return 0;
        }
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) { CloseAllFolderPopups(); return 0; }
            break;
        case WM_ACTIVATE:
            if (LOWORD(wp) == WA_INACTIVE) ArmFolderCloseTimer();
            break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void ShowFolderPopupInternal(BookmarkNode* folder, const std::vector<BookmarkNode*>* items, bool overflow, RECT anchor, int level) {
    if ((!folder && !items) || level < 0 || level >= kMaxFolderLevels) return;
    CloseFolderPopupsFrom(level);
    g_folderPopupNodes[level] = folder;
    g_folderPopupOverflow[level] = overflow;
    g_folderPopupItems[level].clear();
    size_t count = 0;
    if (items) {
        g_folderPopupItems[level] = *items;
        count = items->size();
    } else if (folder) {
        for (auto& c : folder->children) g_folderPopupItems[level].push_back(c.get());
        count = folder->children.size();
    }
    wchar_t className[64];
    swprintf_s(className, L"FeatherFolderPopup%d", level);
    static bool registered[kMaxFolderLevels] = {};
    if (!registered[level]) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = FolderPopupWndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = className;
        RegisterClassExW(&wc);
        registered[level] = true;
    }
    int height = static_cast<int>(count) * 24 + 8;
    if (height < 32) height = 32;
    if (height > 420) height = 420;
    g_folderPopups[level] = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, className, L"",
                                            WS_POPUP | WS_BORDER, anchor.left, anchor.top, 220, height,
                                            g_main, nullptr, GetModuleHandleW(nullptr), nullptr);
    ShowWindow(g_folderPopups[level], SW_SHOWNOACTIVATE);
    ArmFolderCloseTimer();
}

void ShowFolderPopup(BookmarkNode* folder, RECT anchor, int level) {
    ShowFolderPopupInternal(folder, nullptr, false, anchor, level);
}

void ShowOverflowPopup() {
    BookmarkLayout layout = GetBookmarkLayout();
    if (!layout.hasOverflow || layout.overflow.empty()) return;
    POINT pt{layout.overflowRect.left, layout.overflowRect.bottom};
    ClientToScreen(g_main, &pt);
    ShowFolderPopupInternal(nullptr, &layout.overflow, true, RECT{pt.x, pt.y, pt.x + 220, pt.y + 200}, 0);
}

void EnsureFindBar();
void ShowFindBar();
void HideFindBar();
void FindInPage(bool forward);

void ReopenClosedTab() {  // FR-NAV4
    if (g_closedTabs.empty()) return;
    ClosedTab ct = std::move(g_closedTabs.back());
    g_closedTabs.pop_back();
    if (!ct.url.empty()) NewTab(ct.url, true);
}

void HandleShortcut(UINT vk, bool shift) {
    if (vk == 'T') { if (shift) ReopenClosedTab(); else NewTab(kHomePage, true); }
    else if (vk == 'W') CloseTab(g_activeId);
    else if (vk == 'L') { if (g_address) { SetFocus(g_address); SendMessageW(g_address, EM_SETSEL, 0, -1); } }
    else if (vk == 'R') { Tab* t = ActiveTab(); if (t && t->webview) t->webview->Reload(); }
    else if (vk == 'D') ToggleBookmark();
    else if (vk == 'F') { ShowFindBar(); }
    else if (vk == 'G') { if (g_findVisible) FindInPage(!shift); }
    else if (vk == 'H') { ShowHistoryWindow(); }  // FR-HIS1
    else if (vk == 'J') { ShowDownloadsWindow(); } // FR-DL1
    else if (vk == VK_TAB) {
        const int count = static_cast<int>(g_tabs.size());
        const int index = IndexOf(g_activeId);
        if (count > 0 && index >= 0) ActivateTab(g_tabs[static_cast<size_t>((index + (shift ? count - 1 : 1)) % count)]->id);
    } else if (vk >= '1' && vk <= '9') {
        const int count = static_cast<int>(g_tabs.size());
        const int index = vk == '9' ? count - 1 : static_cast<int>(vk - '1');
        if (index >= 0 && index < count) ActivateTab(g_tabs[static_cast<size_t>(index)]->id);
    }
}

HWND CreateChild(HWND parent, const wchar_t* cls, const wchar_t* text, DWORD style, DWORD exStyle, int id) {
    return CreateWindowExW(exStyle, cls, text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | style, 0, 0, 0, 0, parent,
                           reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
}

LRESULT CALLBACK AddressProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_KEYDOWN) {
        if (wp == VK_RETURN) { NavigateActive(GetText(hwnd)); return 0; }
        if (wp == VK_ESCAPE) { Tab* t = ActiveTab(); if (t) SetWindowTextW(hwnd, t->url.c_str()); FocusActive(); return 0; }
        if (IsCtrlDown() && IsShortcutKey(static_cast<UINT>(wp))) { PostMessageW(g_main, kMsgShortcut, wp, IsShiftDown() ? 1 : 0); return 0; }
    }
    if (msg == WM_CHAR && (wp == VK_RETURN || wp == VK_ESCAPE || (IsCtrlDown() && wp < 0x20))) return 0;
    return CallWindowProcW(g_addressProc, hwnd, msg, wp, lp);
}

LRESULT CALLBACK ButtonProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_MOUSEMOVE:
            if (g_hoverButton != hwnd) { g_hoverButton = hwnd; InvalidateRect(hwnd, nullptr, FALSE); }
            { TRACKMOUSEEVENT tr{sizeof(tr), TME_LEAVE, hwnd, 0}; TrackMouseEvent(&tr); }
            break;
        case WM_MOUSELEAVE:
            if (g_hoverButton == hwnd) { g_hoverButton = nullptr; InvalidateRect(hwnd, nullptr, FALSE); }
            break;
    }
    return CallWindowProcW(g_buttonProc, hwnd, msg, wp, lp);
}

enum class StripHit { None, Tab, Close, NewTab };
StripHit HitTestStrip(int x, int y, int& id) {
    if (y < kTabStripTop || y >= kTabStripTop + kTabStripHeight) return StripHit::None;
    int stripEnd = 0;
    for (size_t i = 0; i < g_tabs.size(); ++i) {
        RECT r = TabRect(static_cast<int>(i));
        stripEnd = std::max(stripEnd, static_cast<int>(r.right));
        if (x >= r.left && x < r.right) {
            id = g_tabs[i]->id;
            const bool pinned = g_tabs[i]->pinned;
            RECT cr = CloseRect(r, pinned);
            const bool onClose = !pinned && x >= cr.left && x < cr.right && y >= cr.top && y < cr.bottom;
            return onClose ? StripHit::Close : StripHit::Tab;
        }
    }
    if (x >= stripEnd && x < stripEnd + kNewTabWidth) return StripHit::NewTab;
    return StripHit::None;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            g_whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
            SetTimer(hwnd, 3, 60000, nullptr);  // FR-TAB6 idle check
            g_back = CreateChild(hwnd, L"BUTTON", L"", BS_OWNERDRAW, 0, kBackButton);
            g_forward = CreateChild(hwnd, L"BUTTON", L"", BS_OWNERDRAW, 0, kForwardButton);
            g_reload = CreateChild(hwnd, L"BUTTON", L"", BS_OWNERDRAW, 0, kReloadButton);
            g_star = CreateChild(hwnd, L"BUTTON", L"", BS_OWNERDRAW, 0, kStarButton);
            g_shield = CreateChild(hwnd, L"BUTTON", L"", BS_OWNERDRAW, 0, kShieldButton);
            g_pip = CreateChild(hwnd, L"BUTTON", L"", BS_OWNERDRAW, 0, kPipButton);
            g_media = CreateChild(hwnd, L"BUTTON", L"", BS_OWNERDRAW, 0, kMediaButton);
            g_address = CreateChild(hwnd, L"EDIT", L"", ES_AUTOHSCROLL, 0, kAddressBar);
            g_addressProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(g_address, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(AddressProc)));
            for (HWND btn : {g_back, g_forward, g_reload, g_star, g_shield, g_pip, g_media}) {
                if (!g_buttonProc) g_buttonProc = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(btn, GWLP_WNDPROC));
                SetWindowLongPtrW(btn, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(ButtonProc));
            }
            for (HWND child : {g_back, g_forward, g_reload, g_star, g_shield, g_pip, g_media, g_address}) {
                SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);
            }
            return 0;
        }
        case WM_CTLCOLOREDIT:
            if (reinterpret_cast<HWND>(lp) == g_address || reinterpret_cast<HWND>(lp) == g_findEdit) {
                SetBkColor(reinterpret_cast<HDC>(wp), RGB(255, 255, 255));
                return reinterpret_cast<LRESULT>(g_whiteBrush);
            }
            break;
        case WM_APP + 4:
            MessageBoxW(g_main,
                L"A .crx extension file was downloaded, but WebView2 cannot install Chrome Web Store/.crx extensions.\n\n"
                L"To use Bitwarden/PiP extensions: get an UNPACKED extension folder (with manifest.json) or a ZIP of the unpacked extension, then use Extensions > Install Extension from ZIP/CRX... or Add Unpacked Extension...\n\n"
                L"Firefox extensions are not supported (featherWIN uses WebView2/Chromium).",
                L"Extensions", MB_OK | MB_ICONINFORMATION);
            return 0;
        case WM_DRAWITEM: {
            const auto* item = reinterpret_cast<const DRAWITEMSTRUCT*>(lp);
            if (item && item->CtlType == ODT_BUTTON) { PaintToolbarButton(*item); return TRUE; }
            break;
        }
        case WM_SIZE:
            LayoutChrome();
            LayoutContent();
            InvalidateRect(hwnd, nullptr, FALSE);
            CloseAllFolderPopups();
            return 0;
        case WM_MOVE:
        case WM_WINDOWPOSCHANGED:
            CloseAllFolderPopups();
            break;
        case WM_ACTIVATEAPP:
            if (wp == FALSE) CloseAllFolderPopups();
            break;
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(hwnd, &ps);
            RECT client{};
            GetClientRect(hwnd, &client);
            RECT chrome{0, 0, client.right, ChromeHeight()};
            HBRUSH bg = CreateSolidBrush(kThemeBg);
            FillRect(dc, &chrome, bg);
            DeleteObject(bg);
            FillRoundedRect(dc, AddressPillRect(), 16, RGB(255, 255, 255), kThemeBorder);
            PaintMenuBar(dc);
            PaintTabStrip(dc);
            if (g_findVisible) {
                RECT fr{0, FindBarTop(), client.right, FindBarTop() + kFindBarHeight};
                HBRUSH fb = CreateSolidBrush(RGB(240, 242, 245));
                FillRect(dc, &fr, fb);
                DeleteObject(fb);
                HGDIOBJ fo = SelectObject(dc, g_smallFont ? g_smallFont : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
                SetBkMode(dc, TRANSPARENT);
                SetTextColor(dc, RGB(80, 84, 88));
                RECT lbl{std::max(8L, static_cast<LONG>(client.right - 430)), static_cast<LONG>(FindBarTop()), std::max(50L, static_cast<LONG>(client.right - 390)), static_cast<LONG>(FindBarTop() + kFindBarHeight)};
                DrawTextW(dc, L"Find", -1, &lbl, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
                SelectObject(dc, fo);
            }
            PaintBookmarkBar(dc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            const int x = GET_X_LPARAM(lp);
            const int y = GET_Y_LPARAM(lp);
            if (y < BookmarkBarTop() || y >= BookmarkBarTop() + kBookmarkBarHeight) CloseAllFolderPopups();
            const int menuHit = HitTestMenu(x, y);
            if (menuHit != kMenuNone) { ShowMenu(menuHit); return 0; }
            if (OverflowAt(x, y)) { ShowOverflowPopup(); return 0; }
            BookmarkNode* bm = BookmarkAt(x, y);
            if (bm) {
                g_barDragNode = bm;
                g_barDragStart = POINT{x, y};
                g_barDragging = false;
                SetCapture(hwnd);
                return 0;
            }
            int id = 0;
            switch (HitTestStrip(x, y, id)) {
                case StripHit::Tab: ActivateTab(id); break;
                case StripHit::Close: CloseTab(id); break;
                case StripHit::NewTab: NewTab(kHomePage, true); break;
                default: break;
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            const int x = GET_X_LPARAM(lp);
            const int y = GET_Y_LPARAM(lp);
            if (g_barDragNode) {
                ReleaseCapture();
                if (g_barDragging) {
                    BookmarkNode* target = BookmarkAt(x, y);
                    if (target && target->type == BookmarkNode::Folder && target != g_barDragNode) {
                        MoveNode(g_bookmarks, g_barDragNode, target, nullptr);
                    } else {
                        // Reorder on bar (append/move to root near position - simplified)
                        MoveNode(g_bookmarks, g_barDragNode, nullptr, target && target != g_barDragNode ? target : nullptr);
                    }
                    SaveBookmarks();
                    InvalidateBookmarkBar();
                    CloseAllFolderPopups();
                } else {
                    if (g_barDragNode->type == BookmarkNode::Bookmark) {
                        Tab* tab = ActiveTab();
                        if (tab && tab->webview) tab->webview->Navigate(g_barDragNode->url.c_str());
                    } else if (g_barDragNode->type == BookmarkNode::Folder) {
                        RECT anchor{};
                        for (const auto& chip : BookmarkChips()) {
                            if (chip.node == g_barDragNode) { anchor = chip.rect; break; }
                        }
                        POINT tl{anchor.left, anchor.bottom};
                        ClientToScreen(g_main, &tl);
                        ShowFolderPopup(g_barDragNode, RECT{tl.x, tl.y, tl.x + 220, tl.y + 200}, 0);
                    }
                }
                g_barDragNode = nullptr;
                g_barDragging = false;
                g_barDropTarget = nullptr;
                return 0;
            }
            break;
        }
        case WM_MOUSEMOVE: {
            const int x = GET_X_LPARAM(lp);
            const int y = GET_Y_LPARAM(lp);
            const int h = HitTestMenu(x, y);
            if (h != g_hoverMenu) { g_hoverMenu = h; InvalidateMenuBar(); }
            if (g_barDragNode && (GetKeyState(VK_LBUTTON) & 0x8000)) {
                if (!g_barDragging && (abs(x - g_barDragStart.x) > 4 || abs(y - g_barDragStart.y) > 4)) g_barDragging = true;
                if (g_barDragging) {
                    BookmarkNode* target = BookmarkAt(x, y);
                    if (target == g_barDragNode) target = nullptr;
                    if (g_barDropTarget != target) { g_barDropTarget = target; InvalidateBookmarkBar(); }
                }
            }
            TRACKMOUSEEVENT tr{sizeof(tr), TME_LEAVE, hwnd, 0};
            TrackMouseEvent(&tr);
            break;
        }
        case WM_MOUSELEAVE:
            if (g_hoverMenu != kMenuNone) { g_hoverMenu = kMenuNone; InvalidateMenuBar(); }
            break;
        case WM_CONTEXTMENU: {
            POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            ScreenToClient(hwnd, &pt);
            const int x = pt.x;
            const int y = pt.y;
            if (y >= kTabStripTop && y < kTabStripTop + kTabStripHeight) {
                int tabId = 0;
                const StripHit hit = HitTestStrip(x, y, tabId);
                if (hit == StripHit::Tab && tabId != 0) {
                    Tab* t = FindTab(tabId);
                    HMENU menu = CreatePopupMenu();
                    AppendMenuW(menu, MF_STRING, 101, t && t->pinned ? L"Unpin Tab" : L"Pin Tab");
                    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
                    AppendMenuW(menu, MF_STRING, 102, L"Close Tab");
                    AppendMenuW(menu, MF_STRING, 103, L"New Tab");
                    POINT sp{x, y};
                    ClientToScreen(hwnd, &sp);
                    const int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, sp.x, sp.y, 0, hwnd, nullptr);
                    DestroyMenu(menu);
                    if (cmd == 101 && t) { t->pinned = !t->pinned; SortTabsPinnedLeft(); SaveSession(); InvalidateStrip(); }
                    else if (cmd == 102) { CloseTab(tabId); SaveSession(); }
                    else if (cmd == 103) { NewTab(kHomePage, true); }
                    return 0;
                }
                if (hit == StripHit::NewTab) { NewTab(kHomePage, true); return 0; }
            }
            if (y >= BookmarkBarTop()) {
                BookmarkNode* bm = BookmarkAt(x, y);
                HMENU menu = CreatePopupMenu();
                if (bm && bm->type == BookmarkNode::Bookmark) {
                    AppendMenuW(menu, MF_STRING, 1, L"Open in New Tab");
                    AppendMenuW(menu, MF_STRING, 2, L"Copy URL");
                    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
                }
                // Requested order: Add New Folder, Rename, Delete
                AppendMenuW(menu, MF_STRING, 5, L"Add New Folder");
                AppendMenuW(menu, MF_STRING | (bm ? MF_ENABLED : MF_GRAYED), 3, L"Rename");
                AppendMenuW(menu, MF_STRING | (bm ? MF_ENABLED : MF_GRAYED), 4, L"Delete");
                POINT sp{x, y};
                ClientToScreen(hwnd, &sp);
                const int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, sp.x, sp.y, 0, hwnd, nullptr);
                DestroyMenu(menu);
                if (cmd == 1 && bm && bm->type == BookmarkNode::Bookmark) NewTab(bm->url, true);
                else if (cmd == 2 && bm && bm->type == BookmarkNode::Bookmark) {
                    if (OpenClipboard(hwnd)) {
                        EmptyClipboard();
                        const size_t bytes = (bm->url.size() + 1) * sizeof(wchar_t);
                        HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, bytes);
                        if (h) { void* p = GlobalLock(h); if (p) { memcpy(p, bm->url.c_str(), bytes); GlobalUnlock(h); SetClipboardData(CF_UNICODETEXT, h); } }
                        CloseClipboard();
                    }
                } else if (cmd == 3 && bm) {
                    std::wstring name = InputBox(L"Rename", L"New name:", bm->title);
                    if (!name.empty()) { bm->title = name; SaveBookmarks(); InvalidateBookmarkBar(); }
                } else if (cmd == 4 && bm) {
                    RemoveNode(g_bookmarks, bm); SaveBookmarks(); InvalidateBookmarkBar(); RefreshChrome();
                } else if (cmd == 5) {
                    BookmarkTree* target = (bm && bm->type == BookmarkNode::Folder) ? &bm->children : &g_bookmarks;
                    std::wstring name = InputBox(L"New Folder", L"Folder name:", L"New Folder");
                    if (!name.empty()) { AddFolder(*target, name); SaveBookmarks(); InvalidateBookmarkBar(); }
                }
                return 0;
            }
            break;
        }
        case WM_COMMAND:
            if (HIWORD(wp) == BN_CLICKED) {
                const int id = LOWORD(wp);
                if (id == kStarButton) { ToggleBookmark(); return 0; }
                if (id == kShieldButton) {
                    ShowShieldsMenu();  // icon shows state; menu allows/blocks ads for this website (FR-SHD2)
                    return 0;
                }
                if (id == kPipButton) { RequestSelfPiP(); return 0; }  // FR-VID1
                if (id == kMediaButton) { ShowMediaMenu(); return 0; }
                if (id == kFindNext) { FindInPage(true); return 0; }
                if (id == kFindPrev) { FindInPage(false); return 0; }
                if (id == kFindClose) { HideFindBar(); return 0; }
                Tab* tab = ActiveTab();
                if (tab && tab->webview) {
                    if (id == kBackButton) tab->webview->GoBack();
                    else if (id == kForwardButton) tab->webview->GoForward();
                    else if (id == kReloadButton) tab->webview->Reload();
                }
                return 0;
            }
            break;
        case WM_KEYDOWN:
            if (IsCtrlDown() && IsShortcutKey(static_cast<UINT>(wp))) { HandleShortcut(static_cast<UINT>(wp), IsShiftDown()); return 0; }
            break;
        case WM_TIMER:
            if (wp == 3) {  // FR-TAB6: suspend idle background tabs (5 min default)
                const DWORD now = GetTickCount();
                const DWORD idleMs = 5 * 60 * 1000;
                for (auto& tab : g_tabs) {
                    if (!tab || tab->id == g_activeId || !tab->webview) continue;
                    if (tab->lastActiveTick == 0 || (now - tab->lastActiveTick) <= idleMs) continue;
                    ComPtr<ICoreWebView2_3> wv3;
                    if (FAILED(tab->webview.As(&wv3)) || !wv3) continue;
                    BOOL isSuspended = FALSE;
                    if (SUCCEEDED(wv3->get_IsSuspended(&isSuspended)) && isSuspended) {
                        tab->suspended = true;
                        continue;
                    }
                    const int id = tab->id;
                    wv3->TrySuspend(Callback<ICoreWebView2TrySuspendCompletedHandler>(
                        [id](HRESULT, BOOL isSuccessful) -> HRESULT {
                            if (isSuccessful) {
                                Tab* t = FindTab(id);
                                if (t) t->suspended = true;
                            }
                            return S_OK;
                        }).Get());
                }
                return 0;
            }
            if (wp == kTimerFolderClose) {
                POINT pt{};
                GetCursorPos(&pt);
                if (!g_menuTracking && !IsPointOverFolderUI(pt) && g_dragPopup == nullptr) {
                    KillTimer(g_main, kTimerFolderClose);
                    CloseAllFolderPopups();
                }
                return 0;
            }
            break;
        case kMsgShortcut:
            HandleShortcut(static_cast<UINT>(wp), lp != 0);
            return 0;
        case WM_CLOSE:
            SaveSession();
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            CloseAllFolderPopups();
            if (g_whiteBrush) { DeleteObject(g_whiteBrush); g_whiteBrush = nullptr; }
            if (g_iconFont) { DeleteObject(g_iconFont); g_iconFont = nullptr; }
            if (g_uiFont) { DeleteObject(g_uiFont); g_uiFont = nullptr; }
            if (g_smallFont) { DeleteObject(g_smallFont); g_smallFont = nullptr; }
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int show) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    g_uiFont = CreateFontW(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    g_smallFont = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    g_iconFont = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe MDL2 Assets");
    PWSTR appData = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appData))) {
        g_dataFolder = TakeString(appData) + L"\\featherWIN";
        SHCreateDirectoryExW(nullptr, g_dataFolder.c_str(), nullptr);
        g_bookmarkPath = g_dataFolder + L"\\bookmarks.txt";
        g_shieldExceptionsPath = g_dataFolder + L"\\shields-exceptions.txt";
        g_extensionsFolder = g_dataFolder + L"\\extensions";
        g_historyPath = g_dataFolder + L"\\history.txt";
        g_pinnedPath = g_dataFolder + L"\\pinned.txt";
        g_sessionPath = g_dataFolder + L"\\session.txt";
        SHCreateDirectoryExW(nullptr, g_extensionsFolder.c_str(), nullptr);
        g_settingsPath = g_dataFolder + L"\\settings.txt";
        g_tabGroupsPath = g_dataFolder + L"\\tab-groups.json"; // FR-GRP1
        LoadBookmarks();
        LoadShieldExceptions();
        LoadHistory();
        LoadSettings();
        LoadTabGroups(); // FR-GRP1
    }
    HICON appIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_FEATHERWIN), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED));
    HICON appIconSm = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_FEATHERWIN), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED));
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hIcon = appIcon ? appIcon : LoadIconW(nullptr, IDI_APPLICATION);
    wc.hIconSm = appIconSm ? appIconSm : LoadIconW(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = kWindowClass;
    RegisterClassExW(&wc);
    g_main = CreateWindowExW(0, kWindowClass, L"featherWIN", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                             CW_USEDEFAULT, CW_USEDEFAULT, 1100, 760, nullptr, nullptr, instance, nullptr);
    ShowWindow(g_main, show);
    std::wstring dataFolder;
    PWSTR localAppData = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData))) {
        dataFolder = TakeString(localAppData) + L"\\featherWIN\\WebView2";
        SHCreateDirectoryExW(nullptr, dataFolder.c_str(), nullptr);
    }
    auto options = Make<CoreWebView2EnvironmentOptions>();
    ComPtr<ICoreWebView2EnvironmentOptions6> options6;
    if (options && SUCCEEDED(options.As(&options6))) {
        options6->put_AreBrowserExtensionsEnabled(TRUE);
    }
    CreateCoreWebView2EnvironmentWithOptions(
        nullptr, dataFolder.empty() ? nullptr : dataFolder.c_str(), options.Get(),
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(result)) {
                    MessageBoxW(g_main, L"WebView2 Runtime is required.", L"featherWIN", MB_ICONERROR | MB_OK);
                    return S_OK;
                }
                g_env = env;
                auto session = LoadSessionData();
                if (session.empty()) {
                    NewTab(kHomePage, true);
                } else {
                    std::wstring activeUrl;
                    int ai = session[0].activeIndex;
                    if (ai >= 0 && ai < static_cast<int>(session.size())) activeUrl = session[static_cast<size_t>(ai)].url;
                    std::stable_sort(session.begin(), session.end(), [](const SessionTab& a, const SessionTab& b) { return a.pinned && !b.pinned; });
                    int active = 0;
                    if (!activeUrl.empty()) {
                        for (int i = 0; i < static_cast<int>(session.size()); ++i) if (session[static_cast<size_t>(i)].url == activeUrl) { active = i; break; }
                    }
                    for (int i = 0; i < static_cast<int>(session.size()); ++i) {
                        const auto& s = session[static_cast<size_t>(i)];
                        NewTab(s.url, i == active, s.pinned, s.title);
                        if (!g_tabs.empty()) g_tabs.back()->groupId = s.groupId; // FR-GRP4 restore membership
                    }
                }
                return S_OK;
            }).Get());
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_TAB && IsCtrlDown()) {
            HandleShortcut(VK_TAB, IsShiftDown());
            continue;
        }
        if (g_inputDlg && IsWindow(g_inputDlg) && IsDialogMessageW(g_inputDlg, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    CoUninitialize();
    return 0;
}
