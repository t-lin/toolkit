#!/bin/bash
if [[ $# -ne 1 ]]; then
    echo "ERROR: Expecting only one argument (interface name)"
    exit 1
else
    IFACE=$1
fi

START_TX_COUNT=`cat /sys/class/net/${IFACE}/statistics/tx_packets`
START_RX_COUNT=`cat /sys/class/net/${IFACE}/statistics/rx_packets`

echo "Starting Tx count: $START_TX_COUNT"
echo "Starting Rx count: $START_RX_COUNT"

while [[ 1 ]]; do
    CURR_COUNT=`cat /sys/class/net/${IFACE}/statistics/tx_packets`
    TX_DIFF=`echo "$CURR_COUNT - $START_TX_COUNT" | bc`
    echo -n "Tx: $TX_DIFF"

    echo -n "  --  "

    CURR_COUNT=`cat /sys/class/net/${IFACE}/statistics/rx_packets`
    RX_DIFF=`echo "$CURR_COUNT - $START_RX_COUNT" | bc`
    echo -n "Rx: $RX_DIFF"

    echo -n "  --  "

    DIFF=`echo "$TX_DIFF - $RX_DIFF" | bc`
    echo -n "Drop count: $DIFF"

    echo -n "  --  "

    if [[ $TX_DIFF -ne 0 ]]; then
        DIFF=`echo "scale=3; $DIFF / $TX_DIFF" | bc`
    fi
    echo -ne "Drop %: $DIFF\r"

    sleep `echo "scale=9; $(date +%s.0) + 1 - $(date +%s.%N)" | bc`
done
