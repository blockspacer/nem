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

$CC -o ./bin/server $BUILD_FLAGS $LIBS src/server.c ../libnem/bin/libnem.a
$CC -o ./bin/client $BUILD_FLAGS $LIBS src/client.c ../libnem/bin/libnem.a

$CC -o ./bin/dump-geom $BUILD_FLAGS $LIBS \
	src/dump-geom.c ../libnem/bin/libnem.a -lgeom

