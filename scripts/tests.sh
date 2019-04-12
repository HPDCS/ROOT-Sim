#!/bin/bash

# USAGE NOTE:
# This script is intended to be used both for testing and for increasing
# the code coverage of the tests.
# Therefore, it is organized so as to generate code coverage paths which
# make sense for gcov to merge all the different runs in proper reports.
# If you cd in the scripts folder and run it, it won't work.
# You should call it from the main ROOT-Sim source folder as:
# ./scripts/tests.sh

tests=()
mpi=()
normal=()
sequential=()

unit_tests=()
unit_results=()

retval=0

function do_unit_test() {
	unit_tests+=($1)
	make -f tests/Makefile $1 > /dev/null
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
}

function do_test() {

	# Compile and store the name of the test suite
	rootsim-cc models/$1/*.c -o model
	tests+=($1)

	# Run this model using MPI
	echo -n "Running test $1 using MPI... "
	mpiexec --np 2 ./model --wt 2 --lp $2 --no-core-binding > /dev/null
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
	./model --wt 2 --lp $2 > /dev/null
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
	./model --sequential --lp 1 > /dev/null
	if test $? -eq 0; then
		sequential+=('Y')
		echo "passed."
	else
		sequential+=('N')
		retval=1
		echo "failed."
	fi
}

function do_test_custom() {

        # Compile and store the name of the test suite
        rootsim-cc models/$1/*.c -o model
        tests+=("$1-cust")

        # Run this model using MPI
        echo -n "Running test $1-cust using MPI... "
        mpiexec --np 2 ./model --wt 2 ${@:2} --no-core-binding > /dev/null
        if test $? -eq 0; then
                mpi+=('Y')
                echo "passed."
        else
                mpi+=('N')
                retval=1
                echo "failed."
        fi

        # Run this model using only worker threads
        echo -n "Running test $1-cust using parallel simulator... "
        ./model --wt 2 ${@:2} > /dev/null
        if test $? -eq 0; then
                normal+=('Y')
                echo "passed."
        else
                normal+=('N')
                retval=1
                echo "failed."
        fi

        # Run this model sequentially
        echo -n "Running test $1-cust sequentially... "
        ./model --sequential ${@:2} > /dev/null
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
do_test collector 4
do_test stupid_model 16

# Additional tests to increase code coverage
do_test_custom pcs --lp 16 --output-dir dummy --npwd --gvt 500 --gvt-snapshot-cycles 3 --verbose info --seed 12345 --scheduler stf --cktrm-mode normal --simulation-time 1000



# Dump test information
echo ""
echo "    SUMMARY OF TEST RESULTS"
echo ""
echo "          _Unit Tests_"
echo "╒══════════════════════════╤══════════╕"
echo "│ Unit Test                │  Passed  │"
echo "╞══════════════════════════╪══════════╡"
for((i=0;i<${#unit_tests[@]};i++));
do
	printf "│ %-24s │    %1s     │\n" ${unit_tests[$i]} ${unit_results[$i]}
	echo "╞══════════════════════════╪══════════╡"
done

echo ""
echo "          _Model Runs_"
echo "╒═══════════════════╤═════╤═════╤═════╕"
echo "│ Testcase          │ Seq │ Par │ MPI │"
echo "╞═══════════════════╪═════╪═════╪═════╡"
for((i=0;i<${#tests[@]};i++));
do
	printf "│ %-17s │  %1s  │  %1s  │  %1s  │\n" ${tests[$i]} ${sequential[$i]} ${normal[$i]} ${mpi[$i]}
	echo "╞═══════════════════╪═════╪═════╪═════╡"
done

exit $retval
