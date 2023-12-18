#!/bin/sh
# tst.sh - Run many tests.

# Host names that can be "ssh"ed into without password prompt.
export H1=mamba   # Pinger
export H2=hal     # Ponger
export H_BLD=hal  # build system.

ASSRT() {
  eval "test $1"
  if [ $? -ne 0 ]; then echo "ASSRT ERROR `basename ${BASH_SOURCE[1]}`:${BASH_LINENO[0]}, not true: '$1'" >&2; exit 1; fi
}  # ASSRT

kill_all() {
  automation/local_kill.sh $H1 pinger
  automation/local_kill.sh $H2 ponger
  wait $PINGER_PID
  wait $PONGER_PID
}

export CP="-classpath .:/home/sford/UMP_6.14/java/UMS_6.14.jar"

rm -f *.pid *.log tmp.* *.tmp

D=`/bin/pwd`

# Build on H2 machine.
ssh $H_BLD "cd $D; ./bld.sh"; ASSRT "$? -eq 0"

# Cleanup on cntrol-c.
trap "kill_all; exit 1" 1 2 3 15

export PONGER_A="-a 12"
export PINGER_A="-A 4 -a 12"
### PONGER_A=""
### PINGER_A=""

T=c1
automation/local_run.sh $H2 ponger ./um_lat_pong -s f -x um.xml $PONGER_A -E >test$T.ponger.log 2>&1 &
PONGER_PID=$!
sleep 1
automation/local_run.sh $H1 pinger ./um_lat_ping -s f $PINGER_A -x um.xml -m 24 -n 500000 -r 50000 -w 5,5 -H 300,1000 >test$T.pinger.log 2>&1 &
PINGER_PID=$!
echo "wait $PINGER_PID (pinger)"
wait $PINGER_PID
echo "wait $PONGER_PID (ponger)"
wait $PONGER_PID
automation/summary.sh test$T.pinger.log

T=c2
automation/local_run.sh $H2 ponger ./um_lat_pong -s f -x um.xml $PONGER_A -E >test$T.ponger.log 2>&1 &
PONGER_PID=$!
sleep 1
automation/local_run.sh $H1 pinger ./um_lat_ping -s f $PINGER_A -x um.xml -m 24 -n 500000 -r 50000 -w 5,5 -H 300,1000 >test$T.pinger.log 2>&1 &
PINGER_PID=$!
echo "wait $PINGER_PID (pinger)"
wait $PINGER_PID
echo "wait $PONGER_PID (ponger)"
wait $PONGER_PID
automation/summary.sh test$T.pinger.log

T=c3
automation/local_run.sh $H2 ponger ./um_lat_pong -s f -x um.xml $PONGER_A -E >test$T.ponger.log 2>&1 &
PONGER_PID=$!
sleep 1
automation/local_run.sh $H1 pinger ./um_lat_ping -s f $PINGER_A -x um.xml -m 24 -n 500000 -r 50000 -w 5,5 -H 300,1000 >test$T.pinger.log 2>&1 &
PINGER_PID=$!
echo "wait $PINGER_PID (pinger)"
wait $PINGER_PID
echo "wait $PONGER_PID (ponger)"
wait $PONGER_PID
automation/summary.sh test$T.pinger.log

T=co1
automation/local_run.sh $H2 ponger EF_POLL_USEC=-1 onload ./um_lat_pong -s f -x um.xml $PONGER_A -E >test$T.ponger.log 2>&1 &
PONGER_PID=$!
sleep 1
automation/local_run.sh $H1 pinger EF_POLL_USEC=-1 onload ./um_lat_ping -s f $PINGER_A -x um.xml -m 24 -n 500000 -r 50000 -w 5,5 -H 300,1000 >test$T.pinger.log 2>&1 &
PINGER_PID=$!
echo "wait $PINGER_PID (pinger)"
wait $PINGER_PID
echo "wait $PONGER_PID (ponger)"
wait $PONGER_PID
automation/summary.sh test$T.pinger.log

T=co2
automation/local_run.sh $H2 ponger EF_POLL_USEC=-1 onload ./um_lat_pong -s f -x um.xml $PONGER_A -E >test$T.ponger.log 2>&1 &
PONGER_PID=$!
sleep 1
automation/local_run.sh $H1 pinger EF_POLL_USEC=-1 onload ./um_lat_ping -s f $PINGER_A -x um.xml -m 24 -n 500000 -r 50000 -w 5,5 -H 300,1000 >test$T.pinger.log 2>&1 &
PINGER_PID=$!
echo "wait $PINGER_PID (pinger)"
wait $PINGER_PID
echo "wait $PONGER_PID (ponger)"
wait $PONGER_PID
automation/summary.sh test$T.pinger.log

T=co3
automation/local_run.sh $H2 ponger EF_POLL_USEC=-1 onload ./um_lat_pong -s f -x um.xml $PONGER_A -E >test$T.ponger.log 2>&1 &
PONGER_PID=$!
sleep 1
automation/local_run.sh $H1 pinger EF_POLL_USEC=-1 onload ./um_lat_ping -s f $PINGER_A -x um.xml -m 24 -n 500000 -r 50000 -w 5,5 -H 300,1000 >test$T.pinger.log 2>&1 &
PINGER_PID=$!
echo "wait $PINGER_PID (pinger)"
wait $PINGER_PID
echo "wait $PONGER_PID (ponger)"
wait $PONGER_PID
automation/summary.sh test$T.pinger.log

T=j1
automation/local_run.sh $H2 ponger java --add-opens java.base/java.nio=ALL-UNNAMED $CP UmLatPong -s f -x um.xml -E -S >test$T.ponger.log 2>&1 &
PONGER_PID=$!
sleep 1
automation/local_run.sh $H1 pinger java --add-opens java.base/java.nio=ALL-UNNAMED $CP UmLatPing -s f -x um.xml -m 24 -n 500000 -r 50000 -w 5000,5000 -H 300,1000 -S >test$T.pinger.log 2>&1 &
PINGER_PID=$!
echo "wait $PINGER_PID (pinger)"
wait $PINGER_PID
echo "wait $PONGER_PID (ponger)"
wait $PONGER_PID
automation/summary.sh test$T.pinger.log

T=j2
automation/local_run.sh $H2 ponger java --add-opens java.base/java.nio=ALL-UNNAMED $CP UmLatPong -s f -x um.xml -E -S >test$T.ponger.log 2>&1 &
PONGER_PID=$!
sleep 1
automation/local_run.sh $H1 pinger java --add-opens java.base/java.nio=ALL-UNNAMED $CP UmLatPing -s f -x um.xml -m 24 -n 500000 -r 50000 -w 5000,5000 -H 300,1000 -S >test$T.pinger.log 2>&1 &
PINGER_PID=$!
echo "wait $PINGER_PID (pinger)"
wait $PINGER_PID
echo "wait $PONGER_PID (ponger)"
wait $PONGER_PID
automation/summary.sh test$T.pinger.log

T=j3
automation/local_run.sh $H2 ponger java --add-opens java.base/java.nio=ALL-UNNAMED $CP UmLatPong -s f -x um.xml -E -S >test$T.ponger.log 2>&1 &
PONGER_PID=$!
sleep 1
automation/local_run.sh $H1 pinger java --add-opens java.base/java.nio=ALL-UNNAMED $CP UmLatPing -s f -x um.xml -m 24 -n 500000 -r 50000 -w 5000,5000 -H 300,1000 -S >test$T.pinger.log 2>&1 &
PINGER_PID=$!
echo "wait $PINGER_PID (pinger)"
wait $PINGER_PID
echo "wait $PONGER_PID (ponger)"
wait $PONGER_PID
automation/summary.sh test$T.pinger.log

T=jo1
automation/local_run.sh $H2 ponger EF_POLL_USEC=-1 onload java --add-opens java.base/java.nio=ALL-UNNAMED $CP UmLatPong -s f -x um.xml -E -S >test$T.ponger.log 2>&1 &
PONGER_PID=$!
sleep 1
automation/local_run.sh $H1 pinger EF_POLL_USEC=-1 onload java --add-opens java.base/java.nio=ALL-UNNAMED $CP UmLatPing -s f -x um.xml -m 24 -n 500000 -r 50000 -w 5000,5000 -H 300,1000 -S >test$T.pinger.log 2>&1 &
PINGER_PID=$!
echo "wait $PINGER_PID (pinger)"
wait $PINGER_PID
echo "wait $PONGER_PID (ponger)"
wait $PONGER_PID
automation/summary.sh test$T.pinger.log

T=jo2
automation/local_run.sh $H2 ponger EF_POLL_USEC=-1 onload java --add-opens java.base/java.nio=ALL-UNNAMED $CP UmLatPong -s f -x um.xml -E -S >test$T.ponger.log 2>&1 &
PONGER_PID=$!
sleep 1
automation/local_run.sh $H1 pinger EF_POLL_USEC=-1 onload java --add-opens java.base/java.nio=ALL-UNNAMED $CP UmLatPing -s f -x um.xml -m 24 -n 500000 -r 50000 -w 5000,5000 -H 300,1000 -S >test$T.pinger.log 2>&1 &
PINGER_PID=$!
echo "wait $PINGER_PID (pinger)"
wait $PINGER_PID
echo "wait $PONGER_PID (ponger)"
wait $PONGER_PID
automation/summary.sh test$T.pinger.log

T=jo3
automation/local_run.sh $H2 ponger EF_POLL_USEC=-1 onload java --add-opens java.base/java.nio=ALL-UNNAMED $CP UmLatPong -s f -x um.xml -E -S >test$T.ponger.log 2>&1 &
PONGER_PID=$!
sleep 1
automation/local_run.sh $H1 pinger EF_POLL_USEC=-1 onload java --add-opens java.base/java.nio=ALL-UNNAMED $CP UmLatPing -s f -x um.xml -m 24 -n 500000 -r 50000 -w 5000,5000 -H 300,1000 -S >test$T.pinger.log 2>&1 &
PINGER_PID=$!
echo "wait $PINGER_PID (pinger)"
wait $PINGER_PID
echo "wait $PONGER_PID (ponger)"
wait $PONGER_PID
automation/summary.sh test$T.pinger.log

##########################
# Now do throughput tests
##########################

./automation/cf.sh

./automation/jf.sh
