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
	-licuuc
	-lcxxrt
	-lz
	-lgeom
	-lbson-1.0
	/home/lye/code/libtoml2/bin/libtoml2.a
"

BUILD_FLAGS="
	-g
	-Wall -Werror -Wno-unused-function
	-fno-omit-frame-pointer
	-fsanitize=undefined
	-ferror-limit=5
	-std=c11
	-isystem/usr/local/include
	-isystem/home/lye/code/libtoml2/inc
	-Iinc
	-I../libnem/inc
"

mkdir -p bin
mkdir -p obj

OBJ_FILES=

for f in src/* ; do
	OBJ_FILE=`echo $f | sed -e 's#^src#obj#' -e 's#.c$#\.o#'`
	$CC -c -o $OBJ_FILE $BUILD_FLAGS $f

	if [ $f != 'src/main.c' ] ; then 
		OBJ_FILES="$OBJ_FILES $OBJ_FILE"
	fi
done

$CC \
	$BUILD_FLAGS \
	$OBJ_FILES \
	obj/main.o \
	-o ./bin/nem-rootd \
	$LIBS \
	../libnem/bin/libnem.a

$CC \
	$BUILD_FLAGS \
	$OBJ_FILES \
	test/*.c \
	-o ./bin/rootd.test \
	$LIBS -lcheck \
	../libnem/bin/libnem.a

./bin/rootd.test
