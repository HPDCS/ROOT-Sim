# Download and Installation Guide

ROOT-Sim can be downloaded cloning the official repository, or downloading a prepackaged tarball containing the distributed source code. To compile the library, a C11 compiler is required.

If you decide to clone the repository, there are two main branches of interest: the `master` branch contains the latest release version, while the `develop` branch contains the latest development version. At the time of this writing, the latest stable version is 2.0.0.

To clone the repository, which is hosted on GitHub, you can issue the following command:

```
git checkout https://github.com/HPDCS/ROOT-Sim
```

## Installation Guide

If you have downloaded a tarball, you will find in the main folder a `configure` script. This script will check the availability of the required dependencies on your machine, and generate a `Makefile`. To compile the library, run the following commands:

```
./configure
make
make install
```

This will install the library system-wide in your machine.

There are several useful options which can be passed to `configure`, depending on how you want ROOT-Sim to be compiled and installed. Some of these options relate to specific subsystems which are described in other sections. The important options are:

* `--prefix`: by default, `make install` will install all the files in `/usr/local/bin`, `/usr/local/lib` etc.  You can specify an installation prefix other than `/usr/local` using `--prefix`, for instance `--prefix=$HOME`. This is particularly useful if you are installing ROOT-Sim on a machine for which you do not have administrator privileges. Be sure to include `prefix/bin` in your path, otherwise you will not have direct access to the `rootsim-cc` compiler which is required to generate ROOT-Sim compatible simulation models.
*  `--enable-mpi`: if you intend to run simulation models on a cluster, ROOT-Sim can do it transparently by relying on MPI. `--enable-mpi` instructs `configure` to look for an available MPI compiler (such as `mpicc`) to generate the distributed code. We do not require a particular implementation of MPI to be used, although ROOT-Sim has been efficiently used relying on OpenMPI and MPICH. We rely on advanced asynchronous/multithreaded facilities, so an environment compliant with MPI 3.0 specification should be used.
* `--enable-modules`: ROOT-Sim offers advanced facilities when running on Linux. To this end, ad-hoc kernel modules must be compiled and installed. This flag tells `configure` to look for kernel headers and build the modules if they are available.
* `--enable-ecs`: by definition of PDES, a simulation model can describe interactions across different LPs only by means of message passing. Due to the distributed and optimistic nature of ROOT-Sim, this has the disadvantage that a simulation model is not allowed to pass pointers to the simulation state of any LP in a message, rather all the information should be packed (or linearized) in a buffer for transmission. If the model passes a pointer, the simulation will either crash or generate undefined results. Event Cross State (ECS) is an advanced feature of ROOT-Sim which allows to pass pointers in messages, ensuring correctness also in a distributed environment. To enable ECS, kernel modules are required.
* `--disable-rebinding`: to maximize the performance of the simulation, the runtime environment periodically checks whether the workload on all the available threads in a node is even. If it is not, LPs are moved (rebound) across the different worker threads. This flag disables this feature.
* `--enable-debug`: this flag instructs `configure` to generate debug symbols and perform extra checks on the source code. Simulations run when this flag has been used in compile mode can be as much as 20x slower.
*  `--enable-coverage`: this flag enables code coverage. This is used mostly in development, to see whether the test cases are covering the various possible execution paths in a suitable way.
*  `--enable-extra-checks`: this flag tells `configure` to generate additional checks on the runtime dynamics of the models. This option kills performance drastically, but it can be used to debug models when strange bugs appear, which could be related to the speculative nature of ROOT-Sim. As an example, consistency of messages is always checked at any access, or it is checked whether an event handler has altered some of the internal data structures related to events, which is forbidden in a speculative simulation.
* `--enable-profile`: this flag, which is used in development, relies on `gprof` to generate performance reports of the internal routines of ROOT-Sim.

These are the requirements to build ROOT-Sim. Note that if you exclude specific subsystems, you can ignore the dependency.

| Dependency | Minimum Version |
|:-----------|:---------------:|
| Linux      | 2.6.5           |
| gcc        | 6.0.0           |
| OpenMPI(*) | 3.0.1           |
| MPICH(*)   | 3.2.0           |

(*) Only one between OpenMPI and MPICH are required.

If you have cloned the repository, you will not find the `configure` script, as this has to be generated locally. To this end, we provide the `autogen.sh` script which automatically generates `configure` for you. This script depends on `automake`, `autoconf`, and `libtool`, so they must be installed in your machine.