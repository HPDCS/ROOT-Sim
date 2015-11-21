cd models/packet/
rm model
rootsim-cc application.c -o model
./model --np 1 --nprc 512

