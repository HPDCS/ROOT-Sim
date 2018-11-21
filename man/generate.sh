#!/bin/bash

# Generate manpages starting from md-like syntax using ronn

for page in *.md; do
	ronn --roff $page --manual="ROOT-Sim Manual"
done
