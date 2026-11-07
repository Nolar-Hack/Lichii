# Adventure notes modules

A module adds a game to the Adventure notes panel. Copy it to `notes/<PROGRAM ID>.json` in the
user folder, with the program ID in 16 upper-case hex digits. No rebuild is needed.

- `save`: `folder` in the save directory, `file` in each slot folder, `layout`. Only
  `gamedata-table` is read so far: hash and value pairs from 0x28.
- `play_time`: the 32-bit hash of the play time.
- `views`: `name`, `tab` (the tab it sits in), `checklist` (counted in Global), then `fields` or a `collection` index.
- `fields`: `title`, 32-bit `hash`, `kind`: `count` (64-bit value at the offset), `flag` or `number`.
- `collections`: `name`, then `zones` of `name` and 64-bit `keys`, counted when in the save.

- `summary`: Global lines, `title` and `terms` of `hash`, `collection` or `constant`, with `scale`.

A view whose name matches a zone also shows that zone's collections. Keep game content out of a
module: identifiers and short names only.
