#!/bin/bash
set -xe
cp ../sdcard.img disk.img
mv disk.img disk.img.ori
# dd if=disk.img.ori of=disk.img bs=`../fat32Reader disk.img.ori | grep "End size:" | cut -d ' ' -f 3` count=1
dd if=disk.img.ori of=disk.img bs=`./fat32Reader disk.img.ori | grep "last non-empty" | cut -d ' ' -f 3` count=1
./ringBuffer disk.img
truncate -s +16 disk.img.lz4s-0
# cp /home/wmj/k210-sd-dump/flash/build_sddd_rom/flash-list.json .
# cp /home/wmj/k210-sd-dump/flash/build_sddd_rom/sddd_rom.bin .
# cp /home/wmj/k210-sd-dump/flash/build_sddd_rom/ktool.py .
zip img.kfpkg sddd_rom.bin flash-list.json disk.img.lz4s-0
echo "sudo python3 ktool.py -B dan -b 3000000 -p /dev/ttyUSB0 img.kfpkg -t"