.\" The ROme OpTimistic Simulator (ROOT-Sim) Manual
.\" written by the High Performance and Dependable Computing Systems
.\" Sapienza, University of Rome
.\" http://www.dis.uniroma1.it/~hpdcs
.\"
.\" Nov 15 2018, Alessandro Pellegrini
.\" 	Revised manpages
.\" May 09 2011, Alessandro Pellegrini
.\" 	First version of the manpages

.TH ROOT-Sim.h 3 2018-11-15 "The ROme OpTimistic Simulator"

.SH NAME
ROOT-Sim.h - The main ROOT-Sim header file.

.SH SYNOPSIS
.B #include <ROOT-Sim.h>

.SH DESCRIPTION

ROOT-Sim.h defines all the data structures and the API for the ROOT-Sim simulation platform. In particular,
this header declares the following:

.IP * 2
.B \fIINIT\fP macro:
this macro is associated with a specia event which is scheduled by the simulation kernel to every
logical process in the simulation model upon initialization of the simulation. In the INIT event,
all the logical processes can instantiate their data structures, according to the runtime parameters
which are passed as a payload of this special purpose event in the form of a NULL-terminated array.

.IP
.B int \fIn_prc_tot\fP:
the total number of the Logical Processes hosted at runtime by ROOT-Sim's kernel is stored into this
global variable, which is accessible from the application-level software. If an application has to set
a range of global ids defining the processes involved in the simulation, it can rely on it.

.PP

The ROOT-Sim's API exposed by this header consists of the following functions:

.IP * 2
.B void SetState(void *\fInew_state\fP);

.IP * 2
.B void ScheduleNewEvent(int \fIwhere\fP, time_type \fItimestamp\fP, int \fIevent_type\fP, void *\fIcontent\fP, int \fIsize\fP);

.PP

To find more information on them, read their specific manpages.

In addition, in ROOT-Sim.h the following functions, belonging to the simulator's internal numerical library,
are exposed to the application level code:

.IP * 2
.B double Random(void):
returns a number in between [0,1], according to a Uniform Distribution;

.IP *
.B double Expent(double \fImean\fP):
returns a random number according to an Exponential Distribution of mean value \fImean\fP;

.IP *
.B double Normal(void):
returns a number according to a Normal Distribution with mean 0;

.IP *
.B double Gamma(int \fIia\fP):
returns a number according to a Gamma Distribution of Integer Order \fIia\fP,
i.e. a waiting time to the ia-th event in a Poisson process of unit mean.
.PP

Whenever a simulation model wants to rely on statistical distribution for its implementation, it is
important to use the ROOT-Sim's internal numerical library, as it ensures that the internal states
related to the random number generators are checkpointed together with the simulation objects' states.
In this way, whenever a rollback operation is performed, all the simulation objects are given the
same sequence of random numbers, thus conforming to the Piece-Wise-Deterministic paradigm.


.SH SEE ALSO
.BR ROOT-Sim (7),
.BR ProcessEvent (3),
.BR OnGVT (3),
.BR ScheduleNewEvent (3),
.BR SetState (3),
.BR rootsim (3)

.SH COPYRIGHT
ROOT-Sim is developed by the
.I High Performance and Dependable Computing Systems
(HPDCS) Group at
.I Sapienza, University of Rome,
all the copyrights belong to the Group, and the software is released under GPL-3 License.


For further details, see the page http://www.dis.uniroma1.it/~hpdcs/ROOT-Sim/
