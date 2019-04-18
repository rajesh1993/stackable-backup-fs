#!/bin/sh
# Testing delete backup functionality
maxbkpver=4
mkdir /test/rt
mkdir /test/rt/mnt
mkdir /test/rt/lower
insmod ../../fs/bkpfs/bkpfs.ko
mount -t bkpfs -o maxver=$maxbkpver /test/rt/lower /test/rt/mnt

echo "Creating a sample file and writing total 4 times..."
echo "hello world 1" > /test/rt/mnt/file_$$.txt
echo "hello world 2" > /test/rt/mnt/file_$$.txt
echo "hello world 3" > /test/rt/mnt/file_$$.txt
echo "hello world 4" > /test/rt/mnt/file_$$.txt

echo "Viewing existing versions..."
../bkpctl -l /test/rt/mnt/file_$$.txt

echo "Calling user prgram to delete oldest version..."
../bkpctl -d oldest /test/rt/mnt/file_$$.txt

test -f /test/rt/lower/file_$$.txt.bkp001
if [ $? -ne 0 ]; then
    echo Success! file_$$.txt.bkp001 deleted.
else
    echo Fail! file_$$.txt.bkp001 was not deleted.
fi

echo "Calling user program to delete newest version..."
../bkpctl -d newest /test/rt/mnt/file_$$.txt

test -f /test/rt/lower/file_$$.txt.bkp004
if [ $? -ne 0 ]; then
    echo Success! file_$$.txt.bkp004 deleted.
else
    echo Fail! file_$$.txt.bkp004 was not deleted.
fi

echo "calling user program to delete all bkps..."

../bkpctl -d all /test/rt/mnt/file_$$.txt

test -f /test/rt/lower/file_$$.txt.bkp002
if [ $? -ne 0 ]; then
    echo Success! file_$$.txt.bkp002 deleted.
else
    echo Fail! file_$$.txt.bkp002 was not deleted.
fi

test -f /test/rt/lower/file_$$.txt.bkp003
if [ $? -ne 0 ]; then
    echo Success! file_$$.txt.bkp003 deleted.
else
    echo Fail! file_$$.txt.bkp003 was not deleted.
fi
# Cleanup
umount -t bkpfs /test/rt/lower
rm -rf /test/rt/
rmmod bkpfs

