#! /bin/bash
PROFILERDIR=$(pwd)"/parsec/utility"
PROFILER=$PROFILERDIR/profiler
TMPFILE=tmp_time
LOGDIR=$(pwd)/"log"
PARTIALDIR=$(pwd)/"partial"
# you can pass the number of run as 2° argument - default is 5
RUN=${1:-5}
SIZE=$2
CYCLES=$3
b="synthetic"

INIT=${4:-2048}
STOP=${5:-32768}
STEP=${6:-1024}

echo $PROFILERDIR
if [ ! -d "$PROFILERDIR" ]; then
	echo "Error: cannot find profiler directory." >&2
	exit 1
fi

gcc $PROFILER.c -I../include -o $PROFILER
if [ $? != 0 ]; then
	echo "Unable to compile profiler, exit"
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

idx=1
if [ -d "$PARTIALDIR" ]; then
	while [ -d "$PARTIALDIR.${idx}" ]
	do
		idx=$(echo "$idx + 1" | bc)
	done
	mv $PARTIALDIR "$PARTIALDIR.${idx}"
fi
mkdir $PARTIALDIR

#declare -a frequencies=(0 128 512 1024 2048 8192)
# declare -a frequencies=(128 192 256 384 512)
# frequencies=$4
# declare -a frequencies=(2048  160 192 224 256 288 320 352 384 512)
# declare -a frequencies=(512 384 352 320 288 256 224 192 160 128)
# this is run on 32 (no SMT) cores machine
# declare -a num_threads=(1 2 4 8)
declare -a num_threads=(1)

TIMECMD=/usr/bin/time
TIMESTR="%e, %U, %S"
SUBCMD="$TIMECMD -f '${TIMESTR}' -o $(pwd)/$TMPFILE"

for i in $(seq 1 $RUN)
do
	echo "RUN #$i" 
	for f in {2048..10240..1024}
	do
		echo "Running $b at frequency sampling $f"

		echo "#FREQUENCY ${f}" >> $PARTIALDIR/"${b}.bch"

		for t in "${num_threads[@]}"
		do
			# echo -e ".\c"

			# these are used for time stats
			val_real=0
			val_user=0
			val_sys=0

			echo "#FREQUENCY ${f}" >> $PARTIALDIR/"${b}_${t}.bch"

			## NOTE this could be moved above, but the on/off clears the HOP stats
			# switch off the profiler
			if [ $f == 0 ]; then
				$PROFILER -f
				if [ $? != 0 ]; then
					echo "Unable to perform $PROFILER -f at ${b}-${t}-${f}"
					exit 1
				fi
			# switch on the profiler and set up the sampling frequency
			else
				$PROFILER -s $f
				$PROFILER -n
				if [ $? != 0 ]; then
					echo "Unable to perform $PROFILER -n -s $f at ${b}-${t}-${f}"
					exit 1
				fi
			fi

			# run the benchmark
			$TIMECMD -f "$TIMESTR" -o $(pwd)/$TMPFILE ./synthetic -n $t -i $SIZE -c $CYCLES

			# >> $LOGDIR"/${t}.log"
			if [ $? != 0 ]; then
				echo "Unable to perform ./synthetic -n ${t} -i ${SIZE} -c ${CYCLES} at ${t}-${f}"
				exit 1
			fi
			
			# $SUBCMD saves into $TMPFILE, now read it and split 
			IFS=', ' read -r -a val_tmp <<< $(cat $(pwd)/$TMPFILE)

			echo "${t} ${i} - real: ${val_tmp[0]} user: ${val_tmp[1]} sys: ${val_tmp[2]}"
			echo "${t} ${i} ${val_tmp[0]} ${val_tmp[1]} ${val_tmp[2]}" >> $PARTIALDIR/"${b}.bch"
			
			echo "#RUN ${i}" >> $PARTIALDIR/"${b}_${t}.bch"
			echo "#TIDS" >> $PARTIALDIR/"${b}_${t}.bch"

			for e in $(ls /dev/hop)
			do
				if [ $e == 'ctl' ]; then
					echo "#CPUS" >> $PARTIALDIR/"${b}_${t}.bch"
					prof=$($PROFILER -p)
					IFS=' ' read -r -a stats_tmp <<< $prof
					echo "CPU samples: ${stats_tmp[3]}"
					echo "SPS: $(echo "${stats_tmp[3]} / ${val_tmp[0]}" | bc)"
					echo $prof >> $PARTIALDIR/"${b}_${t}.bch"
				else
					threads=$($PROFILER -t $e)
					echo $threads
					# echo "SPS: $(echo "${stats_tmp[3]} / ${val_tmp[0]}" | bc)"
					echo $threads >> $PARTIALDIR/"${b}_${t}.bch"
				fi
			done

			# val_real=$(echo "$val_real + ${val_tmp[0]}" | bc)
			# val_user=$(echo "$val_user + ${val_tmp[1]}" | bc)
			# val_sys=$(echo "$val_sys + ${val_tmp[2]}" | bc)

			# clean HOP module
			$PROFILER -c

			# val_real=$(echo "scale=3; $val_real / $RUN" | bc)
			# val_user=$(echo "scale=3; $val_user / $RUN" | bc)
			# val_sys=$(echo "scale=3; $val_sys / $RUN" | bc)

			# #threads #freq #real # user #sys
			# echo "${t} $val_real $val_user $val_sys" >> $RESULTDIR/"${b}.bch"
		done
		# echo "!"
	done
done
echo "done."
