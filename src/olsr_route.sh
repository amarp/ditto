#!/bin/bash
TMP_FILE=/tmp/_olsr_tmp.txt

out_file=$1
rm -f $out_file
/sbin/route -n > $out_file 


