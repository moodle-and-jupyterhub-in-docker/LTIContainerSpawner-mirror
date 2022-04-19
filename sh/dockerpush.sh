#!/bin/bash

IDSTR="ltictr"
DKRREP="www.nsl.tuis.ac.jp:5000"

#
LST=`docker images | grep $IDSTR | awk -F" " '{print $1":"$2}' | grep -v "$DKRREP" | sed -e "s/localhost\///" `

#echo $LST
for IMG in $LST ; do
    echo $IMG
    REP=`echo $IMG | awk -F":" '{print $1}'`
    REP=$DKRREP"/"$REP
    docker tag  $IMG  $REP
    docker push $REP
    docker rmi  $REP
done

