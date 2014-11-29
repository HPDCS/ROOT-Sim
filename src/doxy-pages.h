/*! \mainpage The Project
 *
 * \section intro_sec Introduction
 *
 * The ROme OpTimistic Simulator is an x86-64 Open Source, parallel/distributed simulation platform developed using C/POSIX technology, which is based on a simulation kernel layer that ultimately relies on MPI for data exchange across different kernel instances. The platform transparently supports all the mechanisms associated with parallelization (e.g., mapping of simulation objects on different kernel instances) and optimistic synchronization (e.g., state recoverability). 
 *
 * The programming model supported by ROOT-Sim allows the simulation-model developer to use a simple application-callback function named <code>ProcessEvent()</code> as the event handler, whose parameters determine which simulation object is currently taking control for processing its next event, and where the state of this object is located in memory. In ROOT-Sim, a simulation object is a data structure, whose state can be scattered on dynamically allocated memory chunks, hence the memory address passed to the callback locates a top level data structure implementing the object state-layout. 
 *
 * This web page is intended as a means to disseminate information about ROOT-Sim internals, the offered functionalities and the supported programming model, and to provide access to a repository where source code and manual/guide documentation can be downloaded.
 *
 * ROOT-Sim's development started as a research project, and is currently run by the <a href="http://www.dis.uniroma1.it/~hpdcs" target="_blank">High Performance and Dependable Computing Systems</a> group at Dipartimento di Ingegneria Informatica, Automatica e Gestionale, Sapienza, University of Rome.
 *
 * <p>&nbsp;</p>
 * \section doc-org Organization of this web page
 *
 * In this page you can find information on how to download and install the software, on how to develop a model to be simulated on top of ROOT-Sim, and details related to ROOT-Sim's internals and subsystems.
 *
 * Please visit the following pages to get the information accordingly:
 *
 * \arg \subpage install
 * \arg \subpage users
 * \arg \subpage numerical 
 * \arg \subpage tpl
 * \arg \subpage arch-internals
 * \arg \subpage developers
 * \arg \subpage publications
 *
 * <p>&nbsp;</p>
 * \section contacts Bugs & Contacts
 *
 * If you appear to have found a bug when using ROOT-Sim, if you want to submit any patch or suggestion, if you want to collaborate with us, or if you just want to drop us a line, please use the <a href="http://www.dis.uniroma1.it/~hpdcs/index.php?option=com_contact&view=contact&id=1&Itemid=9" target="_blank">form on our contact page</a>.
 *
 * <p>&nbsp;</p>
 * \section changelog Change Log
 *
 * <strong>19/12/2011</strong>: ROOT-Sim 0.9.0-RC1
 * 
 * <ul> <li>First Release Version</li>
 *      <li> Supported subsystems are:
 *      <ul> <li>Dynamic Memory Logger Subsystem</li>
 *           <li>Incremental Dynamic Memory Logger Subsystem</li>
 *           <li>Autonomic State Management Subsystem</li>
 *           <li>Smallest Timestamp First O(n) CPU Scheduler</li>
 *           <li>Smallest Timestamp First O(1) CPU Scheduler</li>
 *           <li>Global Consistent Snapshot Subsystem</li>
 *      </li></ul>
 *
 *
 * <p>&nbsp;</p>
 * \section copyright License & Copyright
 *
 * ROOT-Sim is developed by the High Performance and Dependable Computing Systems (HPDCS) Group at Sapienza,  University  of  Rome, all the copyrights belong to the Group.
 * 
 * ROOT-Sim is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 3 of the License, or (at your option) any later version.
 *
 * ROOT-Sim is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 */









/*! \page install Download and Installation Guide
 *
 * ROOT-Sim is completely Open Source, therefore no binaries are being distributed.
 *
 * There are two main ways to get your hands on a copy of ROOT-Sim. We provide a direct link to a tarball containing the latest stable version, which you can later expand, compile and install.
 * Otherwise, you can get (read-only) access to our svn repository, to check out the latest trunk version.
 *
 * <p>&nbsp;</p>
 * \section down Get Source Code
 *
 * \subsection browse Browse Online Source Code Repository
 *
 * If you want to browse the full ROOT-Sim source code, you can point your browser to <a href="http://svn.dis.uniroma1.it/svn/hpdcs/root_sim/" target="_blank">our online repository</a>.
 *
 * <p>&nbsp;</p>
 * \subsection stable Last stable version
 *
 * The latest stable ROOT-Sim version is <a href="http://www.dis.uniroma1.it/~hpdcs/ROOT-Sim/stable/rootsim-0.9.0-rc1.tar.bz2" onClick="javascript: _gaq.push(['_trackPageview', '/downloads/rootsim-0.9.0']);">0.9.0-RC1</a>.
 *
 * <p>&nbsp;</p>
 * \subsection svn Last trunk version through SVN
 *
 * Please note that ROOT-Sim is an ongoing research project, therefore if you rely on the trunk version, you might find the software in an evolutionary state, which might produce an unstable behaviour.
 *
 * <code>svn checkout http://svn.dis.uniroma1.it/root_sim/trunk/</code>
 *
 * When you are asked for credential, use the following:
 * <code>username:</code> guest
 * <code>password:</code> read
 *
 * This will give you a read-only access to the trunk repository.
 *
 * <p>&nbsp;</p>
 * \section inst Installation Guide
 *
 * <p>&nbsp;</p>
 * \subsection conf Configuration
 *
 * ROOT-Sim configuration is performed at installation time. The only dependencies are:
 * \arg <code>OpenMPI</code> (>= 1.2.0)
 * \arg <code>flex</code>
 * \arg <code>bison</code>
 *
 * No check will be performed on their installation, so please make sure you have all of them installed in your system before installing ROOT-Sim.
 *
 * Note that supports for the Autonomic/Incremental subsystem rely on machine-dependant code snippets which have been developed only for x86-64 architectures.
 *
 * <p>&nbsp;</p>
 * \subsection inst-command Installation
 *
 * To install ROOT-Sim, simply run <code>install.sh</code> in the untared ROOT-Sim folder, and follow on-screen instructions.
 *
 * <p>&nbsp;</p>
 * \section usage Usage Guide
 *
 * After installation has completed, rootsim can be run through the interactive shell:
 *
 * <code>$> rootsim</code>
 *
 * Or using the command line.
 *
 * Simulation models' source code must be placed in a subfolder of the installation path, which is <code>./app_layer/src/model_name/</code>
 * All the source files must appear in one sole directory, no subdirectories are allowed.
 * 
 * Two demo projects are installed:
 * \arg <strong>PCS</strong>: a mobile system workload simulator, develope according to <a href="http://www.stanford.edu/~boyd/papers/pdf/outage.pdf" target="_blank">this paper</a>;
 * \arg <strong>Phold</strong>: a synthetic benchmark for the platform.
 *
 * For further and deeper information on usage, please see \ref users
 *
 */















/*! \page users User Guide
 *
 * This page covers two main aspects related to ROOT-Sim's utilization:
 *
 * \arg \ref use-platform gives information on how to actually run a simulation model on top of ROOT-Sim
 * \arg \ref write-code gives information on how to develop a new simulation model for ROOT-Sim
 *
 * <p>&nbsp;</p>
 * \section use-platform How to use ROOT-Sim
 *
 * ROOT-Sim can be run in three different ways: thorught its internal interactive shell, using configuration scripts, and via the command line. These three modes are discussed in details here.
 *
 * <p>&nbsp;</p>
 * \subsection interactive-shell ROOT-Sim Interactive Shell
 *
 * The interactive shell is the simplest way to instruct ROOT-Sim on how to run a simulation model. When entering interactive shell, you can type a sequence of commands in order to set the proper configuration needed to run your simulation model. In addition, you can ask ROOT-Sim to compile a specified Simulation Model in a fashion targeted at executing it on top of the simulator.
 *
 * The shell can be run by simply typying:
 *
 * <code>$> rootsim</code>
 *
 * The shell accepts several commands:
 *
 * <ul>
 * <li><code>list [what]</code>: it can be used to display several pieces of information. In particular, depending on <code>[what]</code>'s value:
 *     <ul>
 *     <li><code>source</code>: lists the available sources to be compiled.</li>
 *     <li><code>app</code>: lists the available binaries to be run.</li>
 *     <li><code>config</code>: lists the current configuration ROOT-Sim will be run with.</li>
 *     </ul>
 * </li>
 * <li><code>compile [source]</code>: starts the compilation of <code>[source]</code>. It must be a valid source, visible from <code>list source</code></li>
 * <li><code>set [var] [value]</code>: sets the configuration variable <code>[var]</code> to <code>[value]</code> </li>
 * <li><code>unset [var]</code>: deletes the configuration variable <code>[var]</code>.</li>
 * <li><code>run [app]</code>: starts the simulation of <code>[app]</code>. It must be a valid application, visible from <code>list app</code></li>
 * <li><code>exit/quit</code>: leaves the interactive shell</li>
 * <li><code>help</code>: shows contextual help on the supported commands</li>
 * </ul>
 *
 * Through these commands, you can configure ROOT-Sim to simulate model using different subsystems, or having different models' intial conditions, and so on. Provided that you program your model being able to accept runtime parameters, you are given the widest degree of freedom on execution.
 *
 *
 * <p>&nbsp;</p>
 * \subsubsection runtime-params Basic Runtime Parameters
 *
 * ROOT-Sim has the need to know which subsystem the user wants to activate when simulatin a model, and how to configure some internal parameters.
 *
 * In particular, when you enter the interactive shell, you will find some presets for fundamental configuration variables. Their meaning and their defaults are explained below:
 *
 * \arg <code>np</code>: The number of Simulation Kernels to be run, that is the total number of instances of the simulator running concurrently on the same simulation model. The legal range is from 1 to max number of available cores, the default is 2.
 * \arg <code>gvt</code>: The time period to wait before a new GVT calculation is performed (in msec). The lega range is [500, 5000], the default is 1000.
 * \arg <code>gvt_snapshot_cycles</code>: Number of consecutive GVT calculations before rebuilding a state for temination check. There is no limit on this value, provided that it is non-negative. The default is 2. Consider that, the higher is this number, the longer the user might wait before the simulator notices that the simulation can be stopped.
 * \arg <code>scheduler</code>: Scheduling algorithm. Valid values are: {<code>stf</code>, <code>star</code>}. The default is <code>stf</code>. For further information on supported scheduling algorithms, see \ref scheduler
 * \arg <code>ckpt</code>: State Saving mode. Determines the policy according to which ROOT-Sim takes snapshots of the simulation states. Legal values are: {<code>copy</code>, <code>periodic</code>}, the default is <code>periodic</code>. For further information on the checkpointing subsystem, see \ref state-manager
 * \arg <code>period</code>: Checkpointing interval. If <code>periodi</code> is selected as State Saving mode, this tells how many events should pass before a snapshot is taken. Legal values are in the range [1, 40], the default is 10.
------> * \arg <code>snapshot</code>: Type of snapshot. Tells how ROOT-Sim will take a snapshot. Legal values are {full, incremental, autonomic}, the default is full. For further information see \ref 
 * \arg <code>ckpt_mode</code>: Checkpointing mode. Legal values are {synch, semi_synch}, the default is synch
------> * \arg <code>cktrm_mode</code>: CCGS manager. Tells how termination predicate check should be performed. Legal values are {incremental, standard}, the default is standard. For further information see \ref 
 * \arg <code>nprc</code>: Number of logical processes simulated. Legal values are in the range [1, 8192], the default is 1024.
 * \arg <code>debug</code>: Debugging flag. If it's set to 0, a normal execution is run. If it's set to 1, after having initialized the platfom, ROOT-Sim enters an infinite loop, waiting for the user to attach to it using <code>gdb</code>. The default value is 0.
 *
 * Note that ROOT-Sim's policy is that if a configuration variable is found, which does not belong to its internal configuration, it is passed to the application level software as a payload to the <code>INIT</code> event, 
 *
 * <p>&nbsp;</p>
 * \subsection config-files ROOT-Sim Configuration Files
 *
 * ROOT-Sim can be instructed about the simulation(s) to be performed using configuration files.
 *
 * Configuration files are simply ASCII files where commands accepted by the interactive shell are placed one per line. Therefore ROOT-Sim can be launched as follows:
 *
 * <code>$> rootsim config-file</code>
 *
 * In this way, several simulations can be launched at once, and parameters can be easily tuned within the same file. An example of such a file might be the following:
 *
 * <code>compile model</code><br/>
 * <code>set nprc 2048</code><br/>
 * <code>set scheduler star</code><br/>
 * <code>set completed-events 10000000</code><br/>
 * <code>run model</code><br/>
 * <code>set nprc 4096</code><br/>
 * <code>run model</code>
 *
 * Notice how this script calls the compilation of a custom simulation model called <code>model</code>, sets some configuration variables (one of which, <code>completed-events</code> is not a ROOT-Sim one and therefore is set to control the execution of <code>model</code>) and then starts the execution twice, with a different number of logical processes. 
 *
 * <p>&nbsp;</p>
 * \section command-line ROOT-Sim through the Command Line
 *
 * ROOT-Sim can be run through the command line as well. The first important thing to do, before running ROOT-Sim, is compiling the application-level software. This can be easily done through a custo <code>Makefile</code> which is included with the installation. In particular, compiling a simulation model can be done using the following commands:
 *
 * <code>$> cd $ROOTSIM_PATH/app_layer/</code><br/>
 * <code>make APP=application</code>
 *
 * $ROOTSIM_PATH is an environment variable created at install time which allows to point directly to the ROOT-Sim installation directory. In the <code>./app_layer/</code> subfolder there is a <code>Makefile</code> which accepts as <code>APP</code> the subfolder of <code>./app_layer/src/</code> containing all the sources of the simulation model which is to be compiled. Using this <code>Makefile</code>, the compilation of the executable is trivial. Notice that compiling a software for ROOT-Sim is a complex task, which involves calling some ad-hoc routines to trasform the actual application object file. Compiling an application without using this <code>Makefile</code> might be long and difficult, and is strongly discouraged.
 * 
 * ROOT-Sim ultimately relies on <code>openMPI</code> for message passing. Therefore, to launch an application one should use <code>mpirun</code> with the following syntax:
 *
 * <code>mpirun -v -np 2 $ROOTSIM_PATH/app_layer/bin/APPLICATION/APPLICATION -gvt 1000 -gvt_snapshot_cycles 2 -scheduler stf -ckpt periodic -period 10 -snapshot full -ckpt_mode synch -cktrm_mode standard -nprc 4 -debug 0</code>
 * 
 * As you can see, this is a standard <code>mpirun</code> command. The important parts to be noticed are that the application <code>mpirun</code> will handle is placed automatically in the <code>$ROOTSIM_PATH/app_layer/bin/APPLICATION/</code> folder, and is itself called <code>APPLICATION</code>, where of course <code>APPLICATION</code> is the name of the model itself. 
 * Additional parameters are the ones required by ROOT-Sim and discussed in section \ref runtime-params. If any additional parameter is passed, it will be hopped by ROOT-Sim to the application-level software as a payload of the <code>INIT</code> message, therefore allowing to customize the model's behaviour at runtime.
 *
 * 
 *
 *
 * <p>&nbsp;</p>
 * \section write-code Tutorial: How to write a ROOT-Sim simulation model
 *
 * ROOT-Sim exposes a reduced-size set of APIs, which can be easily used to build complex simulation models. If you want to have details on how an application suitable for ROOT-Sim can be written, or if you want to have details on the API, you can always check the ROOT-Sim manpage (by typing <code>man ROOT-Sim</code>) after having installed the simulator.
 *
 * Here we present a quick overview on ROOT-Sim API and on how a simple simulation model can be implemented in <code>C</code>.
 *
 *
 * <p>&nbsp;</p>
 * \subsection ProcessEvent void ProcessEvent(int me, time_type now, int event_type, void *content, int size, void *state);
 *
 * ProcessEvent is the main application-level callback. A simulation model must implement this function in order to provide control to the application for the actual processing of simulation events.
 *
 * The arguments passed to this callback are:
 *
 * \arg <code>me</code>: the global id associated with the Simulation Object which is being scheduled for event execution.
 * \arg <code>now</code>: the current value of the logical time for event occurrency on that Simulation Object.
 * \arg <code>event_type</code>: the numerical code determining the type of the event to be processed.
 * \arg <code>content</code>: the buffer where the event payload will be delivered by the ROOT-Sim kernel (may be <code>NULL</code> in case the event has no payload). NOTE: that buffer should be accessed in read-only mode by the application-level software. Write mode access to that buffer content might yield to undetermined runtime behaviour. Please make your own copy of that buffer's content if you wish to operate in write-mode on it.
 * \arg <code>size</code>: the size (in bytes) of the event payload, upper bounded by <code>MAX_EVENT_SIZE</code> environment macro.
 * \arg <code>state</code>: the pointer to the top data structure forming the simulation state layout.
 *
 * ProcessEvent is the sole entry point at application level which is used to schedule the actual events to  be  simulated.  Therefore,  it  should  be  implemented  using  a switch-case construct to demultiplex the various events belonging to the simulation model across the various Simulation Objects.
 *
 * Upon initialization, ROOT-Sim schedules the special <code>INIT</code> event (with numerical code 0) once on each Simulation Object in the interval <code>[O, nprc - 1]</code>. This  means both  that  the  code  should handle this event in the switch-case construct, and that and event with id 0 cannot be used by the application-level code. INIT is defined into ROOT-Sim.h.  The purpose of this event is to allow  logical  processes  to  perform initialization  operations (such as allocating space for their states).  INIT's payload content is not user defined. Instead, it consists of a NULL-terminated array of strings containing all the parameters passed at command  line  (or  via  the  interactive shell)  which  do  not belong to ROOT-Sim (see rootsim manpage for further details on this). At the same time, size contains the number of elements in this array.  In this way, the application-level code can initialiaze the  actual  instance  of  simulation accordingly to the needs.
 *
 * Other  events bring a payload which could be NULL or any buffer, and size states what is the size of the current payload, allowing the programmer to use different payloads associated with different events, if needed.
 *
 * A simulation object is not dispatched again, unless a real application-level event is scheduled for it during the simulation run.
 *
 *
 *
 *
 * <p>&nbsp;</p>
 * \subsection ScheduleNewEvent void ScheduleNewEvent(int where, time_type timestamp, int event_type, void *content, int size);
 *
 * ScheduleNewEvent is part of the ROOT-Sim API. It allows the simulation model to generate a new event and inject it into the system, destined at any simulation object. By using this function, the application-level programmer can make the simulation advance at the same logical process, or create interaction between different objects in the system.
 *
 * The arguments passed to this funtion are:
 *
 * \arg <code>where</code>: the global id of the logical processes where the simulation event must be delivered to. This should be in the interval <code>[0, nprc - 1]</code>.
 * \arg <code>timestamp</code>: the logical time when the recipient of the event must execute it at.
 * \arg <code>event_type</code>: the numerical code for the event to be injected into the system.
 * \arg <code>content</code>: the pointer to the buffer mantaining the application-defined event payload.
 * \arg <code>size</code>: the size (in bytes) of the event payload, upper bounded by <code>MAX_EVENT_SIZE</code> environment macro.
 *
 * The application-level code can is aware of the total number of simulation objects belonging to the current simulation run by using the variable <code>n_prc_tot</code> defined into ROOT-Sim.h. The value of this variable is set at simulation startup time (see the ROOT-Sim.h manpage for further details).
 *
 *
 *
 *
 *
 * <p>&nbsp;</p>
 * \subsection OnGVT bool OnGVT(int me, void *snapshot);
 *
 * OnGVT  is an application-level callback. Any software implementing a simulation model which is to be run relying on the ROOT-Sim simulation platform must implement this function.  By using this callback, the simulation kernels implements a distributed termination  check  procedure.  When the Global Virtual Time (GVT) operation is performed, they ask all the Logical Processes (LPs) whether the simulation (at that particular process) has come to a complete state or not.  In the case that all  processes  reply with a true value, the simulations complets.
 *
 * The arguments passed to this callback are:
 *
 * \arg <code>me</code>: the global id associated to the LP which is being scheduled for termination check.
 * \arg <code>snapshot</code>:  the last consistent simulation state, which can be used by the LP to decide whether the simulation can terminate or not.
 *
 * The distributed termination check can be executed in a standard or incremental fashion.  Depending  on  the  cktrm_mode  runtime parameter,  the platform can be intructed to ask all the LPs if they want to halt the simulation every time the GVT is computed, or if an LP should be exclueded from the check once it has replied in a positive way.
 *
 * This difference can be useful to enhance the simulation performance when dealing with models which can have an oscillating  termination  condition (i.e., there is a certain phase of simulation where a simulation object wants to terminate, and a subsequent phase where it no longer wants to) or a monotone termination condition (i.e., when a process decides to terminate, it will never change its mind), respectively.
 *
 * Nevertheless, the same implementation for the termination check can be used in both ways, so that the OnGVT function can be left untouched.
 *
 * OnGVT is given a consistent simulation snapshot on a periodic frequency. Therefore, if the simulation model  wants  to  dump  on file some statitics, in this function this task can be correctly implemented.
 *
 *
 *
 * 
 *
 * <p>&nbsp;</p>
 * \subsection SetState void SetState(void *new_state);
 *
 * SetState  is  part of ROOT-Sim's API. By definition of the programming model, the simulation state is located into malloc'd memory, and the platform silently and transparently restores it to previous one, whenever a rollback operation is performed, due to an inconsistency in the simulation caused by an out-of-order execution of events.
 *
 * This means that the simulation kernel must be aware of the location of the simulation state. To allow perfect transparency, the first call to <code>malloc</code> is considered by the simulation kernel to be the main container of the simulation state. Nevertheless, a programmer  might want to perform some other malloc operations in the INIT event before actually allocating the state or, during the simulation, might want to allocate a completely new state. If any of these situations are encountered, a  call to <code>SetState</code> must be performed, in order to inform the simulation kernel that user's requirements are different from the standard one specified in the programming model and to make it able to correctly track changes in the objects' states, needed to correctly perform rollback operations, if needed.
 *
 *
 * \subsection example A Minimal Example
 *
 * In this minimal example, one single event is specified, and a state structure containing an event counter is defined. Each Logical Process in the simulation model schedules an event to a random process according to a Uniform distribution, and the times associated with the events are determined according to the same distribution.
 *
\code
#include <ROOT-Sim.h>

#define EVENT 1
#define TOTAL_NUMBER_OF_EVENTS 1000000

typedef struct _state_type {
	int events_executed;
} state_type;

void ProcessEvent(int me, time_type now, int event_type, void *content, int size, void *state) {

	time_type timestamp = (time_type)(now + 10 * Random());
	int receiver = (int)(n_prc_tot * Random());

	switch(event_type) {
		case INIT:
			state = (state_type *))malloc(sizeof(state_type));
			SetState(state);
			state->events_executed = 0;
			ScheduleNewEvent(me, timestamp, EVENT, NULL, 0);
		break;	

		case EVENT:
			ScheduleNewEvent(me, timestamp, EVENT, NULL, 0);
		break;
	}
}

bool OnGVT(int me, void *snapshot) {
	if (snapshot->events_executed < TOTAL_NUMBER_OF_EVENTS)
		return false;
	return true;
}

\endcode
 *
 * This block of code can be used as a skeleton to develop every simulation model. Remember that, when the <code>INIT</code> message is received, <code>content</code> can be cast to <code>(char **)</code>, becoming an array of <code>size</code> strings which are all the parameters passed at runtime to the application-level software.
 */










/*! \page arch-internals ROOT-Sim Internals
 *
 * ROOT-Sim is oranized as a multi-layered architecture. It is composed by three layers:
 *
 * \arg <strong>Application Layer</strong>: It is the implementation of the simulation model. It only encloses the simulation logic, and exploits an interfaces with the Kernel Layer through a simple API (\ref write-code). The Application Layer can inject new events to be processed into the system, and it is triggered for their execution. In addition, it periodically receives a consistent and commited snapshot from the Kernel Layer on which control operations (like termination control or statistics printing) can be performed;
 * \arg <strong>Kernel Layer</strong>: It is the simulation platform's core. It implements all the scheduling, memory management, communication, and housekeeping operations, providing a synchronization between all the Kernel instances. All these operations are completely trasparent towards the <em>Application Layer</em>;
 * \arg <strong>Communication Layer</strong>: This layer manages the inter-kernel communication. It relies on MPI standard.
 *
 * The current architecture is sketched in the following picture:
 * \image html rs-arch-last.png
 *
 * A programmer who wants to exploit ROOT-Sim to achieve a distributed/parallel simulation only needs to implement the Simulation Object (Application Layer), relying on the platform's API, which is documented here: \ref write-code.
 *
 * ROOT-Sim relies on a Master/Slave approach, the Master starts and coordinates all the distributed algorithms. 
 * The ROOT-Sim kernel presents a very modular architecture that allows a simple managment of any subsystem, providing a good extensibility and a good maintenance.
 * A brief description of all the subsystems follows:
 *
 * 
 * \section event-manager Event Manager
 * 
 * This module interfaces with the Application Layer and triggers the execution of the next upcpoming event. When the execution is complete, it gives control to the <em>Memory Mangement</em> subsystem in order to take a new snapshot of the simulation state in case the currently running policy is met. In addition, it checks if the event to be processed violates causality consistency (i.e., its timestamp preceedes the current Simulation Object's Logical Virtual Time), triggering a rollback operation.
 *
 *
 * \section scheduler Scheduler
 *
 * This module implements the system scheduler, which decides the next event to be executed. Two different scheduling algorithms are currently available, one that executes in linear time on the LPs number (suggested for simulations with a small number of LPs), and the other that executes in costant time (suggested for simulations with a big number of LPs). Both algorithms implement the Smallest Timestamp First (STF) policy, that according to several researches is less rollback-prone.
 *
 *
 * 
 * \section memory-manager Memory Manager
 *
 * This module manages Simulation Objects' memory, by intercepting allocation/deallocation routines, and providing LPs state managment and Log and Restore mechanisms.
 * 
 *
 * \section gvt Global Virtual Time (GVT) Manager
 *
 * This module implements the computation of the GVT that represents the committed horizon. Along with this computation, it performs some housekeeping operations to clean all the information preceeding the GVT that are no longer of interest for the platform.
 *
 *
 * \section queue-manager Queue Manager
 *
 * This module manages the messages queues. The system adopts the three following messages queues:
 *
 * \arg <em>Incoming Messages queue</em>: This queue stores the messages (events) that have been delivered by the Simulation Kernel and are still to be processed. The messages remain in the queue until they are committed, which means that a their re-execution can not be requested because of a coasting forward operation;
 *
 * \arg <em>Outgoing Messages queue</em>: This queue stores the messages that have been sent. They remain in the queue until a delivery-ack is received, in order to maintain system's consistency throughout all the kernels;
 *
 *
 * \section ccgs Committed and Consistent Global State Manager
 *
 * This module determines which are the per-LP committed and, if requested, also consistent states within the system through a distributed algorithm. Committed snapshots are then passed to the Application Layer via the <code>OnGVT()</code> function, so that the simulation logic is allowed to periodically perform checks on them.
 *
 *
 *
 */


/*! \page developers List of ROOT-Sim developers
 *
 * In this page we present a list of the current and former developers of ROOT-Sim
 *
 * \section curr Current Developers:
 *
 * \arg Francesco Quaglia (quaglia@dis.uniroma1.it)
 * \arg Alessandro Pellegrini (pellegrini@dis.uniroma1.it)
 * \arg Roberto Vitali (vitali@dis.uniroma1.it)
 * \arg Sebastiano Peluso (peluso@dis.uniroma1.it)
 * \arg Diego Didona (d.didona@gmail.com)
 *
 * \section former Former Developers:
 *
 * \arg Giovanni Castellari
 * \arg Diego Cucuzzo
 * \arg Stefano D'Alessio
 * \arg Valerio Gheri
 * \arg Tiziano Santoro
 * \arg Roberto Toccaceli
 *
 */




/*! \page numerical Numerical Library
 *
 * ROOT-Sim offers a fully-featured numerical library designed according to the Piece-Wise Determinism Paradigm. The main idea behind this library is that if a Logical Process incurs into a Rollback, the seed which is associated with the random number generator associated with that logical process must be rolled back as well. The numerical library provided by ROOT-Sim transparently does so, while if you rely on a different numerical library, you must implement this feature by hand, if you want that a logical process is always given the same sequence of random numbers, even when the execution is restarted from a previous timestamp.
 *
 * \section Random double Random(void);
 * Returns a number in between [0,1], according to a Uniform Distribution
 *
 * \section Expent double Expent(double mean);
 * Returns a random number according to an Exponential Distribution of mean value mean
 *
 * \section Normal double Normal(void);
 * Returns a number according to a Normal Distribution with 0-mean
 *
 * \section Gamma double Gamma(int ia);
 * Returns a number according to a Gamma Distribution of Integer Order ia, i.e. a waiting time to the ia-th event in a Poisson process of unit mean.
 */




/*! \page tpl Third-Party Libraries
 *
 * In ROOT-Sim there is the possibility to use any kind of third party libraries in order to support the event execution and to allow the user to implement the simulation logic in an easier way.
 *
 * Nevertheless, according to the internal ROOT-Sim architecture, there are some limitations. In particular, they are related to the log/restore mechanism embodied in optimistic simulation platforms. If a call to a funcion library modifies some buffers in a way which ROOT-Sim cannot be aware of, then consistency in the result cannot be ensured. In particular:
 *
 * When running with the <strong>Full Log</strong> facilities turned on, any <emph>stateless</emph> library can be used. Stateful libraries are not yet supported, because so far ROOT-Sim does not have any facility aimed at supporting rollback within libraries' internal buffers.
 *
 * When running with the <strong>Autonomic</strong> or <strong>Incremental Log</strong> facilities turned on, ROOT-Sim has the need to intercept memory writes in order to perform a consistent incremental log. Up to now, although a more sophisticated interceptor is under development, only <emph>standard-C library's stateless</emph> libraries are supported, which namely are:
 *
 * \arg <pre>char *strcpy(char *, const char *);</pre>
 * \arg <pre>char *strncpy(char *, const char *, size_t);</pre>
 * \arg <pre>char *strcat(char *, const char *);</pre>
 * \arg <pre>char *strncat(char *, const char *, size_t);</pre>
 * \arg <pre>void *memcpy(void *, const void *, size_t);</pre>
 * \arg <pre>void *memmove(void *, const void *, size_t);</pre>
 * \arg <pre>void *memset (void *, int, size_t);</pre>
 * \arg <pre>char *strdup(char *);</pre>
 * \arg <pre>char *strndup(char *s, size_t n);</pre>
 * 
 */






/*! \page publications List of publications related to ROOT-Sim
 *
 * \section p-2011 2011
 * 
 * <em>Alessandro Pellegrini, Roberto Vitali and Francesco Quaglia</em><br/>
 * <strong>The ROme OpTimistic Simulator: Core Internals and Programming Model</strong><br/>
 * In The 4-th International ICST Conference of Simualtion Tools and Techniques (SIMUTOOLS) - Barcelona (Spain) - Demo Paper
 * 
 * <em>Roberto Vitali, Alessandro Pellegrini and Francesco Quaglia</em><br/>
 * <strong>An Evolutionary Algorithm to Optimize Log/Restore Operations within Optimistic Simulation Platforms</strong><br/>
 * In The 4-th International ICST Conference of Simualtion Tools and Techniques (SIMUTOOLS) - Barcelona (Spain)
 * 
 * <em>Sebastiano Peluso, Diego Didona and Francesco Quaglia</em><br/>
 * <strong>Application Transparent Migration of Simulation Objects with Generic Memory Layout</strong><br/>
 * Principles of Advanced and Sistributed Simulation (PADS), 2011 IEEE Workshop on, 14-17 June 2011
 * 
 * \section p-2010 2010
 * 
 * <em>Roberto Vitali, Alessandro Pellegrini and Francesco Quaglia</em><br/>
 * <strong>Autonomic Log/Restore for Advanced Optimistic Simulation Systems</strong><br/>
 * In The 18-th Annual Meeting IEEE International Symposium on Modeling, Analysis and Simulation of Computer and Telecommunication System (MASCOTS) - Miami, USA, July 2010
 * 
 * <em>Tiziano Santoro and Francesco Quaglia</em><br/>
 * <strong>A Low Overhead Costant Time LTF Scheduler for Optimistic Simulation Systems</strong><br/>
 * Proc. 15th IEEE International Symposium on Computers and Communications (ISCC), IEEE Computer Society Press, Riccione, Italy, June 2010
 * 
 * \section p-2009 2009
 * 
 * <em>Roberto Vitali, Alessandro Pellegrini and Francesco Quaglia</em><br/>
 * <strong>Benchmarking Memory Management Capabilities within ROOT-Sim</strong><br/>
 * In The 13-th IEEE/ACM International Symposium on Distributed Simulation and Real Time Applications (DS-RT) - Sigapore, September 2009
 * 
 * <em>Alessandro Pellegrini, Roberto Vitali and Francesco Quaglia</em><br/>
 * <strong>Di-DyMeLoR: Logging only Dirty Chunks for Efficient Management of Dynamic Memory Based Optimistic Simulation Objects</strong><br/>
 * In Proc. 23nd ACM/IEEE/SCS Workshop on Principles of Advanced and Distributed Simulation (PADS) - Lake Placid, New York, USA, June 2009
 * <strong>Candidate for (but not winner of) the Best Paper Award</strong>
 * 
 * \section p-2008 2008
 * 
 * <em>V. Gheri, G. Castellari and F. Quaglia</em><br/>
 * <strong>Controlling Bias in Optimistic Simulations with Space Uncertain Events</strong><br/>
 * Proc. 12th IEEE/ACM International Symposium on Distributed Simulation and Real Time Applications (DS-RT) - IEEE Computer Society Press, Vancouver, British Columbia, Canada, October 2008
 * <strong>Candidate for (but not winner of) the Best Paper Award</strong>
 * 
 * <em>R. Toccaceli and F. Quaglia</em><br/>
 * <strong>DyMeLoR: Dynamic Memory Logger and Restorer Library for Optimistic Simulation Objects with Generic Memory Layout</strong><br/>
 * Proc. 22nd ACM/IEEE/SCS Workshop on Principles of Advanced and Distributed Simulation (PADS) - Rome, Italy, IEEE Computer Society Press, June 2008
 * 
 * \section p-2007 2007
 * 
 * <em>D. Cucuzzo, S. D'Alessio, F. Quaglia and P. Romano</em><br/>
 * <strong>A Lightweight Heuristic-based Mechanism for Collecting Committed Consistent Global States in Optimistic Simulation</strong><br/>
 * Proc. 11th IEEE/ACM International Symposium on Distributed Simulation and Real Time Applications (DS-RT) - IEEE Computer Society Press, Chania, Crete Island, Greece, October 2007.
 * 
 */

