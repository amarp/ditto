#!/usr/bin/ruby

schemes = ['dot', 'proxy', 'proxy-mhear']
#schemes_tags = ['end-to-end', 'proxy', 'proxy + inter-stream reassembly']
#schemes_tags = ['no caching', 'on-path caching', 'ditto']
schemes_tags = ['End-to-end', 'On-path caching', 'Ditto']
#chunk_sizes = ['8K', '16K', '32K']
chunk_sizes = ['8K']


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
line_styles << "lt 0 lw 4"
line_styles << "lt 3 lw 3"
line_styles << "lt -1 lw 2"
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


point_styles = Array.new
point_styles << "pt 7"
point_styles << "pt 4"
point_styles << "pt 3 ps 1.5"
point_styles << "pt 2"
point_styles << "pt 6"
point_styles << "pt 1"
point_styles << "pt 8"


#set terminal postscript eps color
gtypes = ['svg', 'color', 'monochrome']
gtype_tags = ["svg", "postscript eps color", "postscript eps monochrome"]


def sys(cmd)
  if not system(cmd)
    raise "command failed: #{cmd}"
  end
end

if ARGV.size < 1
    puts "Usage: ./graph_xput_improvement_across_receivers.rb <results_dir - the directory which has \"results\">  <numRuns>"
    puts "Precondition - needs the files that have cdf data for various schemes (do, po, pm). Make sure to run generate_xput_improvement_cdf.rb"
    exit
end

RESULTS_DIR = ARGV[0]
nr = ARGV[1]
numRuns = nr.to_i

GRAPH_DIR = "#{RESULTS_DIR}/graphs"
#Dir.mkdir(GRAPH_DIR) unless GRAPH_DIR == '' || File.exist?(GRAPH_DIR)
system("mkdir -p #{GRAPH_DIR}")

results_base_dir = "#{RESULTS_DIR}/results/xput"
#puts "#{results_base_dir}"

# +1 is to skip over the receiver node's name (in the first column); +2 for mean
# array indexing starts from 0
col_number = 1 + numRuns + 2

for chunk_size in chunk_sizes

    puts "##################################################"
    puts "chunk size = #{chunk_size}"



    gtype_tags.each_index { |i|
        gtype_tag = gtype_tags[i]
        gtype = gtypes[i]

        plots = []
        schemes.each_index { |j|
            comb = schemes[j]
            comb_tag = schemes_tags[j]
            graph_data_dir = "#{results_base_dir}/#{chunk_size}/#{comb}"
            puts "'#{graph_data_dir}/xput.dat' using #{col_number} with points #{point_styles[j]} title '#{comb_tag}'"
            plots << "'#{graph_data_dir}/xput.dat' using #{col_number} with points #{point_styles[j]} title '#{comb_tag}'"
        }
        plots = plots.join(', ')

        File.open("cr.gnuplot", "w") do |out|
            out.puts %Q{
set terminal #{gtype_tag}
set size 0.65
set output '#{GRAPH_DIR}/xput_improvement_across_recvrs_#{chunk_size}_#{gtype}.eps'
#set title "#{chunk_size}"
set xlabel "Receivers"
set ylabel "Throughput (Kbps)"
set xrange [-1:5]
set log y
#set key left top
#set key box lt -1
set key below
# ugly - but should be ok for the time being
set xtics ("node1" 0, "node2" 1, "node3" 2, "node5" 3, "node7" 4)
plot #{plots}
}
        end
        sys("cat cr.gnuplot | gnuplot -")
        sys("rm cr.gnuplot")
    }
    #sys("rm #{GRAPH_DATA_DIR}/*.dat")
end
