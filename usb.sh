#!/bin/sh

echo "Writing to USB..."
sudo dd if=./mbr.bin of=/dev/sdb bs=512 count=1 conv=notrunc
sudo dd if=./loader.bin of=/dev/sdb  bs=512 count=4 seek=2 conv=notrunc
sudo dd if=./kernel.bin of=/dev/sdb  bs=512 count=256 seek=9 conv=notrunc

rm -f *.bin
