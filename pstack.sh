#!/bin/bash

pgrep $@ | while read p; do
  echo '##############################'
  echo -n '### '
  ps -o cmd -p $p --no-heading
  echo '##############################'
  gdb -q -p $p -ex 'thread apply all bt' -ex 'set confirm off' -ex q
done
