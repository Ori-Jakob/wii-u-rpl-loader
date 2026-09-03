#include "rplloader/rpl_scan.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

namespace Rpl {
namespace Scan {

static bool endsWithRpl(const char* name, size_t len)
{
    return len > 4 && strcasecmp(name + len - 4, ".rpl") == 0;
}

void TitleDir(uint64_t titleId, char* out, int cap)
{
    snprintf(out, (size_t)cap, "fs:/vol/external01/wiiu/rpl-loader/%016llX/",
             (unsigned long long)titleId);
}

int ScanDir(const char* dir, Entry* out, int cap)
{
    DIR* d = opendir(dir);
    if (!d)
        return 0;

    int n = 0;
    struct dirent* e;
    while ((e = readdir(d)) != 0 && n < cap) {
        const size_t len = strlen(e->d_name);
        if (!endsWithRpl(e->d_name, len) || len >= (size_t)kNameChars)
            continue;
        Entry& entry = out[n];
        memset(&entry, 0, sizeof(entry));
        memcpy(entry.file, e->d_name, len + 1);   // len < kNameChars, checked above
        memcpy(entry.stem, e->d_name, len - 4);
        entry.stem[len - 4] = '\0';
        entry.nameTooLong = (len - 4) > (size_t)kMaxStem;
        entry.stemHasDot = strchr(entry.stem, '.') != 0;
        ++n;
    }
    closedir(d);

    // Load order is chain order at equal priority, so sort like a file browser
    for (int i = 1; i < n; ++i) {
        Entry value = out[i];
        int j = i - 1;
        while (j >= 0 && strcasecmp(value.file, out[j].file) < 0) {
            out[j + 1] = out[j];
            --j;
        }
        out[j + 1] = value;
    }
    return n;
}

} // namespace Scan
} // namespace Rpl
