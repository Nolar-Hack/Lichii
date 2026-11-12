# Changelog

All notable changes to Eglantine are documented here. Each release's notes are taken from its
`## [X.Y.Z]` section (release candidates use the section of the version they are a candidate for).
New entries go under `## [Unreleased]`; `tools/ci/bump-version.sh` turns them into a version section.

## [Unreleased]

- Taken from zortos293 on 2026-11-11 (28 commits): device memory overallocation on AMD (#1335), oversized images that fit in 1 GiB (#1325), fewer idle submits (#1329), cheaper exclusive loads on x86 (#1327, #1331), read-only native compute bindings (#1333), less shader compile work (#1320), Ocarina guard house freeze patch (#1313), AMD FSR 4 for Ocarina, frame stats overlay redesign (#1345), Lichii dark theme (#1365), and shader fixes for other games (Yoshi, Mario Kart, Duskbloods).
- Left out for now: shader cache v2 and what depends on it (#1334, #1337), native HDR (#1326), the own data folder (#1253).
- Idle emulated cores precompile game code other cores already compiled (`EDEN_NX2_JIT_WARM=0` turns it off): fewer stutters after area loads.
- Transient draws: draws into targets drawn every frame are skipped while a shader is built (`native_async_draws=Transient`, `EDEN_NX2_TRANSIENT_TARGETS=0` for the older rule).
- Texture cache: emergency clean-up near the video memory budget (`EDEN_NX2_TC_EMERGENCY=0` turns it off) and a cap, `EDEN_NX2_TC_CEILING_MIB` (default 7680 MiB on Windows); zero-page trim of idle guest memory on Windows.
- Linux: the `.tar.gz` now holds the same self-contained folder as the AppImage (about 120 MB instead of 35 MB), for systems without FUSE.
- Adventure Notes: Gold Skulltulas and ground Heart Pieces in the overworld are listed per age (child / adult); a spot reachable at either age is under adult.
- Many upstream fixes taken from zortos293 (#1197, #1200, #1199, #1210, #1212, #1236, #1231, #1246, #1245, #1252, #1257, #1258, #1128, #1255 and others).
- Taken from zortos293 on 2026-11-12 (25 more commits): `EDEN_NX2_TC_MIN_AGE` applies only while the device has room (#1439), AMD geometry output limits and sparse block size (#1466), a clean stop without a Userspace PANIC (#1450, #1449), buffer cache fixes (#1459, #1436, #1441, #1452, #1458, #1460), shader compiler SSA (#1468), RTC offset (#1465), Windows commit-limit survival (#1451), FSR 4 through AMD's official runtime, the redesigned game list (#1371) and LAN play / network provider (#1359, #1344). Still left out: shader cache v2 and what depends on it (#1485 OoT water grid, #1463, #1337), native HDR, the own data folder.
- Texture cache: the emergency clean-up now ignores `EDEN_NX2_TC_MIN_AGE`, and recently used images are kept only up to a quarter past the critical threshold, on any card. Ocarina's Skulltula house (14 new render targets a frame) filled 28-36 GB of a 24 GB card with `MIN_AGE=300` and crashed; with `MIN_AGE=60` it stays near 15 GB.
- Vulkan: images of 6 to 24 MiB share 64 MiB memory blocks instead of one dedicated allocation each (~20 ms down to ~0.6 ms per allocation on RADV); `EDEN_NX2_MIDSIZE_IMAGE_POOL=0` turns it off.
- Diagnostics: `EDEN_NX2_TC_LOG=2` logs the texture cache every 4 s (image ages, collector passes by mode, images used again after 10 idle frames, VMA pools) and a snapshot while the memory is within 15% of its budget. The pipeline feedback CSV is now written only with `EDEN_NX2_PIPELINE_FEEDBACK=1` (it only grew).

## [0.1.0]

- Initial release candidate
