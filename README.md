# wkRandMap 0.1.0

Experimental WormKit module for 32-bit Worms Armageddon 3.8.1.

## Installation

1. Close Worms Armageddon.
2. Copy `wkRandMap.dll` and `wkRandMap.ini` into the same folder as `WA.exe`.
3. Enable **Options > Advanced > Load WormKit modules** and restart the game.
4. Create the `User\SavedLevels\RandMap` folder and add maps that you can already open manually in the game's editor.

To use a different folder, edit the INI file. You can use an absolute path or a path relative to `WA.exe`.

```ini
[RandMap]
Folder=User\SavedLevels\TTRR
```

The module reads the INI file each time you run the command. If the path contains accented letters or other characters outside the system code page, save the INI file as UTF-16 LE with a BOM.

## Usage and Testing

1. Host a lobby, for example on a LAN.
2. Type `/rm` in the chat and press Enter. The editor may appear briefly while the map is imported. The local chat displays the selected filename or an error.
3. Check that the map preview changes, then use a second client to verify that it receives the map and can start the game.

The random selection includes `.png`, `.bit`, `.lev`, and `.img` files in the configured folder and all its subfolders. Every compatible map has an equal chance of being selected. Symbolic links and junctions are skipped to avoid directory loops. If a folder cannot be read or a path is too long, selection stops with an error. Each selection is independent, so the same map may be selected again. The game validates the file contents during import. Only the host needs to install the module. Map transfer uses the lobby's normal logic.

Loading a map creates a temporary copy inside `User\SavedLevels`, so the game needs write permission in that folder. To uninstall, close the game and remove the DLL and INI files.

## Compatibility and Building

The PE32 DLL has been compiled, and its entry points have been checked against the executable from the official 3.8.1 patch for the CD edition. The DLL checks routine signatures and disables itself if they differ. In-game testing on Windows is still pending, just tested on GOG edition. Unsupported versions are reported through `OutputDebugStringA`.

To build with Zig 0.15.2, run the following command from the source folder:

```sh
zig cc -target x86-windows-gnu -O2 -Wall -Wextra -Werror -shared -o wkRandMap.dll wkRandMap.c -lbcrypt
```

Technical references: the 3.8.1 patch executable distributed by [Worms Knowledge Base](<https://worms2d.info/Updates_(Worms_Armageddon)>) and the chat conventions documented in the [wkTerrainSync](https://github.com/rm-rf-worms/wkTerrainSync) source code.
