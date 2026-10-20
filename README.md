# 3Input - A System Module Input Monitor

3Input is an input viewer that works behind the scenes on your 3DS. Much like [HorizonMod](https://github.com/Bas25/HorizonMod), the goal is to run as a daemoized process/system module, which you can then use a client to connect to and read your inputs. This is essentially an inverse to InputRedirection in [Luma/Rosalina](https://github.com/LumaTeam/Luma3DS/wiki/Rosalina#inputredirection).

## Features
Launch the Launcher, write down the IP address and port that is presented to you, press A, and then connect to the 3DS IP with a client, such as the example [viewer](https://github.com/phlexplexico/3input/viewer/) (this viewer does not require the port, only the IP).

The module streams a fixed-size 32 byte binary packet per tick over TCP (port `65534`). There is no text formatting on the 3DS side, which keeps CPU use to a minimum; clients decode the bytes themselves. All fields are little endian, with no padding or delimiters:

| Offset | Type   | Field                                                                    |
|--------|--------|--------------------------------------------------------------------------|
| 0      | u8[2]  | Magic, `'I'` `'3'`. Scan for it to resync.                               |
| 2      | u8     | Protocol version (`4`)                                                   |
| 3      | u8     | Wrapping frame counter, to spot dropped frames                           |
| 4      | u32    | Buttons, libctru `KEY_*` bit layout with ZL/ZR and the C-Stick merged in |
| 8      | s16    | Circle pad X                                                             |
| 10     | s16    | Circle pad Y                                                             |
| 12     | s16    | Touch X (pixels, 0 when not touching)                                    |
| 14     | s16    | Touch Y (pixels, 0 when not touching)                                    |
| 16     | s16    | C-Stick X (about +-156, New 3DS)                                         |
| 18     | s16    | C-Stick Y                                                                |
| 20     | u8     | `ir_status`: how the ZL/ZR/C-Stick keys path is doing (see below)        |
| 21     | u8     | `cs_status`: how the analog C-Stick path is doing (see below)            |
| 22     | u8[2]  | Zero                                                                     |
| 24     | u32    | `ir_error`: detail for `ir_status`                                       |
| 28     | u32    | `cs_error`: detail for `cs_status`                                       |

Button bits: `A`=0, `B`=1, `Select`=2, `Start`=3, `Right`=4, `Left`=5, `Up`=6, `Down`=7, `R`=8, `L`=9, `X`=10, `Y`=11, `ZL`=14, `ZR`=15, `HOME`=16 (a ~100 ms pulse per press, the system only announces presses), `C-Stick Right/Left/Up/Down`=24/25/26/27, `Circle pad Right/Left/Up/Down`=28/29/30/31. The wire format is defined in [`source/protocol.h`](source/protocol.h), and [`viewer/threeinput.py`](viewer/threeinput.py) is a reference decoder (`struct.Struct('<2sBBIhhhhhhBB2xII')`).

## Why?
Currently, the only way to view inputs for the 3DS is either using a handcam, or via the use of NTR and [coNTRoller-overlay](https://github.com/zed0/coNTRoller-overlay). NTR can be rather buggy, and also uses a lot more resources than necessary to just show inputs. The aim is to replace it with something less buggy, and that can run on both new and old 3DS'.

## Contributors/Credit
- [N3rds](https://github.com/n3rdswithgame) for the entirety of the old codebase. Since then I've been going through and documenting as much as I can to get a further understanding of the codebase itself.
- [leoetlino](https://github.com/leoetlino) for helping with allocation heap and helping with the sysmodule.
- [MarcusD](https://github.com/Bas25/HorizonMod) for HorizonMod. Some code was graciously borrowed within the heap allocation, and a decent reference for running a system module.
- [hax0kartik](https://github.com/hax0kartik) for his [rehid](https://github.com/hax0kartik/rehid) work and helping me out getting the RST services working!

## Building
 - Install [devkitPro](http://devkitpro.org/) 3DS tools.
 - `make`
 - [`bannertool`](https://github.com/Steveice10/bannertool/releases) on your `PATH`.
 - [`makerom`](https://github.com/3DSGuy/Project_CTR/releases) on your `PATH`.

 Run `make` in both the `Launcher` directory, and the root directory. Install both `CIAs` on your 3DS, *make sure you're connected to a Wifi Access Point* and launch the Launcher! Now connect with a client (such as [3SharpView](https://github.com/PhlexPlexico/3SharpView) ). Alternatively, check the [Releases](https://github.com/PhlexPlexico/3input/releases) tab for the files.

## Installing
Install *both* CIA files (`Launcher.cia` and `3input.cia`) and make sure Luma's "Enable loading external FIRMs and modules" option is on (hold SELECT while booting for the config menu). The IR module needs `code.ips` (rehid's patch with one addition, described below, so it stays compatible with [rehid](https://github.com/hax0kartik/rehid)) at `/luma/sysmodules/0004013000003302.ips` on the SD card (a flat file named after the title ID, not a folder; older Luma versions used `/luma/titles/0004013000003302/code.ips`), followed by a reboot.

You do not need to copy it yourself: `Launcher.cia` bundles `code.ips`. On a New 3DS, or an Old 3DS with a Circle Pad Pro attached (the launcher checks), it looks for the file on start and tells you whether it is installed. If it is missing it creates `/luma/sysmodules` and copies the file in, then asks you to **restart the console and relaunch the app**. An existing file is never overwritten, so rehid's own copy is left alone. If you installed an earlier `code.ips` (rehid's, or an older 3Input), the launcher reports that it differs from the bundled copy: delete `/luma/sysmodules/0004013000003302.ips` and run the launcher again to get the current one, then reboot. Once it is installed, write down the IP and port shown by the launcher and press A to launch.

Use rehid's `code.ips` with rehid's HID module, or on its own? With rehid's patch ZL/ZR are no longer reported to apps through `ir:rst` (rehid puts them into HID instead), so without rehid installed an app that reads ZL/ZR from `ir:rst` will not see them. 3Input itself reads ZL/ZR/C-Stick from the patched module either way.

## Changing Sending Frequency
The module runs a single loop that reads inputs, handles the hotkeys below and sends to clients. While nobody is connected it idles at a low tick rate, and only runs at the send rate once a client connects. Hold `SELECT` and press:
- `SEL+X` - 140 per second.
- `SEL+Y` - 60 per second. (Default)
- `SEL+A` - 10 per second.
- `SEL+B` - 1 per second.
- `SEL+L` / `SEL+R` - decrease / increase the rate by 10 per second.
- `SEL+START` - Exits the module.

`ir_status` is `0` pending (patched `ir:u` not reached yet), `1` IR module **not patched** (`code.ips` not applied), `2` passive, `3` holding an `ir:rst` session, `4` yielded to a game using `ir:USER`, `5` could not get an `ir:rst` session. `cs_status` is `0` pending, `1` mapped (`cs_error` is the raw pad word of the newest `ir:rst` entry), `2` could not open `ir:rst`, `3` GetHandles failed, `4` no shared memory returned, `5` mapping failed (`cs_error` is the Result for the failures). Both viewers show both, which is the quickest way to tell why ZL/ZR/C-Stick read as 0.

## How ZL/ZR and the C-Stick are read
HID does not report ZL, ZR or the C-Stick, they come from the IR module (`ir:rst` on New 3DS). Taking `ir:rst` over from a separate module means a game can no longer get it, so 3Input never does. It follows the same approach as [rehid](https://github.com/hax0kartik/rehid):
- `code.ips` (rehid's) hooks the IR module's `ir:rst` sampling thread so it copies the latest ZL/ZR/C-Stick word into the module's own memory every time it samples, raises the session limit from 2 to 4, and exposes the physical address of that word through `ir:u`.
- 3Input resolves that address once (a brief `ir:u` open/close at startup) and from then on reads the keys with a plain memory load.
- It never calls `Initialize` or `Shutdown` on `ir:rst` and holds no `ir:rst` session. Whenever a game (or the system) has `ir:rst` running, the word is live; all four sessions stay available to games.
- libctru's own `ir:rst` session is disabled too (previously `ir:rst` was initialised, shut down and initialised again at startup).

Because nothing here drives the sampling, ZL/ZR/C-Stick read as 0 while no process is using `ir:rst`.

## Viewing the Output
In order to view the output, use a client that understands the binary packet described above, such as the bundled viewer. `python3 viewer/main.py <3ds ip>` is the tkinter overlay, and `python3 viewer/cli.py <3ds ip>` is a terminal version that needs nothing but Python (on NixOS, `nix-shell -p 'python3.withPackages (p: [p.tkinter])'` provides tkinter for the overlay). Both use [`viewer/threeinput.py`](viewer/threeinput.py), a reference decoder. Clients written for the old JSON output, such as 3SharpView, need updating to the new format.

## TODO
There's a few more things in the TODO pile, and these can be found mainly in the code, which will be ported over to the Issues/Project section in GitHub. Most of it is quality of life (i.e. WiFi issues).

## License
This project is licensed under [GNU GENERAL PUBLIC LICENSE V3](https://www.gnu.org/licenses/gpl-3.0.en.html). Feel free to distribute, contribute, and make changes! Pull requests to improve functionality is always welcome!
