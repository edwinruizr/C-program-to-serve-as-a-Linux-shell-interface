#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>
#include <errno.h>

#define CMD_HISTORY_SIZE 10 // 10 most recently typed commands

static char * cmd_history[CMD_HISTORY_SIZE];
static int cmd_history_count = 0;

// this function helps with receiving and processing commands from the user
static void exec_cmd(const char * line)
{
  char * CMD = strdup(line);
  char *params[10];
  int argc = 0;

  // parse the command parameters
  params[argc++] = strtok(CMD, " ");
  while(params[argc-1] != NULL){// as long as you get a token
          params[argc++] = strtok(NULL, " ");
  }

  argc--; // Decrement argc to not take the null parameter as a token

  // backplane operation controls

  int background = 0;
  if(strcmp(params[argc-1], "&") == 0){
          background = 1; // set to work on backplane
          params[--argc] = NULL;  // delete "&" from params array
  }

  int fd[2] = {-1, -1};   // file descriptors for input and output

  while(argc >= 3){
    // routing parameter control
      if(strcmp(params[argc-2], ">") == 0){   // output
            // open file
            // link for open function parameters : http://pubs.opengroup.org/onlinepubs/7908799/xsh/open.html
            fd[1] = open(params[argc-1], O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP|S_IWGRP);
          if(fd[1] == -1){
                  perror("open");
                  free(CMD);
                  return;
          }
          // update parameter array
          params[argc-2] = NULL;
          argc -= 2;
        }else if(strcmp(params[argc-2], "<") == 0){ // input
                fd[0] = open(params[argc-1], O_RDONLY);
                if(fd[0] == -1){
                        perror("open");
                        free(CMD);
                        return;
                }
                params[argc-2] = NULL;
                argc -= 2;
        }else{
                break;
        }
    }
     int status;
     pid_t pid = fork();     // let's fork this!
     switch(pid){
            case -1:        // error :(
                     perror("fork");
                     break;
            case 0: // child :)
                 if(fd[0] != -1){        // if there is a redirect
                         if(dup2(fd[0], STDIN_FILENO) != STDIN_FILENO){
                                 perror("dup2");
                                 exit(1);
                         }
                 }
                 if(fd[1] != -1){        // if there is a redirect
                         if(dup2(fd[1], STDOUT_FILENO) != STDOUT_FILENO){
                                 perror("dup2");
                                 exit(1);
                         }
                 }
                 execvp(params[0], params);
                 perror("execvp");
                 exit(0);
            default: // parent
                 close(fd[0]);close(fd[1]);
                 if(!background)
                   waitpid(pid, &status, 0);
                   break;
       }
       free(CMD);
}

 // function that adds command to history

 static void add_to_history(const char * cmd){
    if(cmd_history_count == (CMD_HISTORY_SIZE-1)){  // if history is full
       int i;
       free(cmd_history[0]);   // first command in history

       // shift an index to other commands

       for(i=1; i < cmd_history_count; i++)
               cmd_history[i-1] = cmd_history[i];
       cmd_history_count--;
    }
   cmd_history[cmd_history_count++] = strdup(cmd); // add to command history
 }

  // function to execute a command from the history

static void run_from_history(const char * cmd){
   int index = 0;
   if(cmd_history_count == 0){
     printf("No commands in history!\n");
                    return ;
            }

            if(cmd[1] == '!') // second character '!'  get the index of the last command entered
                    index = cmd_history_count-1;
            else{
                    index = atoi(&cmd[1]) - 1;      // fetch from the user for the second character history index
                    if((index < 0) || (index > cmd_history_count)){ // If no such index in history, let 'em know
                            fprintf(stderr, "No such commands in history!\n");
                            return;
                    }
            }
            printf("%s\n", cmd);    // print whatever index command is entered
            exec_cmd(cmd_history[index]);   // lets execute that command from history
}

// prints history items from buffer
static void list_history(){
  int i;
  for(i=cmd_history_count-1; i >=0 ; i--){        // for each command in history print it out
  printf("%i %s\n", i+1, cmd_history[i]);
  }
}

// function for signal processing

static void signal_handler(const int rc){
        switch(rc){
            case SIGTERM:
            case SIGINT:
                    break;

            case SIGCHLD:
                     // * Wait for all dead processes and if finished in another part of a child program Use a non-blocking call to block the signal handler's operation * /
                    while (waitpid(-1, NULL, WNOHANG) > 0);
                    break;
        }
}

// main
int main(int argc, char *argv[]){
// grab the signals
struct sigaction act, act_old;
act.sa_handler = signal_handler;
act.sa_flags = 0;
sigemptyset(&act.sa_mask);
  if(     (sigaction(SIGINT,  &act, &act_old) == -1) || (sigaction(SIGCHLD,  &act, &act_old) == -1)){ // Ctrl^C
    perror("signal");
    return 1;
  }

// allocate buffer for line

size_t line_size = 100;
char * line = (char*) malloc(sizeof(char)*line_size);
  if(line == NULL){       //  error for malloc
    perror("malloc");
    return 1;
  }

int inter = 0; // flag for row fetches
  while(1){
    if(!inter)
      printf("osh > ");
    if(getline(&line, &line_size, stdin) == -1){    // read line to get input
      if(errno == EINTR){      // If errno is the error code
        clearerr(stdin); // clear error string
        inter = 1;       // set flag to 1
        continue;        // go back to the loop without going down
      }
      perror("getline");
      break;
    }
  inter = 0; // reset flag

  int line_len = strlen(line);
    if(line_len == 1){      // if only new line is entered
      continue;
    }
  line[line_len-1] = '\0'; // delete new line


    if(strcmp(line, "exit") == 0){ // exit the loop if the input line is "exit"
      break;
    }else if(strcmp(line, "history") == 0){ // call the list_history function if the input line is "history"
      list_history();
    }else if(line[0] == '!'){ // the first character of the entered line is "!" Call the function run_from_history
      run_from_history(line);
    }else{ // if not one of the cases above
      add_to_history(line); // call the add_to_history function to append to the input history
      exec_cmd(line); // call the exec_cmd function to get a new line
    }
  }

free(line);
return 0;
}
