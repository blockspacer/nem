#!/bin/sh -e

cd `dirname $0`

CC=clang50
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
	-lbson-1.0
	-ljson-c
	-lyaml
	-licuuc
	-lcxxrt
	../../libtoml2/bin/libtoml2.a
"

BUILD_FLAGS="
	-g
	-Wall -Werror -Wno-unused-function
	-fno-omit-frame-pointer
	-fsanitize=undefined
	-ferror-limit=5
	-std=c11
	-isystem/usr/local/include
	-isystem/usr/local/include/json-c
	-isystem/usr/local/include/libbson-1.0
	-isystem../../libtoml2/inc
	-Iinc
"

mkdir -p bin
mkdir -p obj

for f in obj/* ; do
	if [ "$f" != "obj/rootcert_raw.o" ] ; then
		rm -f "$f"
	fi
done

if [ \! -f obj/rootcert_raw.o ] ; then
	echo "Missing rootcert_raw.o; generate with obj_cert.sh"
	exit 1
fi

OBJ_FILES="$OBJ_FILES obj/rootcert_raw.o"

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

