#!/bin/bash


function usage 
{
  echo 
  echo $0 "<install directory> <sfslite install dir>"
  echo "e.g.: $0 /home/aphanish/life/research/dist/ditto/install /home/aphanish/life/research/dist/ditto/dot_snap_20070206/sfslite"
  echo	
}

function intro
{
  echo "This script will be build and install dot binaries"
  echo "using 8K,16K, and 32K chunk sizes."
  echo "Binaries will be installed as ..."
  echo $dot_8K
  echo $dot_16K
  echo $dot_32K
  echo "sfslite installation to be used is " $sfslite_dir
}

function rm_dot_installs
{
 rm -f -r $dot_8K
 rm -f -r $dot_16K
 rm -f -r $dot_32K
}

function initialize_dot_installs
{
  dot_8K=$install_dir/dot_8K
  dot_16K=$install_dir/dot_16K
  dot_32K=$install_dir/dot_32K
}

function check_usage 
{
  if [ $# -ne 2 ]; then
    usage;
    exit;	
  fi
}

function build_dot_8K
{
  make clean && make distclean
  cp gtcd/chunkerPlugin_8K.h gtcd/chunkerPlugin.h
  cp gtcd/chunker/chunkAlg_fixed_8K.h gtcd/chunker/chunkAlg_fixed.h
  autoreconf -i -s 
  ./configure --with-sfs=$sfslite_dir --prefix=$dot_8K
  make 
  make install
  mv $dot_8K/bin/sniffTcp $dot_8K/bin/sniffTcp_8K
  mv $dot_8K/sbin/gtcd $dot_8K/sbin/gtcd_8K
}

function build_dot_16K
{
  make clean && make distclean
  cp gtcd/chunkerPlugin_16K.h gtcd/chunkerPlugin.h
  cp gtcd/chunker/chunkAlg_fixed_16K.h gtcd/chunker/chunkAlg_fixed.h
  autoreconf -i -s
  ./configure --with-sfs=$sfslite_dir --prefix=$dot_16K
  make
  make install
  mv $dot_16K/bin/sniffTcp $dot_16K/bin/sniffTcp_16K
  mv $dot_16K/sbin/gtcd $dot_16K/sbin/gtcd_16K
}

function build_dot_32K
{
  make clean && make distclean
  cp gtcd/chunkerPlugin_32K.h gtcd/chunkerPlugin.h
  cp gtcd/chunker/chunkAlg_fixed_32K.h gtcd/chunker/chunkAlg_fixed.h
  autoreconf -i -s
  ./configure --with-sfs=$sfslite_dir --prefix=$dot_32K
  make
  make install
  mv $dot_32K/bin/sniffTcp $dot_32K/bin/sniffTcp_32K
  mv $dot_32K/sbin/gtcd $dot_32K/sbin/gtcd_32K
}


function reset_dot_sources
{
  make clean && make distclean
  cp gtcd/chunkerPlugin_16K.h gtcd/chunkerPlugin.h
  cp gtcd/chunker/chunkAlg_fixed_16K.h gtcd/chunker/chunkAlg_fixed.h
}


function copy_emulab_header
{
  cp gtcd/testbed_emulab.h gtcd/testbed.h
}


function copy_map_header
{
  cp gtcd/testbed_map.h gtcd/testbed.h
}


check_usage $1 $2;
install_dir=$1
sfslite_dir=$2

# emulab
install_dir=$1/emulab
initialize_dot_installs;
intro;
rm_dot_installs;
copy_emulab_header;
build_dot_8K;
build_dot_16K;
build_dot_32K;
reset_dot_sources;

# map
install_dir=$1/map
initialize_dot_installs;
intro;
rm_dot_installs;
copy_map_header;
build_dot_8K;
build_dot_16K;
build_dot_32K;
reset_dot_sources;

exit;

