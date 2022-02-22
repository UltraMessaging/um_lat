#!/bin/sh
# bld.sh - build the programs on Linux.

# User must have environment set up:
# LBM - path for your LBM platform. Eg: "$HOME/UMP_6.14/Linux-glibc-2.17-x86_64"
# LD_LIBRARY_PATH - path for your LBM libraries. Eg: "$LBM/lib"
# CP - classpath option for java. Eg: "-classpath .:$HOME/UMP_6.14/java/UMS_6.14.jar"
# LBM_LICENSE_INFO - license key. Eg: "Product=LBM:Organization=Your org:Expiration-Date=never:License-Key=..."

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

javac $CP lbmpong.java
if [ $? -ne 0 ]; then echo error in lbmpong.java; exit 1; fi
