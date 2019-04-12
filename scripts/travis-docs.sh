#!/bin/bash

# This script automates the generation of the doxygen documentation.
# It is used by .travis.yml to autodeploy the configuration on the website.

# Build the doxygen documentation
cd docs
make
cd ..

# Clone the github pages website
git clone --depth=1 -b gh-pages --single-branch https://github.com/HPDCS/ROOT-Sim.git
mkdir -p ROOT-Sim/docs/

# Copy the documentation related to the current branch (this is triggered only for master
# and develop by the .travis.yml deploy section)
rm -rf ROOT-Sim/docs/$TRAVIS_BRANCH
mv docs/html ROOT-Sim/docs/$TRAVIS_BRANCH

# Copy the current version readme file, which is used in the website About section
cp README.md ROOT-Sim

# We use also some content from the Wiki in the website, so clone it
# and copy what is needed.
git clone --depth=1 https://github.com/HPDCS/ROOT-Sim.wiki.git
cp ROOT-Sim.wiki/chapters/part2/installation.md ROOT-Sim
cp ROOT-Sim.wiki/chapters/part2/libraries.md ROOT-Sim
cp ROOT-Sim.wiki/chapters/part2/sample-model.md ROOT-Sim
cp ROOT-Sim.wiki/chapters/part2/usage.md ROOT-Sim

