---
title: About
layout: default
---

{% include_relative README.md %}

# ROOT-Sim History

ROOT-Sim started as a research project to study distributed synchronization back in 1997.

Since then, the project has evolved significantly and it has allowed to experiment in many research directions. In the early days, the goal was to devise innovative synchronization schemes for distributed HPC environments, specifically to reduce the cost associated with the rollback operation. At the time, the name of the project was _SimCore_.

In the 90's, a lot of research has been carried out to simplify the programming API, synthesizing a very-reduced API which allows to implement any simulation model in a mostly-sequential way, without the need to take into account all the idiosyncrasies related to speculative executions. The name _ROOT-Sim_ appeared at this time.

In the 2000's, the research has been focused on the optimization of the checkpointing schemes, introducing transparent support to use any kind of POSIX function to manage memory, demanding from the runtime environment the burden to keep everythign synchronized. In the last years, the focus has been towards programming transparency: new features have been added which allow to implement models which are portable across some kind of heterogeneous architectures, taking into account e.g. NUMA systems and clusters composed of different machines.