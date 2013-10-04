export ARCH=arm
#export CROSS_COMPILE=/data/buildroot-AmLogic/output/external-toolchain/bin/arm-none-linux-gnueabi-
#export CROSS_COMPILE=/data/joybox/buildroot-AmLogic/toolchain/arm-2010.09/bin/arm-none-linux-gnueabi-

export CROSS_COMPILE=/data/joybox/buildroot-mx/output/host/usr/bin/arm-linux-gnueabihf-
#export PATH="$PATH":/data/amlogic-common/toolchain/gcc-linaro-arm-linux-gnueabihf-4.7-2013.04-20130415_linux/bin/
#export CROSS_COMPILE=/data/buildroot-AmLogic/output/external-toolchain/bin/arm-linux-gnueabihf-
export CROSS_COMPILE=arm-linux-gnueabihf-
#export CROSS_COMPILE=arm-linux-gnueabi-
#make clean
#make meson6_atv1200_defconfig
#make meson_stv_mbx_mc_atv1200_defconfig
#make meson6_refg02_defconfig
#make oldconfig
#make menuconfig
#cp .config arch/arm/configs/meson6_atv1200_defconfig
#cp .config arch/arm/configs/meson_stv_mbx_mc_atv1200_defconfig
#make vmlinux

TOP=${PWD}
export MKIMAGE=${TOP}/arch/arm/boot/mkimage

#make uImage -j$1 
#make uImage
make modules -j$1 
