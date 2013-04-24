rpi_osc_video_player
====================
Video player for Raspberry Pi with perspective correction and OSC control
Antoine Villeret - 2013

Dependencies : liblo and opencv
to get it, just run : 
$ sudo apt-get install liblo7 libopencv-dev

to build, just run :
$ make

to use :
$ ./osc_video_player.bin

and then start an OSC application (for example the puredata patch in example/ folder)
