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


  parser.y: This is the ROOT-Sim Shell parser's grammar rules file

  CHANGES:
  01/03/2011 - Alessandro Pellegrini:
	Created the grammar

********************************************************************************/

#include <stdlib.h>
#include <string.h>

#include "globals.h"

extern int yylex(void);
extern int yyerror(const char *m);
%}

%union
{
  cmd_t *cmd;
  char str[256];
}

%token END 0
%token CONFIG
%token SET
%token HELP
%token RUN
%token NEWLINE
%token EXIT
%token UNSET
%token RESET
%token CMDLINE
%token WATERMARK
%token<str> STRING
%type<cmd> command config set unset help run exit reset cmdline watermark
%start command

%%

command: NEWLINE		{yylval.cmd=NULL; YYACCEPT;}
       | help NEWLINE		{yylval.cmd=$$; YYACCEPT;}
       | config NEWLINE		{yylval.cmd=$$; YYACCEPT;}
       | set NEWLINE		{yylval.cmd=$$; YYACCEPT;}
       | reset NEWLINE		{yylval.cmd=$$; YYACCEPT;}
       | unset NEWLINE		{yylval.cmd=$$; YYACCEPT;}
       | run NEWLINE		{yylval.cmd=$$; YYACCEPT;}
       | cmdline NEWLINE	{yylval.cmd=$$; YYACCEPT;}
       | exit NEWLINE		{yylval.cmd=$$; YYACCEPT;}
       | watermark NEWLINE	{yylval.cmd=$$; YYACCEPT;}
       | error command		{yyerrok;}
       | END			{YYABORT;}
       ;

help: HELP			{$$ = new_cmd(HELP_C);}
    | HELP CONFIG		{$$ = new_cmd(HELP_C);
				 add_arg($$, "config");}
    | HELP SET			{$$ = new_cmd(HELP_C);
				 add_arg($$, "set");}
    | HELP UNSET		{$$ = new_cmd(HELP_C);
				 add_arg($$, "unset");}
    | HELP RUN			{$$ = new_cmd(HELP_C);
				 add_arg($$, "run");}
    | HELP CMDLINE		{$$ = new_cmd(HELP_C);
				 add_arg($$, "cmdline");}
    | HELP STRING		{$$ = new_cmd(HELP_C);
				 add_arg($$, (char *)&$2);}
    ;

config: CONFIG			{$$ = new_cmd(CONFIG_C);}
    ;

reset: RESET			{$$ = new_cmd(RESET_C);}
     ;

set: SET STRING STRING		{$$ = new_cmd(SET_C);
				 add_arg($$, (char *)&$2);
				 add_arg($$, (char *)&$3);}
   | SET STRING			{$$ = new_cmd(SET_C);
				add_arg($$, (char *)&$2);}
   ;

unset: UNSET STRING		{$$ = new_cmd(UNSET_C);
				 add_arg($$, (char *)&$2);}
     ;

run: RUN STRING			{$$ = new_cmd(RUN_C);
				 add_arg($$, (char *)&$2);}
   ;

cmdline: CMDLINE STRING		{$$ = new_cmd(CMDLINE_C);
				 add_arg($$, (char *)&$2);}
   ;

watermark: WATERMARK		{$$ = new_cmd(WATERMARK_C);}
    ;

exit: EXIT			{$$ = new_cmd(EXIT_C);}
    ;

%%

cmd_t *new_cmd(int cmd_code) {
        cmd_t *command = (cmd_t*)malloc(sizeof(cmd_t));
	bzero(command, sizeof(cmd_t));
	command->cmd_code = cmd_code;
        command->nargs = 0;
        return command;
}

cmd_t *add_arg(cmd_t *cmd, char *arg) {
	if(cmd->nargs + 1 > MAX_ARGV)
		return NULL;

        strcpy(cmd->argv[cmd->nargs++], arg);
        return cmd;
}

