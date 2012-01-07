set terminal postscript eps monochrome
#set size 0.75
set size 1*4/5., 0.7*3/3.
set output 'x_across_recvrs_monochrome.eps'
#set x2label "Receivers"
set ylabel "Average reconstruction effectiveness (% chunks)"

#set key below right
set nokey

set xrange [-1:25]
set yrange [-5:115]
set x2tic rotate by 90
unset xtic



# ugly - but should be ok for the time being
#set xtics ("map16" 0, "map18" 1, "map1" 2, "map25" 3, "map15" 4, "map7" 5, "map2" 6, "map29" 7, "map27" 8, "map3" 9, "map4" 10, "map28" 11, "map13" 12, "map14" 13, "map22" 14, "map34" 15, "map30" 16, "map37" 17, "map5" 18)

#set x2tics ("16" 0, "18" 1, "1" 2, "25" 3, "15" 4, "7" 5, "2" 6, "29" 7, "27" 8, "3" 9, "4" 10, "28" 11, "13" 12, "14" 13, "22" 14, "34" 15, "30" 16, "37" 17, "5" 18)

#"map31" 0, "map15" 1, "map13" 2, "map16" 3, "map4 " 4, "map30" 5, "map3 " 6, "map14" 7, "map19" 8, "map12" 9, "map2 " 10, "map27" 11, "map34" 12, "map1 " 13, "map18" 14, "map5 " 15, "map22" 16, "map7 " 17, "map28" 18, "map37" 19, "map38" 20, "map29" 21, "map25" 22

#set x2tics ("map31" 0, "map15" 1, "map13" 2, "map16" 3, "map4 " 4, "map30" 5, "map3 " 6, "map14" 7, "map19" 8, "map12" 9, "map2 " 10, "map27" 11, "map34" 12, "map1 " 13, "map18" 14, "map5 " 15, "map22" 16, "map7 " 17, "map28" 18, "map37" 19, "map38" 20, "map29" 21, "map25" 22)

#set x2tics ("map1 " 0, "map12" 1, "map15" 2, "map16" 3, "map18" 4, "map19" 5, "map2 " 6, "map25" 7, "map28" 8, "map31" 9, "map3 " 10, "map38" 11, "map4 " 12, "map7 " 13, "map27" 14, "map13" 15, "map29" 16, "map22" 17, "map37" 18, "map5 " 19, "map34" 20, "map30" 21, "map14" 22)






set x2tics("node31" 0, "node19" 1, "node12" 2, "node25" 3, "node18" 4, "node4 " 5, "node38" 6, "node15" 7, "node2 " 8, "node7 " 9, "node3 " 10, "node28" 11, "node16" 12, "node1 " 13, "node27" 14, "node13" 15, "node29" 16, "node22" 17, "node37" 18, "node5 " 19, "node34" 20, "node30" 21, "node14" 22) 



#plot 'sorted_proxy-mhear_8K_xput.dat' using :3:4:5 with errorbars pt 2;
#plot 'crap2' using :3:4:5 with errorbars pt 2;
#plot 'crap2' using :3 with linespoints, 'crap2' using :4 with points, 'crap2' using :5 with points
#plot 'crap2' using :3 with points pt 7 title 'median', 'crap2' using :5 with points pt 4 title 'max'


plot 'crap3' using :3 with points pt 7 title 'median', 'crap3' using :5 with points pt 4 ps 1.5 title 'max'
