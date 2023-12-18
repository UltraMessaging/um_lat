#!/bin/sh
# remote_kill.sh - kill ponger process

DIR="$1"; shift
ID="$1"; shift
cd $DIR
  
if [ ! -f "$ID.pid" ]; then :
  echo "remote_kill.sh: ERROR, '$ID.pid' not found." >&2
  exit 1
fi

PID=`cat $ID.pid`
echo "kill $PID"
kill $PID
