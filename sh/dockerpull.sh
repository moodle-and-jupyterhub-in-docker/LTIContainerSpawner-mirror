#!/bin/bash

IDSTR="jupyterhub-ltictr"
DKRREP="www.nsl.tuis.ac.jp:5000"

#
LST="jupyter-base jupyter-singleuser jupyter-datascience jupyter-tensorflow jupyter-scipy vhtec_jupyter xeus-cling java php swift-tensorflow"

for IMG in $LST ; do
    echo 
    echo Pull $IDSTR/$IMG
    IMGID=`docker images | grep "$DKRREP/$IDSTR/$IMG" | grep latest | awk -F" " '{print $3}'`
    docker pull $DKRREP/$IDSTR/$IMG 
    #
    if [ "$IMGID" != "" ]; then
        NONE=`docker images | grep $IMGID | grep "\<none\>"`
        if [ "$NONE" != "" ]; then
            TMPN="$DKRREP/$IDSTR/$IMG:del"
            docker tag $IMGID $TMPN
            docker rmi $TMPN
        fi
    fi   
done

