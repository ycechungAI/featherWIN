// Spike (FR-EXT1, FR-EXT5): load one unpacked Chromium extension into WebView2, then open its popup
// page in a WebView2 tab. Prints what happened; the window stays open so you can look at the result.
//
// Usage: ext_popup_spike.exe <extension folder> [popup page, default popup.html]

#include <windows.h>
#include <wrl/client.h>
#include <wrl/event.h>
#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>

#include <cstdio>
#include <string>

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;

namespace {

HWND g_hwnd = nullptr;
ComPtr<ICoreWebView2Environment> g_env;
ComPtr<ICoreWebView2Controller> g_controller;
ComPtr<ICoreWebView2> g_webview;
std::wstring g_extensionFolder;
std::wstring g_popupPage = L"popup.html";

std::wstring TakeString(LPWSTR text) {
    std::wstring value = text ? text : L"";
    CoTaskMemFree(text);
    return value;
}

void OpenPopup(const std::wstring& extensionId) {
    const std::wstring url = L"chrome-extension://" + extensionId + L"/" + g_popupPage;
    wprintf(L"navigating to %s\n", url.c_str());
    g_webview->Navigate(url.c_str());
}

void ListenForResult() {
    EventRegistrationToken token{};
    g_webview->add_NavigationCompleted(
        Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [](ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                BOOL ok = FALSE;
                COREWEBVIEW2_WEB_ERROR_STATUS error{};
                args->get_IsSuccess(&ok);
                args->get_WebErrorStatus(&error);
                LPWSTR title = nullptr;
                sender->get_DocumentTitle(&title);
                wprintf(L"navigation %s (web error status %d), page title \"%s\"\n",
                        ok ? L"succeeded" : L"FAILED", static_cast<int>(error),
                        TakeString(title).c_str());
                return S_OK;
            })
            .Get(),
        &token);
}

void AddExtension() {
    ComPtr<ICoreWebView2_13> webview13;
    g_webview.As(&webview13);
    ComPtr<ICoreWebView2Profile> profile;
    webview13->get_Profile(&profile);
    ComPtr<ICoreWebView2Profile7> profile7;
    profile.As(&profile7);

    const HRESULT hr = profile7->AddBrowserExtension(
        g_extensionFolder.c_str(),
        Callback<ICoreWebView2ProfileAddBrowserExtensionCompletedHandler>(
            [](HRESULT errorCode, ICoreWebView2BrowserExtension* extension) -> HRESULT {
                if (FAILED(errorCode) || !extension) {
                    wprintf(L"AddBrowserExtension failed: 0x%08lX\n", static_cast<unsigned long>(errorCode));
                    return S_OK;
                }
                LPWSTR idRaw = nullptr;
                LPWSTR nameRaw = nullptr;
                extension->get_Id(&idRaw);
                extension->get_Name(&nameRaw);
                const std::wstring id = TakeString(idRaw);
                wprintf(L"extension loaded: \"%s\" (id %s)\n", TakeString(nameRaw).c_str(), id.c_str());
                OpenPopup(id);
                return S_OK;
            })
            .Get());
    if (FAILED(hr)) {
        wprintf(L"AddBrowserExtension call failed: 0x%08lX\n", static_cast<unsigned long>(hr));
    }
}

void CreateWebView() {
    auto options = Make<CoreWebView2EnvironmentOptions>();
    ComPtr<ICoreWebView2EnvironmentOptions6> options6;
    options.As(&options6);
    options6->put_AreBrowserExtensionsEnabled(TRUE);

    CreateCoreWebView2EnvironmentWithOptions(
        nullptr, nullptr, options.Get(),
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(result)) {
                    wprintf(L"WebView2 environment failed: 0x%08lX (is the WebView2 Runtime installed?)\n",
                            static_cast<unsigned long>(result));
                    return S_OK;
                }
                g_env = env;
                g_env->CreateCoreWebView2ControllerAsync(
                    g_hwnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [](HRESULT controllerResult, ICoreWebView2Controller* controller) -> HRESULT {
                            if (FAILED(controllerResult) || !controller) {
                                wprintf(L"WebView2 controller failed: 0x%08lX\n",
                                        static_cast<unsigned long>(controllerResult));
                                return S_OK;
                            }
                            g_controller = controller;
                            g_controller->get_CoreWebView2(&g_webview);
                            RECT bounds{};
                            GetClientRect(g_hwnd, &bounds);
                            g_controller->put_Bounds(bounds);
                            ListenForResult();
                            AddExtension();
                            return S_OK;
                        })
                        .Get());
                return S_OK;
            })
            .Get());
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_SIZE:
        if (g_controller) {
            RECT bounds{};
            GetClientRect(hwnd, &bounds);
            g_controller->put_Bounds(bounds);
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) {
        wprintf(L"usage: ext_popup_spike.exe <extension folder> [popup page]\n");
        return 2;
    }
    wchar_t fullPath[MAX_PATH];
    GetFullPathNameW(argv[1], MAX_PATH, fullPath, nullptr);
    g_extensionFolder = fullPath;
    if (argc > 2) {
        g_popupPage = argv[2];
    }

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WndProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = L"featherWINExtensionSpike";
    RegisterClassExW(&windowClass);

    g_hwnd = CreateWindowExW(0, windowClass.lpszClassName, L"featherWIN extension popup spike",
                             WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1000, 700,
                             nullptr, nullptr, instance, nullptr);
    ShowWindow(g_hwnd, SW_SHOW);
    wprintf(L"loading extension from %s\n", g_extensionFolder.c_str());
    CreateWebView();

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    CoUninitialize();
    return 0;
}
