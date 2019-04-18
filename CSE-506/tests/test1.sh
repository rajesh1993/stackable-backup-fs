#!/bin/sh
# Testing simple backup creation
mkdir /test/rt
mkdir /test/rt/mnt
mkdir /test/rt/lower
insmod ../../fs/bkpfs/bkpfs.ko
mount -t bkpfs -o maxver=1 /test/rt/lower /test/rt/mnt
echo "creating a file at mount point ..."
echo "hello world" > /test/rt/mnt/file1_$$.txt
echo "Running user program to view current backup files..."
../bkpctl -l /test/rt/mnt/file1_$$.txt
echo "Comparing test file with backup..."
if cmp /test/rt/mnt/file1_$$.txt /test/rt/lower/file1_$$.txt.bkp001; then
	echo "Success! backup created with same contents"
else
	echo "Fail! backup created with incorrect contents"
fi
# Cleanup
umount -t bkpfs /test/rt/lower
rm -rf /test/rt/
rmmod bkpfs

