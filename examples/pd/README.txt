client.pd is a puredata patch
it connects to osc_video_player on RPi
then you can send video corner coordinates or directly set the perspective matrix

you should have a bin/ directory in this folder with binaries for [udpsend] and [packOSC] (from Mr Peach)

you will find binaries for x86 64bit (build on Ubuntu) in bin_x86_64/ and for the Raspberry Pi in bin_rpi/ (build on Raspbian)
just symlink it to bin/ :
$ ln -s bin_x86_64 bin
if you are on a 64 bit system
