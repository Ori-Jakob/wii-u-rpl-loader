#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RPL_MAGIC        0x52504C4Cu   /* 'RPLL' */
#define RPL_ABI_VERSION  5u
#define RPL_TITLES_MAGIC   0x5250544Cu   /* 'RPTL' */
#define RPL_TITLES_SECTION ".rpltitles"
#define RPL_TITLE_ANY      0xFFFFFFFFFFFFFFFFull

typedef enum RplShape {
    // At a function entry this is a plain replacement, *original is the rest of it
    RPL_SHAPE_JUMP = 0,
    // Entered as if by bl, LR = site+4. Assembly only, clobbers r11 and CTR
    RPL_SHAPE_CALL = 1,
    // Replaces the site's word. Position independent, must not read r11, never chains
    RPL_SHAPE_REWRITE = 2,
} RplShape;

enum {
    RPL_HOOK_OPTIONAL  = 0u,
    // Unlink the whole RPL if this one fails
    RPL_HOOK_REQUIRED  = 1u << 0,
    // Refuse to share the site
    RPL_HOOK_EXCLUSIVE = 1u << 1,
};

typedef enum RplHookState {
    RPL_STATE_DECLARED = 0,
    RPL_STATE_BLOCKED,   // title, deltas or patcher not ready
    RPL_STATE_REFUSED,   // the live word is not the expected instruction
    RPL_STATE_COLLIDED,  // another hook holds the site and will not share
    RPL_STATE_APPLIED,
    RPL_STATE_FAILED,    // the backend refused; see the log
} RplHookState;

typedef struct RplHook {
    const char* name;
    uint32_t    linkAddr;     // link-time address, as a disassembler shows it
    uint32_t    expected;     // the word the RPX image holds there
    uint32_t    shape;        // RplShape
    uint32_t    flags;        // RPL_HOOK_*
    const void* hook;         // JUMP / CALL target; NULL for REWRITE
    uint32_t    replacement;  // REWRITE word
    void**      original;     // receives the original thunk; may be NULL

    // An export of a system library instead of an address in the title. The
    // loader resolves it in the running process, so linkAddr and expected are
    // ignored and the shape has to be JUMP. See RPL_REPLACE_LIB.
    const char* module;       // "gx2", "coreinit", "vpad", ... without the .rpl
    const char* function;     // the exported name
} RplHook;

enum { RPL_LOG_ERROR = 0, RPL_LOG_WARN = 1, RPL_LOG_INFO = 2, RPL_LOG_VERBOSE = 3 };
enum { RPL_NOTIFY_INFO = 0, RPL_NOTIFY_ERROR = 1 };

// Laid out like VPADTouchData so it can be handed straight to
// VPADGetTPCalibratedPoint, which is what turns it into screen coordinates.
typedef struct RplTouch {
    uint16_t x, y;
    uint16_t touched;
    uint16_t validity;
} RplTouch;

// The GamePad as the title last read it, VPADButtons bits and sticks -1..1
typedef struct RplPad {
    uint32_t hold;
    uint32_t trigger;
    uint32_t release;
    float    leftX, leftY;
    float    rightX, rightY;
    RplTouch touch;
    uint32_t sample;     // counts the title's reads; 0 until the first
} RplPad;

#define RPL_KPAD_CHANNELS 4u

// One KPAD channel as the title last read it. `extension` is a
// WPADExtensionType and says which raw button set `hold` was taken from:
// WPAD_EXT_PRO_CONTROLLER, WPAD_EXT_CLASSIC or WPAD_EXT_MPLUS_CLASSIC. Any
// other extension reports buttons of 0.
typedef struct RplKpad {
    uint32_t extension;
    uint32_t hold;
    uint32_t trigger;
    uint32_t release;
    float    leftX, leftY;
    float    rightX, rightY;
    uint32_t sample;
} RplKpad;

enum {
    RPL_INPUT_PASS  = 0,   // the title reads the pad as it is
    RPL_INPUT_BLOCK = 1,   // the title reads nothing, on every controller
};

// Every call takes its own host back so the loader knows who is asking
typedef struct RplHost RplHost;
struct RplHost {
    uint32_t    version;
    const char* name;
    const char* dir;      // this RPL's own directory on the SD card
    void*       impl;

    void     (*log)(const RplHost* h, int level, const char* fmt, ...);
    void     (*notify)(const RplHost* h, int kind, const char* text);

    uint32_t (*original)(const RplHost* h, const RplHook* hook);
    int      (*state)(const RplHost* h, const RplHook* hook);
    // One more hook now, negative on failure. The RplHook must outlive the process
    int      (*addHook)(const RplHost* h, const RplHook* hook);
    int      (*removeHook)(const RplHost* h, const RplHook* hook);

    // Mapped memory, not the title's heap
    void*    (*alloc)(const RplHost* h, uint32_t size, uint32_t align);
    void     (*free)(const RplHost* h, void* p);

    uint64_t (*titleId)(const RplHost* h);
    uint32_t (*textDelta)(const RplHost* h);   // runtime - link-time
    uint32_t (*dataDelta)(const RplHost* h);

    int      (*getBool)(const RplHost* h, const char* key, int def);
    int      (*setBool)(const RplHost* h, const char* key, int value);

    // Reading the pad directly steals samples the title expects
    int      (*pad)(const RplHost* h, RplPad* out);
    int      (*kpad)(const RplHost* h, uint32_t chan, RplKpad* out);

    // What the title sees on its next read. RPL_INPUT_BLOCK is what an overlay
    // holds while it has the controller; the loader still reports every sample.
    void     (*setInputMode)(const RplHost* h, int mode);
    // Two floats -1..1 driving the left stick in the title's place, or NULL to
    // hand it back. Applies on every controller, and survives BLOCK.
    void     (*setStick)(const RplHost* h, const float* leftXY);
};

enum {
    // Let a failed RPL be unloaded instead of left resident
    RPL_FLAG_ALLOW_RELEASE = 1u << 0,
};

typedef struct RplManifest {
    uint32_t        magic;
    uint32_t        abiVersion;
    const char*     name;
    const char*     version;
    const char*     author;
    const uint64_t* titleIds;      // NULL means any title
    uint32_t        titleIdCount;
    const RplHook*  hooks;
    uint32_t        hookCount;
    int32_t         priority;      // higher runs earlier when RPLs share a site
    uint32_t        flags;
    int  (*onInit)(const RplHost* host);   // 0 to stay loaded
    void (*onDeinit)(void);                // may be NULL
    // Room for the table plus whatever onInit adds. 0 takes the loader's
    // default; more than it can serve is clamped, and the log says so.
    uint32_t        maxHooks;
    // The HOME menu took the screen, and gave it back. GPU resources do not
    // survive the trip: drop them in the first and rebuild them lazily after
    // the second. Both may be NULL.
    void (*onReleaseForeground)(void);
    void (*onAcquiredForeground)(void);

    // Called from inside VPADRead, after this RPL's sample has been published
    // and BEFORE the title's buffers are edited. setInputMode() from here takes
    // effect on THIS read instead of the next one, which is the only way to
    // stop the frame a combo completes from also reaching the title.
    //
    // It runs on the title's own input path, so keep it short and do not call
    // back into VPAD. May be NULL.
    void (*onPadSampled)(void);
} RplManifest;

typedef const RplManifest* (*RplManifestFn)(void);
#define RPL_MANIFEST_EXPORT "rpl_manifest"

// Both are looked up by name so neither may be mangled
#ifdef __cplusplus
#define RPL_EXPORT extern "C"
#else
#define RPL_EXPORT
#endif

// Declares real_<name> and my_<name> like WUPS's DECL_FUNCTION
#define RPL_DECL_REPLACE(ret, name, ...)           \
    static ret (*real_##name)(__VA_ARGS__) = 0;    \
    static ret my_##name(__VA_ARGS__)

#define RPL_REPLACE(name, linkAddr, expectedWord, hookFlags)                \
    { #name, (linkAddr), (expectedWord), RPL_SHAPE_JUMP, (hookFlags),       \
      (const void*)&my_##name, 0u, (void**)&real_##name, 0, 0 }

// Replace an export of a system library, the way a WUPS plugin replaces one.
// The C name is the exported name, so RPL_DECL_REPLACE takes it verbatim:
//   RPL_DECL_REPLACE(void, GX2SetContextState, GX2ContextState* s) { ... }
//   RPL_REPLACE_LIB(GX2SetContextState, "gx2", RPL_HOOK_REQUIRED)
#define RPL_REPLACE_LIB(name, moduleName, hookFlags)                        \
    { #name, 0u, 0u, RPL_SHAPE_JUMP, (hookFlags),                           \
      (const void*)&my_##name, 0u, (void**)&real_##name, (moduleName), #name }

// When the export is not spelled the way the C function is
#define RPL_REPLACE_LIB_AS(name, moduleName, exportName, hookFlags)         \
    { #name, 0u, 0u, RPL_SHAPE_JUMP, (hookFlags),                           \
      (const void*)&my_##name, 0u, (void**)&real_##name, (moduleName), (exportName) }

// asmHook honours the CALL contract, realSlot takes the thunk or NULL
#define RPL_HOOK_CALL(label, linkAddr, expectedWord, hookFlags, asmHook, realSlot) \
    { label, (linkAddr), (expectedWord), RPL_SHAPE_CALL, (hookFlags),             \
      (const void*)(asmHook), 0u, (void**)(realSlot), 0, 0 }

#define RPL_HOOK_REWRITE(label, linkAddr, expectedWord, hookFlags, word) \
    { label, (linkAddr), (expectedWord), RPL_SHAPE_REWRITE, (hookFlags), \
      (const void*)0, (word), (void**)0, 0, 0 }

#define RPL_NOP 0x60000000u   /* ori r0, r0, 0 */

#define RPL_MANIFEST(manifestVar)                              \
    RPL_EXPORT const RplManifest* rpl_manifest(void) { return &(manifestVar); }

#ifdef __cplusplus
} /* extern "C" */
#endif
