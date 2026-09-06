ACEvoPerf - performance mod for Assetto Corsa EVO

INSTALL
  Close the game. Copy all three files (dstorage.dll, dstorage_orig.dll, acevo_perf.ini)
  into the game folder, next to AssettoCorsaEVO.exe, and let Windows replace the
  existing dstorage.dll. Start the game. That is all.
  acevo_perf.log next to the exe shows what the mod applied.

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

UNINSTALL
  Delete dstorage.dll, then rename dstorage_orig.dll to dstorage.dll. Delete
  acevo_perf.ini, the acevo_mods folder if you made one, and any acevo_perf*.log /
  acevo_perf_*.csv files. Or verify the game files in Steam, which restores the
  original dstorage.dll.

AFTER A GAME UPDATE
  If an update replaces dstorage.dll, copy the three files in again.

dstorage_orig.dll is Microsoft's DirectStorage 1.2.3 runtime, redistributed as
allowed by its license, byte identical to the one the game ships.
