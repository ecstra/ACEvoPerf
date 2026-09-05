ACEvoPerf - performance mod for Assetto Corsa EVO

INSTALL
  Close the game. Copy all three files (dstorage.dll, dstorage_orig.dll, acevo_perf.ini)
  into the game folder, next to AssettoCorsaEVO.exe, and let Windows replace the
  existing dstorage.dll. Start the game. That is all.
  acevo_perf.log next to the exe shows what the mod applied.

SETTINGS
  Everything is in acevo_perf.ini, each key is explained in the file.
  Defaults are tuned for a 6 GB GPU. On 8 GB set tile_pool_mb=1536, on 12 GB or
  more tile_pool_mb=2048.

UNINSTALL
  Delete dstorage.dll, then rename dstorage_orig.dll to dstorage.dll. Delete
  acevo_perf.ini and any acevo_perf*.log / acevo_perf_*.csv files. Or verify the
  game files in Steam, which restores the original dstorage.dll.

AFTER A GAME UPDATE
  If an update replaces dstorage.dll, copy the three files in again.

dstorage_orig.dll is Microsoft's DirectStorage 1.2.3 runtime, redistributed as
allowed by its license, byte identical to the one the game ships.
