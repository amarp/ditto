#!/bin/tcsh

set CONF="test/other_gcp.conf"
#set CONF="test/other_gcp_set.conf"
#set CONF="test/other_gcp_opt.conf"

setenv DOT_CACHE_HOME tmp-dot
#setenv XFER_GTC_PORT 15001
gtcd/gtcd -f $CONF -p /tmp/gtcd_deux || sleep 24d
