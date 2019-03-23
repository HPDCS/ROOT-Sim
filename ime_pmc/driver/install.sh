make clean
make install 
cd ../main
make 

ENABLE=$(sudo cat /proc/kallsyms | grep enablePMC)
IFS=' ' read -ra ADDR <<< "$ENABLE"
HOOK_ENABLE=${ADDR[0]}

DISABLE=$(sudo cat /proc/kallsyms | grep disablePMC)
IFS=' ' read -ra ADDR <<< "$DISABLE"
HOOK_DISABLE=${ADDR[0]}

cd ../../joejoe/driver
make clean
make
sudo insmod joejoe.ko enable=$HOOK_ENABLE disable=$HOOK_DISABLE
cd ../tools
make
