#!/bin/sh
# summary.sh

for F in $*; do :
  RESULT_RATE=`sed -n 's/^.*result_rate=\([^,]*\),.*$/\1/p' $F`
  # Round the result rate to a whole number.
  RESULT_RATE=`printf "%.0f\n" $RESULT_RATE`
  HIST_OVERFLOWS=`sed -n 's/^.*hist_overflows=\([^,]*\),.*$/\1/p' $F`
  AVERAGE_SAMPLE=`sed -n 's/^.*average_sample=\([^,]*\),.*$/\1/p' $F`
  PERCENTILES=`sed -n 's/^.*Percentiles: \(.*\)$/\1/p' $F`

  ERRORS=`egrep ERROR $F | wc -l`
  if [ "$ERRORS" -ne 0 ]; then :
    echo "ERROR(s) in $F:" >&2
    ERR=`egrep ERROR $F`
    echo "$F | $RESULT_RATE | $AVERAGE_SAMPLE | $HIST_OVERFLOWS | $PERCENTILES | $ERR"
    exit 1
  else :
    echo "$F | $RESULT_RATE | $AVERAGE_SAMPLE | $HIST_OVERFLOWS | $PERCENTILES"
  fi
done
