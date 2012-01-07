#!/bin/bash -x

BASE_DIR=/home/aphanish/life/research/dist/ditto
SRC_DIR=${BASE_DIR}/dot_snap_20070206
INSTALL_DIR=${SRC_DIR}/install

rm -rf $INSTALL_DIR
mkdir -p $INSTALL_DIR

cd $SRC_DIR
autoreconf -i -s
./configure --with-sfs=${SRC_DIR}/sfslite --prefix=${INSTALL_DIR}
make clean
make
make install
