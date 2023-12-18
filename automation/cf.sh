#!/bin/sh
# cf.sh - Run many tests.

T=cf1
automation/local_run.sh $H2 ponger EF_POLL_USEC=-1 onload ./um_lat_pong -s f -x um.xml $PONGER_A -E >test$T.ponger.log 2>&1 &
PONGER_PID=$!
sleep 1
automation/local_run.sh $H1 pinger EF_POLL_USEC=-1 onload ./um_lat_ping -s f $PINGER_A -x um.xml -m 24 -n 1000000 -r 100000 -w 5,5 -H 300,1000 >test$T.pinger.log 2>&1 &
PINGER_PID=$!
wait $PINGER_PID
wait $PONGER_PID
automation/summary.sh test$T.pinger.log; ST="$?"; if [ $ST -ne 0 ]; then exit $ST; fi

T=cf3
automation/local_run.sh $H2 ponger EF_POLL_USEC=-1 onload ./um_lat_pong -s f -x um.xml $PONGER_A -E >test$T.ponger.log 2>&1 &
PONGER_PID=$!
sleep 1
automation/local_run.sh $H1 pinger EF_POLL_USEC=-1 onload ./um_lat_ping -s f $PINGER_A -x um.xml -m 24 -n 3000000 -r 300000 -w 5,5 -H 300,1000 >test$T.pinger.log 2>&1 &
PINGER_PID=$!
wait $PINGER_PID
wait $PONGER_PID
automation/summary.sh test$T.pinger.log; ST="$?"; if [ $ST -ne 0 ]; then exit $ST; fi

T=cf5
automation/local_run.sh $H2 ponger EF_POLL_USEC=-1 onload ./um_lat_pong -s f -x um.xml $PONGER_A -E >test$T.ponger.log 2>&1 &
PONGER_PID=$!
sleep 1
automation/local_run.sh $H1 pinger EF_POLL_USEC=-1 onload ./um_lat_ping -s f $PINGER_A -x um.xml -m 24 -n 5000000 -r 500000 -w 5,5 -H 300,1000 >test$T.pinger.log 2>&1 &
PINGER_PID=$!
wait $PINGER_PID
wait $PONGER_PID
automation/summary.sh test$T.pinger.log; ST="$?"; if [ $ST -ne 0 ]; then exit $ST; fi

T=cf7
automation/local_run.sh $H2 ponger EF_POLL_USEC=-1 onload ./um_lat_pong -s f -x um.xml $PONGER_A -E >test$T.ponger.log 2>&1 &
PONGER_PID=$!
sleep 1
automation/local_run.sh $H1 pinger EF_POLL_USEC=-1 onload ./um_lat_ping -s f $PINGER_A -x um.xml -m 24 -n 7000000 -r 700000 -w 5,5 -H 300,1000 >test$T.pinger.log 2>&1 &
PINGER_PID=$!
wait $PINGER_PID
wait $PONGER_PID
automation/summary.sh test$T.pinger.log; ST="$?"; if [ $ST -ne 0 ]; then exit $ST; fi

T=cf9
automation/local_run.sh $H2 ponger EF_POLL_USEC=-1 onload ./um_lat_pong -s f -x um.xml $PONGER_A -E >test$T.ponger.log 2>&1 &
PONGER_PID=$!
sleep 1
automation/local_run.sh $H1 pinger EF_POLL_USEC=-1 onload ./um_lat_ping -s f $PINGER_A -x um.xml -m 24 -n 9000000 -r 900000 -w 5,5 -H 300,1000 >test$T.pinger.log 2>&1 &
PINGER_PID=$!
wait $PINGER_PID
wait $PONGER_PID
automation/summary.sh test$T.pinger.log; ST="$?"; if [ $ST -ne 0 ]; then exit $ST; fi

T=cf11
automation/local_run.sh $H2 ponger EF_POLL_USEC=-1 onload ./um_lat_pong -s f -x um.xml $PONGER_A -E >test$T.ponger.log 2>&1 &
PONGER_PID=$!
sleep 1
automation/local_run.sh $H1 pinger EF_POLL_USEC=-1 onload ./um_lat_ping -s f $PINGER_A -x um.xml -m 24 -n 11000000 -r 1100000 -w 5,5 -H 300,1000 >test$T.pinger.log 2>&1 &
PINGER_PID=$!
wait $PINGER_PID
wait $PONGER_PID
automation/summary.sh test$T.pinger.log; ST="$?"; if [ $ST -ne 0 ]; then exit $ST; fi

T=cf12
automation/local_run.sh $H2 ponger EF_POLL_USEC=-1 onload ./um_lat_pong -s f -x um.xml $PONGER_A -E >test$T.ponger.log 2>&1 &
PONGER_PID=$!
sleep 1
automation/local_run.sh $H1 pinger EF_POLL_USEC=-1 onload ./um_lat_ping -s f $PINGER_A -x um.xml -m 24 -n 12000000 -r 1200000 -w 5,5 -H 300,1000 >test$T.pinger.log 2>&1 &
PINGER_PID=$!
wait $PINGER_PID
wait $PONGER_PID
automation/summary.sh test$T.pinger.log; ST="$?"; if [ $ST -ne 0 ]; then exit $ST; fi

T=cf13
automation/local_run.sh $H2 ponger EF_POLL_USEC=-1 onload ./um_lat_pong -s f -x um.xml $PONGER_A -E >test$T.ponger.log 2>&1 &
PONGER_PID=$!
sleep 1
automation/local_run.sh $H1 pinger EF_POLL_USEC=-1 onload ./um_lat_ping -s f $PINGER_A -x um.xml -m 24 -n 13000000 -r 1300000 -w 5,5 -H 300,1000 >test$T.pinger.log 2>&1 &
PINGER_PID=$!
wait $PINGER_PID
wait $PONGER_PID
automation/summary.sh test$T.pinger.log; ST="$?"; if [ $ST -ne 0 ]; then exit $ST; fi
