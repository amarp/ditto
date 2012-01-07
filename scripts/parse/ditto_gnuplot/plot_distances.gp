set terminal postscript eps monochrome
set size 1*4/5., 0.3*3/3.
set output 'distance_recvrs_monochrome.eps'
set ylabel "Relative distance \n from gateway"

set xrange [-1:25]
set yrange [0:5]
#set x2tic rotate by 90
unset xtic


#set x2tics("node31" 0, "node19" 1, "node12" 2, "node25" 3, "node18" 4, "node4 " 5, "node38" 6, "node15" 7, "node2 " 8, "node7 " 9, "node3 " 10, "node28" 11, "node16" 12, "node1 " 13, "node27" 14, "node13" 15, "node29" 16, "node22" 17, "node37" 18, "node5 " 19, "node34" 20, "node30" 21, "node14" 22) 



plot 'distance.dat' using :3 with impulses title ''
