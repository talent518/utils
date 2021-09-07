#!/bin/sh

listen_port_connects() {
        PORT=$1
        NAME=$2

        ss state connected -n 'sport = :'${PORT} | awk 'NR>1{if($1=="tcp") a[$2]++; else a[$1]++;}END{printf "% 9s % 8s % 8s % 5s % 10s % 10s % 10s % 8s % 9s\n", "'${NAME}'", a["SYN-SENT"]+0, a["SYN-RECV"]+0, a["ESTAB"]+0, a["FIN-WAIT-1"]+0, a["CLOSE-WAIT"]+0, a["FIN-WAIT-2"]+0, a["LAST-ACK"]+0, a["TIME-WAIT"]+0;}'
}

echo "  program SYN-SENT SYN-RECV ESTAB FIN-WAIT-1 CLOSE-WAIT FIN-WAIT-2 LAST-ACK TIME-WAIT"
echo "-------------------------------------------------------------------------------------"

listen_port_connects 3306 mysql
listen_port_connects 80 http
listen_port_connects 443 https
listen_port_connects 8080 proxy
listen_port_connects 11211 memcached
listen_port_connects 27017 mongod
listen_port_connects 3000 node
