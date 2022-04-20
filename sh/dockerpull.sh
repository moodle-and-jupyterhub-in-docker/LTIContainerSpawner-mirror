#!/bin/bash

IDSTR="jupyterhub-ltictr"
DKRREP="www.nsl.tuis.ac.jp:5000"

#
LST="jupyter-base jupyter-singleuser jupyter-datascience jupyter-tensorflow jupyter-scipy vhtec_jupyter xeus-cling java php swift-tensorflow"

for IMG in $LST ; do
    echo $IDSTR/$IMG
    docker pull $DKRREP/$IDSTR/$IMG 
done

