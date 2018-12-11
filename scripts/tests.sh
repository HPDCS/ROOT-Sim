#!/bin/bash

tests=()
mpi=()
normal=()
sequential=()

unit_tests=()
unit_results=()

retval=0

function do_unit_test() {
	unit_tests+=($1)
	cd tests
	make $1 > /dev/null
	echo -n "Running unit test $1... "
	./$1 > /dev/null
	
	if test $? -eq 0; then
		unit_results+=('Y')
		echo "passed."
	else
		unit_results+=('N')
		retval=1
		echo "failed."
	fi
	cd ..
}

function do_test() {

	# Compile and store the name of the test suite
	rootsim-cc models/$1/*.c -o model
	tests+=($1)

	# Run this model using MPI
	echo -n "Running test $1 using MPI... "
	mpiexec --np 2 ./model --np 2 --nprc $2 --no-core-binding > /dev/null
	if test $? -eq 0; then
		mpi+=('Y')
		echo "passed."
	else
		mpi+=('N')
		retval=1
		echo "failed."
	fi

	# Run this model using only worker threads
	echo -n "Running test $1 using parallel simulator... "
	./model --np 2 --nprc $2 > /dev/null
	if test $? -eq 0; then
		normal+=('Y')
		echo "passed."
	else
		normal+=('N')
		retval=1
		echo "failed."
	fi
	
	# Run this model sequentially
	echo -n "Running test $1 sequentially... "
	./model --sequential --nprc 1 > /dev/null
	if test $? -eq 0; then
		sequential+=('Y')
		echo "passed."
	else
		sequential+=('N')
		retval=1
		echo "failed."
	fi
}



# Run available unit tests
do_unit_test dymelor
do_unit_test numerical


# Run models to make comprehensive tests
do_test pcs 16
do_test packet 4



# Dump test information
echo ""
echo "    SUMMARY OF TEST RESULTS"
echo ""
echo "          _Unit Tests_"
echo "╒═══════════════════╤══════════╕"
echo "│ Unit Test         │  Passed  │"
echo "╞═══════════════════╪══════════╡"
for((i=0;i<${#unit_tests[@]};i++));
do
	printf "│ %-17s │    %1s     │\n" ${unit_tests[$i]} ${unit_results[$i]}
	echo "╞═══════════════════╪══════════╡"
done

echo ""
echo "          _Model Runs_"
echo "╒════════════╤═════╤═════╤═════╕"
echo "│ Testcase   │ Seq │ Par │ MPI │"
echo "╞════════════╪═════╪═════╪═════╡"
for((i=0;i<${#tests[@]};i++));
do
	printf "│ %-10s │  %1s  │  %1s  │  %1s  │\n" ${tests[$i]} ${sequential[$i]} ${normal[$i]} ${mpi[$i]}
	echo "╞════════════╪═════╪═════╪═════╡"
done

exit $retval
