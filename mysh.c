#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>

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
void getHistory();
void writeHistory();
void pushHistory(char** word_array, size_t n);
void freeHistory();
void printHistory();

// GLOBAL VARIABLES
int EXIT = 0;
int SIZE = 100;
int IDX = -1;
HISTORY *hist;
char *currentdir;

int main(){
  getHistory();
  currentdir = malloc(4 * sizeof(char));

  while(!EXIT){
    // INITIALIZE INPUT VARIABLES
    char* buf; // Initial input
    char** word_array = NULL; // Input split by " "
    size_t size = 1024;
    buf = malloc(size);

    // GET USER INPUT
    printf(">> ");
    getline(&buf, &size, stdin);

    // PARSE INPUT AND PUSH TO HISTORY
    size_t n = string_parser(buf, &word_array);
    pushHistory(word_array, n);

    if(strcmp(word_array[0], LOOKUP[byebye].val) == 0){
      EXIT = 1;
    }else if(strcmp(word_array[0], LOOKUP[history].val) == 0){
      if(word_array[1] != NULL){
        if(strcmp(word_array[1], "-c") == 0){
          IDX = -1;
        }else{
          printf("Invalid arguments for history. Did you mean -c?\n");
        }
      }else{
        printHistory();
      }
    }else if(strcmp(word_array[0], LOOKUP[whereami].val) == 0){
      printf("%s\n", currentdir);
    }else if(strcmp(word_array[0], LOOKUP[movetodir].val) == 0){
      if(word_array[1] == NULL){
        printf("Invalid arguments for movetodir. Make sure to include a directory.\n");
      }

      DIR* dir = opendir(word_array[1]);

      if(dir){
        currentdir = realloc(currentdir, sizeof(char) * strlen(word_array[1]));
        strcpy(currentdir, word_array[1]);
      }else if(ENOENT == errno){
        printf("Directory '%s' does not exist.\n", word_array[1]);
      }else{
        printf("Failed to open directory, please try again.\n");
      }
    }else{
      printf("Command '%s' does not exist.\n", word_array[0]);
    }
  }
  writeHistory();
  freeHistory();
  free(currentdir);
  // getHistory();
}

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

void freeHistory(){
  for(int i = 0; i < SIZE; i++){
    free(hist[i].cmd);
    for(int j = 0; j < hist[i].numParams; j++){
      free(hist[i].params[j]);
    }
  }
  free(hist);
}

void printHistory(){
  if(IDX - 1 < 0){
    printf("No history to print.\n");
    return;
  }

  for(int i = 0; i <= IDX - 1; i++){
    printf("%d: %s", i, hist[i].cmd);
    for(int j = 0; j < hist[i].numParams; j++){
      printf(" %s", hist[i].params[j]);
    }
    printf("\n");
  }

  return;
}

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
