EspQtFirmwareLoad is a simple application made to load blocks of firmwares in the correct position in the flash memory.

Input file is a zip file with extention fwrepo which contains all blobs binary images and a index file called firmware_repository_fat.txt

The format of the index file is described below:

0x000000:boot.bin
0x001000:user1.flash.bin
0x081000:user2.flash.bin

