#!/bin/sh

sudo killall olsrd
rm -rf /tmp/olsr*
sudo rm -f /etc/olsr*
cp -f ~/OLSR/olsrd-0.5.4.tar.bz2 /tmp/
cd /tmp
tar xvjf olsrd-0.5.4.tar.bz2
cd olsrd-0.5.4
make
sudo make install
sudo cp -r ~/OLSR/olsrd.conf /etc/

#line_count=`ifconfig | grep ath0 | wc -l`
line_count=`/sbin/ifconfig | grep ath0 | wc -l`
if [[ $line_count = 1 ]]
then
    sudo ./olsrd -i ath0 -d 0
else
    sudo ./olsrd -i ath1 -d 0
fi
