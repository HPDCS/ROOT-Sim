# The ROme OpTimistic Simulator (ROOT-Sim) [![Build Status](https://travis-ci.org/alessandropellegrini/ROOT-Sim.svg?branch=master)](https://travis-ci.org/alessandropellegrini/ROOT-Sim)

*Brought to you by the High Performance and Dependable Computing Systems (HPDCS)
at Sapienza, University of Rome*

http://www.dis.uniroma1.it/~hpdcs/ROOT-Sim

----------------------------------------------------------------------------------------

The ROme OpTimistic Simulator is an x86-64 Open Source, parallel/distributed simulation platform developed using C/POSIX technology, which is based on a simulation kernel layer that ultimately relies on MPI for data exchange across different kernel instances. The platform transparently supports all the mechanisms associated with parallelization (e.g., mapping of simulation objects on different kernel instances) and optimistic synchronization (e.g., state recoverability).

The programming model supported by ROOT-Sim allows the simulation model developer 
to use a simple application-callback function named ProcessEvent() as the event handler,
whose parameters determine which simulation object is currently taking control for
processing its next event, and where the state of this object is located in memory. 
An object is a data structure, whose state can be scattered on dynamically allocated
memory chunks, hence the memory address passed to the callback locates a top level
data structure implementing the object state-layout.

ROOT-Sim's development started as a research project, and is currently run by the High Performance and Dependable Computing Systems group at Dipartimento di Ingegneria Informatica, Automatica e Gestionale, Sapienza, University of Rome.

For information on configuration, installation, and usage, please refer to our online
documentation web page: http://www.dis.uniroma1.it/~hpdcs/ROOT-Sim

