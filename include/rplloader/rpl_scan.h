#pragma once

#include <stdint.h>

namespace Rpl {
namespace Scan {

static const int kMaxEntries = 16;
static const int kNameChars  = 48;
// "~/wiiu/rpl-loader/<16 hex>/" is 37 chars, so 63 - 37 - ".rpl" leaves 22
static const int kMaxStem    = 22;

struct Entry {
    char file[kNameChars];   // as found, e.g. "autowind.rpl"
    char stem[kNameChars];   // "autowind"
    bool nameTooLong;
    bool stemHasDot;
    bool universal;          // came from the shared folder, not a title folder
};

// .rpl files in dir sorted by name, flagged when they break a name rule
int ScanDir(const char* dir, Entry* out, int cap);

// fs:/vol/external01/wiiu/rpl-loader/<tid>/
void TitleDir(uint64_t titleId, char* out, int cap);

// fs:/vol/external01/wiiu/rpl-loader/universal/ - scanned for every title, with
// each plugin's own title section deciding whether it is actually for this one
void UniversalDir(char* out, int cap);

// The trailing path element, for building an OSDynLoad acquire name
void DirLeaf(const Entry& e, uint64_t titleId, char* out, int cap);

static const char* const kUniversalLeaf = "universal";

} // namespace Scan
} // namespace Rpl
