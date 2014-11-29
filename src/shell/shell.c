/**
*			Copyright (C) 2008-2014 HPDCS Group
*			http://www.dis.uniroma1.it/~hpdcs
*
*
* This file is part of ROOT-Sim (ROme OpTimistic Simulator).
* 
* ROOT-Sim is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; either version 3 of the License, or (at your option) any later
* version.
* 
* ROOT-Sim is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
* 
* You should have received a copy of the GNU General Public License along with
* ROOT-Sim; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
* 
* @file shell.c
* @brief Interactive Shell entry point
* @date March 01, 2011
* @author Alessandro Pellegrini
*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

#include "globals.h"
#include "yy_bison.h"

extern void yy_set_input_file(void);
extern void execute_reset(void);

extern int file_num;
extern int file_num_max;
extern char **files;



/// Did the last command's execution produced an error?
int shell_error = 0;




/**
* This function implements the main loop used to parse multiple script files.
* The main difference from the interactive shell's main loop lies in that it
* quits with failure whenever an error is encountered.
*
* @author Alessandro Pellegrini
*/
void execute_script(void) {

	// Loop differentiated to handle errors in a different way
	// yyparse() returns 0 if parsing succeeded
	while(yyparse() == 0) {

		// If an error was found, abort
		if (shell_error == 1) {
			exit(EXIT_FAILURE);
		}

		// Execute the command
		if(yylval.cmd != NULL)
			execute_cmd(yylval.cmd);
	}

}


/**
* This function implements the main loop used to parse interactive commands.
* The main difference from the script parsing's main loop lies in that if an
* error is encountered, a consistent state is resumed and an error message
* is print to the user (both through flex/bison facilities).
*
* @author Alessandro Pellegrini
*/
void interactive(void) {
	int i;


	// This prints the header. The following define defines the width of the box
	// If it is smaller than the text to be put inside it won't work
	#define LINE_LEN 66
	#define HEADING PACKAGE_NAME " v. " PACKAGE_VERSION
	#define INFORMATION "Developed by the HPDCS Group @ Sapienza, University of Rome"
	#define SPACING_LEFT(str) ((LINE_LEN-2)/2 + (int)strlen(str) / 2)
	#define SPACING_RIGHT(str) ((LINE_LEN-2)/2 - (int)strlen(str) / 2)
	#define center_line(str) (printf("*%*s%*s*\n", SPACING_LEFT(str), str, SPACING_RIGHT(str), ""))

	putchar('\n');
	for(i = 0; i < LINE_LEN; i++) {
		putchar('*');
	}
	putchar('\n');

	center_line(HEADING);
	center_line(INFORMATION);
	center_line(PACKAGE_URL);
	center_line(PACKAGE_BUGREPORT);
	
	for(i = 0; i < LINE_LEN; i++) {
		putchar('*');
	}
	puts("\n\n\n");

	#undef center_line
	#undef SPACING_RIGHT
	#undef SPACING_LEFT
	#undef INFORMATION
	#undef SQUARE_LEN


	// Shell main loop
	while(1) {

		fprintf(stderr, "ROOT-Sim> ");

		yyparse();

		// If an error was found, ignore the input and continue
		if (shell_error == 1) {
			shell_error = 0;
			continue;
		}

		// Execute the command
		if(yylval.cmd != NULL)
			execute_cmd(yylval.cmd);
	}

}




/**
* This is the entry point for the interactive shell. It checks whether script
* files are passed to be parsed or not. Depending on this, the correct
* execution loop is called. In addition, pre-defined execution variables are set.
*
* @author Alessandro Pellegrini
*/
int main(int argc, char **argv) {

	// Ignore SIGINT (which can be used to kill the simulator within the shell
	signal(SIGINT, SIG_IGN);

	// Set some pre-defined execution variables
	execute_reset();

	// Should we use a script-file or launch the interactive shell?
	if(argc > 1) {
		// Set the files to execute. Passing from one file to another is handled by yywrap()
		file_num = 1;
		file_num_max = argc;
		files = argv;

		yy_set_input_file();

		// Start the script execution loop
		execute_script();
	} else {
		interactive();
	}

	exit(EXIT_SUCCESS);

}

