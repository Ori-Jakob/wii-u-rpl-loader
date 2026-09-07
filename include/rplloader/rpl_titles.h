#pragma once

#include <stdint.h>

namespace Rpl {
namespace Titles {

// A plugin may carry the titles it supports in an RPL section, so the loader
// can decide from the file on the SD card whether to load it at all. See
// RPL_TITLES_SECTION in rplloader.h for the layout.
static const int kMaxIds = 8;

struct List {
    uint64_t ids[kMaxIds];
    int      count;      // ids actually stored
    int      declared;   // ids the section claimed, before kMaxIds clamped it
    bool     any;        // an RPL_TITLE_ANY entry was present
};

enum Result {
    ABSENT = 0,   // no section, or it could not be read: load and let the
                  // manifest decide, exactly as before
    MATCH,        // this title is listed, or the list is a wildcard
    NO_MATCH,     // the section is good and this title is not in it
};

Result Check(const char* path, uint64_t titleId, List* out);

// "0005000010143500, 0005000010143600, +2 more" for a log line
void Describe(const List& list, char* out, int cap);

} // namespace Titles
} // namespace Rpl
