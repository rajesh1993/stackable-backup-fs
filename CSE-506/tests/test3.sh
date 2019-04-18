#!/bin/sh
# Testing for backup removal on reaching max backup versions N = 3
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

echo "creating test file..."
echo "hello world 1" > /test/rt/mnt/file_$$.txt
test -f /test/rt/lower/file_$$.txt.bkp001
if [ $? -eq 0 ]; then
    echo file_$$.txt.bkp001 was created
else
    echo file_$$.txt.bkp001 was not created
fi

echo "writing to test file..."
echo "hello world 2" > /test/rt/mnt/file_$$.txt
test -f /test/rt/lower/file_$$.txt.bkp002
if [ $? -eq 0 ]; then
    echo file_$$.txt.bkp002 was created
else
    echo file_$$.txt.bkp002 was not created
fi

echo "writing again to test file..."
echo "hello world 3" > /test/rt/mnt/file_$$.txt
test -f /test/rt/lower/file_$$.txt.bkp003
if [ $? -eq 0 ]; then
    echo file_$$.txt.bkp003 was created
else
    echo file_$$.txt.bkp003 was not created
fi

echo "running user program to view backups..."
../bkpctl -l /test/rt/mnt/file_$$.txt

echo "writing to test file for final time..."
echo "hello world 4" > /test/rt/mnt/file_$$.txt
test -f /test/rt/lower/file_$$.txt.bkp004
if [ $? -eq 0 ]; then
    echo file_$$.txt.bkp004 was created
else
    echo file_$$.txt.bkp004 was not created
fi

echo "Checking if the oldest backup has been removed..."
test -f /test/rt/lower/file_$$.txt.bkp001
if [ $? -eq 0 ]; then
    echo Fail! file_$$.txt.bkp001 was not removed
else
    echo Success! file_$$.txt.bkp001 was removed
fi

# Cleanup
umount -t bkpfs /test/rt/lower /test/rt/mnt
rm -rf /test/rt/
rmmod bkpfs

