#!/bin/sh
# Testing Restore n (1...N) backup functionality
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

echo "Calling user prgram to restore backup version 2..."
../bkpctl -r 2 /test/rt/mnt/file_$$.txt

if cmp /test/rt/mnt/file_$$.txt.bkpt /test/rt/lower/file_$$.txt.bkp002; then
	echo "Success! restore tempfile created with bkp file 2's contents"
else
	echo "Fail! restore tempfile incorrect"
fi

# Cleanup
umount -t bkpfs /test/rt/lower
rm -rf /test/rt/
rmmod bkpfs

