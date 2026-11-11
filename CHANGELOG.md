# Changelog

All notable changes to Eglantine are documented here. Each release's notes are taken from its
`## [X.Y.Z]` section (release candidates use the section of the version they are a candidate for).
New entries go under `## [Unreleased]`; `tools/ci/bump-version.sh` turns them into a version section.

## [Unreleased]

- Idle emulated cores precompile game code other cores already compiled (`EDEN_NX2_JIT_WARM=0` turns it off): fewer stutters after area loads.
- Transient draws: draws into targets drawn every frame are skipped while a shader is built (`native_async_draws=Transient`, `EDEN_NX2_TRANSIENT_TARGETS=0` for the older rule).
- Texture cache: emergency clean-up near the video memory budget (`EDEN_NX2_TC_EMERGENCY=0` turns it off) and a cap, `EDEN_NX2_TC_CEILING_MIB` (default 7680 MiB on Windows); zero-page trim of idle guest memory on Windows.
- Linux: the `.tar.gz` now holds the same self-contained folder as the AppImage (about 120 MB instead of 35 MB), for systems without FUSE.
- Adventure Notes: Gold Skulltulas and ground Heart Pieces in the overworld are listed per age (child / adult); a spot reachable at either age is under adult.
- Many upstream fixes taken from zortos293 (#1197, #1200, #1199, #1210, #1212, #1236, #1231, #1246, #1245, #1252, #1257, #1258, #1128, #1255 and others).

## [0.1.0]

- Initial release candidate
