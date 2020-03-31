---
title: ROOT-Sim 
layout: default
---

# The ROme OpTimistic Simulator

Welcome to the ROOT-Sim website! ROOT-Sim is a HPC library targeting optimistic simulation. This runtime environment is able to deliver
very fast simulation runs on massively parallel multicore machines and or distributed compute clusters/supercomputers.

If you want to have more information on how ROOT-Sim can fit your purposes, or if you want to contribute to the project,
the [wiki](https://github.com/HPDCS/ROOT-Sim/wiki) is a good place to start. The [about]({{site.url}}/about.html) page shows few
steps to get ROOT-Sim running, using some of the example models provided in the distribution.

If you have found a bug, or if you want to request a new feature, feel free to open a new [issue on Github](https://github.com/HPDCS/ROOT-Sim/issues).

You can get the latest version of the library using the links on the right column of this page, or you can clone the [Github repository](https://github.com/HPDCS/ROOT-Sim/).


# Latest News

{% for post in site.posts %}
<h2><a href="{{ site.url }}{{ post.url }}">{{ post.title }}</a></h2>
{{ post.date | date_to_string }} &middot; {{ post.author }}<br/>
{{ post.content | strip_html | remove_first:post.title | truncatewords:75}}<br>
<a href="{{ site.url}}{{ post.url }}">Read more...</a><br><br>
{% endfor %}

