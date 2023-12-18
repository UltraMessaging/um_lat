#!/bin/sh
# remote_run.sh - run a command

DIR="$1"; shift
ID="$1"; shift
cd $DIR
  
. ./lbm.sh

eval $* &
PID=$!
echo $PID >$ID.pid
wait $PID
