#!/bin/bash

IDSTR="ltictr"
DKRREP="www.nsl.tuis.ac.jp:5000"

if [ "$1" != "" ]; then
    TAG=$1
else
    echo "usage... $0 tag"
    exit 1
fi

#
LST=`docker images | grep $IDSTR | grep $TAG | awk -F" " '{print $1":"$2}' | grep -v "$DKRREP" | sed -e "s/localhost\///" `

#echo $LST
for IMG in $LST ; do
    REP=`echo $IMG | awk -F":" '{print $1}'`
    REP=$DKRREP"/"$REP
    echo 
    echo "Push $IMG to $REP"
    docker tag  $IMG  $REP
    docker push $REP
    docker rmi  $REP
done

