#!/bin/sh
# Testing if file is hidden in mount point
mkdir /test/rt
mkdir /test/rt/mnt
mkdir /test/rt/lower
#umount -t bkpfs /test/rt/lower /test/rt/mnt
#rmmod bkpfs
insmod ../../fs/bkpfs/bkpfs.ko
mount -t bkpfs -o maxver=5 /test/rt/lower /test/rt/mnt
echo "hello world" > /test/rt/mnt/file_$$.txt
../bkpctl -l /test/rt/mnt/file_$$.txt
ls /test/rt/mnt/
test -f /test/rt/mnt/file_$$.txt.bkp001
if [ $? -eq 0 ]; then
    echo "Success! file is present but not visible"
else
    echo "Fail! file is not present at mount point"
fi
# Cleanup
umount -t bkpfs /test/rt/lower /test/rt/mnt
rm -rf /test/rt/
rmmod bkpfs

