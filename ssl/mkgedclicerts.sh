#!/bin/sh

cd easy-rsa
source ./vars
./pkitool $1
cd -
cp easy-rsa/keys/$1.crt easy-rsa/keys/$1.key .
chown nobody.nobody $1.crt $1.key
chmod 644 $1.crt $1.key
