#!/bin/bash
#
#  docker -H unix://$LLSOCKET ps
#

SSH_USER=docker
SSH_PASS=********
SSH_HOST=202.26.150.55
WEBGROUP=apache

LLSOCKET=/var/run/mdlds.sock
RTSOCKET=/var/run/docker.sock

#
rm -f $LLSOCKET

if [ -n "$PASSWORD" ]; then
    echo "$PASSWORD"
    exit 0
fi

#
export PASSWORD=$SSH_PASS
export SSH_ASKPASS=$0
export DISPLAY=:0.0

setsid ssh -fNL $LLSOCKET:$RTSOCKET $SSH_USER@$SSH_HOST 

#
while [ ! -e $LLSOCKET ]; do
    sleep 5
done
chmod g+rw $LLSOCKET
chgrp $WEBGROUP $LLSOCKET
