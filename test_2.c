#include<stdlib.h>
#include<stdio.h>
#include<assert.h>
#include<signal.h>
#include<sys/time.h>
#include<string.h>


#include <setjmp.h>


// https://www.gnu.org/software/libc/manual/html_node/Setting-an-Alarm.html

static volatile sig_atomic_t val = 0;

jmp_buf jmpbuffer_initial;
jmp_buf jmpbuffer_final;

#define Total_process 3

static volatile sig_atomic_t process_id = 0;
static sigset_t mask;
static sigset_t orig_mask;

struct task_info
{
  jmp_buf env;
  int is_initialized;
  int is_finished;
  int iter;
  double val;
};

static struct task_info all_tasks[Total_process];

double calcArcsin(int n)
{
  double factor = 2.0;
  volatile int i = 0;
  double val = 2.0;
  all_tasks[process_id].val = 2;
  all_tasks[process_id].iter = 1;
  for(i = 1; i<n; i++)
  {
    sigprocmask(SIG_BLOCK, &mask, &orig_mask);

    factor *= (2*i-1);
    factor /= (2*i);

    val += factor/(2*i+1);
    
    all_tasks[process_id].val = val;
    all_tasks[process_id].iter = i;
    sigsetjmp(all_tasks[process_id].env, process_id + 1);
    
    sigprocmask(SIG_SETMASK, &orig_mask, NULL);
  }
  printf("Soy proceso #%d. \tval: %f.\t It: %d\n", process_id, all_tasks[process_id].val, all_tasks[process_id].iter);
  all_tasks[process_id].is_finished = 1;

  while (1);
  return val;
}

void scheduler(int signum)
{
  if(0 == all_tasks[process_id].is_finished)
    printf("Interrumpting process #%d. \tval: %f.\t It: %d.\n", process_id, all_tasks[process_id].val, all_tasks[process_id].iter);
  all_tasks[process_id].is_initialized = 1;
  int counter_finished = 0;
  do{
    process_id ++;
    process_id %= Total_process;
    counter_finished++;
  }
  while(1 == all_tasks[process_id].is_finished && counter_finished < Total_process);

  if(Total_process == counter_finished)
    siglongjmp(jmpbuffer_final, 150);

  if(1 == all_tasks[process_id].is_initialized)
    siglongjmp(all_tasks[process_id].env, process_id);
  else
    siglongjmp(jmpbuffer_initial, process_id);
  
}

void setInterruption()
{
    struct sigaction sa; 
    struct itimerval timer; 

    // Install our timer_handler as the signal handler for SIGVTALRM
    memset (&sa, 0, sizeof (sa)); 
    sa.sa_handler = &scheduler; 
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
  for(int i=0; i < Total_process; i++){
    all_tasks[i].is_initialized = 0;
    all_tasks[i].is_initialized = 0;
  }

  setInterruption();
  sigsetjmp(jmpbuffer_initial,1);

  if(0 != sigsetjmp(jmpbuffer_final,1)) return 0;

  double valpi = calcArcsin(1500000); 
  return 0;
}