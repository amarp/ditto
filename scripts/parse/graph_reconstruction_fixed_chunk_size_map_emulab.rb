#!/usr/bin/ruby

schemes = ['proxy-mhear']
chunk_sizes = ['8K', '16K', '32K']


linespoint_styles = Array.new
# http://sparky.rice.edu/~hartigan/gnuplot.html
# type 'test' to see the colors and point types available
# lt is for color of the points/lines: -1=black 1=red 2=grn 3=blue 4=purple/pink 5=aqua/light-blue 6=yellow 7=dotted-black 8=orange
# postscipt: 1=+, 2=X, 3=*, 4=square, 5=filled square, 6=circle,
#            7=filled circle, 8=triangle, 9=filled triangle, etc.
linespoint_styles << "lt 1 lw 3 pt 2"
linespoint_styles << "lt 3 lw 3 pt 6"
linespoint_styles << "lt 0 lw 3 pt 7"
linespoint_styles << "lt -1 lw 2 pt 3"
linespoint_styles << "lt 8 lw 3 pt 1"
linespoint_styles << "lt 7 lw 3 pt 4"

line_styles = Array.new
# http://sparky.rice.edu/~hartigan/gnuplot.html
# type 'test' to see the colors and point types available
# lt is for color of the points/lines: -1=black 1=red 2=grn 3=blue 4=purple/pink 5=aqua/light-blue 6=yellow 7=dotted-black 8=orange
# postscipt: 1=+, 2=X, 3=*, 4=square, 5=filled square, 6=circle,
#            7=filled circle, 8=triangle, 9=filled triangle, etc.


# works well with pre 4.2 gnuplot
line_styles << "lt -1 lw 2"
line_styles << "lt 3 lw 3"
line_styles << "lt 0 lw 4"
line_styles << "lt 8 lw 3"
line_styles << "lt 1 lw 3"
line_styles << "lt 7 lw 3"
=begin
line_styles << "lt 8 lw 3"
line_styles << "lt 3 lw 3"
line_styles << "lt -1 lw 2"
line_styles << "lt 0 lw 4"
line_styles << "lt 1 lw 3"
line_styles << "lt 7 lw 3"
=end

#set terminal postscript eps color
gtypes = ['svg', 'color', 'monochrome']
gtype_tags = ["svg", "postscript eps color", "postscript eps monochrome"]


def sys(cmd)
  if not system(cmd)
    raise "command failed: #{cmd}"
  end
end

if ARGV.size > 1
    puts "Usage: ./graph_reconstruction_fixed_chunk_size_map_emulab.rb"
    puts "Precondition - needs the files that have cdf data for various schemes (do, po, pm). Run generate_chunk_reconstruction_cdf_data.rb"
    exit
end

#RESULTS_DIR = ARGV[0]
RESULTS_DIR = "./"
GRAPH_DIR = "#{RESULTS_DIR}/graphs"
#Dir.mkdir(GRAPH_DIR) unless GRAPH_DIR == '' || File.exist?(GRAPH_DIR)
system("mkdir -p #{GRAPH_DIR}")

results_base_dir = "#{RESULTS_DIR}/results"
#puts "#{results_base_dir}"
GRAPH_DATA_DIR = results_base_dir

for chunk_size in chunk_sizes

    puts "##################################################"
    puts "chunk size = #{chunk_size}"


    gtype_tags.each_index { |i|
        gtype_tag = gtype_tags[i]
        gtype = gtypes[i]

        plots = []
        comb = schemes[0]
        #puts "'#{RESULTS_DIR}/emulab/9_Mar_2008/results/#{chunk_size}_#{comb}.cdf' using 1:2 with lines #{line_styles[j]} title 'Emulab'"
        plots << "'#{RESULTS_DIR}/map/8_Mar_2008/results/#{chunk_size}_#{comb}.cdf' using 1:2 with lines #{line_styles[1]} title 'Campus testbed'"
        plots << "'#{RESULTS_DIR}/emulab/9_Mar_2008/results/#{chunk_size}_#{comb}.cdf' using 1:2 with lines #{line_styles[0]} title 'Emulab testbed'"
        plots = plots.join(', ')

        File.open("cr.gnuplot", "w") do |out|
            out.puts %Q{
set terminal #{gtype_tag}
set size 0.65
set output '#{GRAPH_DIR}/#{chunk_size}_#{gtype}.eps'
#set title "#{chunk_size}"
#set xlabel "% chunks reconstructed"
set xlabel "Average reconstruction effectiveness (% chunks)"
set ylabel "% observers"
set xrange [0:100]
set yrange [0:100]
set key right bottom
plot #{plots}
}
        end
        sys("cat cr.gnuplot | gnuplot -")
        sys("rm cr.gnuplot")
    }
    #sys("rm #{GRAPH_DATA_DIR}/*.dat")
end
