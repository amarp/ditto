#!/bin/tcsh

xterm -T router1 -geometry 80x24+510+10 -e /bin/sh -c 'test/router1_gtcd.sh' || sleep 6 &
xterm -T router2 -geometry 80x24+510+10 -e /bin/sh -c 'test/router2_gtcd.sh' || sleep 6 &
