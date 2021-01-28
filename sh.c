#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <err.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <glob.h>

enum{
	MAXBUFFER=1024,
	MAXARGS=128,
	MAXCMDS=64,
	MAXTOKENS = 64,
	MAXSTRING = 256,
};

struct CommandLine{
	char *cmd;
	char *args[MAXARGS];
};
typedef struct CommandLine CommandLine;

struct Tshell{
	char *fin;
	char *fout;
	int bg;
	int ncmds;
	int here;
	CommandLine *cmds;
};
typedef struct Tshell Tshell;

int
split(char *s, char *delim,int max,char *tokens[]){
	int n=0;
	char *token;
	char *rest;
	
	while (((token=strtok_r(s,delim,&rest))!=NULL) && (n<max)){
		tokens[n] = token;
		n++;
		s = rest;
	}
	return n;	
}

char *
read_cmds(char buff[]){
	char *status;

	status = fgets(buff, MAXBUFFER, stdin);
	return status;
}

void
get_args(char *args[], int nargs){
	int i;
	char *aux;
	
	for(i=0;i<nargs;i++){
		aux = args[i];
		if(args[i][0] == '$')
			args[i] = getenv(++aux);
	}
}

void
split_args(CommandLine *cmds, int ncmds, char* tokens[]){
	int i;
	int n;
	
	for(i=0;i<(ncmds);i++){
		n = split(tokens[i], " \t", MAXCMDS, cmds[i].args);
		get_args(cmds[i].args, n);
		cmds[i].args[n] = NULL;
	}
}

void
parse_cmds(char *s, Tshell *shell){
	char *tokens[MAXCMDS];

	shell->ncmds = split(s, "|\n", MAXCMDS, tokens);

	shell->cmds = malloc(sizeof(CommandLine)*(shell->ncmds));
	
	split_args(shell->cmds, shell->ncmds, tokens);
}

char *
checkpath(char *cmd){
	char *path;
	int i;
	int n;
	char *tokens[MAXTOKENS];
	char cmdaux[MAXSTRING];
	char *cmdpath;
	
	path = getenv("PATH");
	if(path == NULL)
		return NULL;

	if(access(cmd, X_OK)==0){
		cmdpath = strdup(cmd);
		return cmdpath;
	}
	
	n = split(path, ":", MAXTOKENS, tokens);
	for(i=0;i<n;i++){
		snprintf(cmdaux, MAXSTRING, "%s/%s", tokens[i], cmd);
		if(access(cmdaux, X_OK)==0){
			cmdpath = strdup(cmdaux);
			return cmdpath;
		}
	}
	return NULL;
}

void
exec_cmd(char *path, char *cmd[]){
	execv(path, cmd);
	errx(EXIT_FAILURE, "%s: command not found", cmd[0]);
}

void
exec_cd(char *cmd[]){
	char *home;

	if(cmd[1] == NULL){
		home = getenv("HOME");
		if(home == NULL)
			fprintf(stderr, "error in home");
		
		if(chdir(home) < 0)
			fprintf(stderr, "error in chdir (path: %s)\n", home);
	}else{
		if(chdir(cmd[1]) < 0)
			fprintf(stderr, "error in chdir (path: %s)\n", cmd[1]);
	}
}

int **
allocate_pipes(int ncmds){
	int i;
	int **p;
	
	p = malloc(sizeof(int *) * ncmds);
	for(i=0;i<ncmds-1;i++){
		p[i] = malloc(sizeof(int) * 2);
		pipe(p[i]);
	}

	return p;
}

void
set_pipe(int **p, int ncmds, int i){
	if(i == 0)
		dup2(p[0][1], 1);
	else if(i == ncmds-1)
		dup2(p[i-1][0], 0);
	else{
		dup2(p[i-1][0], 0);
		dup2(p[i][1], 1);
	}
}

void
close_pipes(int **p, int ncmds){
	int i;

	for(i=0;i<ncmds-1;i++){
		close(p[i][0]);
		close(p[i][1]);
		free(p[i]);
	}	
}

void
set_fin(Tshell *shell, int i){
	int fd;
	
	if((shell->fin != NULL) && (i == 0)){
		fd = open(shell->fin, O_RDONLY);
		if(fd < 0)
			err(EXIT_FAILURE, "open");
		dup2(fd, 0);
		close(fd);
	}
	
	if((shell->bg) && (shell->fin == NULL) && (i == 0)){
		fd = open("/dev/null", O_RDONLY);
		if(fd < 0)
			err(EXIT_FAILURE, "open");
		dup2(fd, 0);
		close(fd);
	}
}

void
set_fout(Tshell *shell, int i){
	int fd;

	if((shell->fout != NULL) && (i == shell->ncmds - 1)){
		fd = creat(shell->fout, 0664);
		if(fd < 0)
			err(EXIT_FAILURE, "creat");
		dup2(fd, 1);
		close(fd);
	}
}

void
set_fds(Tshell *shell, int i){
		set_fin(shell, i);
		set_fout(shell, i);
}

pid_t *
allocate_pids(int bg, int ncmds){
	pid_t *p;
	
	p = malloc(sizeof(pid_t) * ncmds);

	return p;
}

int
append_glob_path(char *arr[], char **args, int n, int j){
	int i;
	
	for(i=0;i<n;i++){
		arr[j] = strdup(args[i]);
		j++;
	}
	return j;
}

void
copy_glob_args(char *args[], char **gl_args){
	int i = 0;
	
	while(gl_args[i] != NULL){
		args[i] = gl_args[i];
		i++;
	}
}

void
check_globbing(char *argv[]){
	glob_t globbuf;
	char *argvf[MAXBUFFER];
	int i;
	int n;
	
	i=0;
	n=0;
	
	while(argv[i]!= NULL){
			if((glob(argv[i] , GLOB_DOOFFS , NULL,&globbuf))!=0){
				argvf[n]=argv[i];
				n++;
			}
			else {
				n = append_glob_path(argvf, globbuf.gl_pathv, globbuf.gl_pathc, n);
			}
			i++;
		}
	
	copy_glob_args(argv,argvf);
	
}

void
set_pipehere(Tshell *shell, int phere[], int i){
	if((shell->here) && (i == 0))
		dup2(phere[0], 0);

	if(shell->here){
		close(phere[0]);
		close(phere[1]);
	}
}

void
read_here(int phere[]){
	char aux[MAXBUFFER];
	
	while(fgets(aux, MAXBUFFER, stdin) != NULL){
		if(strcmp(aux, "}\n") == 0)
			break;
		write(phere[1], aux, strlen(aux));
	}
	close(phere[1]);
	close(phere[0]);
}

pid_t *
throw_forks(Tshell *shell){
	int i;
	int **p;
	pid_t *fg_pids;
	int phere[2];
	
	fg_pids = allocate_pids(shell->bg, shell->ncmds);

	//PIPES
	if(shell->ncmds > 1)
		p = allocate_pipes(shell->ncmds);
	
	//PIPES HERE
	if(shell->here)
		pipe(phere);
	
	for(i=0;i<shell->ncmds;i++){
		fg_pids[i] = fork();
		switch(fg_pids[i]){
			case -1:
				errx(EXIT_FAILURE, "fork");
			case 0:
				//Set pipehere
				set_pipehere(shell, phere, i);
				
				if(shell->ncmds > 1){
					set_pipe(p, shell->ncmds, i);
					close_pipes(p, shell->ncmds);
				}
				/*
				 *	Non Built-In cmds
				*/
				
				//Globbing
				check_globbing(shell->cmds[i].args);
				
				shell->cmds[i].cmd = checkpath(shell->cmds[i].args[0]);
				set_fds(shell, i);
				
				exec_cmd(shell->cmds[i].cmd, shell->cmds[i].args);
		}
	}
	if(shell->ncmds > 1)
		close_pipes(p, shell->ncmds);
	if(shell->here)
		read_here(phere);

	return fg_pids;
}

void
save_sts(int sts){
	char sts_string[MAXBUFFER];
	
	if(WIFEXITED(sts)){
		snprintf(sts_string, MAXBUFFER, "%d", WEXITSTATUS(sts));
		setenv("result", sts_string, 1);
	}
}

void
wait_forks(int ncmds, int bg, pid_t *fg_pids){
	int i;
	int sts;

	if(!bg){
		for(i=0;i<ncmds;i++){
			waitpid(fg_pids[i], &sts, 0);
			save_sts(sts);
		}
	}
}

void
exec_cmds(Tshell *shell){
	pid_t *fg_pids;

	fg_pids = throw_forks(shell);
	wait_forks(shell->ncmds, shell->bg, fg_pids);
	free(fg_pids);
}

void
copy_args(Tshell *shell, char *args[]){
	int i = 0;
	
	while(args[i+1] != NULL){
		shell->cmds[0].args[i] = args[i+1];
		i++;
	}
	shell->cmds[0].args[i] = NULL;
}

void
exec_ifok(Tshell *shell, char *args[]){
	char *result;
	
	copy_args(shell, args);
	result = getenv("result");
	
	if(result != NULL){
		if(strcmp(result, "0") == 0)
			exec_cmds(shell);
	}else
		fprintf(stderr,"ERROR: var result not initialized\n");


}

void
exec_ifnot(Tshell *shell, char *args[]){
	char *result_str;
	int result;
	
	copy_args(shell, args);
	result_str = getenv("result");
	
	if(result_str != NULL){
		result = atoi(result_str);
		if(result != 0)
			exec_cmds(shell);
	}else
		fprintf(stderr,"ERROR: var result not initialized\n");
}

void
check_cmds(Tshell *shell){
	if(strcmp(shell->cmds[0].args[0], "cd") == 0){
		// Built-In cd
		exec_cd(shell->cmds[0].args);
	}else if(strcmp(shell->cmds[0].args[0], "ifok") == 0){
		exec_ifok(shell, shell->cmds[0].args);
	}else if(strcmp(shell->cmds[0].args[0], "ifnot") == 0){
		exec_ifnot(shell, shell->cmds[0].args);
	}else{
		exec_cmds(shell);
	}
}

char *
rm_blank(char *f){
	char *tokens[1];

	split(f, " \n\t", 1, tokens);
	return tokens[0];
}

char *
check_env(char *f){
	char *aux;
	
	aux = f;
	if(f[0] == '$')
		return getenv(++aux);
	return f;
}

void 
check_io(Tshell *shell, char buffer[]){
	int n;
	char *tokens[MAXCMDS];
	char *tokens2[MAXCMDS];
	
	n = split(buffer, ">", MAXCMDS, tokens);
	if(n == 2){
		n = split(tokens[1], "<", MAXCMDS, tokens2);
		if(n == 2){
			shell->fout = tokens2[0];
			shell->fin = tokens2[1];
		}else
			shell->fout = tokens2[0];
	}

	n = split(buffer, "<", MAXCMDS, tokens);
	if(n == 2)
		shell->fin = tokens[1];

	//SUBPROGRAMA
	if(shell->fin != NULL){
		shell->fin = rm_blank(shell->fin);
		shell->fin = check_env(shell->fin);
	}
	if(shell->fout != NULL){
		shell->fout = rm_blank(shell->fout);
		shell->fout = check_env(shell->fout);
	}
}

int
check_env_var(char buffer[]){
	int n = 0;
	char *tokens[MAXCMDS];
	
	n = split(buffer, "=\n", MAXCMDS, tokens);
	if(n==2){
		setenv(tokens[0], tokens[1], 1);
		return 1;
	}
	return 0;
}

void
rm_char(char *p){
	*p = '\0';
}

void
check_background(Tshell *shell, char buffer[]){
	char *p;
	
	 shell->bg = ((p = strstr(buffer, "&")) != NULL);
	 if(shell->bg)
		rm_char(p);
}

void
check_here(Tshell *shell, char buffer[]){
	char *p;
		
	if((!shell->bg) && (shell->fin == NULL) && (shell->fout == NULL))
		shell->here = ((p = strstr(buffer, "HERE{")) != NULL);
	if(shell->here)
		rm_char(p);
}

void
handle_zombie_proc(){
	waitpid(-1, NULL, WNOHANG);
}

void
init_shell(Tshell *shell){
	shell->fin = NULL;
	shell->fout = NULL;
	shell->bg = 0;
	shell->ncmds = 0;
	shell->here = 0;
}

int
main(int argc, char *argv[]){
	Tshell shell;
	char buffer[MAXBUFFER];
	
	signal(SIGCHLD, handle_zombie_proc);

	while(1){
		init_shell(&shell);
		
		// Comprobar que la ultima entrada ha sido de stdin para pintar el prompt
		if(isatty(0))
		printf("$ ");

		if(read_cmds(buffer)==NULL)
			break;
		
		if(!((check_env_var(buffer)) || (buffer[0] == '\n'))){
			//Comprobar si hay background
			check_background(&shell, buffer);
				
			//Comprobar si hay redirecciones
			check_io(&shell, buffer);
			
			//Comprobar si hay HERE
			check_here(&shell, buffer);
			
			parse_cmds(buffer, &shell);
			check_cmds(&shell);
			free(shell.cmds);
		}
	}
	
	exit(EXIT_SUCCESS);
}
