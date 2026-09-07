#include "rplloader/rpl_titles.h"

#include "rplloader/rplloader.h"

#include <stdio.h>
#include <string.h>

namespace Rpl {
namespace Titles {

// The file is big-endian and so is the CPU, so nothing is swapped here.
static const uint32_t kShtRplFileInfo = 0x80000004u;
static const uint32_t kShfDeflated    = 0x08000000u;
static const int      kShEntSize      = 0x28;
static const uint32_t kFileInfoSize   = 0x60u;

static bool readAt(FILE* f, long off, void* dst, size_t len)
{
    return fseek(f, off, SEEK_SET) == 0 && fread(dst, 1, len, f) == len;
}

Result Check(const char* path, uint64_t titleId, List* out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!path)
        return ABSENT;

    FILE* f = fopen(path, "rb");
    if (!f)
        return ABSENT;

    Result result = ABSENT;
    uint8_t ident[0x34];

    if (!readAt(f, 0, ident, sizeof(ident)) ||
        memcmp(ident, "\x7f" "ELF", 4) != 0 || ident[5] != 2)
        goto done;

    {
        const long     shoff     = (long)*(const uint32_t*)(ident + 0x20);
        const uint16_t shentsize = *(const uint16_t*)(ident + 0x2E);
        const uint16_t shnum     = *(const uint16_t*)(ident + 0x30);

        if (shentsize != kShEntSize || shnum == 0 || shoff == 0)
            goto done;

        // The list rides in the file-info section, found by TYPE - no string
        // table to consult and no compressed section to inflate.
        for (uint16_t i = 0; i < shnum; ++i) {
            uint32_t sh[10];
            if (!readAt(f, shoff + (long)i * kShEntSize, sh, sizeof(sh)))
                goto done;
            if (sh[1] != kShtRplFileInfo || (sh[2] & kShfDeflated))
                continue;
            if (sh[5] < kFileInfoSize + 16u)
                goto done;   // no room for a list: an older build

            const long base = (long)sh[4] + (long)kFileInfoSize;
            uint32_t head[4];
            if (!readAt(f, base, head, sizeof(head)))
                goto done;
            if (head[0] != RPL_TITLES_MAGIC || head[1] != 1u)
                goto done;   // built before this existed

            const uint32_t room = (sh[5] - kFileInfoSize - 16u) / 8u;
            uint32_t n = head[2] < room ? head[2] : room;
            if (n > (uint32_t)kMaxIds)
                n = (uint32_t)kMaxIds;

            bool matched = false, any = false;
            for (uint32_t k = 0; k < n; ++k) {
                uint64_t id = 0;
                if (!readAt(f, base + 16 + (long)k * 8, &id, sizeof(id)))
                    goto done;
                if (out)
                    out->ids[out->count++] = id;
                if (id == RPL_TITLE_ANY)
                    any = true;
                else if (id == titleId)
                    matched = true;
            }
            if (out) {
                out->declared = (int)head[2];
                out->any = any;
            }
            result = (matched || any) ? MATCH : NO_MATCH;
            goto done;
        }
    }

done:
    fclose(f);
    return result;
}

void Describe(const List& list, char* out, int cap)
{
    if (!out || cap <= 0)
        return;
    out[0] = '\0';
    if (list.any) {
        snprintf(out, (size_t)cap, "every title");
        return;
    }

    int used = 0;
    for (int i = 0; i < list.count && used < cap - 1; ++i) {
        const int n = snprintf(out + used, (size_t)(cap - used), "%s%016llX",
                               i ? ", " : "", (unsigned long long)list.ids[i]);
        if (n < 0 || n >= cap - used) {
            used = cap - 1;
            break;
        }
        used += n;
    }
    if (list.declared > list.count && used < cap - 1)
        snprintf(out + used, (size_t)(cap - used), ", +%d more",
                 list.declared - list.count);
}

} // namespace Titles
} // namespace Rpl
