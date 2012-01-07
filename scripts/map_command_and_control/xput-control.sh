#!/bin/tcsh

perl setup/setup.pl TOPOS/MAP.TXT map

perl xput-control-1.pl TOPOS/MAP.TXT expt >& xput_out_1.txt

perl xput-control-2.pl TOPOS/MAP.TXT expt >& xput_out_2.txt
