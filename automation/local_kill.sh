#!/bin/sh
# local_kill.sh - run on orchestrator; use ssh to remotely kill cmd

HOST="$1"; shift
ID="$1"; shift
D=`pwd`

echo "local_kill.sh: ssh $HOST automation/remote_kill.sh $D $ID $*"
ssh $HOST $D/automation/remote_kill.sh $D $ID $*
