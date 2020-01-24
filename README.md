# fixNES
This is yet another NES Emulator which was written so I can learn about the NES, right now it is not in the most "complete" or cleanest state.  
If you want to check it out for some reason I do include a windows binary in the "Releases" tab, if you want to compile it see the compiling infos below.    

Loading Files:  
NTSC and PAL .nes ROMs are supported, it also creates .sav files if the chosen game supports saving.
If you are starting a PAL NES title then make sure it has (E),(Europe),(PAL),(Australia),(France),(Germany),(Italy),(Spain) or (Sweden) in the filename to be started in PAL mode.  
You can also play FDS titles if you have the FDS BIOS named disksys.rom in the same folder as your .fds/.qd files.  
You can also listen to .nsf music files, changing tracks works by pressing left/right.  
To start a file, simply drag and drop it into the fixNES Application or call it via command line with the file as argument.  
You can also load from a .zip file, the first found supported file from that .zip will be used.    

Supported .nes Mappers:  
0,1,2,3,4,5,7,9,10,11,12,13,15,19,21,22,23,24,25,26,28,30,31,32,33,34,36,37,38,40,41,43,44,45,46,47,48,49,50,52,57,58,60,61,62,64,65,66,67,68,69,70,71,73,75,76,77,78,79,80,82,85,87,88,89,93,94,95,97,101,107,112,113,118,119,132,133,136,137,138,139,140,141,144,145,146,147,148,149,152,154,155,156,158,162,163,164,172,173,174,180,184,185,193,200,201,202,203,205,206,207,210,212,221,224,225,226,228,229,230,231,232,235,237,240 and 242.  
Supported Audio Expansions (for both ROMs and NSF Files):  
VRC6, VRC7, FDS, MMC5, N163, Sunsoft 5B    

Controls right now are keyboard only and do the following:  
Y/Z is A  
X is B  
A is Start  
S is select  
Arrow Keys is DPad  
Keys 1-9 integer-scale the window to number  
P is Pause  
Ctrl+R is Soft Reset  
B is Disk Switching (for FDS)  
O is Enable/Disable vertical Overscan  
If you really want controller support and you are on windows, go grab joy2key, it works just fine with fixNES (and fixGB).    

Infos on compiling:  
The easiest you can do is use the build .bat/.sh files to just call up gcc with the full command line to compile the standalone version.  
For the standalone you will at least need freeglut as well as openal-soft, it should run on most systems since it is fairly generic C code.  
Also you can use this as a core for retroarch by using the Makefile in the libretro folder.    

There are several defines you can add/remove for different options for both the standalone version and the retroarch core:
AUDIO_LOWERFREQ  
Reduces the output frequency of the emulator for more performance  
FAKE_STEREO  
Adds 8ms of delay to the right channel to give a stereo effect to the output  
COL_32BIT  
Changes the output texture to be 32bit per pixel (RGBA8) instead of 16bit (RGB565)  
COL_BGRA  
Used in combination with COL_32BIT, this will change the output texture to BGRA8 instead of RGBA8  
COL_TEX_BSWAP  
In 32bit per pixel mode, this will change the byte order of the output texture, in 16bit mode this will output in BGR565  
DO_INLINE_ATTRIBS  
Will use optimized function inline attributes for more overall performance    

These defines are specific to the standalone version:  
AUDIO_FLOAT  
Will output the audio as 32bit float instead of 16bit short  
COL_GL_BSWAP  
Switches the byte order used for the drawn opengl texture, does not need to be touched if you dont have performance issues on your system  
WINDOWS_BUILD  
Changes some calls to make it easily compilable on windows with something like mingw gcc  
ZIPSUPPORT  
Allows you to load roms directly from .zip files if you have a usable zlib installation  
DISPLAY_PPUWRITES  
Will show red dots each frame of when the PPU was written to which can be useful for understanding certain rendering aspects.  
Also extends the emu window size to show all the normally invisible Hblank/Vblank areas so you can see all write activity.  
DO_4_3_ASPECT  
Instead of rendering at a 1:1 pixel aspect ratio will render the image at a 4:3 aspect ratio, while using interpolation on scaling the image to avoid shimmering and obviously uneven pixels, this is done by first upscaling the image 4x its normal size and then rescaling it using bilinear scaling.
