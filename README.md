# fixNES
This is yet another NES Emulator which was written so I can learn about the NES, right now it is not in the most "complete" or cleanest state.  
If you want to check it out for some reason I do include a windows binary in the "Releases" tab, if you want to compile it go check out the "build" files.  
You will need freeglut as well as openal-soft to compile the project, it should run on most systems since it is fairly generic C code.  
NTSC NES ROMs and Mappers 0,1,2,3,4,7,9 and 10 are supported right now, it also creates .sav files if the chosen game supports saving.  
To start a game, simply drag and drop its .nes file into it or call it via command line with the .nes file as argument.    

Controls right now are keyboard only and do the following:  
Y/Z is A  
X is B  
A is Start  
S is select  
Arrow Keys is DPad  
Keys 1-9 integer-scale the window to number  
P is Pause  
O is Enable/Disable vertical Overscan    

That is all I can say about it right now, who knows if I will write some more on it.  