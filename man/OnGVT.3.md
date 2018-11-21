OnGVT(3) -- Performs Performs a local termination check.
========================================================


## SYNOPSIS

 #include <ROOT-Sim.h>

 bool OnGVT(unsigned int _me_, void *_snapshot_);

## DESCRIPTION

**OnGVT** is an application-level callback. Any software implementing a simulation model which is
to be run on the ROOT-Sim simulation platform must implement this function.
The runtime environment uses this function to implment a distributed termination detection.
When the Global Virtual Time (**GVT**) is reduced, the runtime (periodically) asks all the Logical Processes
(**LP**) whether the simulation (at that particular process) has to be considered as completed
or not.
In case that all processes reply with a **true** value, the simulation is considered completed, and
thus halted, by the runtime.

The arguments passed to this callback are:

* _me_: the global id associated to the LP which is being scheduled for termination detection.
* _snapshot_: the most recent committed simulation state, i.e. the simulation state which is associated
  with the newly-computed GVT value. The LP is allowed to inspect the content of this buffer to decide upon
  the termination detection.

When running an optimistic simulation, the state to be inspected here is one which can be associated with
a timestamp significantly smaller than the current one reached on the speculative boundary.
It is therefore meaningless (and unsafe) to alter the content of this state.
Similarly, the model cannot send any new event during the execution of OnGVT.

The distributed termination detection can be executed in a **normal** or **incremental** fashion.
Depending on the **cktrm_mode** runtime parameter, the platform can be intructed to ask all the LPs
if they want to halt the simulation every time the GVT is computed, or if an LP should be exclueded
from the check once it has responded positively.

This difference can be useful to enhance the simulation performance when dealing with models which can
have an oscillating termination condition (i.e., there is a certain phase of simulation where a simulation
object wants to terminate, and a subsequent phase where it no longer wants to) or a monotone termination
condition (i.e., when a process decides to terminate, it will never change its mind), respectively.

Nevertheless, the same implementation for the termination check can be used in both ways, so that
the OnGVT function can be left untouched.

OnGVT is given a consistent simulation snapshot on a periodic frequency. Therefore, if the simulation
model wants to dump on file some statitics, in this function this task can be correctly implemented.

## RETURN VALUE

This function should return **true** if the Logical Process associated with its execution wants
to terminate the simulation, **false** otherwise.

## ERRORS

This function returns no errors.

## EXAMPLES

This minimalistic example,shows how OnGVT can be used to check some predicate on the consistent
state passed to it, in order to determine whether the simulation should be halted or not.

```C
 bool OnGVT(unsigned int me, void ∗snapshot) { 
	if (snapshot−>events_executed < TOTAL_NUMBER_OF_EVENTS)
		return false;
	return true;
 }
```

## SEE ALSO

ROOT-Sim(7), ProcessEvent(3), ScheduleNewEvent(3), SetState(3), ROOT-Sim.h(3)

## COPYRIGHT
ROOT-Sim is developed by the _High Performance and Dependable Computing Systems_ (HPDCS) Group at
_Sapienza, University of Rome_, all the copyrights belong to the Group, and the software is released
under GPL-3 License.

For further details, see the page http://www.dis.uniroma1.it/~hpdcs/ROOT-Sim/
