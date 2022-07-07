#!/bin/bash
#

if [ "$1" != "" ]; then
    TAG=$1
else
    echo "usage... $0 tag"
    exit 1
fi

#
LST=`docker images | grep $TAG | awk -F" " '{print $1":"$2}'`

for IMG in $LST ; do
    echo "delete .... $IMG"
    docker rmi  $IMG
done

