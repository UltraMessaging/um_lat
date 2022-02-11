#!/bin/sh
# bld.sh - build the programs on Linux.

# For Linux
LIBS="-l pthread -l m -l rt"

gcc -Wall -g $LIBS \
    -o um_lat_jitter cprt.c um_lat_jitter.c
if [ $? -ne 0 ]; then echo error in um_lat_jitter.c; exit 1; fi

gcc -Wall -g -I $LBM/include -I $LBM/include/lbm -L $LBM/lib -l lbm $LIBS \
    -o um_lat_ping cprt.c um_lat_ping.c
if [ $? -ne 0 ]; then echo error in um_lat_ping.c; exit 1; fi

gcc -Wall -g -I $LBM/include -I $LBM/include/lbm -L $LBM/lib -l lbm $LIBS \
    -o um_lat_pong cprt.c um_lat_pong.c
if [ $? -ne 0 ]; then echo error in um_lat_pong.c; exit 1; fi
