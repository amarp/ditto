#!/bin/tcsh

set CONF="test/gcp_test.conf"
#set CONF="test/gcp_test_set.conf"
#set CONF="test/gcp_test_opt.conf"

if ($#argv != 1) then
    echo "Usage ./gcp_test <filename> "
    exit
endif

rm -rf /tmp/tmp-dot /tmp/.dot /tmp/dot-recv-file

killall -9 gtcd && sleep 0.2 && killall -9 sleep 
tcsh -x test/routers_gtcd.sh
xterm -T receiver -geometry 80x24+510+10 -e /bin/sh -c "gtcd/gtcd -f $CONF || sleep 24d" &
xterm -T sender -geometry 80x24+10+10 -e /bin/sh -c 'test/other_gcp.sh' &
sleep 6 

/usr/bin/time -p gcp/gcp -f -p /tmp/gtcd_deux $argv[1] 127.0.0.1:/tmp/dot-recv-file && openssl sha1 $argv[1] /tmp/dot-recv-file

