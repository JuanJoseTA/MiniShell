/**
UNIX Shell Project

Sistemas Operativos
Grados I. Informatica, Computadores & Software
Dept. Arquitectura de Computadores - UMA

AUTOR: Juan Jose Tirado Arregui

Some code adapted from "Fundamentos de Sistemas Operativos", Silberschatz et al.

To compile and run the program:
   $ gcc Shell_project.c job_control.c -o Shell
   $ ./Shell          
	(then type ^D to exit program)

**/

#include "job_control.h"   // remember to compile with module job_control.c 
#include "string.h"

#define MAX_LINE 256 /* 256 chars per line, per command, should be enough. */

// -----------------------------------------------------------------------
//                            MAIN          
// -----------------------------------------------------------------------

job * bgStopped;

void manejador(int signal)
{
	job * j;
	int status, info, pid_wait = 0;  
	enum status estado;
	block_SIGCHLD();
	for(int i = list_size(bgStopped); i>=1; i--){
		j = get_item_bypos(bgStopped, i);
		if(j != NULL){
			pid_wait = waitpid(j->pgid, &status, WUNTRACED | WNOHANG | WCONTINUED);
			if(j->pgid == pid_wait){
				estado = analyze_status(status, &info);
				if(estado == EXITED || estado == SIGNALED){
					printf("\nBackground process sleep(%d) Exited\n", j->pgid);
					delete_job(bgStopped, j);
				}
				else if(estado == SUSPENDED){
					j->state = STOPPED;
					printf("\nBackground process sleep(%d) Suspended\n", j->pgid);
				}
			}
		}
	}
	unblock_SIGCHLD();
}

int main(void)
{
	char inputBuffer[MAX_LINE]; /* buffer to hold the command entered */
	int background;             /* equals 1 if a command is followed by '&' */
	char *args[MAX_LINE/2];     /* command line (of 256) has max of 128 arguments */
	// probably useful variables:
	static char* status_strings[] = { "Suspended","Signaled","Exited", "Continued"};
	int pid_fork, pid_wait; /* pid for created and waited process */
	int status;             /* status returned by wait */
	enum job_state state;
	enum status status_res; /* status processed by analyze_status() */
	int info;				/* info processed by analyze_status() */
	
	ignore_terminal_signals();
	bgStopped = new_list("Tareas BG/STOPPED");
	signal(SIGCHLD,manejador);
	
	while (1)   /* Program terminates normally inside get_command() after ^D is typed*/
	{   		
		printf("COMMAND->");
		fflush(stdout);
		get_command(inputBuffer, MAX_LINE, args, &background);  /* get next command */
		
		
		if(args[0]==NULL) continue;   // if empty command
		
		if(!strcmp(args[0],"cd")){
			chdir(args[1]);
			continue;
		}
		if(!strcmp(args[0], "jobs")){
			block_SIGCHLD();
			print_job_list(bgStopped);
			unblock_SIGCHLD();
			continue;
		}
		if(!strcmp(args[0], "fg")){
			int n = 1;
			if(args[1] != NULL) n = atoi(args[1]);
			job * j = get_item_bypos(bgStopped, n);
			
			if(j == NULL){
				printf("Not found at bgStopped");
			}else{
				if(j->state != FOREGROUND){
					j->state = FOREGROUND;
					set_terminal(j->pgid);
					killpg(j->pgid, SIGCONT);
					waitpid(j->pgid, &status, WUNTRACED);
					
					set_terminal(getpid());
					status_res = analyze_status(status, &info);
					if(status_res == SUSPENDED){
						j->state = STOPPED;
					}else{
						delete_job(bgStopped, j);
					}
					printf("Foreground pid: %d, command: %s, %s, info:%d\n", pid_fork, args[0], status_strings[status_res], info);
				}else{
					printf("Proccess not SUSPENDED or BACKGROUND");
				}
			}
		}
		if (!strcmp(args[0], "bg")){
			block_SIGCHLD();
			int n = 1;
			if(args[1] != NULL) n = atoi(args[1]);
			job * j = get_item_bypos(bgStopped, n);
			if(j != NULL && (j->state ) == STOPPED){
				j->state = BACKGROUND;
				killpg(j->pgid, SIGCONT);
				printf("Background job running... pid: %d, command: %s\n", j->pgid, args[0]);
			}
			unblock_SIGCHLD();
			continue;
		}

			/* the steps are:
				(1) fork a child process using fork()
				(2) the child process will invoke execvp()
				(3) if background == 0, the parent will wait, otherwise continue 
				(4) Shell shows a status message for processed command 
				(5) loop returns to get_commnad() function
			*/
			pid_fork = fork();
			
			if(pid_fork == 0){
				new_process_group(getpid());
				if(background == 0){
					set_terminal(getpid());
				}
				restore_terminal_signals();
				execvp(args[0],args);
				printf("Error, command not found: %s\n", args[0]);
				exit(-1);
			}else{
				new_process_group(pid_fork);
				if(background == 0){
					set_terminal(pid_fork);
					pid_wait = waitpid(pid_fork, &status, WUNTRACED);
					set_terminal(getpid());
					status_res = analyze_status(status, &info);
					if(status_res == SUSPENDED) {
						block_SIGCHLD();
						add_job(bgStopped, new_job(pid_fork, args[0],  STOPPED)); 
						unblock_SIGCHLD();
					}
					printf("Foreground pid: %d, command: %s, %s, info: %d\n", pid_fork, args[0], status_strings[status_res], info);
				}else{
					block_SIGCHLD();
					add_job(bgStopped, new_job(pid_fork, args[0],  BACKGROUND)); 
					unblock_SIGCHLD();
					printf("Background job running... pid: %d, command: %s\n", pid_fork, args[0]);
				}
			}
	} // end while
}
