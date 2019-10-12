#include<stdlib.h>
#include<stdio.h>
#include<assert.h>
#include<signal.h>
#include<sys/time.h>
#include<string.h>
#include <setjmp.h>

jmp_buf jmpbuffer_initial;
jmp_buf jmpbuffer_final;

#define MAX_PROC_NUM 3

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

static struct task_info   Tasks[MAX_PROC_NUM];
static volatile struct vars_Arcsin procContextVars;

double calcArcsin(int n)
{
  /*double& factor = procContextVars.factor;
  int& i = procContextVars.i;
  double& val = procContextVars.val;*/

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

  while (1);
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
    process_id %= MAX_PROC_NUM;
    counter_finished++;
  }
  while(1 == Tasks[process_id].is_finished && counter_finished <= MAX_PROC_NUM);

  if(MAX_PROC_NUM < counter_finished)// We are done!!
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
    memset (&sa, 0, sizeof (sa));
    sa.sa_handler = &RR_Scheduler;
    sigaction (SIGVTALRM, &sa, NULL);

    sigemptyset (&mask);
    sigaddset (&mask, SIGVTALRM);

    // Configure the timer to 250 msec
    timer.it_value.tv_sec = 0; // this is necessary
    timer.it_value.tv_usec = 200;
    // Set a 250 msec interval
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 200;
    setitimer (ITIMER_VIRTUAL, &timer, NULL);
}

int main(void)
{
  // Initialize stuff
  for(int i=0; i < MAX_PROC_NUM; i++)
  {
    Tasks[i].is_initialized = 0;
    Tasks[i].is_finished = 0;
  }
  procContextVars.val = 2;
  procContextVars.i = 1;
  procContextVars.factor = 2;

  setInterruption();

  if(0 != sigsetjmp(jmpbuffer_final, 1)) 
  {
    for(int i=0; i < MAX_PROC_NUM; i++) // Let's see them PIs, baby
      printf("P%d's PI = %f\n", i, Tasks[i].vars.val);
    return 0;
  }

  sigsetjmp(jmpbuffer_initial, 1);

  //double valpi = calcArcsin(1500000000);//1500 mill
  //double valpi = calcArcsin(100000000);//100 mill
  double valpi = calcArcsin(2000000);
  return 0;
}