#! /bin/bash
PARSECDIR=$1
PROFILERDIR=$(pwd)/"utility"
PROFILER=$PROFILERDIR/profiler
PARSEC=$PARSECDIR/bin/parsecmgmt
TMPFILE=tmp_time
LOGDIR=$(pwd)/"log"
RESULTDIR=$(pwd)/"result"
PARTIALDIR=$(pwd)/"partial"
# you can pass the number of run as 2° argument - default is 5
RUN=${2:-5}
DATASET=${3:-'native'} 
 
if [ ! -d "$PROFILERDIR" ]; then
	echo "Error: cannot find profiler directory." >&2
	exit 1
fi

gcc $PROFILER.c -I../../include -o $PROFILER
if [ $? != 0 ]; then
	echo "Unable to compile profiler, exit"
	exit 1
fi	

if [ ! -d "$PARSECDIR" ]; then
	echo "Error: invalid path for parsec dir." >&2
	exit 1
fi

if ! [ -x "$(command -v $PARSEC)" ]; then
  echo 'Error: unknow paserc command.' >&2
  exit 1
fi

if ! [ -x "$(command -v $PROFILER)" ]; then
  echo 'Error: unknow profiler command.' >&2
  exit 1
fi

# backup old log dir
idx=1
if [ -d "$LOGDIR" ]; then
	while [ -d "$LOGDIR.${idx}" ]
	do
		idx=$(echo "$idx + 1" | bc)
	done
	mv $LOGDIR "$LOGDIR.${idx}"
fi
mkdir $LOGDIR

#backup old result dir
idx=1
if [ -d "$RESULTDIR" ]; then
	while [ -d "$RESULTDIR.${idx}" ]
	do
		idx=$(echo "$idx + 1" | bc)
	done
	mv $RESULTDIR "$RESULTDIR.${idx}"
fi
mkdir $RESULTDIR

#backup old partial dir
idx=1
if [ -d "$PARTIALDIR" ]; then
	while [ -d "$PARTIALDIR.${idx}" ]
	do
		idx=$(echo "$idx + 1" | bc)
	done
	mv $PARTIALDIR "$PARTIALDIR.${idx}"
fi
mkdir $PARTIALDIR

declare -a types=(irq nmi)
#declare -a benchmarks=(canneal blackscholes fluidanimate swaptions streamcluster)
declare -a benchmarks=(blackscholes canneal swaptions)
# this is run on 32 (no SMT) cores machine
declare -a num_threads=(1 2 4 8 16 32 64 128)

TIMECMD=/usr/bin/time
TIMESTR="%e, %U, %S"
SUBCMD="$TIMECMD -f '${TIMESTR}' -o $(pwd)/$TMPFILE"

# run this first to be sure that the script won't stop at some bench selection
for b in "${benchmarks[@]}"
do
	# clear previous benchmark builds
	$PARSEC -a uninstall -p $b -c gcc-pthreads >> $LOGDIR"/${b}_clean.log"
	if [ $? != 0 ]; then
		echo "Unable to perform $PARSEC -a uninstall -p ${b} -c gcc-pthreads at ${b}"
		exit 1
	fi

	# build the benchmark
	$PARSEC -a build -p $b -c gcc-pthreads >> $LOGDIR"/${b}_build.log"
	if [ $? != 0 ]; then
		echo "Unable to perform $PARSEC -a build -p ${b} -c gcc-pthreads at ${b}"
		exit 1
	fi

	echo "Benchmark ${b} built and ready to run"
done

for k in  "${types[@]}"
do
	if [ $k == irq ]; then
		cd ../../
		./xclean.sh	
		./irq_load.sh
		cd tests/parsec
		# 0 means no profiling
		declare -a frequencies=(0 1024 2048 4096 8192 12288 16384)
	else
		cd ../../
		./xclean.sh	
		./nmi_load.sh
		cd tests/parsec
		declare -a frequencies=(0 4096 8192 12288 16384 24576 32768)
	fi 

	for b in "${benchmarks[@]}"
	do
		for f in "${frequencies[@]}"
		do
			# header frequencies
			if [ $f == 0 ]; then
				echo -e "\n\n\"no_profiler\"" >> $RESULTDIR/"${k}_${b}.bch"
			else
				echo -e "\n\n\"${f}\"" >> $RESULTDIR/"${k}_${b}.bch"
			fi
	
			echo "Running $b at frequency sampling $f"
	
			echo "#FREQUENCY ${f}" >> $PARTIALDIR/"${k}_${b}.bch"
	
			for t in "${num_threads[@]}"
			do
				echo -e ".\c"
	
				# these are used for time stats
				val_real=0
				val_user=0
				val_sys=0
	
				echo "#FREQUENCY ${f}" >> $PARTIALDIR/"${k}_${b}_${t}.bch"
	
				# Execute the benchmark with given configuration $RUN times
				for i in $(seq 1 $RUN)
				do
					## NOTE this could be moved above, but the on/off clears the HOP stats
					# switch off the profiler
					if [ $f == 0 ]; then
						$PROFILER -f
						if [ $? != 0 ]; then
							echo "Unable to perform $PROFILER -f at ${k}-${b}-${t}-${f}"
							exit 1
						fi
					# switch on the profiler and set up the sampling frequency
					else
						$PROFILER -n -s $f
						if [ $? != 0 ]; then
							echo "Unable to perform $PROFILER -n -s $f at ${k}-${b}-${t}-${f}"
							exit 1
						fi
					fi
	
					# run the benchmark
					$SUBCMD $CPU >> $LOGDIR"/${k}_${b}_${t}.log"
					if [ $? != 0 ]; then
						echo "Unable to perform $PARSEC -a run -s $SUBCMD -p ${b} -c gcc-pthreads -i ${DATASET} at ${k}-${b}-${t}-${f}"
						exit 1
					fi
					
					# $SUBCMD saves into $TMPFILE, now read it and split 
					IFS=', ' read -r -a val_tmp <<< $(cat $(pwd)/$TMPFILE)
	
					echo "${t} ${i} ${val_tmp[0]} ${val_tmp[1]} ${val_tmp[2]}" >> $PARTIALDIR/"${k}_${b}.bch"
					
					echo "#RUN ${i}" >> $PARTIALDIR/"${k}_${b}_${t}.bch"
					echo "#TIME ${val_tmp[0]} ${val_tmp[1]} ${val_tmp[2]}" >> $PARTIALDIR/"${k}_${b}_${t}.bch"
					echo "#TIDS" >> $PARTIALDIR/"${k}_${b}_${t}.bch"
					for e in $(ls /dev/hop)
					do
						if [ $e == 'ctl' ]; then
							echo "#CPUS" >> $PARTIALDIR/"${k}_${b}_${t}.bch"
							$PROFILER -p >> $PARTIALDIR/"${k}_${b}_${t}.bch"
						else
							$PROFILER -t $e >> $PARTIALDIR/"${k}_${b}_${t}.bch"
						fi
					done
	
					val_real=$(echo "$val_real + ${val_tmp[0]}" | bc)
					val_user=$(echo "$val_user + ${val_tmp[1]}" | bc)
					val_sys=$(echo "$val_sys + ${val_tmp[2]}" | bc)
	
					# clean HOP module
					$PROFILER -c
				done
	
				val_real=$(echo "scale=3; $val_real / $RUN" | bc)
				val_user=$(echo "scale=3; $val_user / $RUN" | bc)
				val_sys=$(echo "scale=3; $val_sys / $RUN" | bc)
	
				# #threads #freq #real # user #sys
				echo "${t} $val_real $val_user $val_sys" >> $RESULTDIR/"${k}_${b}.bch"
			done
			echo "!"
		done
	done
done
echo "done."
