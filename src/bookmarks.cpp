#include "bookmarks.h"

#include <algorithm>
#include <functional>

namespace {

std::wstring Sanitize(std::wstring text) {
    std::replace_if(
        text.begin(), text.end(), [](wchar_t c) { return c == L'\t' || c == L'\r' || c == L'\n'; }, L' ');
    return text;
}

BookmarkNode* FindBookmarkNodeImpl(BookmarkTree& tree, const std::wstring& url) {
    for (auto& node : tree) {
        if (node->type == BookmarkNode::Bookmark && node->url == url) {
            return node.get();
        }
        if (node->type == BookmarkNode::Folder) {
            BookmarkNode* found = FindBookmarkNodeImpl(node->children, url);
            if (found) {
                return found;
            }
        }
    }
    return nullptr;
}

const BookmarkNode* FindBookmarkNodeImpl(const BookmarkTree& tree, const std::wstring& url) {
    for (const auto& node : tree) {
        if (node->type == BookmarkNode::Bookmark && node->url == url) {
            return node.get();
        }
        if (node->type == BookmarkNode::Folder) {
            const BookmarkNode* found = FindBookmarkNodeImpl(node->children, url);
            if (found) {
                return found;
            }
        }
    }
    return nullptr;
}

bool RemoveNodeImpl(BookmarkTree& tree, BookmarkNode* node) {
    for (size_t i = 0; i < tree.size(); ++i) {
        if (tree[i].get() == node) {
            tree.erase(tree.begin() + i);
            return true;
        }
        if (tree[i]->type == BookmarkNode::Folder) {
            if (RemoveNodeImpl(tree[i]->children, node)) {
                return true;
            }
        }
    }
    return false;
}

bool IsSameOrDescendant(BookmarkNode* node, BookmarkNode* maybeAncestor) {
    if (!node || !maybeAncestor) return false;
    if (node == maybeAncestor) return true;
    if (maybeAncestor->type != BookmarkNode::Folder) return false;
    for (const auto& c : maybeAncestor->children) {
        if (IsSameOrDescendant(node, c.get())) return true;
    }
    return false;
}

void SerializeNode(const BookmarkNode& node, std::wstring& out, int depth) {
    std::wstring indent;
    for (int i = 0; i < depth; ++i) {
        indent += L"  ";
    }
    if (node.type == BookmarkNode::Folder) {
        out += indent + L"F\t" + node.title + L"\n";
        for (const auto& child : node.children) {
            SerializeNode(*child, out, depth + 1);
        }
        out += indent + L"E\n";  // end folder
    } else {
        out += indent + L"B\t" + node.title + L"\t" + node.url + L"\n";
    }
}

}  // namespace

BookmarkNode* FindBookmarkNode(BookmarkTree& tree, const std::wstring& url) {
    return FindBookmarkNodeImpl(tree, url);
}

const BookmarkNode* FindBookmarkNode(const BookmarkTree& tree, const std::wstring& url) {
    return FindBookmarkNodeImpl(tree, url);
}

bool IsBookmarked(const BookmarkTree& tree, const std::wstring& url) {
    return FindBookmarkNodeImpl(tree, url) != nullptr;
}

bool AddBookmark(BookmarkTree& tree, const std::wstring& title, const std::wstring& url) {
    if (url.empty() || IsBookmarked(tree, url)) {
        return false;
    }
    const std::wstring cleanUrl = Sanitize(url);
    auto node = std::make_unique<BookmarkNode>();
    node->type = BookmarkNode::Bookmark;
    node->title = title.empty() ? cleanUrl : Sanitize(title);
    node->url = cleanUrl;
    tree.push_back(std::move(node));
    return true;
}

BookmarkNode* AddFolder(BookmarkTree& tree, const std::wstring& title) {
    auto node = std::make_unique<BookmarkNode>();
    node->type = BookmarkNode::Folder;
    node->title = Sanitize(title.empty() ? L"New Folder" : title);
    BookmarkNode* ptr = node.get();
    tree.push_back(std::move(node));
    return ptr;
}

bool RemoveNode(BookmarkTree& tree, BookmarkNode* node) {
    return RemoveNodeImpl(tree, node);
}

void MoveNode(BookmarkTree& root, BookmarkNode* node, BookmarkNode* newParent, BookmarkNode* before) {
    (void)root;
    if (!node) {
        return;
    }
    // Prevent cycles/corruption: cannot move a folder into itself or its descendants.
    if (newParent && IsSameOrDescendant(newParent, node)) {
        return;
    }
    if (before == node) before = nullptr;
    // Find and remove node from current location, then insert at target.
    std::function<bool(BookmarkTree&)> remover = [&](BookmarkTree& tree) -> bool {
        for (size_t i = 0; i < tree.size(); ++i) {
            if (tree[i].get() == node) {
                auto moved = std::move(tree[i]);
                tree.erase(tree.begin() + i);
                BookmarkTree* targetTree = &root;
                if (newParent && newParent->type == BookmarkNode::Folder) {
                    // If target became invalid in a weird case, fall back to root.
                    targetTree = &newParent->children;
                }
                if (before) {
                    for (size_t j = 0; j < targetTree->size(); ++j) {
                        if ((*targetTree)[j].get() == before) {
                            targetTree->insert(targetTree->begin() + j, std::move(moved));
                            return true;
                        }
                    }
                }
                targetTree->push_back(std::move(moved));
                return true;
            }
            if (tree[i]->type == BookmarkNode::Folder) {
                if (remover(tree[i]->children)) {
                    return true;
                }
            }
        }
        return false;
    };
    remover(root);
}

std::wstring SerializeBookmarks(const BookmarkTree& tree) {
    std::wstring out;
    for (const auto& node : tree) {
        SerializeNode(*node, out, 0);
    }
    return out;
}

BookmarkTree DeserializeBookmarks(const std::wstring& text) {
    BookmarkTree tree;
    std::vector<BookmarkTree*> stack;
    stack.push_back(&tree);
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find(L'\n', start);
        if (end == std::wstring::npos) {
            end = text.size();
        }
        std::wstring line = text.substr(start, end - start);
        start = end + 1;
        if (!line.empty() && line.back() == L'\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        // Count depth
        int depth = 0;
        size_t pos = 0;
        while (pos + 1 < line.size() && line[pos] == L' ' && line[pos + 1] == L' ') {
            depth++;
            pos += 2;
        }
        line = line.substr(pos);
        while (static_cast<int>(stack.size()) > depth + 1) {
            stack.pop_back();
        }
        if (line == L"E") {
            // Close current folder: stack depth normalization above already handled nesting.
            continue;
        }
        if (line.size() > 0 && line[0] == L'F') {
            // folder
            size_t tab = line.find(L'\t');
            std::wstring title = tab == std::wstring::npos ? L"Folder" : line.substr(tab + 1);
            auto node = std::make_unique<BookmarkNode>();
            node->type = BookmarkNode::Folder;
            node->title = title;
            BookmarkNode* ptr = node.get();
            if (!stack.empty()) {
                stack.back()->push_back(std::move(node));
            }
            stack.push_back(&ptr->children);
        } else if (line.size() > 0 && line[0] == L'B') {
            size_t tab1 = line.find(L'\t');
            if (tab1 == std::wstring::npos) {
                continue;
            }
            size_t tab2 = line.find(L'\t', tab1 + 1);
            std::wstring title = line.substr(tab1 + 1, tab2 == std::wstring::npos ? std::wstring::npos : tab2 - tab1 - 1);
            std::wstring url = tab2 == std::wstring::npos ? L"" : line.substr(tab2 + 1);
            auto node = std::make_unique<BookmarkNode>();
            node->type = BookmarkNode::Bookmark;
            node->title = title.empty() ? url : title;
            node->url = url;
            if (!stack.empty()) {
                stack.back()->push_back(std::move(node));
            }
        }
    }
    return tree;
}

// Helper to extract attribute value from HTML tag
namespace {
std::wstring GetAttr(const std::wstring& tag, const std::wstring& attr) {
    std::wstring lower = tag;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
    std::wstring attrLower = attr;
    std::transform(attrLower.begin(), attrLower.end(), attrLower.begin(), ::towlower);
    size_t pos = lower.find(attrLower + L"=");
    if (pos == std::wstring::npos) return L"";
    pos += attrLower.size() + 1;
    if (pos >= tag.size()) return L"";
    wchar_t quote = tag[pos];
    if (quote == L'"' || quote == L'\'') {
        pos++;
        size_t end = tag.find(quote, pos);
        if (end == std::wstring::npos) return L"";
        return tag.substr(pos, end - pos);
    }
    size_t end = tag.find_first_of(L" >\t\n\r", pos);
    if (end == std::wstring::npos) end = tag.size();
    return tag.substr(pos, end - pos);
}

std::wstring HtmlUnescape(const std::wstring& text) {
    std::wstring result;
    size_t i = 0;
    while (i < text.size()) {
        if (text[i] == L'&') {
            if (text.substr(i, 5) == L"&amp;") { result += L'&'; i += 5; continue; }
            if (text.substr(i, 4) == L"&lt;") { result += L'<'; i += 4; continue; }
            if (text.substr(i, 4) == L"&gt;") { result += L'>'; i += 4; continue; }
            if (text.substr(i, 6) == L"&quot;") { result += L'"'; i += 6; continue; }
            if (text.substr(i, 6) == L"&apos;") { result += L'\''; i += 6; continue; }
        }
        result += text[i];
        i++;
    }
    return result;
}

void ParseHtmlRecursive(const std::wstring& html, size_t& pos, BookmarkTree& tree) {
    while (pos < html.size()) {
        // Find next tag
        size_t tagStart = html.find(L'<', pos);
        if (tagStart == std::wstring::npos) break;
        size_t tagEnd = html.find(L'>', tagStart);
        if (tagEnd == std::wstring::npos) break;
        
        std::wstring tag = html.substr(tagStart, tagEnd - tagStart + 1);
        std::wstring lowerTag = tag;
        std::transform(lowerTag.begin(), lowerTag.end(), lowerTag.begin(), ::towlower);
        
        if (lowerTag.find(L"</dl") == 0) {
            pos = tagEnd + 1;
            return;
        }
        
        if (lowerTag.find(L"<h3") == 0) {
            // Folder title follows
            size_t titleEnd = html.find(L"</h3>", tagEnd);
            // Try case-insensitive
            if (titleEnd == std::wstring::npos) {
                std::wstring lower = html;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
                size_t lowerTagEnd = lower.find(L"</h3>", tagEnd);
                titleEnd = lowerTagEnd;
            }
            std::wstring title = L"Folder";
            if (titleEnd != std::wstring::npos) {
                title = html.substr(tagEnd + 1, titleEnd - tagEnd - 1);
                title = HtmlUnescape(title);
                // Trim
                while (!title.empty() && (title.front() == L' ' || title.front() == L'\t' || title.front() == L'\n' || title.front() == L'\r')) title.erase(title.begin());
                while (!title.empty() && (title.back() == L' ' || title.back() == L'\t' || title.back() == L'\n' || title.back() == L'\r')) title.pop_back();
            }
            auto node = std::make_unique<BookmarkNode>();
            node->type = BookmarkNode::Folder;
            node->title = title.empty() ? L"Folder" : title;
            BookmarkNode* ptr = node.get();
            tree.push_back(std::move(node));
            // Skip to DL (case-insensitive)
            std::wstring lowerHtml = html;
            std::transform(lowerHtml.begin(), lowerHtml.end(), lowerHtml.begin(), ::towlower);
            size_t dlStart = lowerHtml.find(L"<dl", titleEnd != std::wstring::npos ? titleEnd : tagEnd);
            if (dlStart != std::wstring::npos) {
                size_t dlEnd = html.find(L'>', dlStart);
                if (dlEnd != std::wstring::npos) {
                    pos = dlEnd + 1;
                    ParseHtmlRecursive(html, pos, ptr->children);
                } else {
                    pos = tagEnd + 1;
                }
            } else {
                pos = tagEnd + 1;
            }
            continue;
        }
        
        if (lowerTag.find(L"<a ") == 0 || lowerTag.find(L"<a>") == 0) {
            std::wstring url = GetAttr(tag, L"href");
            std::wstring lowerHtml = html;
            std::transform(lowerHtml.begin(), lowerHtml.end(), lowerHtml.begin(), ::towlower);
            size_t titleEnd = lowerHtml.find(L"</a>", tagEnd);
            std::wstring title = url;
            if (titleEnd != std::wstring::npos) {
                title = html.substr(tagEnd + 1, titleEnd - tagEnd - 1);
                title = HtmlUnescape(title);
                while (!title.empty() && (title.front() == L' ' || title.front() == L'\t' || title.front() == L'\n' || title.front() == L'\r')) title.erase(title.begin());
                while (!title.empty() && (title.back() == L' ' || title.back() == L'\t' || title.back() == L'\n' || title.back() == L'\r')) title.pop_back();
                if (title.empty()) title = url;
            }
            if (!url.empty()) {
                auto node = std::make_unique<BookmarkNode>();
                node->type = BookmarkNode::Bookmark;
                node->title = title;
                node->url = url;
                tree.push_back(std::move(node));
            }
            pos = (titleEnd != std::wstring::npos) ? titleEnd + 4 : tagEnd + 1;
            continue;
        }
        
        pos = tagEnd + 1;
    }
}
}  // namespace

BookmarkTree ParseNetscapeBookmarks(const std::wstring& html) {
    BookmarkTree tree;
    size_t pos = 0;
    ParseHtmlRecursive(html, pos, tree);
    return tree;
}

std::wstring SerializeNetscapeBookmarks(const BookmarkTree& tree) {
    std::wstring out;
    out += L"<!DOCTYPE NETSCAPE-Bookmark-file-1>\n";
    out += L"<!-- This is an automatically generated file.\n";
    out += L"     It will be read and overwritten.\n";
    out += L"     DO NOT EDIT! -->\n";
    out += L"<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=UTF-8\">\n";
    out += L"<TITLE>Bookmarks</TITLE>\n";
    out += L"<H1>Bookmarks</H1>\n";
    out += L"<DL><p>\n";
    
    std::function<void(const BookmarkTree&, int)> serialize = [&](const BookmarkTree& t, int depth) {
        std::wstring indent;
        for (int i = 0; i < depth; i++) indent += L"    ";
        for (const auto& node : t) {
            if (node->type == BookmarkNode::Folder) {
                out += indent + L"    <DT><H3>" + node->title + L"</H3>\n";
                out += indent + L"    <DL><p>\n";
                serialize(node->children, depth + 1);
                out += indent + L"    </DL><p>\n";
            } else {
                out += indent + L"    <DT><A HREF=\"" + node->url + L"\">" + node->title + L"</A>\n";
            }
        }
    };
    serialize(tree, 0);
    out += L"</DL><p>\n";
    return out;
}
