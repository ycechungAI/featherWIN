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

if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
  set(WV2_ARCH arm64)
else()
  set(WV2_ARCH x64)
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
