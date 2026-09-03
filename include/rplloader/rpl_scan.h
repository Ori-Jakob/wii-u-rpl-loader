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
};

// .rpl files in dir sorted by name, flagged when they break a name rule
int ScanDir(const char* dir, Entry* out, int cap);

// fs:/vol/external01/wiiu/rpl-loader/<tid>/
void TitleDir(uint64_t titleId, char* out, int cap);

} // namespace Scan
} // namespace Rpl
