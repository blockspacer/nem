#!/bin/sh
set -e

HOST=$1
IDENT=$2
POLICY=$3
if [ \! -d "$HOST" -o -z "$IDENT" -o -z "$POLICY"] ; then
	echo "Usage: $0 <dir> <ident> <policy>"
	exit 1
fi


cat > "$HOST/openssl.cnf" <<EOF
[ ca ]
# man ca
default_ca = CA_default

[ CA_default ]
dir               = `realpath "$HOST"`
certs             = \$dir/certs
crl_dir           = \$dir/crl
new_certs_dir     = \$dir/newcerts
database          = \$dir/index.txt
serial            = \$dir/serial
RANDFILE          = \$dir/private/.rand
private_key       = \$dir/private/$IDENT.key.pem
certificate       = \$dir/certs/$IDENT.cert.pem
crlnumber         = \$dir/crlnumber
crl               = \$dir/crl/$IDENT.crl.pem
crl_extensions    = crl_ext
default_crl_days  = 30
default_md        = sha256

name_opt          = ca_default
cert_opt          = ca_default
default_days      = 375
preserve          = no
policy            = $POLICY

[ policy_strict ]
countryName             = match
stateOrProvinceName     = match
organizationName        = match
organizationalUnitName  = optional
commonName              = supplied
emailAddress            = optional

[ policy_loose ]
countryName             = optional
stateOrProvinceName     = optional
localityName            = optional
organizationName        = optional
organizationalUnitName  = optional
commonName              = supplied
emailAddress            = optional

[ req ]
default_bits        = 2048
distinguished_name  = req_distinguished_name
string_mask         = utf8only
default_md          = sha256
x509_extensions     = v3_ca

[ req_distinguished_name ]
countryName                     = Country Name (2 letter code)
stateOrProvinceName             = State or Province Name
localityName                    = Locality Name
0.organizationName              = Organization Name
organizationalUnitName          = Organizational Unit Name
commonName                      = Common Name
emailAddress                    = Email Address

countryName_default             = US
stateOrProvinceName_default     = Washington
localityName_default            =
0.organizationName_default      = NEM Heavy Industry LLC
emailAddress_default            = root@nem-hi.com

# NOTE: Check x509v3_config for notes on these values
[ v3_ca ]
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid:always,issuer
basicConstraints = critical, CA:true
keyUsage = critical, digitalSignature, cRLSign, keyCertSign

[ v3_intermediate_ca ]
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid:always,issuer
basicConstraints = critical, CA:true, pathlen:0
keyUsage = critical, digitalSignature, cRLSign, keyCertSign

[ nem_cert ]
basicConstraints = critical, CA:FALSE
nsCertType = client, server
nsComment = "$HOST"
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid,issuer:always
keyUsage = critical, nonRepudiation, digitalSignature, keyEncipherment
extendedKeyUsage = critical, clientAuth, serverAuth

[ crl_ext ]
authorityKeyIdentifier=keyid:always

[ ocsp ]
# NOTE: man oscp
basicConstraints = CA:FALSE
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid,issuer
keyUsage = critical, digitalSignature
extendedKeyUsage = critical, OCSPSigning
EOF
