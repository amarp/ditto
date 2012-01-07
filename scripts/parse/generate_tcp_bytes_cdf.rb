#!/usr/bin/ruby

schemes = ['dot-ohear', 'proxy-ohear', 'proxy-mhear']
chunk_sizes = ['8K', '16K', '32K']

def sys(cmd)
  if not system(cmd)
    raise "command failed: #{cmd}"
  end
end

if ARGV.size < 1
    puts "Usage: ./generate_tcp_bytes_cdf_data.rb <results_dir - the directory which has \"results\">"
    puts "Precondition - should have run analyze_2.rb"
    exit
end

RESULTS_DIR = ARGV[0]
puts "#{RESULTS_DIR}"
results_base_dir = "#{RESULTS_DIR}/results"
puts "#{results_base_dir}"

for chunk_size in chunk_sizes

    puts "##################################################"
    puts "chunk size = #{chunk_size}"

    for scheme in schemes
        puts "-----"
        puts "scheme = #{scheme}"
        sys("./cdfgen #{results_base_dir}/#{scheme}_#{chunk_size}_mean_tcp_bytes_overheard.dat 500 2 #{results_base_dir}/tcp_#{chunk_size}_#{scheme}.cdf")
    end
end

sys("./graph_tcp_bytes_fixed_scheme.rb #{RESULTS_DIR}")
