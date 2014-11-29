%{

/********************************************************************************

                        Copyright (C) 2008-2014 HPDCS Group
                        http://www.dis.uniroma1.it/~hpdcs


This file is part of ROOT-Sim (ROme OpTimistic Simulator).

ROOT-Sim is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation; either version 3 of the License, or (at your option) any later
version.

ROOT-Sim is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
ROOT-Sim; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA


  lexer.l: This file describes the token to be understood by the
           ROOT-Sim Shell's parser

  CHANGES:
  26/10/2011 - Alessandri Pellegrini:
	reset config + set with only one parameter
  01/03/2011 - Alessandro Pellegrini:
	Created the lexer

********************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include "globals.h"
#include "yy_bison.h"

typedef YYSTYPE type_t;

extern int shell_error;

int file_num;
int file_num_max = 0;
char **files;

YY_BUFFER_STATE bp;

int yyerror(const char *m);
YYSTYPE text_var(void);
int skip_comment(void);

%}

%option yywrap

CONFIG_S		[cC][oO][nN][fF][iI][gG]
SET_S			[sS][eE][tT]
HELP_S			[hH][eE][lL][pP]
RUN_S			[rR][uU][nN]
EXIT_S			[eE][xX][iI][tT]
QUIT_S			[qQ][uU][iI][tT]
UNSET_S			[uU][nN][sS][eE][tT]
RESET_S			[rR][eE][sS][eE][tT]
CMDLINE_S		[cC][mM][dD][lL][iI][nN][eE]
WATERMARK_S		[wW][aA][tT][eE][rR][mM][aA][rR][kK]

%%

{EXIT_S}		{return EXIT;}
{QUIT_S}		{return EXIT;}
{CONFIG_S}		{return CONFIG;}
{SET_S}			{return SET;}
{HELP_S}		{return HELP;}
{RUN_S}			{return RUN;}
{UNSET_S}		{return UNSET;}
{RESET_S}		{return RESET;}
{WATERMARK_S}		{return WATERMARK;}
{CMDLINE_S}		{return CMDLINE;}
["\r""\n"]		{return NEWLINE;}


 /* Crunch single line comments */
"#"			{if(!skip_comment()) yyerror("Malformed comment line");}

 /* Ignore whitespaces */
[ \t]                   {}

 /* Strings are defined for handling commands' parameters */
[a-zA-Z0-9\-+\[\]\_\.\/]+	{yylval = text_var(); return STRING;}

 /* Everything else is an error */
.                       {yyerror("Unsupported command");}

<<EOF>>			{return END;}

%%

YYSTYPE text_var(void) {
        YYSTYPE text_ret;
        strncpy(text_ret.str, yytext, yyleng);
        text_ret.str[yyleng] = '\0';
        return text_ret;
}

int yyerror(const char *m) {

	shell_error = 1;

	if(strcmp(yytext, "\n") == 0) {
		yytext = "";
	}

	if(file_num_max > 0) { // Parsing a file!
		fprintf(stderr, "%s:%d: ", files[file_num], yylineno);
	}

	fprintf(stderr, "\"%s\": %s.", yytext, m);

	if(file_num_max == 0) { // Running in interactive mode
		fprintf(stderr, " Type HELP for the full list of available commands.");
	}

	fprintf(stderr, "\n");
	fflush(stderr);

	return 1;
}


void yy_set_input_file(void) {
	if((yyin = fopen(files[file_num], "r")) == NULL) {
		perror(files[file_num]);
		exit(EXIT_FAILURE);
	}

	printf("\n*** Executing %s***\n\n", files[file_num]);

	bp = yy_create_buffer(yyin, YY_BUF_SIZE);
	yy_switch_to_buffer(bp);
}


int yywrap(void) {

	if(file_num_max == 0) { // Running in interactive mode
		return 0;
	}

	yy_flush_buffer(bp);
	yy_delete_buffer(bp);

	fclose(yyin);
	if(++file_num < file_num_max) {
		yy_set_input_file();
		return 0;
	} else {
		return 1;
	}
}

int skip_comment(void) {
	char c = !EOF;

	while(1) {
		if(c == EOF) {
			return 1;
		}
		
		if(c == '\n') {
			unput('\n'); // TODO: This should be used to close a command placed before the comment!
			return 1;
		}

		// Crunch characters until end of line is found
		c = (char)input();
	}

	return 0;
}


