#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "wwhd_cheats.h"

namespace Cheats {

Config gConfig = { 3, 36.0f };

namespace {

// Value after key=, or nullptr
const char* ValueFor(const char* line, const char* key)
{
    const std::size_t n = std::strlen(key);
    return std::strncmp(line, key, n) == 0 ? line + n : nullptr;
}

} // namespace

void LoadConfig(const RplHost& host)
{
    char path[128];
    // host.dir is this RPL's own directory
    std::snprintf(path, sizeof(path), "%swwhd_cheats.cfg", host.dir);

    std::FILE* file = std::fopen(path, "r");
    if (!file) {
        Log(RPL_LOG_INFO, "no cfg, using sail x%d jump %.0f",
            gConfig.sailBoost, double(gConfig.jump));
        return;
    }

    int applied = 0;
    char line[64];
    while (std::fgets(line, sizeof(line), file)) {
        if (const char* v = ValueFor(line, "sail=")) {
            const int boost = int(std::strtol(v, nullptr, 10));
            if (boost >= 1 && boost <= 10) {
                gConfig.sailBoost = boost;
                ++applied;
            }
        } else if (const char* v = ValueFor(line, "jump=")) {
            const float jump = std::strtof(v, nullptr);
            if (jump > 0.0f && jump <= 200.0f) {
                gConfig.jump = jump;
                ++applied;
            }
        }
    }
    std::fclose(file);

    Log(RPL_LOG_INFO, "%s: %d setting(s), sail x%d jump %.0f",
        path, applied, gConfig.sailBoost, double(gConfig.jump));
}

} // namespace Cheats
