#pragma once

void ImGuiRplAssert(const char* expression, const char* file, int line);
#define IM_ASSERT(expression)                                                   \
    do {                                                                        \
        if (!(expression))                                                      \
            ImGuiRplAssert(#expression, __FILE__, __LINE__);                    \
    } while (0)

#define IMGUI_DISABLE_DEMO_WINDOWS
#define IMGUI_DISABLE_DEBUG_TOOLS
#define IMGUI_DISABLE_FILE_FUNCTIONS
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
