#!/bin/bash

function do_test() {
	cd models/$1/
	rootsim-cc *.c -o model
	cd ../..
	cp models/$1/model .
	#mpiexec --np 2 ./model --np 2 --nprc $2 --no-core-binding
	./model --np 2 --nprc $2 --no-core-binding
	./model --sequential --nprc 1
}

do_test pcs 16
do_test packet 4
do_test collector 4
