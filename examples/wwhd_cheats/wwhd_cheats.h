#pragma once

#include <rplloader/rplloader.h>

namespace Cheats {

struct Config {
    int   sailBoost;
    float jump;
};

extern const RplHost* gHost;
extern Config          gConfig;

extern const RplHook gHooks[];
extern const uint32_t gHookCount;

void LoadConfig(const RplHost& host);

// Put back anything the hooks changed
void ResetHooks();

// The host log wants its own handle back
template <typename... Args>
inline void Log(int level, const char* fmt, Args... args)
{
    if (gHost)
        gHost->log(gHost, level, fmt, args...);
}

} // namespace Cheats
