/**
 * mysh.c
 * Written by Ricardo Ruiz
 * 
 * A terminal program that provides basic functionality
 * for file system navigation and execution of programs
 * in the foreground and background.
 * 
 * Installation:
 * 1. Download mysh.c
 * 2. Place it in a folder and navigate to that folder in a terminal.
 * 3. Use whatever C compiler that is available. (gcc, clang, etc.)
 * 4. Run executable and enjoy!
 * **/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>

// TYPES
typedef struct HISTORY {
  char *cmd;
  char **params;
  int numParams;
} HISTORY;

typedef struct LUT{
  int type;
  char val[12];
} LUT;

// CONSTANTS
enum  {
  movetodir, whereami, history,
  byebye, replay, start, background,
  dalek, repeat, dalekall
};

LUT LOOKUP[] = {
  {movetodir, "movetodir"},
  {whereami, "whereami"},
  {history, "history"},
  {byebye, "byebye"},
  {replay, "replay"},
  {start, "start"},
  {background, "background"},
  {dalek, "dalek"},
  {repeat, "repeat"},
  {dalekall, "dalekall"}
};

// FUNCTIONS
size_t string_parser( const char *input, char ***word_array);
void runChild(char **word_array);
void *run(void *ptr);
void pushPID(pid_t pid);
void getHistory();
void writeHistory();
void pushHistory(char** word_array, size_t n);
void freeHistory();
void printHistory();
void pushPath(char **dest, char *source);

// COMMAND FUNCTIONS
void historyCommand(int index);
void movetodirCommand(int index);
void replayCommand(int index);
void startCommand(int index);
void backgroundCommand(int index);
void dalekCommand(int index);

// GLOBAL VARIABLES
int EXIT = 0; // EXIT variable to be set when 'byebye' is executed.
int SIZE = 100;
int IDX = -1;
HISTORY *hist; // History variable
int pidSIZE = 0;
int pidLAST =-1;
char *currentdir; // Current working directory (full path).
pid_t *running; // Holds current running background programs.

int main(){
  getHistory();
  currentdir = getcwd(NULL, 0);

  while(!EXIT){
    // INITIALIZE INPUT VARIABLES
    char* buf; // Initial input
    char** word_array = NULL; // Input split by " "
    size_t size = 1024; // Arbitrary size value
    buf = malloc(size);

    // GET USER INPUT
    printf("# "); // Prompt
    getline(&buf, &size, stdin); // Retrieves line from input.

    // PARSE INPUT AND PUSH TO HISTORY
    size_t n = string_parser(buf, &word_array);
    pushHistory(word_array, n);

    runChild(word_array); // Run command in child pthread.

    for(int i = 0; i < n; i++){
      free(word_array[i]);
    }
    free(word_array);
  }
  writeHistory();
  freeHistory();
  free(currentdir);

  return 0;
}

// Creates pthread to run command off of.
void runChild(char** word_array){
  pthread_t pid;
  int status;
  
  pthread_create(&pid, NULL, run, (void *)IDX);

  pthread_join(pid, NULL);
}

// Runs commands.
void *run(void *ptr){
  int index = (int) ptr;

  if(strcmp(hist[index].cmd, LOOKUP[byebye].val) == 0){
    EXIT = 1;
  }else if(strcmp(hist[index].cmd, LOOKUP[history].val) == 0){
    historyCommand(index);
  }else if(strcmp(hist[index].cmd, LOOKUP[whereami].val) == 0){
    printf("%s\n", currentdir);
  }else if(strcmp(hist[index].cmd, LOOKUP[movetodir].val) == 0){
    movetodirCommand(index);
  }else if(strcmp(hist[index].cmd, LOOKUP[replay].val) == 0){
    replayCommand(index);
  }else if(strcmp(hist[index].cmd, LOOKUP[start].val) == 0){
    startCommand(index);
  }else if(strcmp(hist[index].cmd, LOOKUP[background].val) == 0){
    backgroundCommand(index);
  }else if(strcmp(hist[index].cmd, LOOKUP[dalek].val) == 0){
    dalekCommand(index);
  }else{
    printf("Command '%s' does not exist.\n", hist[index].cmd);
  }

  return NULL;
}

// History Command
void historyCommand(int index){
  if(hist[index].numParams > 0){
    if(strcmp(hist[index].params[0], "-c") == 0){
      // freeHistory(); TODO: FIX THIS SO YOU DON'T have dead memory.
      IDX = -1;
    }else{
      printf("Invalid arguments for history. Did you mean -c?\n");
    }
  }else{
    printHistory();
  }
}

// Movetodir Command
void movetodirCommand(int index){
  if(hist[index].numParams == 0){
    printf("Invalid arguments for movetodir. Make sure to include a directory.\n");
    return;
  }

  // Go back directory.
  if(!strcmp(hist[index].params[0], "..") || !strcmp(hist[index].params[0], "../")){
    int lastSeen = -1;
    for(int i = 0; i < strlen(currentdir); i++){
      if(currentdir[i] == '/'){
        lastSeen = i;
      }
    }

    if(lastSeen == -1){
      printf("No previus directory found.\n");
      return;
    }

    currentdir[lastSeen] = '\0';
    return;
  }

  // Open directory
  DIR* dir = opendir(hist[index].params[0]);

  if(dir){ // Directory Exists.
    pushPath(&currentdir, hist[index].params[0]);
  }else if(ENOENT == errno){ // Directory not found.
    printf("Directory '%s' does not exist.\n", hist[index].params[0]);
  }else{ // Random errir.
    printf("Failed to open directory, please try again.\n");
  }
}

// Replay History command.
void replayCommand(int index){
  if(hist[index].numParams == 0){
    printf("Please provide the number of the command to replay.");
    return;
  }else{
    for(int i = 0; i < strlen(hist[index].params[0]); i++){
      if(!isdigit(hist[index].params[0][i])){
        printf("Please enter a valid number.");
        return;
      }
    }
  }

  int newIDX = atoi(hist[index].params[0]);

  if(newIDX > IDX){
    printf("Number provided is out of range.\n");
    return;
  }

  run((void*)  (index - newIDX - 1));
}

// Start program command
void startCommand(int index){
  pid_t pid;
  int status;

  if(hist[index].numParams == 0){
    printf("Please provide a program to run.\n");
    return;
  }else if(hist[index].params[0][0] != '/'){
    pushPath(&hist[index].params[0], hist[index].params[0]);;
  }

  if((pid = fork()) < 0){
    printf("Failed to fork().\n");
  }else if(pid == 0){
    execvp(hist[index].params[0], hist[index].params);
    printf("Failed to run '%s\n'", hist[index].params[0]);
    exit(errno);
  }else do{
    if((pid = waitpid(pid, &status, WNOHANG)) == -1){ // Wait error
      printf("Failed to wait().");
    }else if(pid == 0){ // Child is still running
      sleep(1);
    }

  } while(pid == 0);
}

// Start background program command.
void backgroundCommand(int index){
  pid_t pid;
  int status;

  if(hist[index].numParams == 0){
    printf("Please provide a program to run.\n");
    return;
  }else if(hist[index].params[0][0] != '/'){
    pushPath(&hist[index].params[0], hist[index].params[0]);;
  }

  if((pid = fork()) < 0){
    printf("Failed to fork().\n");
  }else if(pid == 0){
    execvp(hist[index].params[0], hist[index].params);
    printf("Failed to run '%s'\n", hist[index].params[0]);
    exit(errno);
  }else{
    printf("%d\n", pid);
    pushPID(pid);
  }
}

// Kill Process command.
void dalekCommand(int index){
  if(hist[index].numParams == 0){
    printf("Please provide a PID to terminate.\n");
    return;
  }else{
    for(int i = 0; i < strlen(hist[index].params[0]); i++){
      if(!isdigit(hist[index].params[0][i])){
        printf("Please enter a valid PID.");
        return;
      }
    }
  }

  pid_t pid = atoi(hist[index].params[0]);

  int res = kill(pid, SIGKILL);

  if(res == -1){
    if(errno == ESRCH){
      printf("Process %d not found.\n", pid);
    }
    return;
  }

  int status;
  
  do{
    if ((pid = waitpid(pid, &status, WNOHANG)) == -1)
      perror("wait() error");
    else if (pid == 0) {
      sleep(1);
    }
    else {
      if (WIFEXITED(status))
        printf("child exited with status of %d\n", WEXITSTATUS(status));
      else printf("Process: %d terminated successfully.\n", pid);
    }
  } while(pid == 0);
}

void pushPath(char **dest, char *source){
  char *newPath = malloc((strlen(currentdir) + strlen(source) + 1) * sizeof(char));
    
    strcpy(newPath, currentdir);
    
    char slash[] = "/";

    strcat(newPath, slash);
    strcat(newPath, source);

    *dest = realloc(*dest, strlen(newPath) * sizeof(char));
    strcpy(*dest, newPath);
    
    free(newPath);
}

// Puts pid from background into global array to keep track of running processes.
void pushPID(pid_t pid){
  if(running == NULL){
    pidSIZE = 10;
    running = malloc(sizeof(pid_t) * pidSIZE);
  }else if(pidLAST == pidSIZE - 1){
    pidSIZE *= 2;
    running = realloc(running, sizeof(pid_t) * pidSIZE);
  }

  running[++pidLAST] = pid;
}

// Parses history from .txt file.
void getHistory(){
  FILE *ptr;
  ptr = fopen("history.txt", "r");

  if(ptr == NULL){
    hist = malloc(sizeof(HISTORY) * SIZE);
    return;
  }

  // Get number of commands stored
  int x;
  fscanf(ptr, "%d", &x);

  // Allocate memory for history array
  SIZE = (x + 1) * 2;
  hist = malloc(sizeof(HISTORY) * (SIZE));

  for(int i = 0; i < x; i++){
    int len;
    fscanf(ptr, "%d", &len);

    hist[i].cmd = malloc(sizeof(char) * (len));
    fscanf(ptr, "%s", hist[i].cmd);

    // Get number of parameters and allocate
    fscanf(ptr, "%d", &hist[i].numParams);
    hist[i].params = malloc(sizeof(char*) * hist[i].numParams);

    // Scan parameters
    for(int j = 0; j < hist[i].numParams; j++){
      fscanf(ptr, "%d", &len);

      hist[i].params[j] = malloc(sizeof(char) * (len));
      fscanf(ptr, "%s", hist[i].params[j]);
    }
  }

  IDX = --x;

  return;
}

// Writes history to external file.
void writeHistory(){
  FILE *ptr;
  ptr = fopen("history.txt", "w");

  fprintf(ptr, "%d\n", ++IDX);

  for(int i = 0; i < IDX; i++){
    fprintf(ptr, "%lu %s %d", strlen(hist[i].cmd), hist[i].cmd, hist[i].numParams);

    for(int j = 0; j < hist[i].numParams; j++){
      fprintf(ptr, " %lu %s", strlen(hist[i].params[j]), hist[i].params[j]);
    }

    fprintf(ptr, "\n");
  }

  fclose(ptr);
  return;
}

// Adds an item to a history.
void pushHistory(char** word_array, size_t n){
  IDX++;
  n--;

  if(hist == NULL){
    hist = malloc(sizeof(HISTORY) * (SIZE + 1));
  }

  if(IDX == SIZE){
    hist = realloc(hist, sizeof(HISTORY) * ((SIZE + 1) * 2));
  }

  hist[IDX].numParams = n; // Set num parameters
  
  // Copy first command to command
  hist[IDX].cmd = malloc(strlen(word_array[0]) * sizeof(char));
  strcpy(hist[IDX].cmd, word_array[0]); 

  // Allocate and copy parameters
  hist[IDX].params = malloc((n) * sizeof(char*));
  for(int i = 0; i < n; i++){
    hist[IDX].params[i] = malloc(strlen(word_array[i + 1]) * sizeof(char));
    strcpy(hist[IDX].params[i], word_array[i + 1]);
  }

  return;
}

// Frees memory allocated for history.
void freeHistory(){
  for(int i = 0; i < SIZE; i++){
    free(hist[i].cmd);
    for(int j = 0; j < hist[i].numParams; j++){
      free(hist[i].params[j]);
    }
  }
  free(hist);
}

// Prints history in reverse order.
void printHistory(){
  if(IDX < 0){
    printf("No history to print.\n");
    return;
  }

  int temp = 0;

  for(int i = IDX; i >= 0; i--){
    printf("%d: %s", temp++, hist[i].cmd);
    for(int j = 0; j < hist[i].numParams; j++){
      printf(" %s", hist[i].params[j]);
    }
    printf("\n");
  }

  return;
}

// Parses input string into a words array.
size_t string_parser( const char *input, char ***word_array) 
{
    size_t n = 0;
    const char *p = input;

    while ( *p )
    {
        while ( isspace( ( unsigned char )*p ) ) ++p;
        n += *p != '\0';
        while ( *p && !isspace( ( unsigned char )*p ) ) ++p;
    }

    if ( n )
    {
        size_t i = 0;

        *word_array = malloc( n * sizeof( char * ) ); 

        p = input;

        while ( *p )
        {
            while ( isspace( ( unsigned char )*p ) ) ++p;
            if ( *p )
            {
                const char *q = p;
                while ( *p && !isspace( ( unsigned char )*p ) ) ++p;

                size_t length = p - q;

                ( *word_array )[i] = ( char * )malloc( length + 1 );

                strncpy( ( *word_array )[i], q, length );
                ( *word_array )[i][length] = '\0';

                ++i;
            }
        }           
    }

    return n;
}  
