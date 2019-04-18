#!/bin/sh
# Testing Restore newest backup functionality
maxbkpver=3
mkdir /test/rt
mkdir /test/rt/mnt
mkdir /test/rt/lower
insmod ../../fs/bkpfs/bkpfs.ko
mount -t bkpfs -o maxver=$maxbkpver /test/rt/lower /test/rt/mnt

echo "Creating a sample file and writing three times..."
echo "hello world" > /test/rt/mnt/file_$$.txt
echo "hello world" > /test/rt/mnt/file_$$.txt
echo "hello world" > /test/rt/mnt/file_$$.txt

echo "Viewing existing versions..."
../bkpctl -l /test/rt/mnt/file_$$.txt

echo "Calling user program to restore newest version..."
../bkpctl -r oldest /test/rt/mnt/file_$$.txt

if cmp /test/rt/mnt/file_$$.txt.bkpt /test/rt/lower/file_$$.txt.bkp003; then
	echo "Success! restore tempfile created with oldest file contents"
else
	echo "Fail! restore tempfile incorrect"
fi

# Cleanup
umount -t bkpfs /test/rt/lower
rm -rf /test/rt/
rmmod bkpfs

