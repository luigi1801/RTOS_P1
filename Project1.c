#include<stdlib.h>
#include<stdio.h>
#include<assert.h>
#include<signal.h>
#include<sys/time.h>
#include<string.h>
#include <setjmp.h>
#include <ctype.h>//for config reader
#include <gtk/gtk.h>

/*--------------------------Define Section-----------------------------*/
#define MAXLEN 80
#define CONFIG_FILE "config.txt"
#define MAX_TASKS 25

/*--------------------------Enum and structs-----------------------------*/
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
  int  quantum; // In uS
} conf_params;

struct task_widget
{
  GtkWidget* lbl_Active;
  GtkWidget* lbl_Value;
  GtkWidget* pb_Percentage;
};

struct vars_Arcsin
{
  double factor;
  int i;
  double val;
  int N;
};

struct task_info
{
  jmp_buf env;
  int N;
  int arrTime;
  int is_initialized;
  int is_finished;
  int tickets;
  struct vars_Arcsin vars;
} task_info;

/*--------------------------Global Variables-----------------------------*/
// General
int number_Ready_Tasks;
int Total_tasks;
int Quantum;
static struct task_info Tasks[MAX_TASKS];
static volatile sig_atomic_t process_id = 0;
static sigset_t mask;
static sigset_t orig_mask;

// For GUI
static struct task_widget all_Widgets[MAX_TASKS];

// For Scheduler
jmp_buf* jmpbuffer_initial;
jmp_buf* jmpbuffer_final;
jmp_buf* jmpbuffer_BusyWaiting;

static volatile struct vars_Arcsin procContextVars;
static int TotalTickets;
static int actualAlgrthm;
static int quantum_Counter;

/*--------------------------Config reader-----------------------------*/
// For getting rid of trailing and leading whitespace
// including line break char from fgets()
char* trim(char* s)
{
  // Pointers to start & end
  char* s1 = s, *s2 = &s[strlen(s) - 1];

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
  char* s, buff[256];
  FILE* fp = fopen(CONFIG_FILE, "r");
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
    s = strtok(NULL, "="); // Grab right-hand side of '='
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

int compare(const void* a, const void* b)
{
  int int_a = *((int*) a);
  int int_b = *((int*) b);

  return (int_a > int_b) - (int_a < int_b);
}

void custom_qsort(struct conf_params* conf, int length, int size, int(*compar)(const void* a, const void* b))
{
  if(length > 1)
  {
    int* p = malloc(size);
    int* arrT = conf->arrTime;
    int* procW = conf->procWork;
    int* tickN = conf->ticketNum;

    for(int i = 0; i < length - 1; i++)
    {
      for(int j = i; j < length; j++)
      {
        if(compar(&arrT[i], &arrT[j]) > 0) // Compare arrival time only, but sort the other two fields as well
        {
          //--------Arrival time---------
          memcpy(p, &arrT[i], size);
          memcpy(&arrT[i], &arrT[j], size);
          memcpy(&arrT[j], p, size);
          //---------Proc work-----------
          memcpy(p, &procW[i], size);
          memcpy(&procW[i], &procW[j], size);
          memcpy(&procW[j], p, size);
          //--------Ticket num-----------
          memcpy(p, &tickN[i], size);
          memcpy(&tickN[i], &tickN[j], size);
          memcpy(&tickN[j], p, size);
        }
      }
    }

    free(p);
  }
}

void initialize_fromReader()
{
  struct conf_params params;
  memset(&params, 0, sizeof(params));
  memset(&Tasks, 0, sizeof(task_info) * MAX_TASKS);

  printf("Reading config file...\n");
  ReadConfig(&params);

  custom_qsort(&params, params.numProc, sizeof(int), compare);

  actualAlgrthm = params.algorithm;
  Total_tasks = params.numProc;
  Quantum = params.quantum;

  // Initialize stuff
  for(int i = 0; i < Total_tasks; i++)
  {
    Tasks[i].is_initialized = 0;
    Tasks[i].is_finished = 0;
    Tasks[i].tickets = params.ticketNum[i];
    Tasks[i].N = 50 * params.procWork[i];
    Tasks[i].arrTime = params.arrTime[i];
  }
  printf("---------Done---------\n");
}

/*---------------------------------Arcsin-----------------------------------*/
double calcArcsin()
{
  for(procContextVars.i = 1; procContextVars.i<procContextVars.N; procContextVars.i++)
  {
    sigprocmask(SIG_BLOCK, &mask, &orig_mask); // CRITICAL REGION

    procContextVars.factor *= (2*procContextVars.i-1);
    procContextVars.factor /= (2*procContextVars.i);
    procContextVars.val += procContextVars.factor/(2*procContextVars.i+1);

    sigsetjmp(Tasks[process_id].env, process_id + 1);

    sigprocmask(SIG_UNBLOCK, &mask, &orig_mask); // END OF CRITICAL REGION
  }

  while(1);
  return procContextVars.val;
}

/*--------------------------------Schedulers------------------------------------*/
void checkReadyTasks()
{
  while(number_Ready_Tasks < Total_tasks)
  {
    if(quantum_Counter == Tasks[number_Ready_Tasks].arrTime)
    {
      TotalTickets += Tasks[number_Ready_Tasks].tickets;
      number_Ready_Tasks++;
    }
    else
      break;
  }
}

void setNextID_RR()
{
  // Find next available task
  int counter_finished = 0;
  do
  {
    process_id = process_id == number_Ready_Tasks - 1 ? 0 :  process_id + 1;
    counter_finished++;
  }
  while(1 == Tasks[process_id].is_finished && counter_finished <= number_Ready_Tasks);

  if (number_Ready_Tasks < counter_finished)
    process_id = -1;
}

int setLotteryWinner()
{
  process_id = -1;
  if(0 != TotalTickets)
  {
    int acumm = 0;
    int comparator = rand()%TotalTickets;

    for(int i = 0; i < number_Ready_Tasks; i++)
    {
      if(1 == Tasks[i].is_finished) continue;
      acumm += Tasks[i].tickets;
      if (acumm > comparator)
      {
        process_id = i;
        break;
      }
    }
  }
}

void setNextID()
{
  checkReadyTasks();
  switch(actualAlgrthm)
  {
    case RR:
      setNextID_RR();
      break;
    case LS:
    default:
      setLotteryWinner();
      break;
  }
}

void Scheduler(int signum)
{
  quantum_Counter ++;
  jmp_buf* Next_env;

  if(process_id != -1)
  {
    // Set appropriate flags and store relevant exec info
      Tasks[process_id].vars = procContextVars;
      if(procContextVars.i == Tasks[process_id].N)
      {
        printf("Soy proceso #%d. \tval: %f.\t It: %d\n", process_id, procContextVars.val, procContextVars.i);
        Tasks[process_id].is_finished = 1;
        TotalTickets -= Tasks[process_id].tickets;
      }
  }

  setNextID();

  if(-1 == process_id)
  {// We are done!!
    if (number_Ready_Tasks==Total_tasks)
      siglongjmp(*jmpbuffer_final, 150);
    else
      siglongjmp(*jmpbuffer_BusyWaiting, 150);
  }

  if(1 == Tasks[process_id].is_initialized)// Task had already been scheduled before, restore it
  {
    procContextVars = Tasks[process_id].vars; //{factor, i, val, N}
    Next_env = &(Tasks[process_id].env);
  }
  else// Task is being executed for the first time, setup initial state
  {
    Tasks[process_id].is_initialized = 1;
    procContextVars.val = 2;
    procContextVars.i = 1;
    procContextVars.factor = 2;
    procContextVars.N = Tasks[process_id].N;

    Next_env = jmpbuffer_initial;
  }

  siglongjmp(*Next_env, process_id + 1);
}

/*---------------------------------Interrupt-----------------------------------*/
void setInterruption()
{
    struct sigaction sa;
    struct itimerval timer;

    // Install our timer_handler as the signal handler for SIGVTALRM
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &Scheduler;

    sigaction(SIGVTALRM, &sa, NULL);

    // Configure the timer to 200 msec
    timer.it_value.tv_sec = 0; // this is necessary
    timer.it_value.tv_usec = Quantum;
    // Set a 200 msec interval
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = Quantum;
    setitimer(ITIMER_VIRTUAL, &timer, NULL);
}

/*------------------------------Processor------------------------------------*/
void* processor()
{
  jmp_buf jinitial;
  jmp_buf jfinal;
  jmp_buf busy;

  jmpbuffer_initial = &jinitial;
  jmpbuffer_final = &jfinal;
  jmpbuffer_BusyWaiting = &busy;
  setInterruption();

  if(0 != sigsetjmp(*jmpbuffer_final, 1)) 
    for(int i=0; i < number_Ready_Tasks; i++) // Let's see them PIs, baby
      printf("P%d's PI = %f\n", i, Tasks[i].vars.val);
  else if (0 != sigsetjmp(*jmpbuffer_initial, 1))
    calcArcsin();
  else
  {
    sigsetjmp(*jmpbuffer_BusyWaiting, 1);
    while(1);
  }
}

/*-------------------------------GUI-----------------------------------*/
static gboolean update_GUI(gpointer data)
{
  char c[10];
  double percentage = 0.0;
  for(int i = 0; i<Total_tasks; i++)
  {
    if(1 == Tasks[i].is_initialized && 0 < Tasks[i].N)
      percentage = ((double)Tasks[i].vars.i)/Tasks[i].N;
    else
      percentage = 0.001;
  
    sprintf(c, "%d%%", (int)(percentage*100));

    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(all_Widgets[i].pb_Percentage), percentage);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(all_Widgets[i].pb_Percentage), c);
    if(0 == Tasks[i].is_initialized)
      gtk_label_set_text(GTK_LABEL(all_Widgets[i].lbl_Active), "Not Initialized");
    else if(process_id == i)
      gtk_label_set_text(GTK_LABEL(all_Widgets[i].lbl_Active), "Active");
    else if(1 == Tasks[i].is_finished)
      gtk_label_set_text(GTK_LABEL(all_Widgets[i].lbl_Active), "Finished"); 
    else
      gtk_label_set_text(GTK_LABEL(all_Widgets[i].lbl_Active), "Inactive");

    sprintf(c, "%.5f", Tasks[i].vars.val);
    gtk_label_set_text(GTK_LABEL(all_Widgets[i].lbl_Value), c);
  }
  return TRUE;
}

void* control_GUI()
{
  GtkWidget* window;
  GtkWidget* table;
  GtkWidget* label;

  sigprocmask(SIG_BLOCK, &mask, &orig_mask);// MAGIC

  gdk_threads_init();
  gtk_init(NULL, NULL);
    
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  table = gtk_table_new (Total_tasks+2, 4, TRUE);

  switch(actualAlgrthm)
  {
    case RR:
      label = gtk_label_new("Round-Robin");
      break;
    case LS:
    default:
      label = gtk_label_new("Lottery Scheduler");
      break;
  }

  gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 4, 0, 1);

  label = gtk_label_new("Thread #");
  gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 1, 2);

  label = gtk_label_new("% Trabajo Terminado");
  gtk_table_attach_defaults (GTK_TABLE (table), label, 1, 2, 1, 2);

  label = gtk_label_new("Trabajo Activo");
  gtk_table_attach_defaults (GTK_TABLE (table), label, 2, 3, 1, 2);

  label = gtk_label_new("Valor Acumulado");
  gtk_table_attach_defaults (GTK_TABLE (table), label, 3, 4, 1, 2);

  char c[20];
  for(int i = 1; i<=Total_tasks; i++)
  { 
    sprintf(c, "%d", i);
    label = gtk_label_new(c);
    gtk_table_attach_defaults (GTK_TABLE(table), label, 0, 1, i+1, i+2);
    
    sprintf(c, "%d", 0);
    all_Widgets[i-1].pb_Percentage = gtk_progress_bar_new();
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(all_Widgets[i-1].pb_Percentage), 0.001);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(all_Widgets[i-1].pb_Percentage), "0%");
    gtk_table_attach_defaults (GTK_TABLE(table), all_Widgets[i-1].pb_Percentage , 1, 2, i+1, i+2);
  
    all_Widgets[i-1].lbl_Active = gtk_label_new("Inactive");
    gtk_table_attach_defaults (GTK_TABLE(table), all_Widgets[i-1].lbl_Active, 2, 3, i+1, i+2);
    
    all_Widgets[i-1].lbl_Value = gtk_label_new(c);
    gtk_table_attach_defaults (GTK_TABLE(table), all_Widgets[i-1].lbl_Value , 3, 4, i+1, i+2);
  }

  gtk_container_add (GTK_CONTAINER (window), table);

  g_signal_connect (window, "delete-event", G_CALLBACK (gtk_main_quit), NULL);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  gtk_container_set_border_width (GTK_CONTAINER (window), 10);

  gtk_widget_show_all(window);

  gdk_threads_add_timeout(50, update_GUI, window);

  gtk_main ();
}

/*----------------------------------Main----------------------------------*/
int main(void)
{
  // Initializes random number generator
  time_t t;
  srand((unsigned) time(&t));

  initialize_fromReader();

  quantum_Counter = -1;
  number_Ready_Tasks = 0;
  TotalTickets = 0;
  process_id = -1; 

  sigfillset(&mask);
  sigemptyset(&orig_mask);

  GThread* gui_Thread;

  gui_Thread = g_thread_new("", control_GUI, (gpointer)NULL);
  //sleep(2);

  processor();
  sigprocmask(SIG_BLOCK, &mask, &orig_mask); // we are done, disable signal interrupts

  g_thread_join(gui_Thread); // this implicitly unrefs the thread pointer

  return 0;
}
