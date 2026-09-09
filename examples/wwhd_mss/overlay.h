#pragma once

#include <gx2/context.h>
#include <gx2/enum.h>
#include <gx2/surface.h>

#include <rplloader/rplloader.h>

#include "macro_engine.h"

namespace Mss {
namespace Overlay {

void Init();
void BindHost(const RplHost* host);
void Shutdown();
void OnReleaseForeground();
void OnAcquiredForeground();

void Tick(bool comboHeld, bool macroActive, const MacroOutput& output);
bool IsMenuVisible();

using CopyFn = void (*)(const GX2ColorBuffer*, GX2ScanTarget);
void OnPresent(CopyFn copy, const GX2ColorBuffer* buffer, GX2ScanTarget target);
void NoteGameContext(GX2ContextState* state);

} // namespace Overlay
} // namespace Mss
