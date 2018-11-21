# ROOT-Sim(7) -- The ROme OpTimistic Simulator

## NAME

The ROme OpTimistic Simulator

## DESCRIPTION

The **ROme OpTimistic Simulator**
(ROOT-Sim) is an Open Source, multithreaded parallel/distributed simulation runtime library developed using *C/POSIX technology*, which is based on a simulation kernel layer that ultimately relies on *MPI* for data exchange across different kernel instances. The platform transparently supports all the mechanisms associated with parallelization (e.g., mapping of simulation objects on different kernel instances) and optimistic synchronization (e.g., state recoverability issues). The runtime environment targets x86_64 systems, and is shipped with a Linux kernel module which enables additional programmability facilities.

The programming model supported by ROOT-Sim allows the simulation model developer  to use a simple application-callback function named  `ProcessEvent()` as the event handler, whose parameters determine which simulation object is currently taking control for processing its next event, and where the state of this object is located in memory. An object, also called a Logical Process, is a data structure, whose state can be scattered on dynamically allocated memory chunks, hence the memory address passed to the callback locates a top level data structure implementing the object state-layout.

ROOT-Sim is shipped with a numerical rollbackable library, which can be used to develop model relying on statistical distributions, fullfilling the requirements needed by a Piece-Wise-Deterministic programming model, i.e. whenever a rollback operation occurs, the random seed is transparently rolled back, so that the same exact sequence of random numbers will be provided to the simulation model.

Additionally, several libraries to support arbitrary topologies and Agent-Based Simulation are integrated in the runtime library. A description of these libraries is available in the  `ROOT-Sim.h` manpage.

## SEE ALSO

ABM(7), Topology(7), Numerical(7), ROOT-Sim.h (3)

## COPYRIGHT

ROOT-Sim is developed by the _High Performance and Dependable Computing Systems_ (HPDCS) Group at
_Sapienza, University of Rome_, all the copyrights belong to the Group, and the software is released
under GPL-3 License.

For further details, see the page http://www.dis.uniroma1.it/~hpdcs/ROOT-Sim/