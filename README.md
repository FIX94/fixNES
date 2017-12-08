# fixNES
This is yet another NES Emulator which was written so I can learn about the NES, right now it is not in the most "complete" or cleanest state.  
If you want to check it out for some reason I do include a windows binary in the "Releases" tab, if you want to compile it go check out the "build" files.  
You will need freeglut as well as openal-soft to compile the project, it should run on most systems since it is fairly generic C code.  
NTSC and PAL .nes ROMs are supported right now, it also creates .sav files if the chosen game supports saving.  
Supported Mappers:  
0,1,2,3,4,5,7,9,10,11,12,13,15,19,21,22,23,24,25,26,33,34,36,37,38,41,44,45,46,47,48,52,57,58,61,62,66,67,68,69,70,71,75,76,78,79,85,87,88,89,93,94,95,97,99,101,112,113,118,119,133,140,144,145,146,147,148,149,152,154,174,180,184,185,200,201,202,203,205,206,210,212,225,226,228,232,232,240 and 242.  
Supported Audio Expansions:  
VRC6, VRC7, FDS, MMC5, N163, Sunsoft 5B  
Normal .nes files are supported, if you are starting a PAL NES title then make sure it has (E),(Europe),(Australia),(France),(Germany),(Italy),(Spain) or (Sweden) in the filename to be started in PAL mode.  
You can also play FDS titles if you have the FDS BIOS named disksys.rom in the same folder as your .fds/.qd files.  
You can also listen to .nsf files, changing tracks works by pressing left/right.  
To start a file, simply drag and drop it into the fixNES Application or call it via command line with the file as argument.  
You can also use a .zip file, the first found supported file from that .zip will be used.    

Controls right now are keyboard only and do the following:  
Y/Z is A  
X is B  
A is Start  
S is select  
Arrow Keys is DPad  
Keys 1-9 integer-scale the window to number  
P is Pause  
B is Disk Switching (for FDS)  
O is Enable/Disable vertical Overscan  
If you really want controller support and you are on windows, go grab joy2key, it works just fine with fixNES (and fixGB).    

That is all I can say about it right now, who knows if I will write some more on it.  
