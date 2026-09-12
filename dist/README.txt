ACEvoPerf - performance mod for Assetto Corsa EVO

INSTALL
  Close the game. Copy all four files (dstorage.dll, dstorage_orig.dll,
  acevo_dstoragecore.dll, acevo_perf.ini) into the game folder, next to
  AssettoCorsaEVO.exe, and let Windows replace the existing dstorage.dll. Start the
  game. That is all. acevo_perf.log next to the exe shows what the mod applied.
  Nothing else of the game is replaced or edited, only dstorage.dll is taken over,
  and the file it used to be is the dstorage_orig.dll you just copied in.

SETTINGS
  Everything is in acevo_perf.ini, each key is explained in the file.
  The texture pool and the DirectStorage staging buffer are sized from your
  card's memory at start (tile_pool_mb=auto, staging_buffer_mb=auto), the
  chosen values are in acevo_perf.log. Set a number in the ini to override.
  The log also says when the game's window is on a monitor owned by another
  GPU than the one rendering (laptops with two GPUs): every frame is then
  copied across, and a display wired to the render GPU avoids that.

MODS FOLDER (optional)
  Files placed under acevo_mods\ next to the exe replace the file of the same
  path inside the game's content package, new paths are added. For example
  acevo_mods\content\<same folders as in the package>\<file>. The package
  itself is never modified, delete the folder to undo everything. Files are
  picked up at game start and listed in acevo_perf.log.

DIRECTSTORAGE RUNTIME
  The game ships Microsoft's DirectStorage 1.2.3. This mod carries 1.3.0 as
  acevo_dstoragecore.dll and uses that instead. Not oversold: one fix between those
  versions lands on a path this game uses, tile destinations for textures whose
  width and height differ, which is the texture streaming queue. The rest is
  compression fixes for a game that streams uncompressed. So it is being on the
  current runtime, not a speed up. Your game's own copy is left alone and not even
  renamed. acevo_perf.log says which runtime is really running. Set
  bundled_runtime=0 in the ini to go back to the game's, which is the first thing
  to try if streaming ever misbehaves.

UNINSTALL
  Delete dstorage.dll, then rename dstorage_orig.dll to dstorage.dll. Delete every
  other file starting with acevo_ (the ini, acevo_dstoragecore.dll, the generated
  acevo_bigscreen.texture, any logs and CSVs) and the acevo_mods folder if you made
  one. Or verify the game files in Steam, which restores the original dstorage.dll.

AFTER A GAME UPDATE
  If an update replaces dstorage.dll, copy the four files in again.

dstorage_orig.dll and acevo_dstoragecore.dll are Microsoft's DirectStorage 1.3.0
runtime, redistributed as allowed by its license.
