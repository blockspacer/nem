#!/bin/sh
set -e

CERTPATH=$1
TARGET=obj/rootcert_raw.o
TARGETCERT=obj/rootcert.crt

if [ -z "$CERTPATH" ] ; then
	echo "Usage: $0 <certpath>"
	exit 1
fi

if [ -f "$TARGET" ] ; then
	rm -f "$TARGET"
fi
if [ -f "$TARGETCERT" ] ; then
	rm -r "$TARGETCERT"
fi

cp "$CERTPATH" "$TARGETCERT"
unset CERTPATH
chmod u+w "$TARGETCERT"
printf '\0' >> "$TARGETCERT"

objcopy \
	--input binary \
	--output elf64-x86-64-freebsd \
	-B i386:x86-64 \
	"$TARGETCERT" "$TARGET"

objcopy \
	--redefine-sym _binary_obj_rootcert_crt_start=NEM_root_cert_pem_raw \
	--redefine-sym _binary_obj_rootcert_crt_size=NEM_root_cert_pem_len_raw \
	--strip-symbol=_binary_obj_rootcert_crt_end \
	"$TARGET" "$TARGET"

rm -f "$TARGETCERT"
