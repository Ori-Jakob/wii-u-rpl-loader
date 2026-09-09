# rpl-loader

A WUPS plugin that loads `.rpl` files from the SD card into a running Wii U
title and installs the hooks they ask for.

Drop an RPL in `sd:/wiiu/rpl-loader/<title id>/` and it is loaded when that
title starts, before the game runs any of its own code. The RPL says which
addresses it wants to hook and what instruction it expects to find there. The
plugin checks each site, patches the ones that match, and hands the RPL back a
pointer to the code it displaced. Nothing writes to the title's text directly.
Patches go through [libwupatch](../libwupatch), which on Aroma goes through the
FunctionPatcher module.

## What an RPL looks like

One export, `rpl_manifest`, returning a description of the RPL and its hooks.
That is the whole contract.

```c
#include <rplloader/rplloader.h>

// void cCt_Counter(int reset) at link address 0x0200E6EC
RPL_DECL_REPLACE(void, cCt_Counter, int reset)
{
    real_cCt_Counter(reset);
    doSomethingEveryFrame();
}

static const RplHook sHooks[] = {
    RPL_REPLACE(cCt_Counter, 0x0200E6ECu, 0x3D401020u, RPL_HOOK_REQUIRED),
};

static int onInit(const RplHost* host)
{
    host->log(host, RPL_LOG_INFO, "loaded in %s", host->dir);
    return 0;
}

static const RplManifest sManifest = {
    RPL_MAGIC, RPL_ABI_VERSION,
    "my_rpl", "0.1", "me",
    NULL, 0,                     // any title
    sHooks, 1,
    0,                           // priority
    RPL_FLAG_ALLOW_RELEASE,
    onInit, NULL,
    0,                           // maxHooks: the loader default
};

RPL_MANIFEST(sManifest)

int rpl_entry(OSDynLoad_Module module, OSDynLoad_EntryReason reason)
{
    char    name[64];
    int32_t size = sizeof(name);

    OSDynLoad_GetModuleName(module, name, &size);
    OSReport("[example_rpl] %s, reason %d\n", name, (int)reason);
    return 0;
}
```

`rpl_entry` is called by the system loader from inside the `OSDynLoad_Acquire`
that pulled the file in, before the plugin has read the manifest. Put the work
in `onInit`, which the plugin calls once every required hook has been verified.
`reason` is 1 for a load and 2 for an unload; `module` is this RPL's own handle,
for the `OSDynLoad_*` calls that take one.

## Hook types

Three shapes for a site in the title, plus one macro for an export of a system
library. The address-based three take the same first four arguments: a name for
the log, the link-time address, the instruction expected there, and the flags
from the next section.

| Macro | Shape | What happens at the site |
|---|---|---|
| `RPL_REPLACE(name, addr, word, flags)` | `RPL_SHAPE_JUMP` | Branches to your function with LR untouched. At a function's first instruction that makes it a replacement: write a C function with the game function's signature, and `real_<name>` calls the rest of the original. Anywhere else the hook is assembly that never returns. |
| `RPL_HOOK_CALL(label, addr, word, flags, asmHook, realSlot)` | `RPL_SHAPE_CALL` | Enters `asmHook` as if by `bl`, with LR pointing at the instruction after the site. Assembly only: end in `blr`, or branch to the thunk in `realSlot` to run the displaced instruction and carry on. `r11` and CTR are already clobbered on entry; everything else, including CR, must be preserved. |
| `RPL_HOOK_REWRITE(label, addr, word, flags, replacement)` | `RPL_SHAPE_REWRITE` | Runs one instruction of your choosing in place of the site's, then continues at the next one. `RPL_NOP` deletes the instruction. The replacement must be position independent and must not read `r11`. Does not chain. |
| `RPL_REPLACE_LIB(name, module, flags)` | `RPL_SHAPE_JUMP` | Replaces an export of a system library rather than an address in the title. `module` is the RPL name without its `.rpl`, and the C function name is the export. Otherwise it behaves exactly like `RPL_REPLACE`: `real_<name>` is the original. |

`RPL_DECL_REPLACE(ret, name, ...)` goes with `RPL_REPLACE` and declares the pair
of symbols it expects, `my_<name>` for your body and `real_<name>` for the
original, the same shape as WUPS's `DECL_FUNCTION`. A hook that never calls
`real_<name>` replaces the function outright.

### How many hooks

`maxHooks` is the size of the hook table the plugin keeps for this RPL: the
manifest's own hooks plus whatever `onInit` adds through `host->addHook`. Zero
takes the default, 32. Asking for more than the plugin can serve is clamped and
logged rather than refused, and the ceiling is libwupatch's own table, set by
`WUPATCH_MAX_PATCHES` when the plugin is built:

```
make WUPATCH_MAX_PATCHES=255 WUPATCH_MAX_SITES=255
```

Both default to 192 and 160 and neither may exceed 255. A patch and a site each
cost a 64-byte shim slot in the plugin's `.bss`, so the defaults are a size
trade rather than a hard limit.

## Reading the controller

The plugin already replaces `VPADRead` and `KPADReadEx`, so an RPL never hooks
them itself. It reads what the title read:

```c
RplPad pad;
if (host->pad(host, &pad) && (pad.hold & VPAD_BUTTON_ZL))
    doSomething();

RplKpad kpad;
for (uint32_t chan = 0; chan < RPL_KPAD_CHANNELS; ++chan)
    if (host->kpad(host, chan, &kpad) && kpad.extension == WPAD_EXT_PRO_CONTROLLER)
        useProButtons(kpad.hold);
```

`RplPad` is the GamePad in `VPADButtons` bits. `RplKpad` is one KPAD channel,
and `extension` says which raw button set `hold` came from: Pro and Classic
report their own bits, anything else reports 0. Both carry a `sample` counter
and report false until the title has read that controller at least once.
`RplPad::touch` is laid out like `VPADTouchData`, so it goes straight to
`VPADGetTPCalibratedPoint`.

Two calls change what the title sees rather than just observing it.
`host->setInputMode(host, RPL_INPUT_BLOCK)` makes every controller read as
idle, which is what an overlay holds while it has the controller;
`RPL_INPUT_PASS` gives it back. `host->setStick(host, xy)` drives the left
stick in the title's place from two floats in -1..1, and NULL stops. Both
survive until they are changed, and both are cleared when the title exits, so
an RPL that leaves one set does not affect the next one.

## Hook flags

The fourth argument to every hook macro. Combine with `|`.

| Flag | Effect |
|---|---|
| `RPL_HOOK_OPTIONAL` | The default, value 0. A hook that cannot be applied is logged and skipped, and the RPL runs without it. |
| `RPL_HOOK_REQUIRED` | If this hook is not applied, the whole RPL is unlinked before `onInit` runs. Hooks of the same RPL that did apply are removed again first. |
| `RPL_HOOK_EXCLUSIVE` | Refuse to share the site with another RPL. Without it, hooks of the same shape at one address chain. |

One flag lives on the manifest rather than on a hook:

| Flag | Effect |
|---|---|
| `RPL_FLAG_ALLOW_RELEASE` | Lets the plugin unload this RPL with `OSDynLoad_Release` when it fails. Without it a failed RPL stays loaded and inert. |

### Replacing a system library function

An address only means something inside the title. A `bl` to `GX2SetContextState`
is not one: the call sites in the RPX are placeholders that the Cafe loader
fills in when it resolves the import, so the word a disassembler shows there is
not the word the console runs, and there is nothing to declare.

`RPL_REPLACE_LIB` takes the module and the export name instead, the way a WUPS
plugin does. The plugin looks the export up with `OSDynLoad_FindExport` in the
running process and submits it to the FunctionPatcher by library and name, so
the patch is on the function itself and every caller in the process goes through
it, wherever the call is.

```c
RPL_DECL_REPLACE(void, GX2SetContextState, GX2ContextState* state)
{
    noteTheGamesContext(state);
    real_GX2SetContextState(state);
}

static const RplHook sHooks[] = {
    RPL_REPLACE_LIB(GX2SetContextState, "gx2", RPL_HOOK_REQUIRED),
};
```

`RPL_REPLACE_LIB_AS(name, module, export, flags)` is the same thing when the
export is not spelled the way your C function is.

Two limits come with it. The shape has to be `RPL_SHAPE_JUMP`, because the only
thing known about the address is that it is a function entry. And these patches
carry no title-ID gate, so they are removed when the title exits rather than
gated on the way in; a title that goes down without `ON_APPLICATION_ENDS`
running would leave one behind.

Calling the export by name from inside your own hook comes straight back through
it. Use `real_<name>`, and check it: if the function's first instruction cannot
run from a stub there is no original to call, `real_<name>` stays NULL and the
log says so.

### Naming a site

`0x0200E6EC` is a link-time address, the one a disassembler shows, and
`0x3D401020` is the instruction the RPX file holds there. The plugin trusts
neither on its own. It reads the loader's text and data offsets, works out where
the site landed at runtime, reads the word that is actually there and compares
it against the one declared, allowing for the forms the loader rewrites an
instruction into when it relocates a segment. A site that does not match is
refused and logged with both words, so a wrong address reads as a log line
rather than a crash.

## What happens when a title starts

WUPS calls the plugin at `ON_APPLICATION_START`, inside the new process, before
the title's `main`. `Rpl::Loader::OnApplicationStart()` in
`src/rplloader/rpl_loader.cpp` runs the whole sequence there.

It reads its settings, logs which build of the plugin is running, and scans
`fs:/vol/external01/wiiu/rpl-loader/<title id>/` for `.rpl` files, sorted by
name. If the safe-mode buttons are held it stops there and injects nothing.
Otherwise it finds the title's `.rpx` in `OSDynLoad_GetRPLInfo`, takes the text
and data offsets from it, and configures libwupatch for this process and this
title ID.

Then each RPL in turn: `OSDynLoad_Acquire` with a `~/` path, which Aroma's base
module and Mocha's IOSU patch turn into a read from the SD card;
`OSDynLoad_FindExport` for `rpl_manifest`; a check of the magic, ABI version and
title gate; then the hook table is declared, applied and verified, and `onInit`
is called. `Rpl::Linker` owns the loading half, `Rpl::Patcher` the libwupatch
half.

Each RPL is declared and ticked on its own rather than all of them together, so
when a hook fails the rollback takes out that RPL's hooks and nobody else's.

## When a hook cannot be applied

A hook marked `RPL_HOOK_REQUIRED` that does not end up applied takes its whole
RPL down. Everything that RPL did manage to apply is removed first, its sites
restored and its descriptors retired, so nothing in the title still branches
into the module. Then, if the manifest sets `RPL_FLAG_ALLOW_RELEASE` and the
`release_failed` setting is on, the module is unloaded with `OSDynLoad_Release`.
Otherwise it stays loaded and inert.

A failed RPL is unlinked before `onInit` ever runs, so it has had no chance to
start a thread or register a callback that would outlive it. That is why
`release_failed` defaults on while the manifest flag stays a per-RPL decision.

Either way the title boots. `tests/release_test` covers the whole path.

## Several RPLs on one function

Two RPLs that hook the same address do not fight. Their hooks chain: the site is
patched once, each hook's `real_` pointer leads to the next one, and the last
leads to the displaced instruction. `priority` in the manifest orders them,
higher first, ties broken by load order.

An RPL that needs a function to itself sets `RPL_HOOK_EXCLUSIVE` on that hook.
Whoever gets there first keeps the site, because priority orders a chain rather
than evicting an installed hook. A later RPL's hook reports `collided`, which
for a required hook means that RPL unloads itself while the first keeps running.

`tests/chain_test` builds two RPLs from one source to cover both cases.

## Building

devkitPPC, wut, WUPS, and the WUMS libraries `libfunctionpatcher`,
`libmappedmemory` and `libkernel`. `libnotifications` is used if it is installed
and skipped if it is not. From devkitPro's MSYS2 shell:

```sh
git submodule update --init     # once, for libwupatch
make                            # rpl_loader.wps
make DEBUG=1                    # info-level logging; DEBUG=VERBOSE for more
make LIBWUPATCH=path/to/libwupatch
cd examples/wwhd_cheats && make
cd examples/wwhd_mss && make
cd tests && make
```

`examples/wwhd_mss/Cemu` contains the graphics pack and setup instructions for
using the same `wwhd_mss.rpl` build under Cemu.

### Module names and the stock FunctionPatcher

Aroma's stock FunctionPatcher module dereferences the name of every loaded
module while it looks for the title's `.rpx`, in one place, without checking for
NULL. An RPL is listed without a name unless something puts one there, and that
line is then `strlen(NULL)` inside the title.

`tools/rplname.py` is what puts one there, so RPLs built by these Makefiles do
not trip it and the stock module handles them. On top of that the plugin will
not submit a patch at all while an unnamed module is loaded, so even something
else loading one gives a refused hook and a log line rather than a crash.

An RPL needs three things beyond wut, and the example Makefiles do all of them.

The entry code in `crt/`, linked with `-specs=crt/rplloader.specs`, instead of
wut's `crt0_rpl.s`.

`-msdata=none`, because `r2` and `r13` belong to the title and an RPL that
reaches its own globals through them corrupts the game.

`tools/rplname.py` after `elf2rpl`, which writes the RPL's file name into the
file-info section and fixes the section CRC. That name is what the loader lists
the module under, and `elf2rpl` leaves it empty.

An RPL can be C or C++. `examples/wwhd_cheats` is C++, the tests are C. In C++,
`rpl_manifest` and `rpl_entry` have to keep their C names, which `RPL_MANIFEST`
and `RPL_EXPORT` take care of.

## Deploying

```sh
make deploy WIIU_IP=192.168.1.50        # plugin, via wiiload
make deploy-ftp WIIU_IP=192.168.1.50    # plugin, via ftpiiu
cd examples/wwhd_cheats && make deploy WIIU_IP=192.168.1.50
```

`deploy` uses devkitPro's `wiiload`, which needs the wiiload plugin on the
console; it replaces the running plugin and relaunches the title. `deploy-ftp`
copies the `.wps` into `sd:/wiiu/environments/aroma/plugins/` over FTP, which
needs the ftpiiu plugin.

A `.wps` replaced over FTP does not take effect until the console reboots,
because Aroma caches plugins at boot. Relaunching the title is not enough. The
plugin logs its own build as the first line of every title's log, so a log can
always be matched to the binary that produced it:

```
[rpl-loader] build Sep  3 2026 16:17:59
```

RPLs are not cached. The plugin reads them itself every time a title starts, so
those are always current.

## Settings

The plugin's page in Aroma's config menu, stored under `rpl_loader`. All of them
take effect on the next title launch.

| Setting | Default | Effect |
|---|---|---|
| `enabled` | on | Turns injection off entirely. |
| `notify` | on | On-screen messages. |
| `log_level` | info | Off, errors, info or verbose. |
| `dry_run` | off | Loads each RPL and reports what each site would do, without patching anything. |
| `release_failed` | on | Unloads a failed RPL rather than leaving it resident. The manifest still has to allow it. |
| `mapped_allocator` | on | Swaps the dynload allocator around the load so an RPL's `.data` and `.bss` come from MemoryMappingModule instead of the title's heap. |
| `physical_patch` | off | Submits patches to the FunctionPatcher by physical address rather than by executable name, which skips the name lookup a stock module can crash on. Such patches are not gated by title ID, so a title that exits without its hooks being removed leaves one behind. |
| `safe_combo` | L+R+ZL+ZR | Buttons that skip injection when held at launch. The way out of an RPL that hangs a title. |

Under the title's own category there is a switch per `.rpl` found, and a
sub-category per loaded RPL listing each hook, its state, and its position in
any chain.

## File names on the SD card

```
sd:/wiiu/environments/aroma/plugins/rpl_loader.wps
sd:/wiiu/rpl-loader/0005000010143500/autowind.rpl
```

The stem may be at most 22 characters and may not contain a dot; files breaking
either rule are skipped with a log line saying which. The limit comes from the
load path. Aroma's base module copies the acquire name into a 64-byte buffer,
coreinit cuts a module name at its first dot and caps it at 63 characters, and
`~/wiiu/rpl-loader/<16 hex digits>/` already spends 37 of them. At most 16 RPLs
are scanned per title.

## Layout

`src/main.cpp` holds the WUPS entry points and nothing else. The implementation
is in `src/rplloader/`, with its headers mirrored in `include/rplloader/`.
`rpl_loader.cpp` is the sequence above; the rest are the pieces it calls:
`rpl_linker` for loading, `rpl_patcher` for libwupatch, `rpl_library` for
resolving a system-library export, `rpl_scan` for the directory, `rpl_config`
for settings and the menu, `rpl_host` for the API handed to RPLs, `rpl_input`
for the pad mirror, plus logging and notifications.

`include/rplloader/rplloader.h` is the only header an RPL includes. It is C, has
no dependency on wut or libwupatch, and sits alongside the internal headers
because both are addressed as `rplloader/...`; an RPL project only ever includes
the one.
