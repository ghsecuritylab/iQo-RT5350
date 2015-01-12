CC=/home/felix/qodome/RT288x_SDK/toolchain/buildroot-gcc342/bin/mipsel-linux-gcc
LD=/home/felix/qodome/RT288x_SDK/toolchain/buildroot-gcc342/bin/mipsel-linux-ld
CFLAGS= -fno-strict-aliasing -fno-common -Os  -mabi=32 -G 0 -mno-abicalls -fno-pic -pipe -msoft-float -ffreestanding  -march=mips32r2 -Wa,-mips32r2 -Wa,--trap -D__OPTIMIZE__ -fPIC -DDOT11_N_SUPPORT -DRTMP_RBUS_SUPPORT -DRTMP_RF_RW_SUPPORT -DRT305x -DRT5350 -DRTMP_MAC_PCI -DCONFIG_STA_SUPPORT -DDBG -DLINUX -DVCORECAL_SUPPORT -I/home/felix/qodome/ralink_sdk-master/source/ap/src/rt2860v2/include/ -I/home/felix/qodome/ralink_sdk-master/source/linux-2.6.21.x/include/asm-mips/mach-generic/
LDFLAGS=
HAS_DOT11_N_SUPPORT=y
