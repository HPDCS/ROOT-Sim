#!/bin/bash

# This scripts generates a .c output file that needs to be processed by the llvm pass in order not to 
# consider as eligible for instrumentation all the library functions exposed in ROOT-Sim.h.

file_path="src/ROOT-Sim.h"
output_file="src/compiler/rootsim_exposed_functions.cpp"

tmp_file="dump.txt"

rm -f $tmp_file

# First, remove all the (multiline) comments from the string to process. Then, sort out all the strings which 
# represent function declarations. Finally, extract only function names from them, and store them in a temporary file.
sed -r ':a; s%(.*)/\*.*\*/%\1%; ta; /\/\*/ !b; N; ba' $file_path | sed -r 's/\/\/.*//' | grep -P '[A-Z][0-9A-Za-z]*[ \t\n\r\f\v]*\(' | grep -oP '[A-Z][0-9A-Za-z]*' | sort -t: -u -k1,1 > $tmp_file

rm -f $output_file
printf "const char *rootsim_exposed_functions[] = {\n" >> $output_file

# Read all function names in the temporary file and store them in an null-terminated array in the real output file. 
while IFS= read -r line
do
  printf "\t\"$line\",\n" >> $output_file
done < $tmp_file

printf "\tnullptr\n};" >> $output_file

rm -f $tmp_file