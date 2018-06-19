#!/bin/sh -e

cd `dirname $0`

CC=clang50
SOURCE_FILES=src/*.c

if `which ccache > /dev/null 2>&1` ; then
	CC="ccache $CC"
fi

LIBS="
	-L/usr/local/lib
	-lexecinfo
	-lz
	-lbson-1.0
	-lgeom
"

BUILD_FLAGS="
	-g
	-Wall -Werror -Wno-unused-function
	-fno-omit-frame-pointer
	-fsanitize=undefined
	-ferror-limit=5
	-std=c11
	-isystem/usr/local/include
	-Iinc
	-I../libnem/inc
	-I../libnemsvc/inc
"

mkdir -p bin

NORMAL_THINGS="
	dump-mount
	store-geom
"

for exe in $NORMAL_THINGS ; do
	$CC -o ./bin/$exe $BUILD_FLAGS $LIBS src/$exe.c \
		../libnem/bin/libnem.a \
		../libnemsvc/bin/libnemsvc.a 
done

#mkdir -p obj/client.img/bin
#mkdir -p obj/client.img/lib
#mkdir -p obj/client.img/libexec
#cp `ldd -f '%p ' bin/client` obj/client.img/lib
#cp bin/client obj/client.img/bin
#cp /libexec/ld-elf.so* obj/client.img/libexec
#makefs bin/client.img obj/client.img > /dev/null
#rm -rf obj/client.img
