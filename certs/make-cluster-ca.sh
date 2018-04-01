#!/bin/sh
set -e

cd `dirname $0`
HOST=$1
CA=$2

if [ -z "$HOST" -o -z "$CA" ] ; then
	echo "Usage: $0 <hostname> <ca>"
	exit 1
fi
if [ -d "$HOST" ] ; then
	echo "Host '$HOST' already exists"
	exit 1
fi
if [ \! -d "$CA" ] ; then
	echo "CA '$CA' doesn't exist"
	exit 1
fi

mkdir "$HOST"
mkdir "$HOST/certs" "$HOST/crl" "$HOST/csr" "$HOST/newcerts" "$HOST/private"
chmod 700 "$HOST/private"
touch "$HOST/index.txt"
echo 1000 > "$HOST/serial"
echo 1000 > "$HOST/crlnumber"

./make-openssl-config.sh "$HOST" "cluster" "policy_loose"

openssl genrsa -out "$HOST/private/cluster.key.pem" 4096
chmod 400 "$HOST/private/cluster.key.pem"

openssl req -config "$HOST/openssl.cnf" \
	-key "$HOST/private/cluster.key.pem" \
	-new -sha256 \
	-out "$HOST/csr/cluster.csr.pem" <<EOF





$HOST Cluster CA

EOF

openssl ca -config "$CA/openssl.cnf" -extensions v3_intermediate_ca \
	-days 3650 -notext -md sha256 \
	-in "$HOST/csr/cluster.csr.pem" \
	-out "$HOST/certs/cluster.cert.pem"
chmod 444 "$HOST/certs/cluster.cert.pem"

cat "$HOST/certs/cluster.cert.pem" "$CA/certs/ca.cert.pem" > "$HOST/certs/cluster.chain.pem"

openssl x509 -noout -text -in "$HOST/certs/cluster.cert.pem"
openssl verify -CAfile "$CA/certs/ca.cert.pem" \
	"$HOST/certs/cluster.cert.pem"
