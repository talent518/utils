#!/bin/bash

pgrep $@ | while read p; do
  gdb -q -p $p -ex 'thread apply all bt' -ex 'set confirm off' -ex q
done
