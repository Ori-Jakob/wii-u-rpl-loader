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

} // namespace Log
} // namespace Rpl
