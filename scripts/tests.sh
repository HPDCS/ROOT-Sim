#!/bin/bash

tests=()
mpi=()
normal=()
sequential=()

function do_test() {

	# Compile and store the name of the test suite
	cd models/$1/
	rootsim-cc *.c -o model
	cd ../..
	cp models/$1/model .
	tests+=($1)

	# Run this model using MPI
	echo "Running test $1 using MPI..."
	mpiexec --np 2 ./model --np 2 --nprc $2 --no-core-binding > /dev/null
	if test $? -eq 0; then
		mpi+=('Y')
	else
		mpi+=('N')
	fi

	# Run this model using only worker threads
	echo "Running test $1 using parallel simulator..."
	./model --np 2 --nprc $2 > /dev/null
	if test $? -eq 0; then
		normal+=('Y')
	else
		normal+=('N')
	fi
	
	# Run this model sequentially
	echo "Running test $1 sequentially..."
	./model --sequential --nprc 1 > /dev/null
	if test $? -eq 0; then
		sequential+=('Y')
	else
		sequential+=('N')
	fi
}

do_test pcs 16
do_test packet 4

# Dump test information
echo ""
echo "    SUMMARY OF TEST RESULTS"
echo "╒════════════╤═════╤═════╤═════╕"
echo "│ Testcase   │ Seq │ Par │ MPI │"
echo "╞════════════╪═════╪═════╪═════╡"
for((i=0;i<${#tests[@]};i++));
do
	printf "│ %-10s │  %1s  │  %1s  │  %1s  │\n" ${tests[$i]} ${sequential[$i]} ${normal[$i]} ${mpi[$i]}
	echo "╞════════════╪═════╪═════╪═════╡"
done
