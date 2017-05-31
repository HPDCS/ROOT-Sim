#!/bin/sh
device="ktblmgr"

sudo rmmod schedule.ko
cd intercept
./unload.sh
cd ..
sudo rm -f /dev/${device}
sudo rmmod cross_state_manager.ko

echo "Done"
