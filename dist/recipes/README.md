# Shader precompile recipes

Lichii prepares a game's shaders before play from a recipe found here (next to the program, in
`recipes/`) and the player's own game files. A recipe holds no game code: only shader hashes and the
render states the game draws them with, learned from recorded play sessions
(`tools/nx2-precompile`, zortos293/Lichii#895).

- `OoT v131072 (cache v54).nx2recipe`: The Legend of Zelda: Ocarina of Time, game version 1.1.1
  (internal 131072), pipeline cache v54. Made by Frizti for Fritchii (Frizti/Fritchii, v5.x).
