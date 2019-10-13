#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAXLEN 80
#define CONFIG_FILE "config.txt"

enum algthms
{
  RR = 0, // Round-Robin
  LS      // Lottery
};

struct conf_params
{
  int  algorithm;
  int  numProc;
  int  arrTime[25]; // Arrival time in terms of Quantum multiple
  int  procWork[25];
  int  ticketNum[25];
  int  quantum; // In us
} conf_params;

// For getting rid of trailing and leading whitespace
// including line break char from fgets()
char* trim(char* s)
{
  // Pointers to start & end
  char *s1 = s, *s2 = &s[strlen(s) - 1];

  // Trim tail
  while((isspace (*s2)) && (s2 >= s1))
    s2--;
  *(s2+1) = '\0';

  // Trim head
  while((isspace (*s1)) && (s1 < s2))
    s1++;

  strcpy(s, s1);
  return s;
}

void StrArrToIntArr(int* intArr, char* str)
{
  char* piece = strtok(str, " ");
  int cnt = 0;

  while (piece != NULL)              
  {
      intArr[cnt] = atoi(piece);
      ++cnt;
      piece = strtok(NULL, " ");
  }
}

void parse_config(struct conf_params* params)
{
  char *s, buff[256];
  FILE *fp = fopen(CONFIG_FILE, "r");
  if(fp == NULL)
  {
    return;
  }

  // Let's read line by line
  while((s = fgets(buff, sizeof buff, fp)) != NULL)
  {
    // Skip empty lines and comments
    if(buff[0] == '\n' || buff[0] == '#')
      continue;

    // Parse params
    char name[MAXLEN], value[MAXLEN];
    s = strtok(buff, "="); // Grab left-hand side of '='
    if(s==NULL)
      continue;
    else
      strncpy(name, s, MAXLEN);
    s = strtok(NULL, "=");// Grab right-hand side of '='
    if(s==NULL)
      continue;
    else
      strncpy(value, s, MAXLEN);
    trim(value);

    // Insert read value into params struct
    if(0 == strcmp(name, "algorithm"))
      params->algorithm = atoi(value);
    else if(0 == strcmp(name, "numProc"))
      params->numProc = atoi(value);
    else if(0 == strcmp(name, "arrTime")) // array
      StrArrToIntArr(params->arrTime, value);
    else if(0 == strcmp(name, "procWork")) // array
      StrArrToIntArr(params->procWork, value);
    else if(0 == strcmp(name, "ticketNum")) // array
      StrArrToIntArr(params->ticketNum, value);
    else if(0 == strcmp(name, "quantum"))
      params->quantum = atoi(value);
    else
      printf("Error: %s: Unknown tag\n", name);
  }

  fclose(fp);
}

int main (int argc, char *argv[])
{
  int i = 0;
  struct conf_params params;
  memset (&params, 0, sizeof(params));

  printf ("Reading config file...\n");
  parse_config (&params);

  printf ("Final values:\n");
  printf ("  Algorithm: %d, numProc: %d, quantum: %duS\n",
    params.algorithm, params.numProc, params.quantum);
  printf("  Arrival times: ");
  for(i = 0; i < params.numProc; i++)
  {
    printf("%d ", params.arrTime[i]);
  }
  printf("\n  Work per proc: ");
  for(i = 0; i < params.numProc; i++)
  {
    printf("%d ", params.procWork[i]);
  }
  printf("\n  Number of tickets: ");
  for(i = 0; i < params.numProc; i++)
  {
    printf("%d ", params.ticketNum[i]);
  }
  printf("\n");

  return 0;
}