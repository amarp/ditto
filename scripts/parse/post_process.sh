#!/bin/bash

# REMEMBER to change 
#    a) testbed type in the call to analyze_1
#    b) logs directory

echo "analyze_1"
./analyze_1.rb map ./8_Mar_2008 ./8_Mar_2008/receivers.txt 3 1 ./8_Mar_2008/sniffers.txt
echo "================================================================================"

echo "analyze_2"
./analyze_2.rb ./8_Mar_2008 3 ./8_Mar_2008/receivers.txt
echo "================================================================================"

echo "chunk cdf"
./generate_chunk_reconstruction_cdf_data.rb ./8_Mar_2008
echo "================================================================================"

echo "tcp cdf"
./generate_tcp_bytes_cdf.rb ./8_Mar_2008
echo "================================================================================"

echo "useless percent cdf"
./generate_wasted_bytes_cdf.rb ./8_Mar_2008
echo "================================================================================"

echo "xput cdf"
./generate_xput_cdf.rb ./8_Mar_2008
echo "================================================================================"
