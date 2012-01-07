#!/usr/bin/ruby

schemes = ['dot-ohear', 'proxy-ohear', 'proxy-mhear']
schemes_tags = ['End-to-end', 'Ditto (w/o inter-stream reassembly)', 'Ditto']
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

line_styles << "lt 3 lw 3"
line_styles << "lt -1 lw 2"
line_styles << "lt 0 lw 4"
line_styles << "lt 8 lw 3"
line_styles << "lt 1 lw 3"
line_styles << "lt 7 lw 3"

=begin
line_styles << "lt 3 lw 3"
line_styles << "lt -1 lw 2"
line_styles << "lt 8 lw 3"
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

if ARGV.size < 1
    puts "Usage: ./graph_useless_percent_fixed_scheme.rb <results_dir - the directory which has \"results\">"
    puts "Precondition - needs the files that have cdf data for various schemes Run generate_wasted_bytes_cdf_data.rb"
    exit
end

RESULTS_DIR = ARGV[0]
GRAPH_DIR = "#{RESULTS_DIR}/graphs"
#Dir.mkdir(GRAPH_DIR) unless GRAPH_DIR == '' || File.exist?(GRAPH_DIR)
system("mkdir -p #{GRAPH_DIR}")

results_base_dir = "#{RESULTS_DIR}/results"
#puts "#{results_base_dir}"
GRAPH_DATA_DIR = results_base_dir

for comb in schemes

    puts "##################################################"
    puts "scheme = #{comb}"


    gtype_tags.each_index { |i|
        gtype_tag = gtype_tags[i]
        gtype = gtypes[i]

        plots = []
        chunk_sizes.each_index { |j|
            chunk_size = chunk_sizes[j]
            puts "'#{GRAPH_DATA_DIR}/useless_percent_#{chunk_size}_#{comb}.cdf' using 1:2 with lines #{line_styles[j]}  title '#{chunk_size}B'"
            plots << "'#{GRAPH_DATA_DIR}/useless_percent_#{chunk_size}_#{comb}.cdf' using 1:2 with lines #{line_styles[j]}  title '#{chunk_size}B'"
        }
        plots = plots.join(', ')

        File.open("cr.gnuplot", "w") do |out|
            out.puts %Q{
set terminal #{gtype_tag}
set size 0.65
set output '#{GRAPH_DIR}/useless_percent_#{comb}_#{gtype}.eps'
#set title "#{comb}"
#set xlabel "% bytes overheard that were useless"
set xlabel "Memory overhead (% bytes)"
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
