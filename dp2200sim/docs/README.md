# dp2200sim - a Datapoint 2200 simulator

PLEASE NOTE!  STILL WORK IN PROGRESS.<BR> 
 
At this point it can load CTOS from cassette tape images and boot DOS.C from floppy disks. It also support a local printer which means that it is possible to print from DOS.C to a file.

See the [diagnostic tape inventory](DIAGNOSTIC_TAPE_INVENTORY.md) for all tapes in
`tapes/diagnostics/`, measured test results and peripheral implementation ideas.


[![Watch the video](https://i.imgur.com/zhIMYDc.png)](https://youtu.be/XfsMBhP13ww)

## Building on macOS and Linux

Use a C++17 compiler and make. The terminal interface requires ncurses and its
forms library; the optional programmable-font display requires the SDL2
**development** package. Python 3 is needed to run the tests.

On macOS, install the Xcode command-line tools if needed (`xcode-select --install`),
then install SDL through Homebrew:

```sh
brew update
brew install pkgconf sdl2-compat
```

[Homebrew's sdl2-compat package](https://formulae.brew.sh/formula/sdl2-compat)
provides the SDL2 API using SDL3 internally, including Apple Silicon packages.
[pkgconf](https://formulae.brew.sh/formula/pkgconf) supplies `pkg-config`.
On Apple Silicon, use native Homebrew under `/opt/homebrew` and put its `bin`
directory first in PATH, following [Homebrew's installation guide](https://docs.brew.sh/Installation).
An Intel Homebrew installation under `/usr/local` still supplies Intel libraries;
`brew update` does not change their architecture. The compiler and libraries must
match. The macOS SDK supplies ncurses and forms.

On Debian or Ubuntu:

```sh
sudo apt update
sudo apt install build-essential pkg-config libncurses-dev libsdl2-dev python3
```

The [SDL installation guide](https://wiki.libsdl.org/SDL2/Installation) confirms
that `libsdl2-dev` supplies the headers and libraries needed for building.

Then, from the repository root, on either platform:

```sh
cd dp2200sim
make USE_SDL=1
./dp2200sim
```

For the terminal interface alone, use `make` (equivalent to `make USE_SDL=0`);
SDL is not required. `make dp2200sim-headless` builds the test harness without
SDL or ncurses. On Apple Silicon, the default compiler produces native ARM64 code.

The Makefile discovers installed SDL using `pkg-config`, falling back to
`sdl2-config` on PATH. For a custom installation, explicitly select it with
`make USE_SDL=1 SDL2_CONFIG=/path/to/sdl2-config`. It never downloads SDL or
selects a library from `.deps` automatically. On macOS it adds the selected
library directory to the executable's runtime search path.

SDL and terminal builds use separate object directories. Use `make -B USE_SDL=1`
when changing the SDL installation, compiler or architecture within the same
mode, to rebuild existing objects. Validate the terminal interface with
`make test-ui`, or the SDL display with `make test-sdl` after installing SDL.

Start the 5500 with `SET CPU=5500 MEMORY=48 AUTORESTART=TRUE` and `RESTART`;
the ROM initializes the font. Merely selecting the CPU does not run this code.
The SDL window accepts ASCII typing, Enter, Backspace, Escape, F5 and F6, and
uses the same keyboard translation as the terminal view. Closing it hides the
extra display; use `QUIT` in the terminal COMMAND window to exit the simulator.

See [programmable-font research and results](FONT_RESEARCH.md) for the ROM/DOS
font comparison, historical font utilities, graphics limits, and exported images.

Development and runtime testing have primarily been on macOS. The build uses
standard Unix dependencies on Linux; the Linux package instructions above have
not been verified by a Linux build in this session.

The actual cpu simulator code is based on a 8008 simultor by Mike Willegal. I have heavily modified it for the Datapoint 2200 and Datapoint 5500 instruction set and wrapped it into C++.

The simulator supports the 9380 floppy drive system, 9350 cartridge disks,
9370 mass storage disks, and a 9374 cartridge-disk mode. The floppy drive
supports four disks. To run DOS.C, use DP1100DisketteBoot.tap from DOS.C.
Attach is on cassette 0
```
ATTACH F=DP1100DisketteBoot.tap
LOAD
```

The attach 011.IMD in floppy disk drive 0
```
ATTACH F=011.IMD T=FLOPPY 
```
Then type RUN. NOW DOS.C will start and you get a DOS.C prompt:

```
┌DATAPOINT 2200 SCREEN───────────────────────────────────────────────────────────┐
│                                                                                │
│                                                                                │
│                                                                                │
│                                                                                │
│                                                                                │
│                                                                                │
│                                                                                │
│                                                                                │
│DOS.C  DATAPOINT CORPORATION'S  DISK OPERATING SYSTEM  VERSION 2.               │
│                                                                                │
│READY                                                                           │
│                                                                                │
└────────────────────────────────────────────────────────────────────────────────┘
```
USE the CAT command to list files. It list all files on all drives if you have attached .IMD files to drive 1, 2 or 3.
Please note that the DISPLAY button has to be pressed to have the output printed on screen. I.e. pressing F6 twice while in the DATAPOINT 2200 SCREEN window.

Please note that all files are mounted read-only since writing has not yet been implemented.

## 5500 mode

By using the command ```SET CPU=5500``` the simulator supports the 5500 CPU which is quite a big leap compared to the 2200. The 5500 has simple memory management with a user / supervisor mode and memory protection. The MMU also support relocation using a base register. Many old instruction in the 2200 which operated on the A register now can operate on any register using a prefix-byte. There is a new X register used for paged addressing.

The 5500 also have a 4k ROM memory that contains powerup code, a debugger and a restart/boot routine. The boot routine is able to boot from cassette, 9380 floppy drive, 9350 cartridge disk and 9370 multiplatter drives.

The DOS.C operating system can be booted from the restart/boot routne by simply attach the image-file and then do a restart command.

For a cold 5500 boot of `DOS.C/003.IMD`, attach the floppy, select
`SET CPU=5500`, then `RESTART`. The ROM probes optional controllers before
trying the floppy. Absent controllers return zero status and ignore commands;
previously these probes incorrectly raised `E5 ACCESS PROTECT ERROR`, sometimes
at octal `177315`, before DOS was loaded. A prior 2200 cassette boot is not
required. Floppy controller selection and buffer state also start at zero.

```
SET CPU=5500
AT F=../DOS.C/001.IMD T=FLOPPY
RESTART
```

All images in the DOS.C directory is bootable in 5500 mode except for 011.IMD (which can only be booted in 2200 mode with the ```DP1100DisketteBoot.tap``` file loaded as described above) and 005.IMD which unfortunately is result of corrupt read (one sector of track 64 is missing).  

### Using the 5500 ROM debugger

For a fresh debugger session, no tape or disk is needed:

1. In the COMMAND window, enter `SET CPU=5500 MEMORY=48 AUTORESTART=TRUE`.
2. Select the DATAPOINT 2200 SCREEN with TAB and press F6 once to hold DISPLAY.
3. Return to COMMAND and enter `RESTART`.
4. Wait for the ROM to finish initialization and show its debugger. Select the
   DATAPOINT screen and press F6 again to release DISPLAY before typing commands.

From COMMAND, TAB selects the Datapoint screen; two more TAB presses return to
COMMAND. Keep DISPLAY held until the debugger appears. This simulator's
`RESTART` runs the power-up initialization, including clearing RAM; this procedure
starts a fresh session rather than preserving a running program for inspection.

The debugger deliberately uses the **bottom-right corner** of the 80×12 screen.
Its four lines have this format; they are not a full-screen register listing:

```text
AAAAAA    Current address, in octal
X  NNN    Character and 8-bit octal value at that address
MMMMMM    16-bit octal word: byte at address is LSB, next byte is MSB
nnnnnn    Command entry
```

All debugger numbers are **octal**. Type the command letters below in **uppercase
on the host keyboard**: the simulator translates their case to the native keyboard
codes expected by the ROM. A letter executes immediately; only an address by itself
needs Enter. These commands have been checked against the built-in ROM:

| Type | Effect |
| --- | --- |
| `170036` then Enter | Inspect the start of ROM initialization: byte `040`, word `043040`. |
| `I` | Increment the current address and refresh the display. |
| `D` | Decrement the current address and refresh the display. |
| `100` then Enter | Inspect RAM address `000100`. |
| `101M` | Write byte `101` (ASCII `A`) at the current RAM address. |

For example, `100` Enter, then `101M`, shows address `000100`, character `A`,
byte `101`, and word `000101` when the following byte is zero. This changes RAM;
use the ROM address example if you only want to inspect memory.

The terminal view shows unprintable character codes as a single blank cell. Their
octal values remain visible. The optional SDL display uses the programmable ROM
font. Earlier terminal rendering treated these codes as terminal controls or
two-character caret notation, and erases could scroll the bottom row. These bugs
are covered by `make test-ui`; `make test-headless` checks the ROM commands above.

The full command and display reference is on printed page 35 of the
[Datapoint Quick Reference Guide, second edition](https://bitsavers.org/pdf/datapoint/60311_Quick_Reference_Guide_For_Datapoint_Processors_and_Peripherals_2ed.pdf).
Other operations, including breakpoint continuation and resuming saved CPU state,
have not been verified here.

## How to use the simulator

Short description for how to use the simlator. The simulator has three windows. TAB (or Shift-TAB) is used to cycle between windows. The active windo has a highlighted boarder and a cursor visible (the Datapoint 2200 window may have the cursor turned off since it is controlled by the software).

### Commands

| Command   |  Parameters  |  Description |
|-----------|--------------|--------------|
| HELP      |              |  Show help information.  |
| SET       | CPU<br>AUTORESTART<br>MEMORY | Set CPU type, either 2200 (default) or 5500. Set autorestart, TRUE or FALSE on a 5500. Set memory size. Value between 2 and 64 is valid |
| ATTACH    | FILE<br>DRIVE<br>TYPE<br>WRITEPROTECT<br>WRITEBACK  | Attach a file to the simulator. TYPE indicate the device to attach to. Either CASSETTE (default), FLOPPY or PRINTER. FILE is the file name to open. DRIVE is the drive number. Default is drive 0. WRITEPROTECT is if the attached media is to be writeprotected in the simulator. TRUE or FALSE. Default is TRUE. WRITEBACK indicate if the media shall be written back to the file. TRUE or FALSE. Default is FALSE. |
| STEP      |              |  Step one instruction. |
| DETACH     | DRIVE<br>TYPE |  Detach file from cassette drive. Parameter DRIVE specify the drive used. Default drive is 0.│TYPE specify either CASSETTE, FLOPPY or PRINTER. CASSETTE is default.|
| STOP       |      |   Stop execution |
| EXIT       |    |     Exit the simulator |
| QUIT       |         |   Quit the simulator |
| LOADBOOT   |          |   Load the bootstrap from cassette into memory |
| RESTART    |           |   Load bootstrap and restart CPU |
| RESET      |           |  Reset the CPU.|
| HALT       |           |  Stop the CPU. |
| RUN        |           |  Run CPU from current location |
| CLEAR      |           |  Clear memory |
| BREAK      | ADDRESS          |  Add breakpoint.Parameter ADDRESS is used for specifying the address of the breakpoint.|
| NOBREAK    | ADDRESS  |  Remove breakpoint. Parameter ADDRESS is used for specifying the address of the breakpoint. │
| TRACE      |          | Enable trace logging. |
| NOTRACE    |          | Disable trace logging.|
| HEXADECIMAL |          | Use hexadecimal notation. Also possible to toggle in the register view by pressing 'o'.|
| OCTAL      |           | Show in Octal notation. Also possible to toggle in the register view by pressing 'o'. |
| YIELD      | VALUE     | The amount of CPU time consumed byt the simulator.  VALUE parameter specify the amount. Value between 0 and 100. |

### Command window

Commands listed above can be given in the command window. Commands are given in the format <br>\<Command\> \<ParameterName\>=\<ParameterValue\> ... \<ParameterName\>=\<ParameterValue\> 

There is a simple command line editor that allows for LEFT and RIGHT arrow to step back and forth in the command line. Ctrl-A and Ctrl-E can be used to go to the beginning of the line and to end of line respectively.

Long commands scroll horizontally as you type or move the cursor. Backspace
removes the preceding character; Delete removes the character at the cursor.
The full command is retained even when only part of it fits in the window.
`make test-ui` checks long ATTACH commands, editing keys and history in a
temporary curses terminal using disposable disk images.

A command history exist where previous commands can be retrieved by using the UP arrow. If in command history mode it is possible to browse among the entire history using the UP and DOWN keys.

### Register window

When the CPU is stopped it is possible to alter the contents of memory and registers. It is possible to navigate with the arrow keys to a location that you want to update. Press Enter to store it.
Pressing the 'o' key toggle between HEX / OCTAL view in the register view

### DATAPOINT 2200 Window

This is where the simukated system outputs screen data. The F5 and F6 keys are active in this window. They toggle the state of the DATAPOINT 2200 KEYBOARD and DISPLAY keys respectively. On the real hardware these were keys that wasn't scanned in the normal keyboard matrix but acts direc momentarily to the CPU. As this is not possible with ncurses the F5 and F6 toggles the state. Normally the KEYBOARD key stops execution and gets back to the operating system and DISPLAY let the system continue display printout. So if printout is paused press F6 twice to let the simulator printout the full content.

## Requirements

This is an unsorted list of requirements. Not necessarily part of the MVP. More as a list coming from brain-storming.

* ~~Mulithreaded using voluntary pre-emption and a timer-queue using nanosleep().~~ DONE
* ~~One main thread for ncurses and keyboard handling / command processing~~ DONE
* ~~One thread for the CPU simulator~~ DONE
* ~~One IO simulator handling cassette, screen, keyboard interaction. Possibly other IO devices like printers, terminals, disks etc~~ DONE
* ~~Main thread use ncurses with three windows.~~ DONE
* ~~One DP2200 window fixed at 80x12.~~ DONE
* ~~One CPU status / register window to the right. Variable size.~~ DONE
* Status window show currently attached cassette file, configured IO devices, memory contents.
* ~~One command window below these two windows.~~ DONE
* ~~Tab is used to switch between windows. The active window border is highlighted.~~ DONE
* ~~The size of windows is calculated from the total size of the terminal window.~~ DONE
* ~~If to small a message will be shown to increase the size.~~ DONE
* ~~Should detect SIGWINCH and redraw when term size is changing. ncurses handles this and emits KEY_RESIZE.~~ ALMOST WORKS
* ~~The command window should have a history.~~ DONE
* When a printout is overflowing the window it should display ”more” so that the user can press space to not miss anything.
* It should be possible to disable the feature above.
* ~~The register window shall be updated continously when the CPU is running.~~ DONE
* ~~When CPU is halted it shall be possible to change the contents of the registers and memory.~~ DONE
* ~~The main thread takes keypresses in the DP2200 window and translates them into a code relevant for the DP2200.~~ DONE
* ~~When a key is pressed and translated a flag is set.~~ DONE
* ~~The CPU thread counts a global realtime when in run state which is updated for each instruction executed. In nanoseconds.~~ DONE
* ~~All IO operations is checked towards this global realtime so that IO takes place in a way that is relevant to the CPU. Not too fast.~~ DONE
* ~~The main thread is reading the keyboard unprocessed and using non-blocking IO.~~ DONE
* If the cassettes are running they are locked and it should not be possible to be changed by the command line commands.
* ~~CPU simulator has global state. Registers, stack and flags. There is then a run/halt flag and a singlestep flag.~~ DONE
* ~~The run/halt flag is checked as the first thing before fetching the instruction.~~ DONE
* ~~Simulated instruction execution is scheduled from the timerqueue. The wallclock realtime is checked to keeps in sync with the executed realtime. If lagging behind it will execute more instructions to keep up, otherwise it should sleep.~~ DONE
* ~~If the single step flag is set then when command line tool do a continue command causing the halt run flag to be set then the simulator sees the single step flag and clears the halt/run flag to halt condition (which is then checked on next iteration).~~ DONE
* ~~C++~~
* ~~Interrup-thread sending setting the interrupt flag every 500 us.~~
* Commandcompletion. Pressing ? Gives a printout of all matching commands or fill of a single matching command.
* Pressing ? after whitespace in command gives first param. Pressing ? again gives next param ignoring params that has been given.
* If one or more characters of the param is given only those will be part of the param completion toggling.
* ~~Params are delimited with =. I.e param1=23.~~ DONE
* Pressing ? After the = give default value. Pressing ? Toggles between max, min and default.
* ~~CPU simulator should handle breakpoints. Before running an instruction. I.e doing the fetch it should check if the current address is among breakpoints and then halt.~~ DONE
* ~~It should be possible to run program at real speed. I.e. an instruction would sleep the correct amount of time.~~ SORT OF
* It should be able to simulate both the model 1 and model 2. Possibly also later 5500 and 6600?
* The amount of memory shall be configurable.
* IO devices shall be configurable.
* Watchpoints. If it accesses a certain memory location it shall halt. But that would happen mid-instruction. How to continue?? Probably stop after this instruction.
* IO watch. Stop after a certain Input instruction or EX instruction when a specific address has been given.

## Headless test harness

Build and run the screen/keyboard and processor-tape regression tests from the
repository root:

```sh
make -C dp2200sim test-headless
```

This builds `dp2200sim-headless` with the same CPU, cassette and I/O controller as
the interactive simulator. It needs a C++17 compiler and Python 3 for the test
client, and requires neither SDL nor ncurses nor a terminal of any particular
size. The TSTPRO check uses the regression fixture `../tapes/diagnostics/tstpro1.1.tap`.
Success requires the screen to contain `TEST COMPLETED` and the CPU to
halt within an instruction budget. A halt without that text and a budget overrun
both fail. On the current tape it halts at octal PC `04052` after 1,439
instructions, without keyboard input.

The original PCMTEST, TSTDIS and TSTKEY tapes also run as regressions in this
suite. Run just these checks with `make -C dp2200sim test-console-diagnostics`.
PCMTEST must reach pass 2 within 60 million instructions, with all 128 displayed
bit/1K error counters zero at every sample and visible row progression. TSTDIS
must produce the exact character-set and erase layouts, rolling-line patterns,
cursor and light transitions, then return to its instructions. TSTKEY tests the
new-keyboard path through all four rows, shifted rows, spaces and keypad with
no error beeps; a separate check verifies a wrong byte beeps and a short line
is erased for retry. These console checks use native keyboard bytes, including
BACKSPACE, CANCEL and DEL. They verify the headless character/control state;
physical keyboard scanning and rendered glyph appearance remain outside their
scope. The checks are in [test_console_diagnostics.py](../tests/test_console_diagnostics.py).

The executable accepts one command per stdin line and flushes one JSON response
per command. Every response includes the complete 80 × 12 screen, with spaces
preserved and a newline after each row, CPU position, cumulative executed
instructions, simulated time, halt status, keyboard latch status, cursor
visibility and light states. Errors return `ok: false` with an `error` string. `quit` exits without a
response.

| Command | Effect |
| --- | --- |
| `cpu 2200` / `cpu 5500` / `cpu 6600` | Select CPU before loading or executing. 6600 support is still incomplete. |
| `load /path/to/tape.tap` | Attach cassette 0 and load its bootstrap at address zero. Use a fresh process for each tape. |
| `tape N /path/to/tape.tap` | Replace cassette deck 0 (rear) or 1 (front), read-only, without restarting the CPU. Stop the transport first. |
| `floppy N /path/to/disk.IMD` | Attach a read-only floppy image to drive 0–3. |
| `tape-create N /path/to/new.tap` | Create a writable cassette for UBOOT. Existing files are rejected. |
| `tape-stop` | Stop cassette motion and finish an outstanding written block. |
| `cassette-state` | Inspect selected deck, cassette status, pending transport and per-deck position/presence/protection without consuming data. |
| `run N` | Execute up to N instructions, stopping on CPU halt. N must be 1–100,000,000. |
| `pc N` | Set PC to decimal address N and release the halt. |
| `continue` | Release the halt without changing PC. |
| `memory N C` | Read C physical bytes starting at decimal address N, with C from 1 to 256. |
| `inspect` | Include both register banks, flags, stack position and recent instructions in `debug`. |
| `trace /path/to/output` | Write executed instructions and resulting registers to a trace file. |
| `disk-model 9370` / `disk-model 9374` | Select disk controller mode before attaching images. Default is 9370. |
| `disk N WP /path/to/image` | Attach logical unit N, with WP=0 writable or WP=1 read-only. Missing writable images are created with the model's capacity. |
| `disk-protect N 0` / `disk-protect N 1` | Change the simulated write-protect switch. Attach writable first when testing switch changes. |
| `disk-state` | Return transfer/seek counters, error counts and address coverage in `disk`. |
| `printer /path/to/output` | Capture the local printer, required by SV374. |
| `io-trace /path/to/output` | Record disk control commands, transfer completions and cassette reads. |
| `screen` or `state` | Capture current screen and state without executing instructions. |
| `key N` | Inject a native keyboard byte (decimal 0–255). An occupied latch returns an error instead of losing a key. |
| `button keyboard 0` / `button keyboard 1` | Release / press KEYBOARD. |
| `button display 0` / `button display 1` | Release / press DISPLAY. |
| `quit` | Exit. |

`tests/harness.py` provides a Python context manager, command responses as
dictionaries, character input with the existing interactive UI's encoding, and
bounded `run_until` checks. For example, with `dp2200sim/tests` on the Python
import path:

```python
from harness import Harness

with Harness() as simulator:
    simulator.load("tapes/diagnostics/tstpro1.1.tap")
    result = simulator.run_until("TEST COMPLETED")
    print(result["screen"])
```

For interactive programs, send one key, run until `keyboard_ready` becomes false,
then send the next key. Keyboard and display button presses are independent of
normal keys. Screen snapshots represent character codes; programmable glyph
uploads are accepted but are not rendered as pixels in headless mode. This
runner defaults to the 2200 CPU. Cassette callbacks execute at their
scheduled CPU time with no wall-clock sleeps. The tests also exercise a cassette
read beyond the bootstrap, keyboard consumption, buttons, scrolling, erasure,
lights, and rejection of a halt with the wrong success text.

### TEST5556 and the 5500 ROM

`tapes/diagnostics/DIAG5500_V1.1.tap` contains both TEST5500 2.2 (menu entry 2) and
TEST5556 1.1, dated 05/09/78 (entry 3). TEST5500 contains a hardcoded ROM
reference which differs from `5500firmware.inverted.bin`. TEST5556's BRL test
uses RAM patterns to check relocation instead. It works with the current ROM;
this does not establish a byte-for-byte ROM revision match.

The headless regression selects entry 3, loads TEST5556, chooses instruction
option E, and answers the I/O prompt with a native lowercase `d`. It requires
execution through the final BCV test and a return to the diagnostic menu without
an error or CPU halt. The diagnostic tape is included as a regression fixture. The keyboard, screen, memory,
and repeated-cycle menu options are not covered by this check.

Before loading the tape, the runner executes the ROM power-up entry at octal
`170036` to initialize the interrupt vectors in system RAM. ROM generation now
retains all 4,096 bytes of the firmware image, and writes cannot alter ROM.
The instruction fixes exercised by TEST5556 include LFII/LFID, block compare,
BT/BTR, binary and decimal field arithmetic, field shifts, multiple-byte I/O,
and BCV translation. BT/BTR and BCV retain the terminating byte in the residual
count while advancing the pointers past it.

### DIAG6600 tape repair

`../tapes/diagnostics/DIAG6600_V1.1.tap` had no file-1 header between the menu's final
numeric record and UNITEST. The cassette loader consequently loaded UNITEST
over the menu. A 12-byte framed record with payload `81 7e 01 fe` was inserted
at byte offset 1,990; all original bytes remain unchanged.

Original SHA-256: `709c11287e4848fd17c94438de90f2da77917d6eaf7190cdbfa1e774e9f16924`.
Repaired SHA-256: `fd4c13f0eb61b7849732d0901e7e8ed00dd3d836fc256a6bbf0e56d6416ab9e5`.

The regression verifies that the repaired tape loads its menu with TST6600C,
TST6600D and the file-number prompt. This does not validate the complete 6600
instruction set, which remains work in progress.

## 9370 and 9374 disk diagnostics

For EXRCASS transport verification, TAPTIM write/read checks and the measured
timing limitations, see [cassette diagnostic results](CASSETTE_DIAGNOSTICS.md).

Run all five supplied tapes against disposable disk images:

```sh
make -C dp2200sim test-disks
```

Results, screen histories and local-printer output are written to
`dp2200sim/diagnostic-results/`; `summary.json` lists the completed checks and
controller counters. The images are temporary and are removed after each run.
The same checks, a read-only SV374 pass, and controller regression tests also
run under `make -C dp2200sim test-headless`.

| Tape | Checked operation | Operator sequence used by the harness |
| --- | --- | --- |
| MD370_V1.1 | One buffer-test pass; manual disk write/read, zero reported errors | KEYBOARD to leave help; `5` ENTER; KEYBOARD to stop; `2` ENTER; KEYBOARD; `1` ENTER |
| MD370_V1.2 | Forward sequential seeks across cylinders 0–202; KEYBOARD returns to options | Physical drive `0`, option `2`, direction `F` |
| MD374_V1.1 | One buffer-test pass; manual disk write/read, zero reported errors | Same sequence as MD370 1.1, using the 9374 model |
| MA374_V1.2.B | Sequential seeks traverse the 9374 address range and change from ascending to descending | Write-protect both logical units; drive `0` ENTER, logical unit `0` ENTER, head `0` ENTER, test `3` ENTER |
| SV374_V1.1 | Entire logical-disk format/write/verify pass with both patterns; zero hard/soft errors | Drive `0`, logical unit `0`, physical errors `P`, decimal output `D`, full verification `F`, confirm `Y`, serial number ENTER |

The monitor and seek programs run continuously; the tests stop after observing
the stated pass/coverage. KEYBOARD is a separate button, not a typed character.
In the curses UI it is toggled by F5. Hold it until the program returns to its
monitor/options, then release it. MD370/MD374 command lines require ENTER;
SV374's single-choice prompts consume one character immediately.

For a read-only SV374 run, both logical units must be online and protected.
Select `R` instead of `F`, then `N` for no track offset. The verifier reports
results on both the screen and local printer. To capture that separate check:

```sh
python3 dp2200sim/tests/disk_diagnostics.py --read-only \
    --output dp2200sim/diagnostic-results/read-only SV374_V1.1
```

### Controller modes and image layout

The 9370 command/status layout follows the supplied
[Datapoint quick reference](http://bitsavers.informatik.uni-stuttgart.de/pdf/datapoint/60311_Quick_Reference_Guide_For_Datapoint_Processors_and_Peripherals_2ed.pdf).
The diagnostics use the same basic I/O interface for the 9374, with additional
logical-unit/head addressing and a track-offset command. They do not establish
that the physical controller electronics are identical.

| Mode | Logical units | Logical image geometry | Image bytes |
| --- | --- | --- | --- |
| 9370 | 0–7 | 203 cylinders × 20 heads × 24 sectors × 256 bytes | 24,944,640 |
| 9374 | 0–15 | 204 cylinders × 8 logical heads × 24 sectors × 256 bytes | 10,027,008 per logical disk |

Images store sectors in logical cylinder/head/sector order, without sector
headers or CRC bytes. For 9374, logical unit `2 * physical_drive + pack` selects
pack 0 (removable) or 1 (fixed) in the attachment interface. The CPU selects
the physical drive with command 5 and the pack with head bit 3 (command 9).
A physical drive therefore uses two image files.
The observed logical geometry matches the
[9374 data sheet](http://bitsavers.informatik.uni-stuttgart.de/pdf/datapoint/disk/9374_High_Capacity_Cartridge_Disk_Data_Sheet.pdf):
two disks per drive, each with 408 physical cylinders, two surfaces and 48
sectors per track. MA374's head/cylinder selections and SV374's complete pass
support that mapping. Formatting in 9374 mode covers both 24-sector logical
halves of the physical track and preserves the controller's buffer contents.
The [9370 data sheet](http://bitsavers.informatik.uni-stuttgart.de/pdf/datapoint/disk/9370_Mass_Storage_Disk_Controller_Data_Sheet.pdf)
describes the different 20-surface pack geometry.

Interactive attachment selects the mode through TYPE:

```text
ATTACH TYPE=9374 DRIVE=0 FILE=removable.dsk WRITEPROTECT=FALSE
ATTACH TYPE=9374 DRIVE=1 FILE=fixed.dsk WRITEPROTECT=FALSE
```

Detach all images before switching controller modes. A writable attachment can
create an empty image; a read-only attachment requires an existing image.

These tests validate the digital buffer, addressing, seeks and image transfers.
Analog head/sector/VCO alignment, real rotational timing, track offset,
file-unsafe conditions and injected parity/CRC faults are not modeled. The
alignment menus can run, but the simulator cannot establish that a physical
drive is aligned. The timing constants are approximations. No full automatic
MD370/MD374 maintenance cycle is claimed by these checks.

Command 7 returns the documented `001` for 9370. The 9374 identification value
`020` is provisional, inherited from an old source comment: none of these five
diagnostics issues command 7. DOS.D 2.6 INITDISK and cassette generation accept
`020` and use the expected 9374 addressing. Independent hardware documentation
for the identification value is still missing; this is recorded in the root
`questions.md`.

## DOS.D 2.6 installation and disk boot

### Quick start in the interactive simulator

The already installed DOS.D disk and its UBOOT cassette are in
`dp2200sim/dos-d-results/dos-d-2.6/`. Start a fresh simulator from the
`dp2200sim` directory so the relative paths below resolve correctly:

```sh
cd dp2200sim
./dp2200sim
```

In the **COMMAND WINDOW**, select the CPU, attach the media, and restart:

```text
SET CPU=5500 MEMORY=48 AUTORESTART=TRUE
ATTACH TYPE=9374 DRIVE=0 FILE=dos-d-results/dos-d-2.6/removable.dsk WRITEPROTECT=FALSE
ATTACH TYPE=9374 DRIVE=1 FILE=dos-d-results/dos-d-2.6/fixed.dsk WRITEPROTECT=TRUE
ATTACH TYPE=CASSETTE DRIVE=0 FILE=dos-d-results/dos-d-2.6/boot.tap WRITEPROTECT=TRUE
RESTART
```

`RESTART` runs the 5500 ROM's power-up initialization. With `AUTORESTART=TRUE`,
the simulator continues from the ROM's initialization HALT into its boot
routine automatically. There is no need to use `RESET`, `LOADBOOT`, or edit
the program counter. Wait for ROM initialization and boot to finish.

Use TAB to select the **DATAPOINT 2200 SCREEN**. When it displays:

```text
DOS.D 2.6 DATAPOINT'S DISK OPERATING SYSTEM
READY
```

Type `CAT` and press ENTER **in the Datapoint screen**, to list both volumes,
or `CAT :D0` to list only the removable pack. Simulator commands belong in the command window; DOS commands
belong in the Datapoint screen. Attach both packs before booting: unit 0 is the
removable pack and unit 1 is its fixed-pack companion. Both images now contain
DOS filesystems. The fixed pack may remain write protected for cataloguing;
attach it with `WRITEPROTECT=FALSE` when you want to write files to it.
If the DISPLAY or KEYBOARD buttons were toggled with F6/F5, release them before
normal DOS use.

The earlier manual initialization / LOADBOOT / P=0 / RUN procedure disabled
automatic restart so that the test harness could control bootstrap loading.
It is unnecessary for normal interactive startup. The cassette here is the
generated **boot.tap**, rather than any of the five installation tapes.
For a different installation directory, substitute its paths in all three ATTACH
commands.

#### Continuous bell after a command

The previous guide incorrectly said to leave `fixed.dsk` detached. With only
unit 0 attached, an unknown command such as `HELP` can leave DOS repeatedly
executing `EX_BEEP` at octal `002361`, even after a later `CAT :D0` succeeds.
The trace shows DOS polling disk readiness and returning carry set to the
bell branch. ncurses sends each beep to the terminal, which can display it as
a flashing window.

Attach unit 1 **before booting**, using the command above. If the repeating
bell has already started, stop the simulator and repeat the boot sequence with
both packs attached; attaching the companion during the loop did not clear it
in the reproduction. This is a tested workaround for this DOS.D installation;
the intended DOS behavior with an absent fixed pack still needs investigation.

#### Initializing the fixed pack

The original installation left `fixed.dsk` blank. Attaching that image stops
the missing-pack bell, but plain `CAT` then reports
`FAILURE IN SYSTEM DATA AT 021444` on `DRIVE 01`, because it has no DOS directory.
The supplied installation has now been repaired using **`DOSGEN :D1`** and
verified with a fresh boot and plain `CAT`. Reopen the simulator to use the
replaced image if it was already running during the repair.

To initialize a blank fixed pack yourself, boot from the removable pack with
the fixed pack attached as unit 1 and `WRITEPROTECT=FALSE`. If it is currently
write protected, enter these commands in the **COMMAND WINDOW**:

```text
STOP
ATTACH TYPE=9374 DRIVE=1 FILE=dos-d-results/dos-d-2.6/fixed.dsk WRITEPROTECT=FALSE
CONTINUE
```

Then enter **`DOSGEN :D1` in the Datapoint screen**. DOSGEN erases the selected
volume. For the blank fixed pack, answer the prompts as follows:

| Prompt | Answer |
| --- | --- |
| Starts by erasing the disk in :DR1? | `Y` |
| Do you mind losing any files that may be on it? | `N` |
| Are you sure? | `Y` |
| Lock out any cylinders? | `N` |

Wait for cylinder checking, system-file copying, and PUTIPL to finish and DOS
to return to `READY`. Plain `CAT` should now list drives 0 and 1 without errors.
This creates a usable second DOS volume; it does not copy all application files
from drive 0. The simulator's sector images support DOSGEN directly. `INITDISK`
is the separate low-level track-formatting program used for physical media.

For another existing installation, the scripted repair works on copies,
verifies a fresh boot, and saves the original as `fixed.dsk.before-dosgen` before
replacing only the fixed pack:

```sh
python3 tests/install_dos_d.py --initialize-fixed --output dos-d-results/my-installation
```

Check the quiet-console regression with:

```sh
python3 tests/check_dos_d_bell.py --output dos-d-results/dos-d-2.6
```

### Repeating the installation

The supplied `DOS.D/dos.d_2.6_7_MAR_81_1of5.tap` through `5of5.tap`
install successfully on the 5500 with a 9374 controller. The installation
runner uses the original media and normal keyboard input; it does not patch
DOS or bypass the surface test.

```sh
make -C dp2200sim install-dos-d
```

Each run creates a new timestamped directory under `dp2200sim/dos-d-results`.
To choose a directory explicitly, use a path that does not already exist:

```sh
python3 dp2200sim/tests/install_dos_d.py --output /tmp/my-dos-d-install
```

The sequence follows chapters 6 and 44 of the
[DOS 2.6 User's Guide](http://bitsavers.informatik.uni-stuttgart.de/pdf/datapoint/software/50432_DOS_Users_Guide_Version_2.6_May80.pdf):

1. Initialize the 5500 power-up ROM and boot tape 1. Format physical drive 0's
   removable pack, with its fixed pack protected.
2. Start a fresh 5500 process and boot tape 2. Select logical drive 0, complete
   generation, confirm erasure, answer that losing files is acceptable, and
   decline cylinder lockout. Wait for generation to complete and DOS `READY`.
3. Mount tapes 3, 4 and 5 in the front deck in turn. Run `MIN;AO:D0`, waiting
   for the current tape's final file and `MULTIPLE IN COMPLETED` before changing
   media. Tape 4 installs `UTILITY/SYS`; tape 5 includes `DOSD/RFM`.
4. Create a new front-deck cassette and run `UBOOT`. Press ENTER to write it,
   wait for `BOOT VERIFIED`, then decline another copy.
5. Start another fresh 5500 process, load that boot cassette and boot the disk.
   Run `CAT :D0` and check that system and utility files are present without
   system-data errors.

The output directory contains `removable.dsk` (the installed volume),
`boot.tap` (the 702-byte UBOOT record in SIMH framing), `checks.json`, and screen
histories, final screens and disk counters for each stage. `fixed.dsk` is
protected while formatting the removable pack, then initialized with DOSGEN
as a second DOS volume. Keep it attached as unit 1 during interactive use.
Use `CAT` for both volumes or `CAT :D0` for the removable volume specifically.

Repeat the fresh-process boot/catalogue check with:

```sh
python3 dp2200sim/tests/install_dos_d.py --boot-only \
    --output dp2200sim/dos-d-results/dos-d-2.6
```

For direct headless operation, initialize the ROM before loading the boot tape:

```text
cpu 5500
pc 61470
run 10000000
disk-model 9374
disk 0 0 /absolute/path/to/removable.dsk
disk 1 1 /absolute/path/to/fixed.dsk
load /absolute/path/to/boot.tap
run 10000000
```

The installation exposed 9374 pack selection through head-address bit 3 and
cassette readiness/boundary handling that the earlier diagnostics did not
exercise. Cassette writing now supports WBK, byte-ready timing, record
completion, rewind and reread verification. Writable cassettes are created
explicitly with `tape-create`; installation media remain read-only. Timing is
an approximation, and cassette parity faults are not injected.

This run verifies DOS.D installation on a **9374 removable pack**. It does not
claim an installation on the different 9370 pack geometry. DOSGEN also
initializes the fixed 9374 pack and copies system files from the removable
volume. The manual's Appendix D also confirms that DOS uses 203 logical
cylinders; cylinder 203 is reserved for diagnostics, so the image retains the
204-cylinder capacity used by SV374.
