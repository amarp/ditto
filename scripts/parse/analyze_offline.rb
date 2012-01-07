#!/usr/bin/ruby


#schemes = ['dot-ohear', 'proxy-ohear-offline', 'proxy-mhear-offline']
#schemes = ['dot-ohear']
schemes = ['proxy-ohear-offline', 'proxy-mhear-offline']
chunk_sizes = ['8K', '16K', '32K']
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
    puts "Usage: ./analyze_1.rb <testbed> <test_dir_relative_path> <recvr_list> <num_runs> <zip? (1/0)> <sniffer_list>"
    puts "Usage e.g.: ./analyze_1.rb emulab \"./emulab/300KB/\" recvrs.txt 3 1 128 sniffers.txt"
    puts "Purpose: Collect overhearing statistics"
    puts "    In the results directory creates a tree structure as follows -> <receiver#>/<chunk_size>/<scheme>"
    puts "    and in there creates 1 file per stat,"
    puts "    and each file has a row of the form: <overhearing_node>  <stat_run_1>  <stat_run_2> ... <stat_run_n>  <min>  <max>  <mean>  <median>"
    puts "    Current records the following stats :"
    puts "      percentage_chunks_sniffed\
\n      number_of_chunks_sniffed\
\n      num_chunk_bytes_sniffed\
\n      num_tcp_bytes_sniffed\
\n      useless_percent_of_bytes_sniffed"
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

SNIFFER_LIST = ARGV[5]

log_file_post_fix = ""
cat_cmd = "cat"
if bzip == 1
    cat_cmd = "zcat"
    log_file_post_fix = ".gz"
end

qualifier = ".ditto.cmu849.emulab.net"
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

sniffer_list_file_name = "./#{SNIFFER_LIST}"
if not File.exist?(sniffer_list_file_name)
    puts "invalid file name"
    exit
end

sniffers = Array.new
File.open(sniffer_list_file_name).each { |line|
    sniffers << line.chomp
    #puts line.chomp
}

results_base_dir = "#{TEST_DIR}/results"
#system("rm -rf #{results_base_dir}/*")

recvrs.each { |recvr|
    puts "--------------------------------------------------"
    puts "recvr = #{recvr}"

    for chunk_size in chunk_sizes
        
        cs = chunk_size
        cs.gsub("K","")
        total_num_chunks_requested = file_size / cs.to_i
        puts "Chunks #{total_num_chunks_requested.to_i}"

        for comb in schemes

            results_dir = "#{results_base_dir}/#{recvr}/#{chunk_size}/#{comb}"
            system("mkdir -p #{results_dir}")

            out_c = File.new("#{results_dir}/chunk_analysis.dat", "w")
            out_cb = File.new("#{results_dir}/chunk_byte_analysis.dat", "w")
            out_tb = File.new("#{results_dir}/tcp_byte_analysis.dat", "w")
            out_c_percent = File.new("#{results_dir}/chunk_percent_analysis.dat", "w")
            out_useless_percent = File.new("#{results_dir}/useless_percent_analysis.dat", "w")

            sniffers.each { |other_recvr|

                if (other_recvr =~ /(.*?)(\d+)/) 
                  other_recvr_int = $2
                else
                  exit 1
                end

                puts "Index #{other_recvr_int.to_i}"

                if other_recvr != recvr

                    a_num_chunk_stats = []
                    a_num_chunk_bytes_stats = []
                    a_num_tcp_bytes_stats = []
                    a_total_chunks_requested = []
                    a_percentage_chunks_sniffed = []
                    a_useless_percent_stats = []


                    a_num_chunk_stats_display = []
                    a_num_chunk_bytes_stats_display = []
                    a_num_tcp_bytes_stats_display = []
                    a_total_chunks_requested_display = []
                    a_percentage_chunks_sniffed_display = []
                    a_useless_percent_stats_display = []


                    for run_number in 0..(numRuns-1)

                        recvr_prefix = "#{recvr}" + qualifier
                        data_dump_dir = "#{TEST_DIR}/expt-#{recvr}-#{chunk_size}-#{comb}-#{run_number}"
                        if File.exist?("#{data_dump_dir}/gcp/#{recvr_prefix}-gcp.log#{log_file_post_fix}")
                          line_count = `#{cat_cmd} #{data_dump_dir}/gcp/#{recvr_prefix}-gcp.log#{log_file_post_fix} | grep "Finished GET" | wc -l`
                        else
                          line_count = 0
                        end
                      
                      puts "#{data_dump_dir}, #{line_count.to_i}, #{other_recvr}"
                        
                        if line_count.to_i > 0

                          if total_num_chunks_requested < 0
                            # total chunks
                            totalNumChunksRequested = `#{cat_cmd} #{data_dump_dir}/dot/#{recvr_prefix}-dot.log#{log_file_post_fix} | grep ": had to fetch " | awk '{print $5}'`
                          else
                            totalNumChunksRequested = total_num_chunks_requested
                          end
                            a_total_chunks_requested << totalNumChunksRequested.to_i
                            a_total_chunks_requested_display << totalNumChunksRequested.to_i

                            # num chunks overhead
                            other_recvr_prefix = "#{other_recvr}" + qualifier
                            numChunksStored = `#{cat_cmd} #{data_dump_dir}/dot/#{other_recvr_prefix}-dot.log#{log_file_post_fix} | grep "snifferPlugin_tcp: received chunk" | awk '{print $4}' | sort -u | wc -l`
                            numChunksSniffed = `#{cat_cmd} #{data_dump_dir}/sniffer/#{other_recvr_prefix}-sniffer.log#{log_file_post_fix} | tail -n 8 | grep numChunksSniffed | tail -n 1 | awk '{print $3}'`
                            a_num_chunk_stats << numChunksSniffed.to_i
                            a_num_chunk_stats_display << numChunksSniffed.to_i

                            
                            # percent of chunks overheard
                            percentage_chunks_sniffed = 0.0
                            if totalNumChunksRequested.to_i > 0
                                percentage_chunks_sniffed = (numChunksStored.to_f / totalNumChunksRequested.to_f) * 100
                            end
                            a_percentage_chunks_sniffed << percentage_chunks_sniffed
                            a_percentage_chunks_sniffed_display << percentage_chunks_sniffed

                            #puts " =====> #{other_recvr} overheard #{percentage_chunks_sniffed} (#{numChunksStored.to_i} / #{totalNumChunksRequested.to_i})"


                            # bytes of chunks overheard
                            numBytesOfChunksSniffed = `#{cat_cmd} #{data_dump_dir}/sniffer/#{other_recvr_prefix}-sniffer.log#{log_file_post_fix} | tail -n 8 | grep numBytesOfChunksSniffed | tail -n 1 | awk '{print $3}'`
                            a_num_chunk_bytes_stats << numBytesOfChunksSniffed.to_i
                            a_num_chunk_bytes_stats_display << numBytesOfChunksSniffed.to_i
                            

                            # tcp bytes overheard
                            numBytesOfTcpPacketsCaptured = `#{cat_cmd} #{data_dump_dir}/sniffer/#{other_recvr_prefix}-sniffer.log#{log_file_post_fix} | tail -n 8 | grep numBytesOfTcpPacketsCaptured | tail -n 1 | awk '{print $3}'`
                            a_num_tcp_bytes_stats << numBytesOfTcpPacketsCaptured.to_i
                            a_num_tcp_bytes_stats_display << numBytesOfTcpPacketsCaptured.to_i


                            # duplicate bytes for chunks that were indentified
                            num_dup_bytes = `#{cat_cmd} #{data_dump_dir}/sniffer/#{other_recvr_prefix}-sniffer.log#{log_file_post_fix} | tail -n 8 | grep numDuplicateBytes | tail -n 1 | awk '{print $3}'`
                            useless_percent = 0.0
                            if numBytesOfTcpPacketsCaptured.to_i > 0
                                useless_percent = ((numBytesOfTcpPacketsCaptured.to_f - numBytesOfChunksSniffed.to_f - num_dup_bytes.to_f) / numBytesOfTcpPacketsCaptured.to_f) * 100
                            end
                            a_useless_percent_stats << useless_percent
                            a_useless_percent_stats_display << useless_percent

                        else
                            puts "#{data_dump_dir} run failed !!!"
                            a_total_chunks_requested_display << -1
                            a_num_chunk_stats_display << -1
                            a_percentage_chunks_sniffed_display << -1
                            a_num_chunk_bytes_stats_display << -1
                            a_num_tcp_bytes_stats_display << -1
                            a_useless_percent_stats_display << -1
                        end #if

                    end #run

                    pretty_str = "#{other_recvr_int.to_i} " + a_percentage_chunks_sniffed_display.join(' ')
                    puts pretty_str
                    append_agg(a_percentage_chunks_sniffed, out_c_percent, pretty_str)

                    pretty_str = "#{other_recvr_int.to_i} " + a_num_chunk_stats_display.join(' ')
                    append_agg(a_num_chunk_stats, out_c, pretty_str)

                    pretty_str = "#{other_recvr_int.to_i} " + a_num_chunk_bytes_stats_display.join(' ')
                    append_agg(a_num_chunk_bytes_stats, out_cb, pretty_str)

                    pretty_str = "#{other_recvr_int.to_i} " + a_num_tcp_bytes_stats_display.join(' ')
                    append_agg(a_num_tcp_bytes_stats, out_tb, pretty_str)

                    pretty_str = "#{other_recvr_int.to_i} " + a_useless_percent_stats_display.join(' ')
                    #puts pretty_str
                    append_agg(a_useless_percent_stats, out_useless_percent, pretty_str)

                end

            } #other_recvr

            out_c.close
            out_cb.close
            out_tb.close
            out_c_percent.close

        end #comb

    end #chunk_size
}
