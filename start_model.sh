cd models/packet/
rm model
rootsim-cc application.c -o model
./model --np 3 --nprc 4

