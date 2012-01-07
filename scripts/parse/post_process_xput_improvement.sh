#!/bin/bash

# REMEMBER to change 
#    a) testbed type in the call to analyze_1
#    b) logs directory


# EMULAB
# 9_Mar_2008_xput
# 10_Mar_2008_xput_large
# 10_Mar_2008_xput_staggered
# 11_Mar_2008_xput_staggered_large
# 13_March_2008_random

echo "analyze_xput"
./analyze_xput.rb emulab 9_Mar_2008_xput ./9_Mar_2008_xput/receivers.txt 5 1 970
./analyze_xput.rb emulab 10_Mar_2008_xput_large ./10_Mar_2008_xput_large/receivers.txt 5 1 5258
./analyze_xput.rb emulab 10_Mar_2008_xput_staggered ./10_Mar_2008_xput_staggered/receivers.txt 5 1 970
./analyze_xput.rb emulab 11_Mar_2008_xput_staggered_large ./11_Mar_2008_xput_staggered_large/receivers.txt 5 1 5258
./analyze_xput.rb emulab 13_March_2008_random ./13_March_2008_random/receivers.txt 10 1 970
echo "================================================================================"

echo "xput improvement across receivers"
./graph_xput_improvement_across_receivers.rb ./9_Mar_2008_xput 5
./graph_xput_improvement_across_receivers.rb ./10_Mar_2008_xput_large 5
./graph_xput_improvement_across_receivers.rb ./10_Mar_2008_xput_staggered 5
./graph_xput_improvement_across_receivers.rb ./11_Mar_2008_xput_staggered_large 5
./graph_xput_improvement_across_receivers.rb ./13_March_2008_random 10
echo "================================================================================"

echo "xput improvement cdf"
./generate_xput_improvement_cdf.rb ./9_Mar_2008_xput
./generate_xput_improvement_cdf.rb ./10_Mar_2008_xput_large
./generate_xput_improvement_cdf.rb ./10_Mar_2008_xput_staggered
./generate_xput_improvement_cdf.rb ./11_Mar_2008_xput_staggered_large
./generate_xput_improvement_cdf.rb ./13_March_2008_random
echo "================================================================================"


# MAP

# 14_Mar_2008_xput

# echo "analyze_xput"
# ./analyze_xput.rb map 14_Mar_2008_xput ./14_Mar_2008_xput/receivers.txt 3 1 970
# echo "================================================================================"

# echo "xput improvement across receivers"
# ./graph_xput_improvement_across_receivers.rb ./14_Mar_2008_xput 3
# echo "================================================================================"

# echo "xput improvement cdf"
# ./generate_xput_improvement_cdf.rb ./14_Mar_2008_xput 
# echo "================================================================================"
