#!/bin/sh
set -e

HOST=$1
CLUSTER=$2
if [ -z "$HOST" -o -z "$CLUSTER" ] ; then
	echo "Usage: $0 <host> <cluster>"
	exit 1
fi
if [ -d "$HOST" ] ; then
	echo "Host $0 already exists"
	exit 1
fi
if [ \! -d "$CLUSTER" ] ; then
	echo "Cluster $0 doesn't exist"
	exit 1
fi

mkdir "$HOST"

openssl genrsa -out "$HOST/key.pem" 2048
chmod 400 "$HOST/key.pem"

openssl req -config "$CLUSTER/openssl.cnf" \
	-key "$HOST/key.pem" \
	-new -sha256 -out "$HOST/csr.pem" <<EOF





$HOST

EOF

openssl ca -config "$CLUSTER/openssl.cnf" \
	-extensions nem_cert -days 375 -notext -md sha256 \
	-in "$HOST/csr.pem" \
	-out "$HOST/cert.pem"

openssl x509 -noout -text \
	-in "$HOST/cert.pem"

cat "$HOST/cert.pem" "$CLUSTER/certs/cluster.chain.pem" > "$HOST/chain.pem"

openssl verify -CAfile "$CLUSTER/certs/cluster.chain.pem" \
	"$HOST/cert.pem"
