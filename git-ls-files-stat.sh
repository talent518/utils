#!/bin/bash

git ls-files | awk -F/ '{print $NF;}' | awk -F. '{if(NF>1){if($1=="") x++;else a[$NF]++;}else n++;}END{print "!",n;print ".",x;for(k in a) print k,a[k];}' | sort -k2 -n | column -t

