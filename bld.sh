#!/bin/sh
# bld.sh - build the programs on Linux.

# Update TOC in doc
for F in *.md; do :
  if egrep "<!-- mdtoc-start -->" $F >/dev/null; then :
    # Update doc table of contents (see https://github.com/fordsfords/mdtoc).
    if which mdtoc.pl >/dev/null; then mdtoc.pl -b "" $F;
    elif [ -x ../mdtoc/mdtoc.pl ]; then ../mdtoc/mdtoc.pl -b "" $F;
    else echo "FYI: mdtoc.pl not found; see https://github.com/fordsfords/mdtoc"; exit 1
    fi
  fi
done

if [ ! -f lbm.sh ]; then :
  echo "No 'lbm.sh', no tools built."
  exit 1
fi
. ./lbm.sh

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

gcc -Wall -g $LIBS \
    -o sock_send cprt.c sock_send.c

javac $CP lbmpong.java
if [ $? -ne 0 ]; then echo error in lbmpong.java; exit 1; fi

javac $CP UmLatPing.java
if [ $? -ne 0 ]; then echo error in UmLatPing.java; exit 1; fi

javac $CP UmLatPong.java
if [ $? -ne 0 ]; then echo error in UmLatPong.java; exit 1; fi
