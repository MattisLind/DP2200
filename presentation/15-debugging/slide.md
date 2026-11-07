# Tracking down the fault

The tape loaded. The screen stayed blank.

The simulator helped trace the fault to a 7404.

## Pictures to add

- Logic analyzer captures, the failing instruction, schematic detail, and the faulty chip.
- Place image files in this folder and link them with relative paths, for example `![Description](photo.jpg)`.
- Record image sources and credits here.

## Speaker notes

Late 2025 I finally got to the point I could start debug it. The machine is very simple so there is no ROM in it. The boot process is controlled by logic that simply reads a full block into memory form address 0 and at the end of the block it. 

It was able to read the full tape but there were no printout on the screen. Attaching the logic analyzer at various points in the machine made it possible to track down the problem.

Did the really read the contents of the tape correctly ? - Yes it did.
Was it written into memory - Yes it was.
Was it executing instructions correctly. Yes it at least started.

By comparing with the simulator I found a certain instruction that behaved differently.

From studying the schematic and when I know what instruction that failed it was quite clear that it could only be one chip that was the problem which was confirmed by the logic analyzer.

A 7404 was broken.
