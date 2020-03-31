# ROOT-Sim Libraries

To simplify the development of simulation models according to the speculative PDES paradigm, ROOT-Sim offers a set of libraries which can be used to implement important portions of simulation models, or to automatize tedious tasks.

In this section, we describe the available libraries, the exposed API, and we show some usage examples.

## Numerical Library

ROOT-Sim offers a fully-featured numerical library designed according to the Piece-Wise Determinism paradigm. The main idea behind this library is that if a Logical Process incurs into a Rollback, the seed which is associated with the random number generator associated with that LP must be rolled back as well. The numerical library provided by ROOT-Sim transparently does so, while if you rely on a different
numerical library, you must implement this feature by hand, if you want that a logical process is always given the same sequence of pseudo-random numbers, even when the execution is restarted from a previous simulation state.

The following functions are available in the ROOT-Sim numerical library. They can be used to draw samples from random distribution, which are commonly used in many simulation models.

### `Random()`

This function has the following signature:

`double Random(void);`

It returns a floating point number in between [0,1], according to a Uniform Distribution.

### `RandomRange()`

This function has the following signature:

`int RandomRange(int min, int max)`

It returns an integer number in between [`min`,`max`], according to a Uniform Distribution.

### `RandomRangeNonUniform()`

This function has the following signature:

`int RandomRangeNonUniform(int x, int min, int max)`

It returns an integer number in between [`min`,`max`]. The parameter `x` determines the incremented probability according to which a number is generated in the range, according to the following formula:

```c
(((RandomRange(0, x) | RandomRange(min, max))) % (max - min + 1)) + min
```

### `Expent()`

The signature of this function is:

`double Expent(double mean)`

It returns a floating point number according to an Exponential Distribution of mean value `mean`.

### `Normal()`

The signature of this function is:

`double Normal(void)`

It returns a floating point number according to a Normal Distribution with mean zero.

### `Gamma()`

The signature of this function is:

`double Gamma(int ia)`

It returns a floting point number according to a Gamma Distribution of Integer Order `ia`, i.e. a waiting time to the `ia`-th event in a Poisson process of unit mean.

### `Poisson()`

The signature of this function is:

`double Poisson(void)`

It returns the waiting time to the next event in a Poisson process of unit mean.

### `Zipf()`

The signature of this function is:

`int Zipf(double skew, int limit)`

It returns a random sample from a Zipf distribution.

## Topology Library

Many simulation models rely on a representation of the physical space. ROOT-Sim offers a library which you can use to instantiate discretized topologies with little effort. To enable the library it is sufficient to declare a `struct _topology_settings_t` variable in your model which has the following members: TODO

    struct _topology_settings_t{
        const char * const topology_path;
        const enum _topology_type_t type;
        const enum _topology_geometry_t default_geometry;
        const unsigned out_of_topology;
        const bool write_enabled;
    }


### `RegionsCount()`

The signature of this function is:

`RegionsCount(void)`

It returns the number of regions which are part of the instantiated topology.
You can identify an LP as a region at runtime if its id is lower than this value.
This value will be always equal or lower than n_prc_tot.

### `NeighboursCount()`

The signature of this function is:

`NeighboursCount(unsigned int region_id)`

It returns the number of valid neighbours of the LP with id `region_id` in the instantiated topology. This only returns the number of geometrically viable regions, e.g. it doesn't distinguish obstacles from non obstacles regions.

### `DirectionsCount()`

The signature of this function is:

`DirectionsCount(void)`

It returns the number of valid directions in the instantiated topology.
This value coincides with the maximum number of neighbours a region can possibly have in the current topology (e.g. `DirectionsCount() = 4` in a square topology).

### `GetReceiver()`

The signature of this function is:

`GetReceiver(unsigned int from, direction_t direction, bool reachable)`

It returns the LP id of the neighbour you would reach going from region `from` along direction `direction`. The flag `reachable` is set if you only want the id of a LP considered reachable.
In case the function isn't able to deliver a correct id it returns `DIRECTION_INVALID`.

### `FindReceiver()`

The signature of this function is:

`FindReceiver(void)`

It returns the id of an LP selected between the neighbours of the LP it is called in. The selection logic works as follows:
- if the current topology type is `TOPOLOGY_OBSTACLES`, a non obstacle neighbour is uniformly sampled. If there's no suitable neighbours we return the current lp id
- if the current topology type is `TOPOLOGY_PROBABILITIES`, an LP is selected with a probability proportional with the weight of the outgoing topology edge from the current LP
- an error is thrown if you try to use this function in a topology with type `TOPOLOGY_COSTS` since this operation would have little sense.
   
### `FindReceiverToward()`

The signature of this function is:

`FindReceiverToward(unsigned int to)`

It returns the id of the next LP you have to visit in order to reach the LP with id `to` with the smallest possible incurred cost. In case there's no possible route `DIRECTION_INVALID` is returned.
The cost is calculated as follows:
- in a topology having type `TOPOLOGY_OBSTACLES`, the cost is simply the number of hops needed to reach a certain destination
- in a topology having type `TOPOLOGY_COSTS`, the cost is the sum of the weights of the edges traversed in the path needed to reach the destination
- the notion of cost has little sense in a `TOPOLOGY_PROBABILITIES` type topology so an error is thrown if you try to use this function for this purpose.
   
### `ComputeMinTour()`

The signature of this function is:

`ComputeMinTour(unsigned int source, unsigned int dest, unsigned int result[RegionsCount()])`

It computes the minimum cost directed path from region `source` to region `dest`.
You have to pass in `result` a pointer to a memory location with enough space to store `RegionsCount()` LP id (clearly  it's the longest possible minimum cost path).
The returned value is the cost incurred in the path traversal or -1 if it isn't possible to find a path at all.
TODO further details

### `SetValueTopology()`

The signature of this function is:

`SetValueTopology(unsigned int from, unsigned int to, double value)`

TODO

### `GetValueTopology()`

The signature of this function is:

`GettValueTopology(unsigned int from, unsigned int to)`

TODO

## Agent-Based Modeling Library

High level description of ABM library, init details TODO

### `SpawnAgent()`

The signature of this function is:

`SpawnAgent(unsigned user_data_size)`

It returns the id of a newly instantiated agent with additional `user_data_size` bytes where you can carry around your data.

### `DataAgent()`

The signature of this function is:

`DataAgent(agent_t agent)`

It returns a pointer to the user-editable memory area of the agent `agent`.

### `KillAgent()`

The signature of this function is:

`KillAgent(agent_t agent)`

It destroys the agent `agent`. Once killed, an agent disappears from the region.

### `CountAgents()`

The signature of this function is:

`CountAgents(void)`

It returns the number of agents in the current region.

### `IterAgents()`

The signature of this function is:

`IterAgents(agent_t *agent_p)`

TODO

### `ScheduleNewLeaveEvent()`

The signature of this function is:

`ScheduleNewLeaveEvent(simtime_t time, unsigned int event_type, agent_t agent)`

It schedules a leave event at the logical time `time` for the agent `agent` with code `event_type`. That will be the last event the agent can witness before moving to another region. TODO

### `TrackNeighbourInfo()`

The signature of this function is:

`TrackNeighbourInfo(void *neighbour_data)`

It sets the pointer to the data you want to publish to the neighbouring regions. Once set, ROOT-Sim runtime transparently will keep updated the published data with the neighbours. 

### `GetNeighbourInfo()`

The signature of this function is:

`GetNeighbourInfo(direction_t i, unsigned int *region_id, void **data_p)`

It retrieves the data published by the neighbouring region you would reach going from the current LP along direction `i`. `region_id` must point to a variable which will be set to the LP id of the retrieved neighbour, `data_p` must point to a pointer variable which will point to the requested data. In case of success `0` is returned otherwise there's no neighbour along the requested direction which published his data and `-1` is returned.

### `CountVisits()`

The signature of this function is:

`CountVisits(const agent_t agent)`

It returns the number of visits scheduled in the future for the agent `agent`.

### `GetVisit()`

The signature of this function is:

`GetVisit(const agent_t agent, unsigned *region_p, unsigned *event_type_p, unsigned i)`

TODO

### `SetVisit()`

The signature of this function is:

`SetVisit(const agent_t agent, unsigned *region_p, unsigned *event_type_p, unsigned i)`

TODO

### `EnqueueVisit()`

The signature of this function is:

`EnqueueVisit(agent_t agent, unsigned region, unsigned event_type)`

TODO

### `AddVisit()`

The signature of this function is:

`AddVisit(agent_t agent, unsigned region, unsigned event_type, unsigned i)`

TODO

### `RemoveVisit()`

The signature of this function is:

`RemoveVisit(agent_t agent, unsigned i)`

TODO

### `CountPastVisits()`

The signature of this function is:

`CountPastVisits(const agent_t agent)`

It returns the number of visits already completed by the agent `agent` in the past.

### `GetPastVisit()`

The signature of this function is:

`GetPastVisit(const agent_t agent, unsigned *region_p, unsigned *event_type_p, simtime_t *time_p, unsigned i)`

TODO

## JSON Parsing Library

