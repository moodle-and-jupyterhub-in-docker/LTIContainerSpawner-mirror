#!/bin/bash
#
# usage ... docker_rsock.sh host_name user_name user_password socket_file
#
#  ex.) ./docker_rsock.sh docker.hogebar.jp docker passwd  /tmp/docker.sock
#       docker -H unix:///tmp/mdlds_docker.hogebar.jp.sock ps
#

if [ -n "$SSH_PASSWORD" ]; then
    echo $SSH_PASSWORD
    exit 0
fi

#
if [ $# -lt 3 ]; then
    exit 1
fi

printf -v SSH_HOST '%q' "$1"
printf -v SSH_USER '%q' "$2"
printf -v SSH_PASS '%q' "$3"

if [ "$4" != "" ]; then
    printf -v LLSOCKET '%q' "$4"
else
    LLSOCKET=/tmp/mdlds_${SSH_HOST}.sock
fi

WEBGROUP=`groups`
RTSOCKET=/var/run/docker.sock

#
export SSH_PASSWORD=$SSH_PASS
export SSH_ASKPASS=$0
export DISPLAY=:0.0

rm -f $LLSOCKET
ps ax | grep ssh | grep "${LLSOCKET}:${RTSOCKET}" | awk -F" " '{print $1}' | xargs kill -9 > /dev/null 2>&1

#
setsid ssh -oStrictHostKeyChecking=no -oServerAliveInterval=120 -oServerAliveCountMax=3 -fNL ${LLSOCKET}:${RTSOCKET} ${SSH_USER}@${SSH_HOST} 

#
CNT=0
while [ ! -e $LLSOCKET ]; do
    sleep 1
    CNT=`expr $CNT + 1`
    if [ $CNT -gt 5 ]; then
        exit 1
    fi
done

chmod g+rw $LLSOCKET
chgrp $WEBGROUP $LLSOCKET

