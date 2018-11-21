#!/bin/bash

./autogen.sh
./configure --enable-mpi --enable-coverage 
make
sudo make install

