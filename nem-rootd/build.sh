#!/bin/sh -e

cd `dirname $0`

CC=clang37
SOURCE_FILES=src/*.c

if `which ccache > /dev/null 2>&1` ; then
	CC="ccache $CC"
fi

LIBS="
	-L/usr/local/lib
	-lexecinfo
	-lz
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
"

mkdir -p bin

$CC -o ./bin/nem-rootd $BUILD_FLAGS $LIBS src/*.c ../libnem/bin/libnem.a

