#!/bin/sh
# Talks to a running AnyPad PS4 through its page (HTTP, local network only).
# Made for people and for AI assistants alike; see AI-WORKFLOW.md.
#
#   ANYPAD_IP=192.168.x.y tools/live.sh watch            follow the log in real time (saved in ./logs/)
#   ANYPAD_IP=...         tools/live.sh state            state of AnyPad (JSON): pads, network controller
#   ANYPAD_IP=...         tools/live.sh log              the end of the log
#   ANYPAD_IP=...         tools/live.sh ps <n>           hold PS on pad n (1-4), as the page's button
#   ANYPAD_IP=...         tools/live.sh stop             stop AnyPad PS4
#
# The console's address is shown in the notification AnyPad displays when it
# starts, and on its page ("http://<ip>:8095/"). Port 8095 unless ANYPAD_PORT is set.
IP="${ANYPAD_IP:?set ANYPAD_IP to the console's address, e.g. ANYPAD_IP=192.168.1.20}"
URL="http://$IP:${ANYPAD_PORT:-8095}"
H='X-AnyPad: 1'     # every change must carry this header

case "$1" in
watch)
    D="${ANYPAD_LOGS:-./logs}"
    mkdir -p "$D"
    OUT="$D/live-$(date +%Y%m%d-%H%M%S).log"
    N=0; FAILS=0
    echo "following $URL (saving in $OUT)"
    while :; do
        R="$(curl "$URL/api/live?from=$N" -s --max-time 5)"
        if [ -z "$R" ]; then
            FAILS=$((FAILS + 1))
            [ "$FAILS" -eq 3 ] && { echo "--- AnyPad does not answer (stopped, or not started yet)"; N=0; }
            sleep 2; continue
        fi
        FAILS=0
        NX="$(printf '%s\n' "$R" | head -1 | sed -n 's/^#next //p')"
        [ -n "$NX" ] || NX=0
        if [ "$NX" -lt "$N" ]; then            # AnyPad was restarted: its log starts again from zero
            echo "--- AnyPad restarted"; N=0; continue
        fi
        N="$NX"
        printf '%s\n' "$R" | tail -n +2 | sed '/^$/d' | tee -a "$OUT"
        sleep 1
    done
    ;;
state)    curl "$URL/api/state" -s --max-time 5; echo ;;
log)      curl "$URL/api/log" -s --max-time 5 ;;
ps)       curl "$URL/api/ps?slot=${2:?pad number, 1-4}" -s --max-time 5 -X POST -H "$H"; echo ;;
stop)     curl "$URL/api/stop" -s --max-time 5 -X POST -H "$H"; echo ;;
*) sed -n '2,12p' "$0"; exit 2 ;;
esac
