.\" The ROme OpTimistic Simulator (ROOT-Sim) Manual
.\" written by the High Performance and Dependable Computing Systems
.\" Sapienza, University of Rome
.\" http://www.dis.uniroma1.it/~hpdcs
.\"
.\" Nov 15 2018, Alessandro Pellegrini
.\" 	Revised manpages
.\" May 09 2011, Alessandro Pellegrini
.\" 	First version of the manpages

.TH SetState 3 2018-11-15 "The ROme OpTimistic Simulator"

.SH NAME
SetState - Tells the ROOT-Sim to use a certain buffer as the simulation state of a simulation object.

.SH SYNOPSIS
.B #include <ROOT-Sim.h>


.B void SetState(void *\fInew_state\fP);


.SH DESCRIPTION

SetState is part of ROOT-Sim's API. By definition of the programming model, the simulation
state is located into malloc'd memory, and the platform silently and transparently restores
it to previous one, whenever a rollback operation is performed, due to an inconsistency
in the simulation caused by an out-of-order execution of events.

This means that the simulation kernel must be aware of the location of the simulation state.
This buffer can, at any time, be changed by the model. The programmer must issue a call to
SetState, in order to inform the runtime environment of the user's will to use a certain
memory buffer as the main simulation state for the Logical Process which is currently running.
This allows the runtime to correctly track changes in the objects' states, in order to
correctly perform rollback operations, if needed.

.SH ERRORS

This function returns no errors.

.SH EXAMPLES

See \fIProcessEvent\fP manpage for an example involving SetState.

.SH SEE ALSO
.BR ROOT-Sim (7),
.BR ProcessEvent (3),
.BR OnGVT (3),
.BR ScheduleNewEvent (3),
.BR rootsim (3),
.BR ROOT-Sim.h (3)

.SH COPYRIGHT
ROOT-Sim is developed by the
.I High Performance and Dependable Computing Systems
(HPDCS) Group at
.I Sapienza, University of Rome,
all the copyrights belong to the Group, and the software is released under GPL-3 License.


For further details, see the page http://www.dis.uniroma1.it/~hpdcs/ROOT-Sim/
