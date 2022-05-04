    #include <sys/types.h>
    #include <stdio.h>
    #include <unistd.h>
    #include <wait.h>
    #include "shell.h"
    #include <sys/stat.h>
    #include <stdlib.h>
    #include <errno.h>
    #include <sys/wait.h>
    #include <string.h>
    #include <fcntl.h>
    #include <signal.h>
                  
    typedef struct s_process {
        int pid;
        int index;
        struct s_process * next;
    }process;

    typedef struct s_proc_list {
        process * head;
        process * tail;
        int count;
    }proc_list;


    char *infile, *outfile, *appfile;
    struct command cmds[MAXCMDS];
    char bkgrnd;
    char prompt[50];
    proc_list * bg_proc_list;
    process fg_proc;

    void write_process(process * proc) {
        if (proc == NULL) {
            printf("Process == NULL\n");
            return;
        }
        printf("proc->pid: %d\n proc->index: %d\n proc->next: %d\n \n", proc->pid, proc->index, proc->next);
    }

    void write_list(proc_list * list) {
        process * proc;

        if (list == NULL) {
            printf("list == NULL\n");
            return;
        }

        printf("list->head == NULL: %d\n list->tail == NULL: %d\n list->head == list->tail: %d\n list->count: %d\n", list->head == NULL, list->tail == NULL, list->head == list->tail, list->count);
        proc = list->head;
        while(proc != NULL) {
            write_process(proc);
            proc = proc->next;
        }
    }

    int process_setter(process * proc, int pid, int index, process * next) {

        if (proc == NULL) return -1;

        proc->pid = pid; 
        proc->index = index; 
        proc->next = next;
        return 0;
    }

    process * new_process(int pid, int index) {

        process * new_proc = (process *)malloc(sizeof(process));

        process_setter(new_proc, pid, index, NULL);
        
        return new_proc;
    }

    process * add_process(proc_list * list, int pid) {
        
        if (list == NULL) return NULL;

        process * new_proc = new_process(pid, list->count + 1);

        if (new_proc == NULL) return NULL;

        if (list->head == NULL) {
            list->head = new_proc;
            list->tail = new_proc;
            list->count += 1;

            return new_proc;
        }

        list->tail->next = new_proc;
        list->tail = list->tail->next;
        list->count += 1;

        return new_proc;
    }

    int find_proc(proc_list * list, int pid, int index, process ** result, process ** result_parent) {

        if (list == NULL) {
            *result = NULL;
            *result_parent = NULL;

            return -1;
        } 
        if (list->head == NULL) {
            *result = NULL;
            *result_parent = NULL;

            return -1;
        }

        process * prev = list->head;
        process * proc = list->head->next;

        if (pid != 0) {
            if (list->head->pid == pid) {
                *result = list->head;
                *result_parent = NULL;
                
                return 0;
            } 

            if (list->head->next == NULL) {
                if (list->head->pid != pid) {
                    *result = NULL;
                    *result_parent = NULL; 
                    
                    return 0;
                }
            } 

            while (proc != NULL) {
                if (proc->pid == pid) {
                    *result = proc;
                    *result_parent = prev;

                    return 0;
                }
                prev = proc;
                proc = proc->next;
            }
        }
        else if (index != 0) {
            if (list->head->index == index) {
                    *result = list->head;
                    *result_parent = NULL;
                    
                    return 0;
            } 
            if (list->head->next == NULL) {
                if (list->head->index != index) {
                    *result = NULL;
                    *result_parent = NULL; 
                    
                    return 0;
                }
            } 
            
            while (proc != NULL) {
                if (proc->pid == index) {
                    *result = proc;
                    *result_parent = prev;

                    return 0;
                }
                prev = proc;
                proc = proc->next;
            }
        }

        *result = NULL;
        *result_parent = NULL;

        return 0;   
    }

    int rm_process(proc_list * list, int rm_pid, int rm_index) {

        if (list == NULL) return -1;
        if (list->head == NULL) return -1;

        process * proc;
        process * parent;
        
        find_proc(list, rm_pid, rm_index, &proc, &parent);
        
        if (proc == NULL) return -1;
        
        if (parent == NULL) { // Means that proc is head

            list->head = list->head->next;

            free(proc);

            return 0;
        }

        if (proc->next == NULL) {
            list->tail = parent;
        }

        parent->next = proc->next;
        free(proc);
        
        return 0;
    }

    proc_list * new_proc_list(int pid) {
        
        proc_list * new_list = (proc_list *)malloc(sizeof(proc_list));
        
        new_list->head = new_process(pid, 1);
        new_list->tail = new_list->head;
        new_list->count = 1;

        return new_list;
    }
            
    void set_sig_disp();
    void set_child_sig_disp() ;
    void set_bg_child_sig_disp();
    void child_sigint_handler(int signum);
    void child_sigquit_handler(int signum);
    void sigstop_handler(int signum);
    void child_sigtstp_handler(int signum);
    void sigchld_handler(int signum);

    int write_bgp_info(int c_num, int pid, int index) {

        char bgp_info_message[1024];

        sprintf(bgp_info_message, "[%d] placed into background    %d %s\n", index, pid, cmds[c_num].cmdargs[0]); 
        write(STDERR_FILENO, bgp_info_message, strlen(bgp_info_message));

        return 0;
    }

    process * add_bg_proc(int pid) {
        if (bg_proc_list == NULL) {
            bg_proc_list = new_proc_list(pid);

            return bg_proc_list->head;
        }
        else {
            return add_process(bg_proc_list, pid);
        }
    }


    int bg_and_shell_reaction(int c_num, int pid) {
        process * proc;
        process * parent;

        if (tcsetpgrp(STDIN_FILENO, getpid()) == -1) {
            return -1;    
        }

        find_proc(bg_proc_list, pid, 0, &proc, &parent);

        if (proc == NULL) { //process with pid not in bg_list
            proc = add_bg_proc(pid);
            write_bgp_info(c_num, pid, proc->index);
            kill(-pid, SIGCONT);
        }
        
        return 0;
    }

    int fg(int pid) {

        int status;
        process_setter(&fg_proc, pid, 0, NULL);

        if (tcsetpgrp(STDIN_FILENO, pid) == -1) {
            return -1;    
        }
        kill(-pid, SIGCONT); 

        waitpid(pid, &status, WUNTRACED);

        tcsetpgrp(STDIN_FILENO, getpid());

        process_setter(&fg_proc, -1, 0, NULL);

        return status;
    }

    int fg_and_shell_reaction(int c_num, int pid) {

        int status;
        process * proc; 
        
        if ((status = fg(pid)) == -1) {
            perror("Placing into the foreground failed");

            return -1;
        }

        if (WIFSTOPPED(status)) {
            if (WSTOPSIG(status) == SIGTSTP) {
                bg_and_shell_reaction(c_num, pid);
            }
        } 

        if (WIFEXITED(status) == 0) { // Not terminated normally
            return WEXITSTATUS(status);
        }

        return 0;
    }

    int write_int(int i) {

        int delim = 1;
        char digit = '0';

        if (i == 0) write(STDOUT_FILENO, &digit, 1);
        while (i > delim) delim *= 10;
        while (delim > 1) {
            delim /= 10;
            digit = (i / delim) + '0';
            write(STDOUT_FILENO, &digit, 1);
            i %= delim;
        }
    }

    void ex_err() {

        perror("Execution error");
        exit(0);
    }

    int pipeline_length(int c_num, int ncmds) {

        int length = 1;
        
        if (cmds[c_num].cmdflag != OUTPIP) return length;
        
        for (int i = c_num + 1; i < ncmds; i++) {
            length++;

            if (cmds[i].cmdflag == INPIP) return length;
            if (cmds[i].cmdflag == 0) return -1;
                
        }
        
        return -1;
    }

    int input_redirection(int c_num, int ncmds) {
        int i_redirect_fd;

        if (c_num == 0) { 
            if (infile != NULL) {
                if ((i_redirect_fd = open(infile, O_RDONLY)) == -1) ex_err();
                if (dup2(i_redirect_fd, STDIN_FILENO) == -1) ex_err();
            }
        }
    }

    int output_redirection(int c_num, int ncmds) {
        int o_redirect_fd;

        if (c_num == ncmds -1) {  
            if (outfile != NULL) {
                if ((o_redirect_fd = open(outfile, O_WRONLY | O_CREAT, S_IREAD)) == -1) ex_err();
                if (dup2(o_redirect_fd, STDOUT_FILENO) == -1) ex_err();
            }
            else {
                if (appfile != NULL) {
                    if ((o_redirect_fd = open(appfile, O_WRONLY | O_APPEND | O_CREAT, S_IREAD)) == -1) ex_err();
                    if (dup2(o_redirect_fd, STDOUT_FILENO) == -1) ex_err();
                }
            }
        }
    }

    int shell_exex_command(int * c_num_addr, int ncmds) {

        int pid = 0;
        int index = 0;
        int status;
        int i_redirect_fd, o_redirect_fd;

        int c_num = *c_num_addr;

        if (strcmp(cmds[c_num].cmdargs[0], "cd") == 0) {
            if (chdir(cmds[c_num].cmdargs[1]) == -1) {

                return -1;
            }
            
            return 0;
        }
        else if (strcmp(cmds[c_num].cmdargs[0], "exit") == 0){
            exit(0);
        }
        else if (strcmp(cmds[c_num].cmdargs[0], "fg") == 0) {

            process * proc;
            process * parent;

            if (cmds[c_num].cmdargs[1] == NULL) {

                if (bg_proc_list == NULL) return -1;
                if (bg_proc_list->head == NULL) return -1;

                pid = bg_proc_list->head->pid;
                index = bg_proc_list->head->index;
            }
            else {
                index = atoi(cmds[c_num].cmdargs[1]);
                find_proc(bg_proc_list, 0, index, &proc, &parent);
                if (proc == NULL) return -1;
                pid = proc->pid;
            }

            rm_process(bg_proc_list, pid, index);

            return fg_and_shell_reaction(c_num, pid);
        }
        else if (strcmp(cmds[c_num].cmdargs[0], "bg") == 0) {
            process * proc;
            process * parent;
            
            if (cmds[c_num].cmdargs[1] != NULL) {

                index = atoi(cmds[c_num].cmdargs[1]);
                find_proc(bg_proc_list, 0, index, &proc, &parent);
                if (proc == NULL) return -1;
                pid = proc->pid;

                return bg_and_shell_reaction(c_num, pid);
            }
        }

        int length = pipeline_length(c_num, ncmds);

        if (length == -1) {
            perror("Incorrect pipeline");
            
            return -1;
        }
        *c_num_addr += length - 1;
        c_num = *c_num_addr;

        if ((pid = fork()) == -1) {
            return -1;
        }

        if (pid != 0) {

            setpgid(pid, pid);

            if (bkgrnd == 0) {  // PARENT REACTION
   
                return fg_and_shell_reaction(c_num, pid);
            }
            else {

                return bg_and_shell_reaction(c_num, pid);
            }
        }
        else {
            int forward_child_pid = getpid();    
            int fd[2], i_fd;

            int orig_in_fd = STDIN_FILENO;
            int orig_out_fd = STDOUT_FILENO;

            if (bkgrnd != 0)  {  // SET SIGNAL HANDLERS
                set_bg_child_sig_disp();
            }
            else {
                set_child_sig_disp();
            }

            setpgid(forward_child_pid, forward_child_pid);

            if (input_redirection(c_num - length + 1, ncmds) == -1) ex_err();

            if (length > 1) {
                for (int i = c_num - length + 1; i < c_num; i++) {
            
                    if(cmds[i].cmdflag & OUTPIP) {
                        if (pipe(fd) == -1) {
                            perror("Piping error");

                            exit(1);
                        }
                    }

                    if ((pid = fork()) == -1) {
                        perror("Fork failed");

                        return -1;
                    }

                    if (pid == 0) {
                        setpgid(getpid(), getpid());
                        setpgid(getpid(), forward_child_pid);

                        if (cmds[i].cmdflag & OUTPIP) {
                            if (dup2(fd[1], STDOUT_FILENO) == -1) ex_err();
                        }
                        if (cmds[i].cmdflag & INPIP) {
                            
                            if (dup2(i_fd, STDIN_FILENO) == -1) ex_err();
                            
                        }

                        close(fd[0]);
                        close(fd[1]);
                        close(i_fd);

                        if (execvp(cmds[i].cmdargs[0], cmds[i].cmdargs) == -1) {  // EXECUTION
                            ex_err();
                        }
                    }
                    setpgid(pid, pid);
                    setpgid(pid, forward_child_pid);
                    close(fd[1]);
                    close(i_fd);
                    i_fd = fd[0];
                }

                if (cmds[c_num].cmdflag & INPIP) {
                    if (dup2(i_fd, STDIN_FILENO) == -1) ex_err();
                }
                close(fd[0]);
                close(fd[1]);
                close(i_fd);
            }

            if (output_redirection(c_num, ncmds) == -1) ex_err();

            if (execvp(cmds[c_num].cmdargs[0], cmds[c_num].cmdargs) == -1) {  // EXECUTION
                ex_err();
            }
        }
    }

    int main(int argc, char *argv[]) {

        const char * DELIM_STRING = ": ";
        bg_proc_list = NULL;

        register int i;
        char line[1024];      /*  allow large command lines  */
        int ncmds;
         /* shell prompt */

        set_sig_disp();

        sprintf(prompt,"sukhinsky@%s: ", argv[0] + 2);

        while (promptline(prompt, line, sizeof(line)) > 0) {    /* until eof  */
                if ((ncmds = parseline(line)) <= 0)
                        continue;   /* read next line */

/*
            int i, j;
            for (i = 0; i < ncmds; i++) {
                for (j = 0; cmds[i].cmdargs[j] != (char *) NULL; j++)
                    fprintf(stderr, "cmd[%d].cmdargs[%d] = %s\n", i, j, cmds[i].cmdargs[j]);
                    fprintf(stderr, "cmds[%d].cmdflag = %o\n", i, cmds[i].cmdflag);
            }
*/

            for (int i = 0; i < ncmds; i++) {
                if (shell_exex_command(&i, ncmds) == -1) {
                    write(STDOUT_FILENO, cmds[i].cmdargs[0], strlen(cmds[i].cmdargs[0]));
                    write(STDOUT_FILENO, DELIM_STRING, strlen(DELIM_STRING));
                    perror("");
                    continue;
                }
            }
        }  /* close while */
    }

    void set_sig_disp() {

        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
    }

    void set_child_sig_disp() {

        signal(SIGTTOU, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGCHLD, SIG_IGN);

        struct sigaction act;

        sigaction(SIGINT, NULL, &act);
        act.sa_flags |= SA_RESTART;
        act.sa_handler = child_sigint_handler;
        sigaction(SIGINT, &act, NULL);

        sigaction(SIGQUIT, NULL, &act);
        act.sa_flags |= SA_RESTART;
        act.sa_handler = child_sigquit_handler;
        sigaction(SIGQUIT, &act, NULL);
    }

    void set_bg_child_sig_disp() {

        sigset_t mask_set;
        struct sigaction act;

        sigemptyset(&mask_set);
        sigaddset(&mask_set, SIGINT);
        sigaddset(&mask_set, SIGQUIT);
        sigprocmask(SIG_BLOCK, &mask_set, NULL);

        sigaction(SIGINT, NULL, &act);
        act.sa_mask = mask_set;
        sigaction(SIGINT, &act, NULL);

        sigaction(SIGQUIT, NULL, &act);
        act.sa_mask = mask_set;
        sigaction(SIGQUIT, &act, NULL);

        signal(SIGTTOU, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
    }

    void child_sigint_handler(int signum) {
        set_child_sig_disp(); 

        if (signum != SIGINT) return;

        write(STDOUT_FILENO, prompt, strlen(prompt));
        return;
    }

    void child_sigquit_handler(int signum) {
        set_child_sig_disp();

        if (signum != SIGQUIT) return;

        write(STDOUT_FILENO, prompt, strlen(prompt));
        return;
    }

