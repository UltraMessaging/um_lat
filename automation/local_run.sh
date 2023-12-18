#!/bin/sh
# local_run.sh - run on orchestrator; use ssh to remotely execute command

HOST="$1"; shift
ID="$1"; shift
D=`pwd`

echo "local_run.sh: ssh $HOST automation/remote_run.sh $D $ID $*"
ssh $HOST $D/automation/remote_run.sh $D $ID $*
