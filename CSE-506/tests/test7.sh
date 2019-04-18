#!/bin/sh
# Testing Restore oldest backup functionality
maxbkpver=3
mkdir /test/rt
mkdir /test/rt/mnt
mkdir /test/rt/lower
insmod ../../fs/bkpfs/bkpfs.ko
mount -t bkpfs -o maxver=$maxbkpver /test/rt/lower /test/rt/mnt
echo "Creating a sample file and writing three times..."
for i in {1...3}
do
	echo "hello world" > /test/rt/mnt/file_$$.txt
done
echo "Viewing existing versions..."
../bkpctl -l /test/rt/mnt/file_$$.txt

echo "Calling user prgram to restore oldest version..."
../bkpctl -r oldest /test/rt/mnt/file_$$.txt

if cmp /test/rt/mnt/file_$$.txt.bkpt /test/rt/lower/file_$$.txt.bkp001; then
	echo "Success! restore tempfile created with oldest file contents"
else
	echo "Fail! restore tempfile incorrect"
fi

# Cleanup
umount -t bkpfs /test/rt/lower
rm -rf /test/rt/
rmmod bkpfs

