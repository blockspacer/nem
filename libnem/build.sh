#!/bin/sh -e

cd `dirname $0`

CC=clang37
SOURCE_FILES=src/*.c
OBJ_FILES=
OBJ_MAIN=

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
"

mkdir -p bin
mkdir -p obj

rm -f obj/*.o

for C_FILE in src/*.c ; do
	OBJ_FILE=`echo $C_FILE | sed -e 's#\.c$#.o#' | sed -e 's#^src/#obj/#'`
	OBJ_FILES="$OBJ_FILES $OBJ_FILE"

	$CC \
		-c \
		-o $OBJ_FILE \
		$BUILD_FLAGS \
		$C_FILE
done

if [ -f bin/libnem.a ] ; then
	rm bin/libnem.a
fi

ar -rc bin/libnem.a $OBJ_FILES

$CC \
	$BUILD_FLAGS \
	$OBJ_FILES \
	./bin/libnem.a \
	-lcheck test/*.c \
	$LIBS \
	-o bin/libnem.test

./bin/libnem.test

