#!/usr/bin/ruby


schemes = ['dot-ohear', 'proxy-ohear', 'proxy-mhear']
chunk_sizes = ['8K', '16K', '32K']


def sys(cmd)
  if not system(cmd)
    raise "command failed: #{cmd}"
  end
end

if ARGV.size < 4
    puts "Usage: ./graph_percent_chunks_vs_traffic_pattern.rb  <testbed>  <results_dir_relative_path>   <sniffer_list>  <num_runs>"
    puts "Usage e.g.: ./graph_percent_chunks_vs_traffic_pattern.rb  emulab  \"experiment3/test_3/results\" recvrs.txt 5"
    puts "Precondition - analyze_1.rb should have run (<results_dir_relative_path> above is the directory where the results of analyze_1.rb are stored)"
    puts "Needs UnixStat/bin in ~/tools/UnixStat/bin/"
    exit
end

testbed = ARGV[0]
RESULTS_DIR = ARGV[1]
NODE_LIST = ARGV[2]
nr = ARGV[3]
numRuns = nr.to_i

box_width = 0.25

node_prefix = "nodew"
if testbed == "map"
    node_prefix = "map"
end

GRAPH_DIR = "#{RESULTS_DIR}/graphs"
#Dir.mkdir(GRAPH_DIR) unless GRAPH_DIR == '' || File.exist?(GRAPH_DIR)
system("mkdir -p #{GRAPH_DIR}")

GRAPH_DATA_DIR = "#{RESULTS_DIR}/data_for_graphs/chunk_percent"
#puts "#{GRAPH_DATA_DIR}"
system("mkdir -p #{GRAPH_DATA_DIR}")
system("rm -rf #{GRAPH_DATA_DIR}/*")

node_list_file_name = "./#{NODE_LIST}"
if not File.exist?(node_list_file_name)
    puts "invalid file name"
    exit
end

recvrs = Array.new
File.open(node_list_file_name).each { |line|
    recvrs << line.chomp
    #puts line.chomp
}


results_base_dir = "#{RESULTS_DIR}"
#puts "#{results_base_dir}"

for chunk_size in chunk_sizes

    puts "##################################################"
    puts "chunk size = #{chunk_size}"

    out = File.new("#{GRAPH_DATA_DIR}/#{chunk_size}_chunks_overheard_across_combos.dat", "w")
    recvrs.each { |recvr|
        puts "--------------------------------------------------"
        puts "recvr = #{recvr}"


        a_combo_results = []
        for comb in schemes
            results_sub_dir = "#{results_base_dir}/#{recvr}/#{chunk_size}/#{comb}"

            column_num = 1 + numRuns + 2 # <revr_name> <stat_run_1> ... <stat_run_n> <min> <max> <mean> <median>

            # awk '{printf("%5.2f\n", $7)} {n += 1; t += $7} END {printf("\n\n%5.2f \n",t/n)}'
            min_max_mean_of_means = `cat #{results_sub_dir}/chunk_percent_analysis.dat | awk '{print $#{column_num}}' | ~/tools/UnixStat/bin/stats min max mean`
            #puts "#{min_max_mean_of_means.chomp}"
            a_combo_results << "#{min_max_mean_of_means.chomp}"
        end

        pretty_str = "#{recvr.delete "#{node_prefix}"} \t" + a_combo_results.join(' ')
        out.puts "#{pretty_str}"
    }
    out.close

    for gtype in ["monochrome", "color"]
        plots = []
        i = 1
        j = 0.2
        for comb in schemes
            mean_of_means_column_num = 1 + (i*3)
            offset = (i - 1)*box_width
            puts "'#{GRAPH_DATA_DIR}/#{chunk_size}_chunks_overheard_across_combos.dat' using ($1+#{offset}):#{mean_of_means_column_num} with boxes fs solid #{j} title '#{comb}'"
            plots << "'#{GRAPH_DATA_DIR}/#{chunk_size}_chunks_overheard_across_combos.dat' using ($1+#{offset}):#{mean_of_means_column_num} with boxes fs solid #{j} title '#{comb}'"
            i += 1
            j += 0.2
        end
        plots = plots.join(', ')

        File.open("bandwidth.gnuplot", "w") do |out|
            out.puts %Q{
set terminal postscript eps #{gtype}
set size 0.65
set output '#{GRAPH_DIR}/percent_chunk_vs_receivers_#{chunk_size}_#{gtype}.eps'
set title "percentage of chunks reconstituted by overhearing \\n average across runs"
set xlabel "traffic pattern number (when x was a receiver of a file)"
set ylabel "percentage of chunks"
#set xrange [0:100]
set yrange [0:100]
set key below
set boxwidth #{box_width}
plot #{plots}
}
        end
        sys("cat bandwidth.gnuplot | gnuplot -")
        sys("rm bandwidth.gnuplot")
    end
    #sys("rm #{GRAPH_DATA_DIR}/*.dat")
end
