cmd_/home/zhaocc/driverLearn/moduleTest/hello.ko := ld -r -m elf_x86_64 -T /usr/src/linux-headers-3.13.0-119-generic/scripts/module-common.lds --build-id  -o /home/zhaocc/driverLearn/moduleTest/hello.ko /home/zhaocc/driverLearn/moduleTest/hello.o /home/zhaocc/driverLearn/moduleTest/hello.mod.o