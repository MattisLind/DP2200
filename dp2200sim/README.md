# dp2200sim - a Datapoint 2200 simulator

PLEASE NOTE!  THIS IS NOT WORKING AT ALL. JUST WORK IN PROGRESS.

## Requirements

* Mulithreaded using pthreads.
* One main thread for ncurses and keyboard handling / command processing
* One thread for the CPU simulator
* One IO simulator handling cassette, screen, keyboard interaction. Possibly other IO devices like printers, terminals, disks etc
* Main thread use ncurses with three windows.
* One DP2200 window fixed at 80x12.
* One CPU status / register window to the right. Variable size.
* One command window below these two windows.
* Tab is used to switch between windows. The active window border is highlighted.
* The size of windows is calculated from the total size of the terminal window.
* If to small a message will be shown to increase the size.
* Should detect SIGWINCH and redraw when term size is changing.
* The command window should have a history.
* When a printout is overflowing the window it should display ”more” so that the user can press space to not miss anything.
* It should be possible to disable the feature above.
* The register window shall be updated continously when the CPU is running.
* When CPU is halted it shall be possible to change the contents of the registers.
* The IO thread is using mutex to write messages to the main thread for display printout.
* The main thread takes keypresses in the DP2200 window and translates them into a code relevant for the DP2200.
* When a key is pressed and translated a flag is set. This flag is protected by a mutex.
* The IO thread does a trylock to read this flag. If successful it read the flag and if set it will read out the keypress and reset the flag.
* The CPU thread counts a global realtime which is updated for each instruction executed. In nanoseconds.
* All IO operations is checked towards this global realtime so that IO takes place in a way that is relevant to the CPU. Not too fast.
* The main thread is reading the keyboard unprocessed and using non-blocking IO.
* If the cassettes are running they are locked and it should not be possible to be changed by the command line commands. A mutex protects.
* CPU simulator has global state. Registers, stack and flags. There is then a run/halt flag and a singlestep flag.
* The run/halt flag is checked as the first thing before fetching the instruction. If the flag is set to halt it will wait 10 ms and then retry. Polling the flag at 100Hz.
* If the single step flag is set then when command line tool do a continue command causing the halt run flag to be set then the simulator sees the single step flag and clears the halt/run flag to halt condition (which is then checked on next iteration).
* C/C++? Probably C++.
* Atomic for shared global data to guarantee that the updates made from CPUsimulator thread can be read properly from the IO-processing thread or the main thread for updating the register window.
* Kommandofönstret: kan man scrolla ett fönster? En scroll-region. Man vill inte att fönstrets border scrollas, bara innehållet.
* Commandcompletion. Pressing ? Gives a printout of all matching commands or fill of a single matching command.
* Pressing ? after whitespace in command gives first param. Pressing ? again gives next param ignoring params that has been given.
* If one or more characters of the param is given only those will be part of the param completion toggling.
* Params are delimited with =. I.e param1=23.
* Pressing ? After the = give default value. Pressing ? Toggles between max, min and default.
* CPU simulator should handle breakpoints. Before running an instruction. I.e doing the fetch it should check if the current address is among breakpoints and then halt.
* It should be possible to run program at real speed. I.e. an instruction would sleep the correct amount of time.
* It should be able to simulate both the model 1 and model 2. Possibly also later 5500 and 6600?
* The amount of memory shall be configurable.
* IO devices shall be configurable.
* Watchpoints. If it accesses a certain memory location it shall halt. But that would happen mid-instruction. How to continue?? Probably stop after this instruction.
* IO watch. Stop after a certain Input instruction or EX instruction when a specific address has been given.
