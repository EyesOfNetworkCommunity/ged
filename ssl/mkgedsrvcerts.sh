#!/bin/sh

mkdir easy-rsa/keys
cd easy-rsa
source ./vars
./clean-all
./pkitool --initca
./pkitool --server $1
./build-dh
cd -
cp easy-rsa/keys/dh1024.pem easy-rsa/keys/ca.crt easy-rsa/keys/ca.key easy-rsa/keys/$1.crt easy-rsa/keys/$1.key .
chown nobody.nobody dh1024.pem ca.crt ca.key $1.crt $1.key
chmod 600 dh1024.pem ca.crt ca.key $1.crt $1.key
