#!/bin/tcsh

set CONF="test/router1_gtcd.conf"

setenv DOT_CACHE_HOME tmp-dot
#setenv XFER_GTC_PORT 15001
gtcd/gtcd -f $CONF -p /tmp/gtcd_router1 || sleep 24d
