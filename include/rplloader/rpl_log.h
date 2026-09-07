#pragma once

#include <stdarg.h>

namespace Rpl {
namespace Log {

enum Level { OFF = 0, ERROR = 1, WARN = 2, INFO = 3, VERBOSE = 4 };

void Init();
void Deinit();

void SetLevel(int level);
int  GetLevel();

void Printf(int level, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
// Prefixed line for an RPL's own messages
void VPrintf(int level, const char* prefix, const char* fmt, va_list args);

// The sink libwupatch is given
void WuPatchSink(int level, const char* line);

// Mirror every line into sd:/wiiu/rpl-loader/logs/<title id>-<stamp>.log.
// The file is opened on the first line written after this is turned on, so the
// title id and the timestamp are both the running session's.
void SetFileLogging(bool enabled);
const char* FilePath();   // "" when nothing is being written

} // namespace Log
} // namespace Rpl
