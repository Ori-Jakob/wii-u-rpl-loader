#include "rplloader/rpl_log.h"

#include <stdio.h>
#include <string.h>

#include <whb/log.h>
#include <whb/log_cafe.h>
#include <whb/log_module.h>

namespace Rpl {
namespace Log {

static int  s_level = INFO;
static bool s_module = false;
static bool s_cafe = false;

void Init()
{
    // The LoggingModule handle does not survive a title switch
    if (!s_module)
        s_module = WHBLogModuleInit();
    if (!s_module && !s_cafe)
        s_cafe = WHBLogCafeInit();
}

void Deinit()
{
    if (s_module) {
        WHBLogModuleDeinit();
        s_module = false;
    }
    // WHBLogCafeInit has no deinit worth calling
}

void SetLevel(int level)
{
    if (level < OFF) level = OFF;
    if (level > VERBOSE) level = VERBOSE;
    s_level = level;
}

int GetLevel() { return s_level; }

static const char* tagFor(int level)
{
    switch (level) {
    case ERROR:   return "[rpl-loader][E] ";
    case WARN:    return "[rpl-loader][W] ";
    case VERBOSE: return "[rpl-loader][V] ";
    default:      return "[rpl-loader] ";
    }
}

void VPrintf(int level, const char* prefix, const char* fmt, va_list args)
{
    if (level > s_level && level != ERROR)
        return;
    char line[256];
    int used = snprintf(line, sizeof(line), "%s%s%s", tagFor(level),
                        prefix ? prefix : "", prefix ? ": " : "");
    if (used < 0 || used >= (int)sizeof(line))
        return;
    vsnprintf(line + used, sizeof(line) - (size_t)used, fmt, args);
    WHBLogPrint(line);
}

void Printf(int level, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    VPrintf(level, 0, fmt, args);
    va_end(args);
}

void WuPatchSink(int wupatchLevel, const char* line)
{
    // libwupatch levels: 0 info, 1 warn, 2 error, and it formats its own tag
    const int level = wupatchLevel >= 2 ? ERROR : wupatchLevel == 1 ? WARN : INFO;
    if (level > s_level && level != ERROR)
        return;
    WHBLogPrint(line);
}

} // namespace Log
} // namespace Rpl
