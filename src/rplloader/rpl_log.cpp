#include "rplloader/rpl_log.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <coreinit/time.h>
#include <coreinit/title.h>

#include <whb/log.h>
#include <whb/log_cafe.h>
#include <whb/log_module.h>

namespace Rpl {
namespace Log {

static int  s_level = INFO;
static bool s_module = false;
static bool s_cafe = false;

static const char* const kLogDir = "fs:/vol/external01/wiiu/rpl-loader/logs";

static FILE* s_file = 0;
static char  s_filePath[160] = "";
static bool  s_fileWanted = false;
static bool  s_fileTried = false;

static void openFile()
{
    if (s_fileTried || !s_fileWanted)
        return;
    s_fileTried = true;

    mkdir(kLogDir, 0777);   // already there is fine

    // One file per title, truncated at every boot. A file per session reads
    // more tidily but quietly fills the card, and the log that matters is
    // almost always the one from the run that just happened.
    snprintf(s_filePath, sizeof(s_filePath), "%s/%016llX.log", kLogDir,
             (unsigned long long)OSGetTitleID());

    s_file = fopen(s_filePath, "w");
    if (!s_file) {
        s_filePath[0] = 0;
        return;
    }

    // The name no longer carries the date, so the first line does.
    OSCalendarTime ct;
    OSTicksToCalendarTime(OSGetTime(), &ct);
    char head[96];
    snprintf(head, sizeof(head),
             "---- session %04d-%02d-%02d %02d:%02d:%02d ----",
             ct.tm_year, ct.tm_mon + 1, ct.tm_mday, ct.tm_hour, ct.tm_min,
             ct.tm_sec);
    fputs(head, s_file);
    fputc(10, s_file);
    fflush(s_file);
}

static void emit(const char* line)
{
    WHBLogPrint(line);
    if (!s_file)
        openFile();
    if (s_file) {
        fputs(line, s_file);
        fputc('\n', s_file);
        fflush(s_file);
    }
}

void SetFileLogging(bool enabled)
{
    if (enabled == s_fileWanted)
        return;
    s_fileWanted = enabled;
    if (!enabled && s_file) {
        fclose(s_file);
        s_file = 0;
        s_filePath[0] = '\0';
    }
    if (enabled)
        s_fileTried = false;   // a fresh file for the next line
}

const char* FilePath() { return s_filePath; }

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
    if (s_file) {
        fclose(s_file);
        s_file = 0;
    }
    s_fileTried = false;
    s_filePath[0] = '\0';
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
    emit(line);
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
    emit(line);
}

} // namespace Log
} // namespace Rpl
