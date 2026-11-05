#pragma once

#include <memory>
#include <string>
#include <vector>

// Bookmark tree model for folders, nesting, and drag/drop (FR-BM1, FR-BM2).

struct BookmarkNode {
    enum Type { Bookmark, Folder };
    Type type = Bookmark;
    std::wstring title;
    std::wstring url;  // only used for Bookmark
    std::vector<std::unique_ptr<BookmarkNode>> children;
};

using BookmarkTree = std::vector<std::unique_ptr<BookmarkNode>>;

// Find a bookmark node by URL (first match). Returns nullptr if not found.
BookmarkNode* FindBookmarkNode(BookmarkTree& tree, const std::wstring& url);
const BookmarkNode* FindBookmarkNode(const BookmarkTree& tree, const std::wstring& url);

// Returns true if `url` exists anywhere in the tree.
bool IsBookmarked(const BookmarkTree& tree, const std::wstring& url);

// Add a bookmark to the root if not already present. Returns true if added.
bool AddBookmark(BookmarkTree& tree, const std::wstring& title, const std::wstring& url);

// Add a new folder at root. Returns pointer to the new folder.
BookmarkNode* AddFolder(BookmarkTree& tree, const std::wstring& title);

// Remove node at path (root level index or recursive search). Returns true if removed.
bool RemoveNode(BookmarkTree& tree, BookmarkNode* node);

// Move/reorder: move node to be before `before` in `parent`'s children, or append if before is null.
void MoveNode(BookmarkTree& root, BookmarkNode* node, BookmarkNode* newParent, BookmarkNode* before);

// Serialize/deserialize tree. Format: uses indentation or markers for folders.
// Simple format: for each node: "B\t<title>\t<url>\n" for bookmarks, "F\t<title>\n" followed by children indented with "  " or just track depth.
std::wstring SerializeBookmarks(const BookmarkTree& tree);
BookmarkTree DeserializeBookmarks(const std::wstring& text);

// Netscape bookmark file format (standard .html) import/export (FR-BM5).
// Parses a Netscape bookmark HTML file into a BookmarkTree.
BookmarkTree ParseNetscapeBookmarks(const std::wstring& html);
// Serializes a BookmarkTree to Netscape bookmark HTML format.
std::wstring SerializeNetscapeBookmarks(const BookmarkTree& tree);
