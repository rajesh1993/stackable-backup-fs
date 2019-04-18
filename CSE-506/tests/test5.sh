#!/bin/sh
# Stress testing with 100 backup creation
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

echo "writing 100 times to test file..."

echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt
echo "this is not a backup" > /test/rt/mnt/file_$$.txt

echo "running user program to view backups..."
../bkpctl -l /test/rt/mnt/file_$$.txt

echo "testing to see if the backups are correct..."
test -f /test/rt/lower/file_$$.txt.bkp098
if [ $? -eq 0 ]; then
    echo Success! file_$$.txt.bkp098 exists.
else
    echo Fail! file_$$.txt.bkp098 was not created.
    exit 1
fi

test -f /test/rt/lower/file_$$.txt.bkp099
if [ $? -eq 0 ]; then
    echo Success! file_$$.txt.bkp099 exists.
else
    echo Fail! file_$$.txt.bkp099 was not created.
    exit 1
fi

test -f /test/rt/lower/file_$$.txt.bkp100
if [ $? -eq 0 ]; then
    echo Success! file_$$.txt.bkp100 exists.
else
    echo Fail! file_$$.txt.bkp100 was not created.
    exit 1
fi
# Cleanup
umount -t bkpfs /test/rt/lower /test/rt/mnt
rm -rf /test/rt/
rmmod bkpfs


