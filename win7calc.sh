#!/bin/sh
# Launch the Windows 7 Calculator replica.
#   ./win7calc.sh [--scale 1.5] [--mode sci]
#   ./win7calc.sh --web [other options]   # no X display: serve it to a browser via GTK Broadway
cd "$(dirname "$(readlink -f "$0")")" || exit 1

if [ "$1" = "--web" ]; then
    shift
    PORT="${WIN7CALC_PORT:-8085}"
    DISP=":$((PORT - 8080))"
    # Bind to localhost only: reach it through an SSH tunnel, not the open network.
    broadwayd --address 127.0.0.1 --port "$PORT" "$DISP" >/dev/null 2>&1 &
    BPID=$!
    trap 'kill $BPID 2>/dev/null' EXIT INT TERM
    sleep 1
    if ! kill -0 "$BPID" 2>/dev/null; then
        echo "broadwayd failed to start (port $PORT busy? try WIN7CALC_PORT=8090 $0 --web)" >&2
        exit 1
    fi
    echo "Calculator is running at http://localhost:$PORT/"
    echo "From your own computer, open an SSH tunnel first:"
    echo "    ssh -L $PORT:localhost:$PORT $(id -un)@$(hostname -I 2>/dev/null | cut -d' ' -f1)"
    echo "then browse to http://localhost:$PORT/   (Ctrl+C here to stop)"
    GDK_BACKEND=broadway BROADWAY_DISPLAY="$DISP" python3 -m win7calc "$@"
    exit $?
fi

exec python3 -m win7calc "$@"
