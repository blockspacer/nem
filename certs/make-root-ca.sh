#!/bin/sh
set -e 

cd `dirname $0`
HOST=$1

if [ -z "$HOST" ] ; then
	echo "Usage: $0 <hostname>"
	exit 1
fi
if [ -d "$HOST" ] ; then
	echo "Host '$HOST' already exists"
	exit 1
fi

mkdir "$HOST"
mkdir "$HOST/certs" "$HOST/crl" "$HOST/newcerts" "$HOST/private"
chmod 700 "$HOST/private"
touch "$HOST/index.txt"
echo 1000 > "$HOST/serial"

./make-openssl-config.sh "$HOST" "ca" "policy_strict"

openssl genrsa -out "$HOST/private/ca.key.pem" 4096
chmod 400 "$HOST/private/ca.key.pem"

openssl req -config "$HOST/openssl.cnf" \
	-key "$HOST/private/ca.key.pem" \
	-new -x509 -days 3650 -sha256 -extensions v3_ca \
	-out "$HOST/certs/ca.cert.pem" <<EOF





$HOST Root CA

EOF
chmod 444 "$HOST/certs/ca.cert.pem"

openssl x509 -noout -text -in "$HOST/certs/ca.cert.pem"
