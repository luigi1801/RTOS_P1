#include<stdlib.h>
#include<stdio.h>
#include<assert.h>
#include<signal.h>
#include<sys/time.h>
#include<string.h>
#include <setjmp.h>
#include <ctype.h>//for config reader
#include <gtk/gtk.h>

/*--------------------------Config reader-----------------------------*/
#define MAXLEN 80
#define CONFIG_FILE "config.txt"
#define MAX_TASKS 25

enum algthms
{
  RR = 0, // Round-Robin
  LS      // Lottery
};

struct conf_params
{
  int  algorithm;
  int  numProc;
  int  arrTime[MAX_TASKS]; // Arrival time in terms of Quantum multiple
  int  procWork[MAX_TASKS];
  int  ticketNum[MAX_TASKS];
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

void ReadConfig(struct conf_params* params)
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

/*-----------------------------------------GUI-------------------------*/

int number_Tasks;
double value_random;

struct task_widget
{
  GtkWidget *lbl_Active;
  GtkWidget *lbl_Value;
  GtkWidget *pb_Percentage;
};

static struct task_widget all_Widgets[MAX_TASKS];

static gboolean update_GUI(gpointer data)
{

  value_random += 0.05;
  if (value_random>1)
    value_random = 0;
  
  char c[4];
  sprintf(c, "%d%%", (int)(value_random*100));
    
  gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(all_Widgets[0].pb_Percentage), value_random);
  gtk_progress_bar_set_text(GTK_PROGRESS_BAR(all_Widgets[0].pb_Percentage), c); 

  return TRUE;
}

void *control_GUI()
{
  GtkWidget *window;
  GtkWidget *table;
  GtkWidget *label;

  gtk_init(NULL, NULL);
    
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  table = gtk_table_new (number_Tasks+1, 4, TRUE);

  label = gtk_label_new("Thread #");
  gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 0, 1);

  label = gtk_label_new("% Trabajo Terminado");
  gtk_table_attach_defaults (GTK_TABLE (table), label, 1, 2, 0, 1);

  label = gtk_label_new("Trabajo Activo");
  gtk_table_attach_defaults (GTK_TABLE (table), label, 2, 3, 0, 1);

  label = gtk_label_new("Valor Acumulado");
  gtk_table_attach_defaults (GTK_TABLE (table), label, 3, 4, 0, 1);

  char c[20];
  for(int i = 1; i<=number_Tasks; i++)
  { 
    sprintf(c, "%d", i);
    label = gtk_label_new(c);
    gtk_table_attach_defaults (GTK_TABLE(table), label, 0, 1, i, i+1);
    
    sprintf(c, "%d", 0);
    all_Widgets[i-1].pb_Percentage = gtk_progress_bar_new();
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(all_Widgets[i-1].pb_Percentage), 0.0);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(all_Widgets[i-1].pb_Percentage), "0%");
    gtk_table_attach_defaults (GTK_TABLE(table), all_Widgets[i-1].pb_Percentage , 1, 2, i, i+1);
  
    all_Widgets[i-1].lbl_Active = gtk_label_new("Inactivo");
    gtk_table_attach_defaults (GTK_TABLE(table), all_Widgets[i-1].lbl_Active, 2, 3, i, i+1);
    
    all_Widgets[i-1].lbl_Value = gtk_label_new(c);
    gtk_table_attach_defaults (GTK_TABLE(table), all_Widgets[i-1].lbl_Value , 3, 4, i, i+1);
  }

  gtk_label_set_text(GTK_LABEL(all_Widgets[1].lbl_Value), "25");
      
  gtk_container_add (GTK_CONTAINER (window), table);

  g_signal_connect (window, "delete-event", G_CALLBACK (gtk_main_quit), NULL);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  g_timeout_add(1000, update_GUI, table);

  gtk_container_set_border_width (GTK_CONTAINER (window), 10);

  gtk_widget_show_all(window);
    
  gtk_main ();
}

/*--------------------------------------------------------------------*/

jmp_buf jmpbuffer_initial;
jmp_buf jmpbuffer_final;


static volatile sig_atomic_t process_id = 0;
static sigset_t mask;
static sigset_t orig_mask;

struct vars_Arcsin
{
  double factor;
  int i;
  double val;
};

struct task_info
{
  jmp_buf env;
  int n;
  int is_initialized;
  int is_finished;
  struct vars_Arcsin vars;
};

static struct task_info Tasks[MAX_TASKS];
static volatile struct vars_Arcsin procContextVars;

double calcArcsin(int n)
{
  for(procContextVars.i = 1; procContextVars.i<n; procContextVars.i++)
  {
    sigprocmask(SIG_BLOCK, &mask, &orig_mask); // CRITICAL REGION

    procContextVars.factor *= (2*procContextVars.i-1);
    procContextVars.factor /= (2*procContextVars.i);

    procContextVars.val += procContextVars.factor/(2*procContextVars.i+1);

    //printf("P #%d, I =%d\n", process_id, procContextVars.i); //DEBUG
    sigsetjmp(Tasks[process_id].env, process_id + 1);

    sigprocmask(SIG_SETMASK, &orig_mask, NULL); // END OF CRITICAL REGION
  }
  printf("Soy proceso #%d. \tval: %f.\t It: %d\n", process_id, procContextVars.val, procContextVars.i);
  Tasks[process_id].is_finished = 1;

  while(1);
  return procContextVars.val;
}

void RR_Scheduler(int signum)
{
  // Set appropriate flags and store relevant exec info
  Tasks[process_id].is_initialized = 1;
  Tasks[process_id].vars = procContextVars;

  if(0 == Tasks[process_id].is_finished)
    printf("Interrumpting process #%d. \tval: %f.\t It: %d.\n", process_id, Tasks[process_id].vars.val, Tasks[process_id].vars.i);

  // Find next available task
  int counter_finished = 0;
  do{
    process_id ++;
    process_id %= number_Tasks;
    counter_finished++;
  }
  while(1 == Tasks[process_id].is_finished && counter_finished <= number_Tasks);

  if(number_Tasks < counter_finished)// We are done!!
    siglongjmp(jmpbuffer_final, 150);

  if(1 == Tasks[process_id].is_initialized)// Task had already been scheduled before, restore it
  {
    procContextVars = Tasks[process_id].vars; //{factor, i, val}
    siglongjmp(Tasks[process_id].env, process_id);
  }
  else// Task is being executed for the first time, setup initial state
  {
    procContextVars.val = 2;
    procContextVars.i = 1;
    procContextVars.factor = 2;
    siglongjmp(jmpbuffer_initial, process_id);
  }
  
}

void setInterruption()
{
    struct sigaction sa;
    struct itimerval timer;

    // Install our timer_handler as the signal handler for SIGVTALRM
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &RR_Scheduler;
    sigaction(SIGVTALRM, &sa, NULL);

    sigemptyset(&mask);
    sigaddset(&mask, SIGVTALRM);

    // Configure the timer to 250 msec
    timer.it_value.tv_sec = 0; // this is necessary
    timer.it_value.tv_usec = 200;
    // Set a 250 msec interval
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 200;
    setitimer(ITIMER_VIRTUAL, &timer, NULL);
}

 
int main(void)
{
  pthread_t thread1;
  int  iret1;

  number_Tasks = 3;
  value_random = 0.0;

  // Initialize stuff
  for(int i=0; i < number_Tasks; i++)
  {
    Tasks[i].is_initialized = 0;
    Tasks[i].is_finished = 0;
  }
  procContextVars.val = 2;
  procContextVars.i = 1;
  procContextVars.factor = 2;

  //iret1 = pthread_create(&thread1, NULL, control_GUI, NULL);
  setInterruption();

  if(0 != sigsetjmp(jmpbuffer_final, 1)) 
  {
    for(int i=0; i < number_Tasks; i++) // Let's see them PIs, baby
      printf("P%d's PI = %f\n", i, Tasks[i].vars.val);
    //pthread_join( thread1, NULL);
    return 0;
  }

  sigsetjmp(jmpbuffer_initial, 1);

  //double valpi = calcArcsin(1500000000);//1500 mill
  //double valpi = calcArcsin(100000000);//100 mill
  //double valpi = calcArcsin(2000000);
  double valpi = calcArcsin(20000);
  return 0;
};