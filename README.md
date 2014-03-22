rpi_osc_video_player
====================
Video player for Raspberry Pi with perspective correction and OSC control
Antoine Villeret - 2013

Dependencies : `liblo`, `opencv` and `ilclient` to get the first two, just run : 

`$ sudo apt-get install liblo-dev libopencv-dev`

for ilclient, you must build it yourself (anyhow, it's easy) :

~~~~
$ cd /opt/vc/src/hello_pi
$ make -C libs/ilclient
$ make -C libs/vgfont
~~~~
vgfont is not used yet, so it's optional...

to build `rpi_osc_video_player.bin`, just run (in the folder download from git) :

`$ make`

to use :

`$ ./osc_video_player.bin`

and then start an OSC application (for example the puredata patch in example/ folder)
