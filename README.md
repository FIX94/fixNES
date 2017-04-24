# fixNES
This is yet another NES Emulator which was written so I can learn about the NES, right now it is not in the most "complete" or cleanest state.  
If you want to check it out for some reason I do include a windows binary in the "Releases" tab, if you want to compile it go check out the "build" files.  
You will need freeglut as well as openal-soft to compile the project, it should run on most systems since it is fairly generic C code.  
NTSC and PAL .nes ROMs are supported right now, it also creates .sav files if the chosen game supports saving.  
Supported Mappers: 0,1,2,3,4,7,9,10,11,13,15,21,22,23,24,25,26,33,34,36,37,38,44,45,46,47,48,52,61,66,70,71,75,78,79,85,87,89,93,94,97,99,101,113,133,140,144,145,146,147,148,149,152,180,184,185,240 and 242.  
To start a game, simply drag and drop its .nes file into it or call it via command line with the .nes file as argument.  
If you are starting a PAL NES title then make sure it has (E) in the name to be started in PAL mode.  
You can also play FDS titles if you have the FDS BIOS named disksys.rom in the same folder as your .fds/.qd files.  
You can also listen to .nsf files by dragging them in, changing tracks works by pressing left/right.    

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

That is all I can say about it right now, who knows if I will write some more on it.  