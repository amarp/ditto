#!/usr/bin/ruby


schemes = ['dot', 'proxy', 'proxy-mhear']
#chunk_sizes = ['8K', '16K', '32K']
chunk_sizes = ['8K']
file_size = 1024 #KB

def is_nonzero_int_and_file(x)
    begin
        Integer( File.basename(x) ) != 0 and File.file? x
    rescue
        false
    end
end

def append_agg(array, out, param)
    m4 = min_max_mean_median(array)
    out.puts "#{param} #{m4}"
end

def mean(array)
    array.inject{|sum, x| sum += x} / array.size.to_f
end

def min_max_mean_median(array)
    if not array.empty?
        avg = array.inject{|sum, x| sum += x} / array.size.to_f
        med = median(array)
        min = array.min()
        max = array.max()
        return "#{min} #{max} #{avg} #{med}"
    else
        return "0 0 0 0"
    end    
end

def median(array)
    return nil if array.empty?
    sorted_array = array.sort
    m_pos = sorted_array.size / 2
    return sorted_array.size % 2 == 1 ? sorted_array[m_pos] : mean(sorted_array[m_pos-1..m_pos])
end

def modes(array, find_all=true)
    histogram = array.inject(Hash.new(0)) { |h, n| h[n] += 1; h }
    mode_values = nil
    histogram.each_pair do |item, times|
        mode_values << item if mode_values && times == mode_values[0] and find_all
        mode_values = [times, item] if (!mode_values && times>1) or (mode_values && times>mode_values[0])
    end
    return mode_values ? mode_values[1...mode_values.size] : mode_values
end

if ARGV.size < 6
    puts "Usage: ./analyze_1.rb <testbed> <test_dir_relative_path>  <recvr_list>  <num_runs>  <zip? (1/0)>  <file_size_in_KB>"
    puts "Usage e.g.: ./analyze_xput.rb emulab \"./emulab/300KB/\" recvrs.txt 3 1 1024"
    puts "Purpose: Collect xput statistics"
    puts "    In the results directory creates a tree structure as follows -> <receiver#>/<chunk_size>/<scheme>"
    puts "    and in there creates a"
    puts "    where each row has the form: <overhearing_node>  <stat_run_1>  <stat_run_2> ... <stat_run_n>  <min>  <max>  <mean>  <median>"
    puts "To get receivers.txt use a command like - ls ./emulab/300KB/ | grep expt-nodew | cut -d - -f 2 | uniq"
    exit
end

testbed = ARGV[0]
TEST_DIR  = ARGV[1]
NODE_LIST = ARGV[2]
nr = ARGV[3]
numRuns = nr.to_i

zip = ARGV[4]
bzip = zip.to_i

fs = ARGV[5]
file_size = fs.to_i

log_file_post_fix = ""
cat_cmd = "cat"
if bzip == 1
    cat_cmd = "zcat"
    log_file_post_fix = ".gz"
end

qualifier = ".ditto.netarch.emulab.net"
if testbed == "map"
        qualifier = ""
end

#puts qualifier

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

results_base_dir = "#{TEST_DIR}/results"
system("rm -rf #{results_base_dir}/*")

for comb in schemes

    for chunk_size in chunk_sizes

        results_dir = "#{results_base_dir}/xput/#{chunk_size}/#{comb}"
        system("mkdir -p #{results_dir}")

        out_xput = File.new("#{results_dir}/xput.dat", "w")
        out_xput_individual = File.new("#{results_dir}/xput_individual.dat", "w")

        recvrs.each { |recvr|
            puts "--------------------------------------------------"
            puts "recvr = #{recvr}"

             
            a_total_xput = []
            a_total_xput_display = []

            for run_number in 0..(numRuns-1)

              recvr_prefix = "#{recvr}" + qualifier
              data_dump_dir = "#{TEST_DIR}/expt-#{chunk_size}-#{comb}-#{run_number}"
              line_count = `#{cat_cmd} #{data_dump_dir}/gcp/#{recvr_prefix}-gcp.log#{log_file_post_fix} | grep "Finished GET" | wc -l`
              #puts "#{data_dump_dir}, #{line_count.to_i}"
              
              if line_count.to_i > 0
                  # total time
                  totalTime = `#{cat_cmd} #{data_dump_dir}/dot/#{recvr_prefix}-dot.log#{log_file_post_fix} | grep "time for gtcd data start-finish" | awk '{print $7}'`
                  if totalTime.to_f > 0
                      totalXput = (file_size * 1024 * 8) / (totalTime.to_f * 1000) #Kbps
                      a_total_xput << totalXput
                      a_total_xput_display << totalXput

                      out_xput_individual.puts "#{recvr} #{totalXput}"
                  else
                      a_total_xput_display << -1
                  end
              else
                puts "#{data_dump_dir} run failed !!!"
                a_total_xput_display << -1
              end
            end

            pretty_str = "#{recvr}  " + a_total_xput_display.join(' ')
            append_agg(a_total_xput, out_xput, pretty_str)

        }

        out_xput.close
        out_xput_individual.close

    end #chunk_size

end  #comb
