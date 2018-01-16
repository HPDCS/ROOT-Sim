#!/bin/bash

describe=$(git describe)
branchname=$(git branch 2> /dev/null | sed -e '/^[^*]/d' -e 's/* \(.*\)/\1/')

# Split version in major, minor, hotfix
IFS='-' read version garbage <<< "$describe"
IFS='.' read major minor hotfix <<< "$version"

# Get the type of branch, depending on the name
IFS='-' read branchtype garbage <<< "$branchname"

echo "The current branch ($branchname) is of type $branchtype"
echo "Current version is $major.$minor.$hotfix"

# Check if we have to increment the major
if [ "$1" == "major" ]; then
	major=$((major + 1))
elif [ "$branchtype" == "hotfix" ]; then
	hotfix=$((hotfix + 1))
elif [ "$branchtype" == "release" ]; then
	minor=$((minor + 1))
else
	echo "Branch name doesn't tell the version should be bumped."
	exit 1
fi

echo "Applying new version number: $major.$minor.$hotfix. Continue? (y/n)"
old_stty_cfg=$(stty -g)
stty raw -echo ; answer=$(head -c 1) ; stty $old_stty_cfg # Careful playing with stty
if echo "$answer" | grep -iq "^y" ;then
	echo "Applying changes to configure.ac"
else
	exit 0
fi

# Load current configure and build the new one with updated version
config_line=$(grep AC_INIT configure.ac)
new_config_line=${config_line//$version/$major.$minor.$hotfix}

# Apply the change to the file
configure_line=$(grep -n AC_INIT configure.ac | sed 's/:.*//')
configure_line=$((configure_line-1))
i=0
echo "" > configure.new
IFS=
while read -r line
do
	if [ $i -eq $configure_line ]; then
		echo $new_config_line >> configure.new
	else
		echo $line >> configure.new
	fi
	i=$((i+1))
done < configure.ac

mv configure.ac configure.bak
mv configure.new configure.ac

echo "Files modified successfully, version bumped to $major.$minor.$hotfix"
