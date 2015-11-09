sudo rmmod schedule_hook
cd intercept
./unload.sh
cd ..
sudo rmmod cross_state_manager
echo "Done"
