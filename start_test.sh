#!/bin/sh
sudo dmesg --clear

cd src/arch/linux/modules/
./unload.sh

echo "unload modules"

cd -
./configure; make; sudo make install;

echo "install rootsim Done"

cd src/arch/linux/modules/
./load.sh

echo "load modules Done"

cd -
cd models/packet/
rm model
rootsim-cc application.c -o model
./model --np 3 --nprc 81
