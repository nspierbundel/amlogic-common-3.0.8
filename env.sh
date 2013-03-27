export ARCH=arm
export CROSS_COMPILE=/data/buildroot-AmLogic/output/external-toolchain/bin/arm-none-linux-gnueabi-
#export CROSS_COMPILE=/data/toolchain/arm-2010.09/bin/arm-none-linux-gnueabi-

make clean
make meson6_atv1200_defconfig

make menuconfig
cp .config arch/arm/configs/meson6_atv1200_defconfig
#make vmlinux

TOP=${PWD}
export MKIMAGE=${TOP}/arch/arm/boot/mkimage
make uImage -j16
#make uImage
#make modules  -j16
