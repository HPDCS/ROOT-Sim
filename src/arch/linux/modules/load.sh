make
cd schedule-hook
make
sudo insmod schedule-hook.ko
cd ..
cd cross_state_manager
sudo insmod cross_state_manager.ko
cd ../intercept
./load.sh
cat /sys/module/cross_state_manager/parameters/rootsim_pager_hook > /sys/module/schedule_hook/parameters/the_hook
echo "Done"
