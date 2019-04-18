#!/bin/sh
# Testing new backup creation for an overwrite
maxbkp=3
#set -x
mkdir /test/rt
mkdir /test/rt/mnt
mkdir /test/rt/lower
insmod ../../fs/bkpfs/bkpfs.ko
if [ $? -eq 0 ]; then
    echo "Inserted module bkpfs"
else
    echo "Failed to insert module bkpfs"
    exit 1
fi
mount -t bkpfs -o maxver=$maxbkp /test/rt/lower /test/rt/mnt 
if [ $? -eq 0 ]; then
    echo Mounted bkpfs with maxver=$maxbkp
else
    echo "Failed to insert module bkpfs"
    exit 1
fi
echo "creating a test file..."
echo "hello world 1" > /test/rt/mnt/file_$$.txt
test -f /test/rt/lower/file_$$.txt.bkp001
if [ $? -eq 0 ]; then
    echo Success! file_$$.txt.bkp001 was created
else
    echo Fail! file_$$.txt.bkp001 was not created
fi
echo "writing new content to the same test file"
echo "hello world 2" > /test/rt/mnt/file_$$.txt
test -f /test/rt/lower/file_$$.txt.bkp002
if [ $? -eq 0 ]; then
    echo Success! file_$$.txt.bkp002 was created
else
    echo Fail! file_$$.txt.bkp002 was not created
    exit 1
fi
echo "running user program to view backups..."
../bkpctl -l /test/rt/mnt/file_$$.txt

# Cleanup
umount -t bkpfs /test/rt/lower /test/rt/mnt
rm -rf /test/rt/
rmmod bkpfs

