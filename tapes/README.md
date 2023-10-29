# Tapes

Thid directory contains various tapes that originate from bitsavers. I wrote a simple program, dp2200tap.c, that reads the tap-formatted files and tries to verify the contents.

Some files verify ok other seems to have some type of problem.

This is the result from running the program on ```tstsca1.3_1-73.tap```

It reads each record on the .tap file and tries to analyze it. 


```
This is something else. Only the first boot block should be like this. 
This is a FileHeader. The filenumber is 0. 
This is a numeric record with 256 bytes. Load address for this block is 14000. 
This is a numeric record with 256 bytes. Load address for this block is 14370. 
This is a numeric record with 256 bytes. Load address for this block is 14760. 
.
.
.
This is a numeric record with 8 bytes. Load address for this block is 03750. 
This is a FileHeader. The filenumber is 1. 
This is a numeric record with 124 bytes. Load address for this block is 17406. 
This is a FileHeader. The filenumber is 2. 
This is a numeric record with 256 bytes. Load address for this block is 01000. 
This is a numeric record with 256 bytes. Load address for this block is 01370. 
.
.
. 
This is a numeric record with 20 bytes. Load address for this block is 16730. 
This is a numeric record with 8 bytes. Load address for this block is 01172. 
This is a FileHeader. The filenumber is 32. 
This is a FileHeader. The filenumber is 127. This is the end of tape marker. Files beyond this point is probably damaged.
```

Analyzing the .tap files using hexdump also show that ```payrollMastCtos_6-75.tap``` file has a strange boot block as if it misses the block gap and continues into next block. It is much longer than normal boot blocks. This may or may not be a problem.


## The following files seems to be corrupted:

### adsMstr_6-76.tap
```
This is a FileHeader. The filenumber is 0. 
This is a numeric record with 207 bytes. Load address for this block is 50122. Loading address corrupted.
This is a numeric record with 254 bytes. Load address for this block is 50122. Loading address corrupted.
This is a numeric record with 254 bytes. Load address for this block is 50122. Loading address corrupted.
.
.
```

### endure1.6_7-73.tap
```This is something else. Only the first boot block should be like this. 
This is a FileHeader. The filenumber is 127. This is the end of tape marker. Files beyond this point is probably damaged.
This is something else. Only the first boot block should be like this
```

### tapex1.1_mar75.tap

```This is something else. Only the first boot block should be like this. 
This is a FileHeader. The filenumber is 0. 
This is something else. Only the first boot block should be like this. 
This is something else. Only the first boot block should be like this. 
This is a numeric record with 256 bytes. Load address for this block is 02404. 
This is a numeric record with 256 bytes. Load address for this block is 02774. 
```
### tstdis1.1_3-75-clean.tap
```This is something else. Only the first boot block should be like this. 
This is a FileHeader. The filenumber is 0. 
This is a numeric record with 136 bytes. Load address for this block is 01000. 
This is a numeric record with 136 bytes. Load address for this block is 01200. 
This is a numeric record with 43 bytes. The checksum is NOT OK.Load address for this block is 01400. 
This is a numeric record with 25 bytes. The checksum is NOT OK.Load address for this block is 01600. 
This is a numeric record with 136 bytes. Load address for this block is 02000. 
```
### rpgII3.2a_75.tap
```This is a FileHeader. The filenumber is 0. 
This is a numeric record with 41 bytes. Load address for this block is 51120. Loading address corrupted.
This is a FileHeader. The filenumber is 1. 
This is a numeric record with 35 bytes. Load address for this block is 20000. 
This is a numeric record with 221 bytes. Load address for this block is 20773.```
```
### tstdis1.1_3-75.tap
```
This is something else. Only the first boot block should be like this. 
This is a FileHeader. The filenumber is 0. 
This is a numeric record with 136 bytes. Load address for this block is 01000. 
This is a numeric record with 16 bytes. The checksum is NOT OK.Load address for this block is 01200. 
This is a numeric record with 8 bytes. The checksum is NOT OK.Load address for this block is 01400. 
This is a numeric record with 136 bytes. Load address for this block is 01600.
```
### tstpro1.1.tap


```This is something else. Only the first boot block should be like this. 
This is a FileHeader. The filenumber is 127. This is the end of tape marker. Files beyond this point is probably damaged.
This is a numeric record with 105 bytes. Load address for this block is 30060. Loading address corrupted.
This is a numeric record with 105 bytes. Load address for this block is 30060. Loading address corrupted.
```
