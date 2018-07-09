# armboy-init
ArmBoy boot strapper kernel module 

## Description
This kernel module is a system bootstrapper module (module id 4). on boot this module 
attempts to load, modules 1, 2, 3. i.e. Display Driver, Input Driver and File System driver. 
then it, looks for a file at "/boot.bin" and trys to execute it. For example you could write a 
home screen program and place it on the SD card as /boot.bin and this kernel module would launch it 
on system boot.
