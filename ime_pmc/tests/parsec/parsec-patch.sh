PARSECDIR=$1

if [ ! -d "$PARSECDIR" ]; then
	echo "Invalid arg, specific a valid path."
	exit 1
fi

declare -a benchmarks=(blackscholes fluidanimate swaptions canneal streamcluster)

BENCH=blackscholes
# blackscholes patch
rm $PARSECDIR/pkgs/apps/$BENCH/src/blackscholes.c
cp patch/$BENCH/blackscholes.c $PARSECDIR/pkgs/apps/$BENCH/src/
cp ../../include/hop-ioctl.h $PARSECDIR/pkgs/apps/$BENCH/src/

echo "$BENCH successfully patched"

BENCH=fluidanimate
# fluidanimate patch
rm $PARSECDIR/pkgs/apps/$BENCH/src/pthreads.cpp
cp patch/$BENCH/pthreads.cpp $PARSECDIR/pkgs/apps/$BENCH/src/
cp ../../include/hop-ioctl.h $PARSECDIR/pkgs/apps/$BENCH/src/

echo "$BENCH successfully patched"

BENCH=swaptions
# fluidanimate patch
rm $PARSECDIR/pkgs/apps/$BENCH/src/HJM_Securities.cpp
cp patch/$BENCH/HJM_Securities.cpp $PARSECDIR/pkgs/apps/$BENCH/src/
cp ../../include/hop-ioctl.h $PARSECDIR/pkgs/apps/$BENCH/src/

echo "$BENCH successfully patched"

BENCH=canneal
# fluidanimate patch
rm $PARSECDIR/pkgs/kernels/$BENCH/src/main.cpp
cp patch/$BENCH/main.cpp $PARSECDIR/pkgs/kernels/$BENCH/src/
cp ../../include/hop-ioctl.h $PARSECDIR/pkgs/kernels/$BENCH/src/

echo "$BENCH successfully patched"

BENCH=streamcluster
# fluidanimate patch
rm $PARSECDIR/pkgs/kernels/$BENCH/src/streamcluster.cpp
cp patch/$BENCH/streamcluster.cpp $PARSECDIR/pkgs/kernels/$BENCH/src/
cp ../../include/hop-ioctl.h $PARSECDIR/pkgs/kernels/$BENCH/src/

echo "$BENCH successfully patched"
echo "done."

