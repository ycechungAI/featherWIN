cmake_minimum_required(VERSION 3.20)
project(featherWIN LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The WebView2 SDK (headers and static loader) comes from its NuGet package, so nothing is vendored.
set(WEBVIEW2_VERSION "1.0.2739.15")
set(WEBVIEW2_DIR "${CMAKE_BINARY_DIR}/webview2")
if(NOT EXISTS "${WEBVIEW2_DIR}/build/native/include/WebView2.h")
  file(DOWNLOAD "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/${WEBVIEW2_VERSION}"
       "${CMAKE_BINARY_DIR}/webview2.nupkg" STATUS webview2_download TLS_VERIFY ON)
  list(GET webview2_download 0 webview2_code)
  if(NOT webview2_code EQUAL 0)
    message(FATAL_ERROR "Could not download the WebView2 SDK: ${webview2_download}")
  endif()
  file(ARCHIVE_EXTRACT INPUT "${CMAKE_BINARY_DIR}/webview2.nupkg" DESTINATION "${WEBVIEW2_DIR}")
endif()

# Pick the loader for the target platform (-A x64 etc.), not the host CPU, which can differ on ARM64 Windows.
if(CMAKE_GENERATOR_PLATFORM MATCHES "ARM64")
  set(WV2_ARCH arm64)
elseif(CMAKE_GENERATOR_PLATFORM MATCHES "^Win32$")
  set(WV2_ARCH x86)
elseif(CMAKE_GENERATOR_PLATFORM OR NOT CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
  set(WV2_ARCH x64)
else()
  set(WV2_ARCH arm64)
endif()
set(WV2_LOADER "${WEBVIEW2_DIR}/build/native/${WV2_ARCH}/WebView2LoaderStatic.lib")
set(WV2_INCLUDE "${WEBVIEW2_DIR}/build/native/include")

# Feasibility spike: loads one extension and opens its popup (spikes/b-extension-popup/README.md).
add_executable(ext_popup_spike spikes/b-extension-popup/main.cpp)
target_include_directories(ext_popup_spike PRIVATE "${WV2_INCLUDE}")
target_compile_definitions(ext_popup_spike PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(ext_popup_spike PRIVATE "${WV2_LOADER}" advapi32 ole32 shlwapi user32 version)
if(MSVC)
  target_compile_options(ext_popup_spike PRIVATE /utf-8 /W4)
endif()

# The browser window: tabs and address bar (FR-TAB*, FR-NAV1).
add_executable(featherwin WIN32 src/main.cpp src/url.cpp src/blocklist.cpp src/bookmarks.cpp assets/featherwin.rc)
target_include_directories(featherwin PRIVATE "${WV2_INCLUDE}" src assets)
target_compile_definitions(featherwin PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(featherwin PRIVATE "${WV2_LOADER}" advapi32 comdlg32 gdi32 ole32 shell32 user32)
if(MSVC)
  target_compile_options(featherwin PRIVATE /utf-8 /W4)
endif()

# Pure-logic tests (URL tools, blocklist). The same tests run on any OS via tests/run_logic_tests.sh.
add_executable(logic_test tests/logic_test.cpp src/url.cpp src/blocklist.cpp src/bookmarks.cpp)
target_include_directories(logic_test PRIVATE src)
if(MSVC)
  target_compile_options(logic_test PRIVATE /utf-8 /W4)
endif()
enable_testing()
add_test(NAME logic_test COMMAND logic_test)

# Installer/package (NF-4). CPack ZIP works out of the box; NSIS/WIX if installed. Unsinged until release signing is set up.
install(TARGETS featherwin RUNTIME DESTINATION .)
install(FILES README.md SPEC.md DESTINATION . OPTIONAL)
set(CPACK_PACKAGE_NAME "featherWIN")
set(CPACK_PACKAGE_VENDOR "ycechungAI")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A tiny, simple Windows web browser (WebView2)")
set(CPACK_PACKAGE_VERSION "0.2.0-dev")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/README.md")
set(CPACK_GENERATOR "ZIP;NSIS")
set(CPACK_NSIS_DISPLAY_NAME "featherWIN")
set(CPACK_NSIS_PACKAGE_NAME "featherWIN")
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
include(CPack)
