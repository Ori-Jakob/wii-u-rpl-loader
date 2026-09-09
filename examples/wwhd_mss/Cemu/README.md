# Cemu setup

1. Build `wwhd_mss.rpl` with `make` from `examples/wwhd_mss`.
2. Copy the `graphicspack` directory to Cemu's `graphicPacks` directory and rename the copied directory to `WWHD_MSS`.
3. Copy `wwhd_mss.rpl` beside `cking.rpx` in the game's `code` directory.
4. Enable **The Legend of Zelda: The Wind Waker HD → Mods → WWHD Macro MSS** in Cemu's graphics-pack window.

The graphics pack supports the USA executable with module hash `475bd29f` and the EUR executable with module hash `b7e748de`.

Hold **ZR+A** while swimming to run the macro. Press **ZL+L+Minus** to open
the settings panel. The panel changes the delay between injected PLUS presses,
automatic stick control, whether the game is left paused when the combo is
released, and the status watermark, which by default shows only while the
combo is held. The panel and the log both carry a CRC32 of the delay and the
stick switch, so two installs can be checked against each other.

Cemu saves those values in `wwhd_mss.cfg` in its current working directory. If
that location is not writable, the settings still work for the current session
but reset the next time the RPL is loaded.
