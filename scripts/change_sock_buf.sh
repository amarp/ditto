#!/bin/bash -x

#sudo sysctl -w net.core.wmem_max=5120000
sudo sysctl -w net.core.rmem_max=5120000
#sudo sysctl -w net.core.wmem_max=5120000
sudo sysctl -w net.core.rmem_max=5120000
