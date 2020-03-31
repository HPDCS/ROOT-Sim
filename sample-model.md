# Writing your first model

## The ROOT-Sim Programming Model

ROOT-Sim exposes a reduced-size set of APIs, which can be easily used to build complex simulation models. In particular, there are four main functions which are exposed to simulation model developers. Two of them are callbacks (`ProcessEvent()` and `OnGVT()`), which are used to implement event handlers and inspect a committed simulation state, respectively. The third API function is `ScheduleNewEvent()`, which allows to inject new events into the system, destined to any LP. The fourth, `SetState()`, allows the simulation model developer to change the simulation state of an LP at any time.

Here we present a quick overview on ROOT-Sim API and on how a simple simulation model can be implemented in C. Everything exposed by ROOT-Sim is defined in the header `ROOT-Sim.h`, which is installed system-wide (or on a particular target when using `--prefix` during the configuration phase) when running `make`. The `rootsim-cc` compiler is generated automatically to know the location of the installation of the headers, so it will tell the backend compiler where to find this header.

### `ProcessEvent()`

`ProcessEvent()` is the main application-level callback. A simulation model must implement this function  to specify the logic associated with the event handlers. `ProcessEvent` is the sole entry point at application level which is used to schedule the actual events to be simulated. Therefore, it can be seen as the demultiplexer of the various event handlers which should be implemented in the simulation mode. Its full signature is:

`void ProcessEvent(int me, simtime_t now, unsigned int event_type, void *content, unsigned int size, void *state);`

The meaning of the arguments passed to this callback is: 

* `me`: the global id associated with the LP which is being scheduled for event execution. This is automatically assigned by the runtime environment, and is in the range [0, `n_prc_tot`-1], where `n_prc_tot` is the value passed at command line using the `--nprc` flag. The simulation model developer can assume that this exact number of LPs is available in the system, which can be either scheduled by the runtime environment through a call to `ProcessEvent()`, or to which a new simulation event can be fired, using the `ScheduleNewEvent()` API function.
* `now`: the current value of the logical time of the LP. This is consistently and transparently managed by the runtime environment. `simtime_t` is  also defined in `ROOT-Sim.h`. This value, if necessary, can be used as a `double`, e.g. to compute time intervals or for logging.
* `event`: the numerical code determining the type of the event to be processed. Except for the special `INIT` event (see below), these values can be safely chosen by the model developer to best suit their needs.
* `content`: the buffer where the event payload will be delivered by the ROOT-Sim kernel
  (may be `NULL` in case the event has no payload). Be careful! This buffer must be accessed in read-only mode by the model. Writing to that buffer might yield to undefined behaviour due to the nature of optimistic simulation. Please make your own copy of that buffer’s content if you wish to operate in write-mode on it. The `--extra-check` configuration flag runs some hashing before and after event execution on this buffer, to be sure that the modeler does not inadverently alter its content (this can be used for debugging your model).
* `size`: the size (in bytes) of the event payload. It is zero if `content` is set to `NULL`.
* `state`: the pointer to the top data structure forming the simulation state layout. This is decided by the simulation model's code, and can be changed at any time.

Upon initialization, ROOT-Sim schedules the special `INIT` event (with numerical code 0) once to each LP. This means both that the code should handle this event in `ProcessEvent()`, and that an event with id 0 cannot be used by the application-level code. `INIT` is defined in `ROOT-Sim.h`. The purpose of this event is to allow LPs to perform initialization operations (such as allocating space for their states).

A simulation object is not dispatched again, unless a real application-level event is scheduled for it during the simulation run.

### `ScheduleNewEvent()`

`ScheduleNewEvent()` allows the simulation model to generate a new event and inject it into the system, destined at any LP (even itself).  Its full signature is:

`void ScheduleNewEvent(unsigned int receiver, simtime_t timestamp, unsigned int event, void *content, unsigned int size);`

The arguments passed to this function are:

* `receiver`: the global id of the logical processes where the simulation event must be delivered to.
  This should be in the interval [0,  `n_prc_tot` - 1].
* `timestamp`: the logical time when the recipient of the event must execute it. This makes the simulation time of the receiver advance exactly to that simulation time, once the event is executed. Its value can never be smaller that the value of `now` passed to `ProcessEvent()`, as this would make the future affect the past, which is impossible.
* `event`: the numerical code for the event to be injected into the system. This is model-defined, and causes the activation of the corresponding event handler at the recipient.
* `content`: the pointer to the buffer maintaining the application-defined event payload.
* `size`: the size (in bytes) of the event payload.

### `OnGVT()`

`OnGVT` is an application-level callback. All models must implement this function. By using this callback, the runtime environment enforces a (distributed) termination detection procedure. When the GVT is reduced, all LPs are asked whether the simulation (at that particular LP) can be considered as completed. In case that all LPs reply positively, the simulation is halted. Its full signature is:

`bool OnGVT(unsigned int me, void *snapshot);`

The arguments passed to this callback are:

* `me`: the global id associated to the LP which is being scheduled for termination detection.

* `snapshot`: a consistent simulation state, associated with the GVT value, which can be used by
  the LP to decide whether the simulation can terminate or not.

When running an optimistic simulation, the state to be inspected is one which can be associated with  a timestamp significantly smaller than the current one reached on the speculative boundary.  It is therefore meaningless (and unsafe) to alter the content of this state.  Similarly, the model cannot send any new event during the execution of `OnGVT()`.

The distributed termination detection can be executed in a normal or incremental fashion. Depending on the `cktrm_mode` runtime parameter, the platform can be instructed to ask all the LPs if they want to halt
the simulation every time the GVT is computed, or if an LP should be excluded from the check once it has replied in a positive way.

This difference can be useful to enhance the simulation performance when dealing with models which can have an oscillating termination condition (i.e., there is a certain phase of simulation where a simulation object wants to terminate, and a subsequent phase where it no longer wants to) or a monotone termination condition (i.e., when a process decides to terminate, it will never change its mind), respectively.

Nevertheless, the same implementation for the termination check can be used in both ways, so that the OnGVT function can be left untouched.

`OnGVT()` is given a consistent simulation snapshot on a periodic frequency. Therefore, if the simulation model wants to dump on file some statistics, in this function this task can be correctly implemented.

### `SetState()`

By definition of the programming model, the simulation state is located into `malloc`'d memory, and the platform silently and transparently restores it to previous checkpoint, whenever a rollback operation is performed, due to an inconsistency in the simulation caused by an out-of-order execution of events.

This means that the runtime environment must be aware of the location of the base pointer to the simulation state. This buffer can, at any time, be changed by the model. The programmer must issue a call to `SetState()`, in order to inform the runtime environment of the user's will to use a certain
memory buffer as the main simulation state for the Logical Process which is currently running.
This allows the runtime to correctly track changes in the objects' states, in order to correctly perform rollback operations, if needed. The full signature of this function is:

`void SetState(void *new_state);`

`new_state` is a pointer to the base structure of the simulation state. This structure can then keep any other internal pointer to `malloc`'d memory, which is transparently checkpointed by the runtime environment.

Please note that this function can be considered only "syntactic sugar". Indeed, all buffers allocated via a `malloc` call by the model are checkpointed and restored transparently. `SetState()`  only tells ROOT-Sim what is the value to be passed to `ProcessEvent()` as the `state` parameter, to simplify the development of the model. This value can change at any time, and the runtime environment transparently rolls back its content in case of a rollback operation, ensuring that the value of `state` passed to `ProcessEvent()` is always consistent.

## A minimal example

We provide here the source of a minimal functioning ROOT-Sim simulation model. In this example, one single event is specified, and a state structure containing an event counter is defined. Each LP in the simulation model schedules an event to a random process according to a Uniform distribution, and the times associated with the events are determined according to the same distribution.

```c
#include <ROOT-Sim.h>
#define EVENT 1
#define TOTAL_NUMBER_OF_EVENTS 1000000

typedef struct _state_type {
    int executed_events;
} state_type;

void ProcessEvent(unsigned int me, simtime_t now, unsigned int event, void *content, int size, state_type *state)
{
    simtime_t timestamp = now + 10 * Random();
    unsigned int receiver = (unsigned int)(n_prc_tot * Random());
    
    switch(event_type) {
        case INIT:
            state = malloc(sizeof(state_type));
            SetState(state);
            state->executed_events = 0;
            ScheduleNewEvent(me, timestamp, EVENT, NULL, 0);
            break;
            
        case EVENT:
            state->executed_events++;
            ScheduleNewEvent(me, timestamp, EVENT, NULL, 0);
            break;
    }
}

bool OnGVT(int me, void *snapshot)
{
    if (snapshot->executed_events >= TOTAL_NUMBER_OF_EVENTS)
        return true;
    return false;
}
```

This block of code can be used as a skeleton to develop every simulation model. Note that the `ProcessEvent()` callback relies on a `swtich/case` construct to demultiplex the value of `event` to implement the various event handlers. A handler of the special `INIT` event is provided, which allocates the initial simulation state via a `malloc` call (this will be transparently rolled back, in case of inconsistencies).`SetState()` is used to notify ROOT-Sim about the base pointer of the simulation state, only during the execution if `INIT` (the simulation state never changes).

`Random()` is a function belonging to the numerical library of ROOT-Sim, which will be later described. It is essentially a pseudo-random number generator, with a per-LP seed which is transparently rolled back upon  a rollback operation, ensuring repeatability of random variables draws from numerical distributions upon a rollback operation.

The implementation of `OnGVT()` describes when the simulation should be halted. In particular, each LP has in its own state the counter `executed_events` which is incremented any time that the `EVENT` handler is activated. Once this value reaches a certain threshold (`TOTAL_NUMBER_OF_EVENTS`), the simulation is considered completed. Since `OnGVT()` is evaluated at each LP, all LPs must have executed at least `TOTAL_NUMBER_OF_EVENTS` to halt the simulation.

It is important to note the usage of the `>=` comparison in `OnGVT()`. For performance reasons, `OnGVT()` is called periodically (see the `gvt-snapshot-cycles` runtime option). Therefore, it is possible that an exact value of `executed_events` is never evaluated. Using a `==` operator in `OnGVT()` can be unsafe, leading to simulations to never terminate. This means that, by the definition of `OnGVT()`, simulation models return "upper values" to the termination conditions of simulation models.

Additional example models are available in the `models` subfolders of the ROOT-Sim tarball and on the official repository. They can be used as valuable examples to get started.

## Passing Parameters to the Models

It is quite common for a simulation model to be configured at runtime, without having to recompile everything. To this end, ROOT-Sim provides a simple facility to provide runtime parameters to simulation models.

The configuration of ROOT-Sim relies on the standard `argp` library. A simulation model can define some variables which are intercepted by ROOT-Sim at compile time, making the option parser to take into account also model-specified attributes. 

The following code example illustrates how it is possible, for a simulation model, to intercept two options, called `--opt-A` and `--opt-B`, the first accepting a floating point value as its argument, the second accepting an integer.

```c
double A;
int B;

enum {
	OPT_A = 128, /// this tells argp to not assign short options
	OPT_B,
};

const struct argp_option model_options[] = {
		{"opt-A", OPT_A, "FLOAT", 0, "This is the A option", 0},
		{"opt-B", OPT_B, "INT", 0, "This is the B option", 0},
		{0}
};

#define HANDLE_CASE(label, fmt, var)      \
	case label:                           \
		if(sscanf(arg, fmt, &var) != 1) { \
			return ARGP_ERR_UNKNOWN;      \
		}                                 \
	break

static error_t model_parse (int key, char *arg, struct argp_state *state){
	switch (key) {
		HANDLE_CASE(OPT_A, "%lf", A);
		HANDLE_CASE(OPT_B, "%d", B);
        default:
			return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

#undef HANDLE_CASE
```

The code declares an `enum` to define the numerical codes of the options. `argp` uses values greater than 127 to identify long options, so setting `OPT_A` to 128 ensures that it is parsed as `--opt-A`.

The `model_options` array defines, according to the `argp` syntax, what are the options which are handled by the model. A textual description can be provided, which is printed on screen if the following command is issued on the command line:

```c
./model --help
```

`HANDLE_CASE` is a commodity macro which allows to simplify the development of the switch case in `model_parse`, which implements the logic associated with option setting. This is transparently invoked by `argp`.  Note that relying on `sscanf` in `HANDLE_CASE` might not be the most secure way to implement the code (it is used here for the sake of brevity).

This configuration infrastructure is called well before LPs are initialized and `INIT` is scheduled. Therefore, it is important to rely on global variables (such as `A` and `B` in the example) to store the values passed from command line. Later, during the execution of `INIT`, these values can be used to properly initialize the simulation model.