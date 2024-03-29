# dp2200sim - a Datapoint 2200 simulator

PLEASE NOTE!  STILL WORK IN PROGRESS.<BR> 
 
At this point it can load CTOS from cassette tape images and boot DOS.C from floppy disks. It also support a local printer which means that it is possible to print from DOS.C to a file.


[![Watch the video](https://i.imgur.com/zhIMYDc.png)](https://youtu.be/XfsMBhP13ww)

Build the simulator using make.

Tested primairly on MACOS but builds on Linux as well.

The actual cpu simulator code is based on a 8008 simultor by Mike Willegal. I have heavily modified it for the Datapoint 2200 and Datapoint 5500 instruction set and wrapped it into C++.

The simulator now also have support for the 9380 floppy drive system, 9350 cardtridge disk and 9370 top-loaded disk. The floppy drive supports four disks. To be able to run DOS.C use the DP1100DisketteBoot.tap from the DOS.C directory. 
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

```
SET CPU=5500
AT F=../DOS.C/001.IMD T=FLOPPY
RESTART
```

All images in the DOS.C directory is bootable in 5500 mode except for 011.IMD (which can only be booted in 2200 mode with the ```DP1100DisketteBoot.tap``` file loaded as described above) and 005.IMD which unfortunately is result of corrupt read (one sector of track 64 is missing).  

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
