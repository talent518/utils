#!/bin/sh

ss state connected -n 'sport = :3306 or sport = :80 or sport = :8080 or sport = :443 or sport = :1112 or sport = :11211 or sport = :27017' | awk 'NR>1{split($4,p,":");a[p[2],$1]++;}END{for(k in a){split(k,v,SUBSEP);print v[1],v[2],a[v[1],v[2]];}}' | sort -n
