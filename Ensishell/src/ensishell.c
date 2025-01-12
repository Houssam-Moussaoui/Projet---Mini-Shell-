/*****************************************************
 * Copyright Grégory Mounié 2008-2015                *
 *           Simon Nieuviarts 2002-2009              *
 * This code is distributed under the GLPv3 licence. *
 * Ce code est distribué sous la licence GPLv3+.     *
 *****************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 
#include "variante.h"
#include "readcmd.h"

#include <sys/time.h>
#include <signal.h>


#ifndef VARIANTE
#error "Variante non défini !!"
#endif

/* Guile (1.8 and 2.0) is auto-detected by cmake */
/* To disable Scheme interpreter (Guile support), comment the
 * following lines.  You may also have to comment related pkg-config
 * lines in CMakeLists.txt.
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

typedef struct job{
	pid_t pid;
	char * commande;
	struct job *next_job;
	int date_exec;
}job;

job * jobs=NULL;


job * find_job(pid_t pid){
	job * current=jobs;
	while(current){
		if(current->pid==pid){
			return current;
		}
		current=current->next_job;
	}
	return NULL;
}
void handler(int sig, siginfo_t *info, void *context) {
    
    job* job1 = find_job(info->si_pid);

    if (job1) {
		struct timeval fin;
        gettimeofday(&fin, NULL);
        printf("Execution de la commande : %i--> %s est terminé en %li secondes\n", job1->pid,job1->commande, fin.tv_sec - job1->date_exec);
    }
}






pid_t execute(struct cmdline* line,int index_commande,int nombre){
    pid_t pid = fork();  

    if (pid == 0) {
		if(nombre==1){

			if(line->in){
				int file_read=open(line->in,O_RDONLY  , 0644 );
				dup2(file_read , STDIN_FILENO);
				close(file_read); 
			}

			if(line->out){
				int file_write= open(line->out,O_CREAT|O_WRONLY|O_TRUNC, 0644);
				dup2(file_write,STDOUT_FILENO);
				close(file_write); 
			}
			execvp(line->seq[index_commande][0],line->seq[index_commande]);
		}else{

			int pipefds1[nombre-1][2];
			for (int i=0;i<nombre-1;i++){
				pipe(pipefds1[i]);
			}


			for (int i=0; i<nombre; i++){
				pid_t pid2=fork();

				if (pid2==0){


					if(line->in && i==0){
						int file_read=open(line->in,O_RDONLY  , 0644 );
						dup2(file_read , STDIN_FILENO);
						close(file_read); 
					}

					if(line->out && i==nombre-1){
						int file_write= open(line->out,O_CREAT|O_WRONLY|O_TRUNC, 0644);
						dup2(file_write,STDOUT_FILENO);
						close(file_write); 
					}

					if(i>0){
				
						dup2(pipefds1[i-1][0],STDIN_FILENO);
					}
					
					if (i<nombre-1){
						dup2(pipefds1[i][1],STDOUT_FILENO);
					}

					for(int j=0;j<nombre-1;j++){
						close(pipefds1[j][0]);
						close(pipefds1[j][1]);
					}
					execvp(line->seq[index_commande+i][0],line->seq[index_commande+i]);
				}

			}
			for(int i=0;i<nombre-1;i++){
				close(pipefds1[i][0]);
				close(pipefds1[i][1]);
			}

			for (int i = 0; i < nombre; i++) {
    			wait(NULL);
			}
		}
    } else {
		if(!line->bg){
			waitpid(pid,NULL,0);
		}
    }
	return pid;

}



#if USE_GUILE == 1
#include <libguile.h>

int question6_executer(char *line)
{
	/* Question 6: Insert your code to execute the command line
	 * identically to the standard execution scheme:
	 * parsecmd, then fork+execvp, for a single command.
	 * pipe and i/o redirection are not required.
	 */
	printf("Not implemented yet: can not execute %s\n", line);

	/* Remove this line when using parsecmd as it will free it */
	free(line);
	
	return 0;
}

SCM executer_wrapper(SCM x)
{
        return scm_from_int(question6_executer(scm_to_locale_stringn(x, 0)));
}
#endif


void terminate(char *line) {
#if USE_GNU_READLINE == 1
	/* rl_clear_history() does not exist yet in centOS 6 */
	clear_history();
#endif
	if (line)
	  free(line);
	free_all_jobs();
	printf("exit\n");
	exit(0);
}
void free_job(job * job){
	free(job->commande);
	free(job);
}
void print_jobs(){

	job * current_job=jobs;
	job * init_job=malloc(sizeof(job));
	job * previous_job=init_job;
	previous_job->next_job=current_job;
	
	while(current_job){
		int status;
		pid_t result = waitpid(current_job->pid, &status, WNOHANG);
		if(result==0){
			//le processus est encore en execution
			printf("%i-->%s\n",current_job->pid,current_job->commande);
			previous_job=current_job;
			current_job=current_job->next_job;
		}else{
			//le processus ne s'execute plus
			previous_job->next_job=current_job->next_job;
			free_job(current_job);
			current_job=previous_job->next_job;
		}
	}
	//printf("done\n");
	free(init_job);
	
}

void free_all_jobs(){
	job * current=jobs;
	job * next;
	while(current){
		next=current->next_job;
		free_job(current);
		current=next;
	}
}

void save_job(pid_t pid,char* line,int date){
	job * new_job=malloc(sizeof(job));
	new_job->pid=pid;
	new_job->commande=line; 
	new_job->date_exec=date;
	new_job->next_job=jobs;

	jobs=new_job;
}



int main() {
	int i,j;

        printf("Variante %d: %s\n", VARIANTE, VARIANTE_STRING);

#if USE_GUILE == 1
        scm_init_guile();
        /* register "executer" function in scheme */
        scm_c_define_gsubr("executer", 1, 0, 0, executer_wrapper);
#endif

	while (1) {
		struct cmdline *l;
		char *line=0;
		
		char *prompt = "ensishell>";

		/* Readline use some internal memory structure that
		   can not be cleaned at the end of the program. Thus
		   one memory leak per command seems unavoidable yet */
		line = readline(prompt);


		if (line == 0 || ! strncmp(line,"exit", 4)) {
			terminate(line);
		}
		
		if(! strncmp(line,"jobs", 4)){
			add_history(line);
			print_jobs();
			continue;
		}
		char * line_copy_for_jobs=malloc(strlen(line));
		strcpy(line_copy_for_jobs,line);



#if USE_GNU_READLINE == 1
		add_history(line);
#endif


#if USE_GUILE == 1
		/* The line is a scheme command */
		if (line[0] == '(') {
			char catchligne[strlen(line) + 256];
			sprintf(catchligne, "(catch #t (lambda () %s) (lambda (key . parameters) (display \"mauvaise expression/bug en scheme\n\")))", line);
			scm_eval_string(scm_from_locale_string(catchligne));
			free(line);
                        continue;
                }
#endif

		/* parsecmd free line and set it up to 0 */
		l = parsecmd( & line);

		/* If input stream closed, normal termination */
		if (!l) {
		  
			terminate(0);
		}
		
		
		if (l->err) {
			/* Syntax error, read another command */
			printf("error: %s\n", l->err);
			continue;
		}
		

		int nombre=0;
		for (i=0; l->seq[i]!=0; i++) {
			nombre++;
		}
		
		if (l->in) printf("in: %s\n", l->in);
		if (l->out) printf("out: %s\n", l->out);
		if (l->bg) printf("background (&)\n");

		//Display each command of the pipe 
		
		for (i=0; l->seq[i]!=0; i++) {
			char **cmd = l->seq[i];
			printf("seq[%d]: ", i);
                        for (j=0; cmd[j]!=0; j++) {
                                printf("'%s' ", cmd[j]);
                        }
			
			printf("\n");
			
		}
		
		
		pid_t pid=execute(l,0,nombre);
		if(l->bg){
			struct timeval tv;
			gettimeofday(&tv, NULL);
			save_job(pid,line_copy_for_jobs,tv.tv_sec);
		    struct sigaction sa;
    		sa.sa_sigaction = handler;
    		sa.sa_flags = SA_SIGINFO | SA_NOCLDSTOP | SA_RESTART;
    		sigemptyset(&sa.sa_mask);
    		sigaction(SIGCHLD, &sa, NULL);
		}
		else{
			free(line_copy_for_jobs);
		}




	}

}

