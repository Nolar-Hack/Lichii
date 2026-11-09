# Lichii (Ocarina of Time build): quick install

This build targets The Legend of Zelda: Ocarina of Time (`0400486024DEC000`). It is mainly tested on
Linux with an AMD GPU (RADV): i5-12400, RX 7900 XTX, 30 GB of RAM. Other setups may behave
differently.

**What you need (not included, dump them from your own console):** `prod.keys` and `title.keys`,
the Switch firmware, and your game.

## 1. Download

From the [Releases page](https://github.com/Nolar-Hack/Lichii/releases) (no GitHub account needed):

- **Linux:** `lichii-linux-x86_64-pgo.AppImage`
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

## 5. Shaders (recommended, once)

1. With no game running, open **Emulation > Configure > Graphics > Advanced** and turn on:
   - **Shader libraries (faster first-time shader builds)**: builds each shader stage once and runs
     the shader warm-up at game start.
   - **Prewarm predicted mesh shaders (experimental)**: also prepares the mesh shaders OoT uses for
     foliage.
   - **Faster shader compiles** is already on by default. Leave it.
2. Right-click the game > **Prepare shaders (warm-up)**. It reads the game files and extracts the
   shaders. This takes a few minutes and only needs to be done once.
3. Start the game. The warm-up builds every shader on the loading screen (about 10 minutes the
   first time). After that, new areas play without shader stutter. A GPU driver update rebuilds
   them.

*Windows with an AMD RX 5000/6000 card:* if the game crashes while loading shaders, turn
**Shader libraries** off. Those cards had a driver crash with it in other Lichii builds.

## 6. Settings for Ocarina of Time

Right-click the game > **Properties** > Graphics: **Resolution 1x**. Above 1x, some rooms get a
white wash. The other defaults are fine.

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
over the file.

## Problems?

**File > Open Lichii Folders > Log Folder** (or `user/log/eden_log.txt`). Share that log and your
GPU model when you report a bug.
