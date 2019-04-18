cmd_fs/bkpfs/bkpfs.ko := ld -r -m elf_x86_64  -z max-page-size=0x200000 -T ./scripts/module-common.lds  --build-id  -o fs/bkpfs/bkpfs.ko fs/bkpfs/bkpfs.o fs/bkpfs/bkpfs.mod.o ;  true
