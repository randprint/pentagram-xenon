make -f Makefile.xenon clean
rm *.elf32
sh configure-xenon.sh
make -f Makefile.xenon
sh make-elfs.sh
