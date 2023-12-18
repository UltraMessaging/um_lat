#!/bin/sh
# summaries.sh

scp goat.29west.com:backup_exclude/labbox/GitHub/um_lat/test*.log .

echo "## Summaries" >summaries.out
echo "" >>summaries.out

echo "**C Latency**" >>summaries.out
echo "" >>summaries.out
echo "Log file | Rate | Avg Latency | Overflows | Percentiles" >>summaries.out
echo "-------- | ---- | ----------- | --------- | -----------" >>summaries.out
for F in testc[1-3].pinger.log; do :
  echo "$F" >&2
  automation/summary.sh $F
done >>summaries.out
echo "" >>summaries.out

echo "**C Onloaded Latency**" >>summaries.out
echo "" >>summaries.out
echo "Log file | Rate | Avg Latency | Overflows | Percentiles" >>summaries.out
echo "-------- | ---- | ----------- | --------- | -----------" >>summaries.out
for F in testco[1-3].pinger.log; do :
  echo "$F" >&2
  automation/summary.sh $F
done >>summaries.out
echo "" >>summaries.out

echo "**C Throuthput**" >>summaries.out
echo "" >>summaries.out
echo "Log file | Rate | Avg Latency | Overflows | Percentiles" >>summaries.out
echo "-------- | ---- | ----------- | --------- | -----------" >>summaries.out
for F in testcf*.pinger.log; do :
  echo "$F" >&2
  automation/summary.sh $F
done >>summaries.out
echo "" >>summaries.out


echo "**Java Latency**" >>summaries.out
echo "" >>summaries.out
echo "Log file | Rate | Avg Latency | Overflows | Percentiles" >>summaries.out
echo "-------- | ---- | ----------- | --------- | -----------" >>summaries.out
for F in testj[1-3].pinger.log; do :
  echo "$F" >&2
  automation/summary.sh $F
done >>summaries.out
echo "" >>summaries.out

echo "**Java Onloaded Latency**" >>summaries.out
echo "" >>summaries.out
echo "Log file | Rate | Avg Latency | Overflows | Percentiles" >>summaries.out
echo "-------- | ---- | ----------- | --------- | -----------" >>summaries.out
for F in testjo[1-3].pinger.log; do :
  echo "$F" >&2
  automation/summary.sh $F
done >>summaries.out
echo "" >>summaries.out

echo "**Java Throuthput**" >>summaries.out
echo "" >>summaries.out
echo "Log file | Rate | Avg Latency | Overflows | Percentiles | Errors" >>summaries.out
echo "-------- | ---- | ----------- | --------- | ----------- | ------" >>summaries.out
for F in testjf*.pinger.log; do :
  echo "$F" >&2
  automation/summary.sh $F
done >>summaries.out
echo "" >>summaries.out

sed -i.bak -e '/^## Summaries$/,$d' README.md
cat summaries.out >>README.md
