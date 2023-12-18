#!/bin/sh
# jf.sh - Run many tests.

T=jf1
automation/local_run.sh $H2 ponger EF_POLL_USEC=-1 onload java --add-opens java.base/java.nio=ALL-UNNAMED $CP UmLatPong -s f -x um.xml -E -S >test$T.ponger.log 2>&1 &
PONGER_PID=$!
sleep 1
automation/local_run.sh $H1 pinger EF_POLL_USEC=-1 onload java --add-opens java.base/java.nio=ALL-UNNAMED $CP UmLatPing -s f -x um.xml -m 24 -n 1000000 -r 100000 -w 5000,5000 -H 300,1000 -S >test$T.pinger.log 2>&1 &
PINGER_PID=$!
wait $PINGER_PID
wait $PONGER_PID
automation/summary.sh test$T.pinger.log; ST="$?"; if [ $ST -ne 0 ]; then exit $ST; fi

T=jf3
automation/local_run.sh $H2 ponger EF_POLL_USEC=-1 onload java --add-opens java.base/java.nio=ALL-UNNAMED $CP UmLatPong -s f -x um.xml -E -S >test$T.ponger.log 2>&1 &
PONGER_PID=$!
sleep 1
automation/local_run.sh $H1 pinger EF_POLL_USEC=-1 onload java --add-opens java.base/java.nio=ALL-UNNAMED $CP UmLatPing -s f -x um.xml -m 24 -n 3000000 -r 300000 -w 5000,5000 -H 300,1000 -S >test$T.pinger.log 2>&1 &
PINGER_PID=$!
wait $PINGER_PID
wait $PONGER_PID
automation/summary.sh test$T.pinger.log; ST="$?"; if [ $ST -ne 0 ]; then exit $ST; fi

T=jf5
automation/local_run.sh $H2 ponger EF_POLL_USEC=-1 onload java --add-opens java.base/java.nio=ALL-UNNAMED $CP UmLatPong -s f -x um.xml -E -S >test$T.ponger.log 2>&1 &
PONGER_PID=$!
sleep 1
automation/local_run.sh $H1 pinger EF_POLL_USEC=-1 onload java --add-opens java.base/java.nio=ALL-UNNAMED $CP UmLatPing -s f -x um.xml -m 24 -n 5000000 -r 500000 -w 5000,5000 -H 300,1000 -S >test$T.pinger.log 2>&1 &
PINGER_PID=$!
wait $PINGER_PID
wait $PONGER_PID
automation/summary.sh test$T.pinger.log; ST="$?"; if [ $ST -ne 0 ]; then exit $ST; fi

T=jf7
automation/local_run.sh $H2 ponger EF_POLL_USEC=-1 onload java --add-opens java.base/java.nio=ALL-UNNAMED $CP UmLatPong -s f -x um.xml -E -S >test$T.ponger.log 2>&1 &
PONGER_PID=$!
sleep 1
automation/local_run.sh $H1 pinger EF_POLL_USEC=-1 onload java --add-opens java.base/java.nio=ALL-UNNAMED $CP UmLatPing -s f -x um.xml -m 24 -n 7000000 -r 700000 -w 5000,5000 -H 300,1000 -S >test$T.pinger.log 2>&1 &
PINGER_PID=$!
wait $PINGER_PID
wait $PONGER_PID
automation/summary.sh test$T.pinger.log; ST="$?"; if [ $ST -ne 0 ]; then exit $ST; fi

T=jf8
automation/local_run.sh $H2 ponger EF_POLL_USEC=-1 onload java --add-opens java.base/java.nio=ALL-UNNAMED $CP UmLatPong -s f -x um.xml -E -S >test$T.ponger.log 2>&1 &
PONGER_PID=$!
sleep 1
automation/local_run.sh $H1 pinger EF_POLL_USEC=-1 onload java --add-opens java.base/java.nio=ALL-UNNAMED $CP UmLatPing -s f -x um.xml -m 24 -n 8000000 -r 800000 -w 5000,5000 -H 300,1000 -S >test$T.pinger.log 2>&1 &
PINGER_PID=$!
wait $PINGER_PID
wait $PONGER_PID
automation/summary.sh test$T.pinger.log; ST="$?"; if [ $ST -ne 0 ]; then exit $ST; fi

T=jf9
automation/local_run.sh $H2 ponger EF_POLL_USEC=-1 onload java --add-opens java.base/java.nio=ALL-UNNAMED $CP UmLatPong -s f -x um.xml -E -S >test$T.ponger.log 2>&1 &
PONGER_PID=$!
sleep 1
automation/local_run.sh $H1 pinger EF_POLL_USEC=-1 onload java --add-opens java.base/java.nio=ALL-UNNAMED $CP UmLatPing -s f -x um.xml -m 24 -n 9000000 -r 900000 -w 5000,5000 -H 300,1000 -S >test$T.pinger.log 2>&1 &
PINGER_PID=$!
wait $PINGER_PID
wait $PONGER_PID
automation/summary.sh test$T.pinger.log; ST="$?"; if [ $ST -ne 0 ]; then exit $ST; fi

T=jf10
automation/local_run.sh $H2 ponger EF_POLL_USEC=-1 onload java --add-opens java.base/java.nio=ALL-UNNAMED $CP UmLatPong -s f -x um.xml -E -S >test$T.ponger.log 2>&1 &
PONGER_PID=$!
sleep 1
automation/local_run.sh $H1 pinger EF_POLL_USEC=-1 onload java --add-opens java.base/java.nio=ALL-UNNAMED $CP UmLatPing -s f -x um.xml -m 24 -n 10000000 -r 1000000 -w 5000,5000 -H 300,1000 -S >test$T.pinger.log 2>&1 &
PINGER_PID=$!
wait $PINGER_PID
wait $PONGER_PID
automation/summary.sh test$T.pinger.log; ST="$?"; if [ $ST -ne 0 ]; then exit $ST; fi

T=jf11
automation/local_run.sh $H2 ponger EF_POLL_USEC=-1 onload java --add-opens java.base/java.nio=ALL-UNNAMED $CP UmLatPong -s f -x um.xml -E -S >test$T.ponger.log 2>&1 &
PONGER_PID=$!
sleep 1
automation/local_run.sh $H1 pinger EF_POLL_USEC=-1 onload java --add-opens java.base/java.nio=ALL-UNNAMED $CP UmLatPing -s f -x um.xml -m 24 -n 11000000 -r 1100000 -w 5000,5000 -H 300,1000 -S >test$T.pinger.log 2>&1 &
PINGER_PID=$!
wait $PINGER_PID
wait $PONGER_PID
automation/summary.sh test$T.pinger.log; ST="$?"; if [ $ST -ne 0 ]; then exit $ST; fi

T=jf12
automation/local_run.sh $H2 ponger EF_POLL_USEC=-1 onload java --add-opens java.base/java.nio=ALL-UNNAMED $CP UmLatPong -s f -x um.xml -E -S >test$T.ponger.log 2>&1 &
PONGER_PID=$!
sleep 1
automation/local_run.sh $H1 pinger EF_POLL_USEC=-1 onload java --add-opens java.base/java.nio=ALL-UNNAMED $CP UmLatPing -s f -x um.xml -m 24 -n 12000000 -r 1200000 -w 5000,5000 -H 300,1000 -S >test$T.pinger.log 2>&1 &
PINGER_PID=$!
wait $PINGER_PID
wait $PONGER_PID
automation/summary.sh test$T.pinger.log; ST="$?"; if [ $ST -ne 0 ]; then exit $ST; fi

T=jf13
automation/local_run.sh $H2 ponger EF_POLL_USEC=-1 onload java --add-opens java.base/java.nio=ALL-UNNAMED $CP UmLatPong -s f -x um.xml -E -S >test$T.ponger.log 2>&1 &
PONGER_PID=$!
sleep 1
automation/local_run.sh $H1 pinger EF_POLL_USEC=-1 onload java --add-opens java.base/java.nio=ALL-UNNAMED $CP UmLatPing -s f -x um.xml -m 24 -n 13000000 -r 1300000 -w 5000,5000 -H 300,1000 -S >test$T.pinger.log 2>&1 &
PINGER_PID=$!
wait $PINGER_PID
wait $PONGER_PID
automation/summary.sh test$T.pinger.log; ST="$?"; if [ $ST -ne 0 ]; then exit $ST; fi

