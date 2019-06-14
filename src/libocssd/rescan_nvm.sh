#!/bin/bash

rmmod -f nvme
echo 1 > /sys/bus/pci/devices/0000:01:00.0/remove
echo "ready to rescan"
sleep 1
echo 1 > /sys/bus/pci/rescan
modprobe nvme
sleep 1
ls /dev/nvme*
nvme list
