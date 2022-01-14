#!/bin/bash

DKRREP="www.nsl.tuis.ac.jp:5000"

#
LST=`docker images | grep ltictr | awk -F" " '{print $1":"$2}' | grep -v "$DKRREP" | sed -e "s/localhost\///" `

echo $LST
for IMG in $LST ; do
    REP=`echo $IMG | awk -F":" '{print $1}'`
    REP=$DKRREP"/"$REP
    docker tag  $IMG  $REP
    docker push $REP
    docker rmi  $REP
done

