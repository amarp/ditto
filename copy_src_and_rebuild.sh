#!/bin/bash -x

BASE_DIR=/home/aphanish/life/research/dist/ditto
SRC_DIR=${BASE_DIR}/dot_snap_20070206
INSTALL_DIR=${SRC_DIR}/install

tar -zxpvf ./ext/dot_snap_20070206.tar.gz

rm -rf $INSTALL_DIR
mkdir -p $INSTALL_DIR

### copy src files
cp ditto.patch ${SRC_DIR}/
cd ${SRC_DIR}/
patch -f -p1 < ditto.patch


## rebuild sfslite
cd $SRC_DIR/sfslite
autoreconf -f -i -s
./configure --prefix=$SRC_DIR/sfslite/install
make
make install

## rebuild ditto
cd $SRC_DIR
autoreconf -i -s
./configure --with-sfs=${SRC_DIR}/sfslite --prefix=${INSTALL_DIR}
make clean
make
make install

## symlink for gcp
sudo ln -s ${INSTALL_DIR}/bin/gcp /usr/local/bin/