# Lichii (Ocarina of Time build): quick install

This build targets The Legend of Zelda: Ocarina of Time (`0400486024DEC000`). It is mainly tested on
Linux with an AMD GPU (RADV): i5-12400, RX 7900 XTX, 30 GB of RAM. Other setups may behave
differently.

**What you need (not included, dump them from your own console):** `prod.keys` and `title.keys`,
the Switch firmware, and your game.

## 1. Download

From the [Releases page](https://github.com/Nolar-Hack/Lichii/releases) (no GitHub account needed):

- **Linux:** `lichii-linux-x86_64-pgo.AppImage` (or `lichii-linux-x86_64-pgo.tar.gz`, the same files
  unpacked, for systems without FUSE: extract it and run `lichii/lichii`; it needs no Qt install)
- **Windows:** `lichii-windows-x86_64-pgo.zip`

The PGO builds are the recommended ones. Newer test builds sometimes appear first on
[Actions](https://github.com/Nolar-Hack/Lichii/actions/workflows/release-binaries.yml): open the
latest successful run and download an artifact at the bottom of the page (you need to be signed in
to GitHub; artifacts expire after 7 days).

## 2. Install

- **Linux:** put the AppImage in its own folder (for example `~/Lichii/`), make it executable
  (`chmod +x lichii-linux-x86_64-pgo.AppImage`, or right-click > Properties > Permissions >
  *Is executable*), then run it.
- **Windows:** extract the zip into its own folder and run `lichii.exe`.

Lichii is **portable**: on its first start it creates a `user` folder next to the program (next to
the AppImage file) and keeps everything there: keys, firmware, saves, settings, shader caches, the
GPU driver's shader cache and mods. Nothing goes in your home folder or AppData. To move or back up
your install, copy the whole folder.

## 3. Keys and firmware

1. **Tools > Install Decryption Keys**, then pick the folder that holds `prod.keys` and
   `title.keys`.
2. **Tools > Install Firmware > From Folder** (or **From ZIP**), then pick your firmware.
3. Restart Lichii.

## 4. Add your game

Double-click the empty game list (or right-click it > **Add New Game Directory**) and select the
folder with your game. It shows up in the list.

## 5. Shaders (automatic, once)

Lichii ships a shader **recipe** for Ocarina of Time (in `recipes/` next to the program). It holds no
game code: only which shaders the game uses and how it draws them, learned from real play sessions.

1. Start the game. Lichii asks **Precache Shaders**: choose **Precache now**.
2. It reads your game files, rebuilds every shader of the recipe with them and compiles them for
   your GPU (about 20 minutes the first time, with a progress bar and a Skip button).
3. **Restart the game once** after this first preparation. From then on, new areas play without
   shader stutter, even ones you have never visited.

A new Lichii version or GPU driver runs the preparation again; it is much shorter when your GPU
driver has kept its cache (a few minutes). You can also start it from the game list: right-click the
game > **Build Shader Precache**. To turn the question off or on, see **Emulation > Configure >
Graphics > Advanced**.

## 6. Settings for Ocarina of Time

- Right-click the game > **Properties** > Graphics: **Resolution 1x**. Above 1x, some rooms get a
  white wash.
- Keep **FPS Unlock (built-in)** on in **Properties > Add-Ons** (it is on by default). It keeps the
  game at the right speed, also with the 30 fps cap below.

## Bring your save

1. Start the game once in Lichii and reach the title screen, then close it. This creates the save
   folder for your profile.
2. Right-click the game > **Open Save Data Location**. It opens
   `user/nand/user/save/0000000000000000/<profile id>/0400486024DEC000/`.
3. Copy your save files into it, replacing the ones there. For Ocarina of Time they are
   `account.sav`, `option.sav` and the `slot_00` to `slot_05` folders. Copy what is *inside* the
   game's save folder, not the folder itself.

Where to find your save:

- **From another Lichii, Eden or yuzu install:** right-click the game there > **Open Save Data
  Location**, and copy the same files.
- **From an older portable Lichii folder:** the same path under its `user/nand/user/save/...`.
  To move everything at once (saves, settings, shader caches), copy the whole `user` folder
  instead.
- **From your console:** back up the save with JKSV (homebrew), then copy the files it made.

Keep a copy of your old save until you have checked it loads.

## Mods

Right-click the game > **Open Mod Data Location**. Make one folder per mod, with its `romfs` /
`exefs` inside, then restart the game. Turn mods on or off in **Properties > Add-Ons**.

## Adventure Notes (progress panel)

**View > Adventure Notes** (Ctrl+Shift+N). It shows what is left in each dungeon and each place:
chests, Gold Skulltulas, heart pieces, fairy notes, secrets and timers. Click ☆ on any line to pin
it in the Favorites tab.

## Advanced: your own switches

Lichii reads `user/config/nx2_switches.txt` at start (one `NAME=VALUE` per line). For example,
`EDEN_NX2_SWAP_INTERVAL=2` keeps the game at a steady 30 fps. A variable set in the environment wins
over the file. `dist/nx2_switches.txt` (copied next to the program) lists the switches, each commented out.

If the picture stutters or the game crashes after the video memory fills up, check
`VMA heap 1` and `Texture cache: device memory` in the log: `EDEN_NX2_TC_CEILING_MIB=<MiB>` caps the
texture cache, and `EDEN_NX2_TC_EMERGENCY=0` turns off the emergency clean-up (see below).

## Problems?

**File > Open Lichii Folders > Log Folder** (or `user/log/eden_log.txt`). Share that log and your
GPU model when you report a bug.
