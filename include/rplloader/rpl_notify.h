#pragma once

namespace Rpl {
namespace Notify {

void Init();
void Deinit();
void SetEnabled(bool enabled);

void Info(const char* text);
void Error(const char* text);

} // namespace Notify
} // namespace Rpl
