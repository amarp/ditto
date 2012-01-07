#!/usr/bin/ruby

schemes = ['dot-ohear', 'proxy-ohear', 'proxy-mhear']
chunk_sizes = ['8K', '16K', '32K']

if ARGV.size < 3
    puts "Usage: ./analyze_2.rb <relative_path_of_dir_containing_results_dir>   <numRuns>   <receiver list>"
    puts "Usage e.g.: ./analyze_2.rb . 5 receivers.txt"
    puts "Purpose: For each overhearing node, get the average, min, max # of chunks stored"
    puts "for each scheme, for different chunk sizes, this script creates a file '{scheme}_{chunkSize}_numChunksStored.dat' that constains the following information: "
    puts "    \
        \n    total_num_chunks_requested \
        \n    num_chunks_stored_mean \
        \n    num_chunks_stored_min \
        \n    num_chunks_stored_max \
        \n    total_num_bytes_that_madeup_chunks_(mean) \
        \n    total_num_bytes_in_tcp_streams_sniffed_(mean) \
        \n    useful_% \
        \n    useless_% \
         "
    exit
end

TEST_DIR = ARGV[0]
nr = ARGV[1]
RECEIVER_LIST = ARGV[2]
numRuns = nr.to_i

receiver_list_file_name = "./#{RECEIVER_LIST}"
if not File.exist?(receiver_list_file_name)
    puts "invalid file name"
    exit
end

receivers = Array.new
File.open(receiver_list_file_name).each { |line|
    receivers << line.chomp
    #puts line.chomp
}

results_base_dir = "#{TEST_DIR}/results"


for scheme in schemes

    puts ""
    puts ""
    puts "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
    puts "scheme = #{scheme}"
    for chunk_size in chunk_sizes

        puts "##################################################"
        puts "chunk_size = #{chunk_size}"


        out = File.new("#{results_base_dir}/#{scheme}_#{chunk_size}_numChunksStored.dat", "w")
        out1 = File.new("#{results_base_dir}/#{scheme}_#{chunk_size}_mean_useless_percent_overheard.dat", "w")
        out2 = File.new("#{results_base_dir}/#{scheme}_#{chunk_size}_mean_tcp_bytes_overheard.dat", "w")
        out3 = File.new("#{results_base_dir}/#{scheme}_#{chunk_size}_mean_xput.dat", "w")

        receivers.each { |receiver|
            puts "--------------------------------------------------"
            puts "receiver = #{receiver}"


            results_dir = "#{TEST_DIR}/results/#{receiver}/#{chunk_size}/#{scheme}"
            col_number = 1 + numRuns    # +1 is to skip over the overhearing node's name (in the first column)

            chunk_numbers = Array.new
            File.open("#{results_dir}/chunk_analysis.dat").each { |line|
                a = line.split
                # receiver, min, max , mean
                chunk_numbers << "#{receiver}  \t  #{a[0]}  \t  #{a[col_number]}  \t  #{a[col_number + 1]}  \t  #{a[col_number + 2].ljust(20)}"
            }

            chunk_percent_numbers = Array.new
            File.open("#{results_dir}/chunk_percent_analysis.dat").each { |line|
                a = line.split
                # min, max , mean
                chunk_percent_numbers << "#{a[col_number].ljust(15)}  \t  #{a[col_number + 1].ljust(15)}  \t  #{a[col_number + 2].ljust(20)}"
            }

            chunk_numbers.each_index { |i|
                #puts "#{chunk_numbers[i]}  \t  #{chunk_percent_numbers[i]}"
                out.puts "#{chunk_numbers[i]}  \t  #{chunk_percent_numbers[i]}"
            }

            File.open("#{results_dir}/useless_percent_analysis.dat").each { |line|
                a = line.split
                # receiver, overhearing_node, mean
                out1.puts "#{receiver}  \t  #{a[0]}  \t  #{a[col_number + 2].ljust(20)}"
            }

            File.open("#{results_dir}/tcp_byte_analysis.dat").each { |line|
                a = line.split
                # receiver, overhearing_node, mean
                out2.puts "#{receiver}  \t  #{a[0]}  \t  #{a[col_number + 2].ljust(20)}"
            }

            File.open("#{results_dir}/xput.dat").each { |line|
                a = line.split
                # receiver, mean (-1 because col1 tag is missing in this file)
                out3.puts "#{receiver}  \t  #{a[col_number - 1 + 2].ljust(20)}"
            }
        }
        out.close
        out1.close
        out2.close
        out3.close

    end
end
