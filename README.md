# The ROme OpTimistic Simulator (ROOT-Sim) 

[![Build Status](https://travis-ci.org/HPDCS/ROOT-Sim.svg?branch=master)](https://travis-ci.org/HPDCS/ROOT-Sim)
[![codecov.io](https://codecov.io/gh/HPDCS/ROOT-Sim/branch/master/graphs/badge.svg)](http://codecov.io/github/HPDCS/ROOT-Sim)

*Brought to you by the High Performance and Dependable Computing Systems (HPDCS)
at Sapienza, University of Rome*

----------------------------------------------------------------------------------------

The ROme OpTimistic Simulator is an x86-64 Open Source, multithreaded parallel simulation platform
developed using C/POSIX technology. It transparently supports all the mechanisms associated
with parallelization (e.g., mapping of simulation objects on different kernel instances) and
optimistic synchronization (e.g., state recoverability).

The programming model supported by ROOT-Sim allows the simulation model developer 
to use a simple application-callback function named ProcessEvent() as the event handler,
whose parameters determine which simulation object is currently taking control for
processing its next event, and where the state of this object is located in memory. 
An object is a data structure, whose state can be scattered on dynamically allocated
memory chunks, hence the memory address passed to the callback locates a top level
data structure implementing the object state-layout.

ROOT-Sim's development started as a research project late back in 1987, and is currently
run by the High Performance and Dependable Computing Systems group at the 
Dipartimento di Ingegneria Informatica, Automatica e Gestionale, Sapienza, University of Rome.

## About this Version

This version of ROOT-Sim stands as the latest development branch of the simulator.
Currently, it supports a high-performance multithreaded execution on multicore environments.
The goal of this ultimate version of the simulator is to port all the lessons learned during
almost 30 years of research on more modern multicore architectures. While the current
version still does not allow to run on distributed environments, it will in the near
future, after that many low-level optimizations are completed.

This new version strives to be as backwards compatible as possible, letting all the
historic simulation models developed on ROOT-Sim be compatible, although some
recent changes in the simulator's architecture require minor modifications to
the original models' sources (which are nevertheless being updated in the current branch).

## Installation Notes

ROOT-Sim uses autotools to provide an installation workflow which is
common for all supported platforms. This repository does not provide
already-generated installation scripts (while released tarballs do),
rather we provide the convenience `autogen.sh` script which should
build everything on the target machine. Using autotools, `autoconf`,
`automake` and `libtoolize` are required to let `autogen.sh` generate
the correct `configure` script.

Briefly, the shell commands `./configure; make; make install` should
configure, build, and install this package.
Also, you can also type `make uninstall` to remove the installed files.

By default, `make install` installs the package's commands under
`/usr/local/bin`, include files under `/usr/local/include`, etc.  You
can specify an installation prefix other than `/usr/local` by giving
`configure` the option `--prefix=PREFIX`, where `PREFIX` must be an
absolute path name.

ROOT-Sim uses many `gcc` extensions, so the currently supported
compiler is only `gcc`.


### Optional Features

When running the simulation model, ROOT-Sim allocates a separate
stack for each Logical Process, so as to completely separate
their execution contexts, using custom User-Level Threads.
This could require longer simulation startup
time, which could be avoided during model development by passing
`configure` the option `--disable-ult`

When debugging the platform, it is suggested to pass 
`configure` the option `--enable-debug` to compile the simulator
with more strict error checking, and to include all debugging symbols.


## Usage

When running `make install`, the `rootsim-cc` compiler is added to the path.

To compile a simulaton model, simply `cd` into the project's directory
and type `rootsim-cc *.c -o model`. This will create the `model`
executable, which is the model code already linked with the ROOT-sim library.
`rootsim-cc` ultimately relies on `gcc`, so any flag supported by
`gcc` can be passed to `rootsim-cc`.

To test the correctness of the model, it can be run sequentially, typing
`./model --sequential --nprc <number of required LPs>`
This allows to spot errors in the implementation more easily.

Then, to run it in parallel, type
`./model --np <number of available Cores> --nprc <number of required LPs>`
