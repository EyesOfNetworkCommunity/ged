#!/bin/sh

echo "the ged local listener should not set a ca cert and should not verify the peer cert for this command to succeed"
openssl s_client -connect 127.0.0.1:2403 -showcerts 
