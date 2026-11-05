#include "url.h"

namespace {

void AppendUtf8(std::string& out, char32_t cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

// UTF-8 of a wide string. On Windows wchar_t is UTF-16, so surrogate pairs are joined.
std::string ToUtf8(const std::wstring& text) {
    std::string out;
    for (size_t i = 0; i < text.size(); ++i) {
        char32_t cp = static_cast<char32_t>(text[i]);
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < text.size()) {
            const char32_t low = static_cast<char32_t>(text[i + 1]);
            if (low >= 0xDC00 && low <= 0xDFFF) {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                ++i;
            }
        }
        AppendUtf8(out, cp);
    }
    return out;
}

std::wstring PercentEncode(const std::wstring& text) {
    static const char kHex[] = "0123456789ABCDEF";
    std::wstring out;
    for (unsigned char byte : ToUtf8(text)) {
        const bool unreserved = (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
                                (byte >= '0' && byte <= '9') || byte == '-' || byte == '_' ||
                                byte == '.' || byte == '~';
        if (unreserved) {
            out += static_cast<wchar_t>(byte);
        } else {
            out += L'%';
            out += static_cast<wchar_t>(kHex[byte >> 4]);
            out += static_cast<wchar_t>(kHex[byte & 0x0F]);
        }
    }
    return out;
}

std::wstring Trim(const std::wstring& text) {
    const size_t first = text.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) {
        return L"";
    }
    const size_t last = text.find_last_not_of(L" \t\r\n");
    return text.substr(first, last - first + 1);
}

}  // namespace

std::wstring ToUrl(const std::wstring& input) {
    const std::wstring text = Trim(input);
    if (text.rfind(L"https://", 0) == 0 || text.rfind(L"http://", 0) == 0) {
        return text;
    }
    if (text.find(L' ') == std::wstring::npos && text.find(L'.') != std::wstring::npos) {
        return L"https://" + text;
    }
    return L"https://duckduckgo.com/?q=" + PercentEncode(text);
}
