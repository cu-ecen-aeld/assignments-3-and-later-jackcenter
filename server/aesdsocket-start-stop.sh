argument="$1"
current_directory=$PWD

if ! [ $# -eq 1 ]
then
    echo "Usage: aesdsocket-start-stop {start|stop}"
    exit 1
fi

case "$argument" in
    start)
        echo "Starting aesdsocket daemon..."
        start-stop-daemon --start --background --quiet --exec ${PWD}/aesdsocket -- -d
        ;;
    stop)
        echo "Stopping aesdsocket daemon..."
        start-stop-daemon --stop --quiet --exec  ${PWD}/aesdsocket
        ;;
    *)
        echo "Usage: aesdsocket-start-stop {start|stop}"
        exit 1
        ;;
esac
