#!/usr/bin/ruby

schemes = ['dot-ohear', 'proxy-ohear', 'proxy-mhear']
chunk_sizes = ['8K', '16K', '32K']

def sys(cmd)
  if not system(cmd)
    raise "command failed: #{cmd}"
  end
end

if ARGV.size < 1
    puts "Usage: ./generate_chunk_reconstruction_fixed_cdf_data.rb <results_dir - the directory which has \"results\">"
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
        sys("./cdfgen #{results_base_dir}/#{scheme}_#{chunk_size}_numChunksStored.dat 0.5 7 #{results_base_dir}/#{chunk_size}_#{scheme}.cdf")
    end
end

sys("./graph_reconstruction_fixed_chunk_size.rb #{RESULTS_DIR}")
sys("./graph_reconstruction_fixed_scheme.rb #{RESULTS_DIR}")
