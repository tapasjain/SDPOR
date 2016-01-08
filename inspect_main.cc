
#include "scheduler.hh"
#include "scheduler_setting.hh"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sys/time.h>

using namespace std;


Scheduler * g_scheduler = NULL;
int verboseLevel = -1;

/*****************************************************************************
 *
 *   main  function  for the scheduler
 *
 *****************************************************************************/

void print_usage()
{
  cout<<"Inspect version 0.0.3 (c) University of Utah                                 \n"
      <<"                                                                             \n"
      <<"Usage:  inspect [options] test_target                                        \n"
      <<"                                                                             \n"
      <<"Options:                                                                     \n"
      <<"  --timeout            the maxium seconds that Inspect waits for a monitored \n"
      <<"                       thread to response                                    \n"
      <<"  --multi-error(-me)   do not stop after revealing the first error           \n"
      <<"  --verbose (or -v)    set the verbosity level                               \n"
      <<"  --standalone         running in a stand-alone mode                         \n"
      <<"  --race-only(-ro)     only focus on data race detection                     \n"
      <<"  --deadlock-only(-do) only focus on deadlock detection                      \n"
      <<"  --help (or -h)       display the help option                               \n"
      <<"                                                                             \n"
      <<"Please send bugs to yuyang@cs.utah.edu.                                      \n";
}


enum InspectBehavior
{
  IB_NOREPLAY = 0,
  IB_REPLAY_WITH_DPOR = 1,
  IB_SPECIFIC_REPLAY = 2,
  IB_UNKNOWN
};


bool parsing_command_line(int argc, char* argv[])
{
  if (argc < 2){   print_usage();    exit(0);  }

  int pos = 1;
  char * arg, * arg1;
  while (pos < argc){
    arg = argv[pos];

    if (strcmp(arg, "--standalone") == 0){
      setting.standalone_flag = true;
      pos++;
    }
    else if (strcmp(arg, "--lazy") == 0){
      setting.lazy_flag = true;
      pos++;
    }
    else if (strcmp(arg, "--symm") == 0){
      setting.symmetry_flag = true;
      pos++;
    }
    else if (strcmp(arg, "--race-only") == 0 || strcmp(arg, "-ro") == 0){
      setting.race_only_flag = true;
      pos++;
    }
    else if (strcmp(arg, "--deadlock-only") == 0 || strcmp(arg, "-do") == 0){
      setting.deadlock_only_flag = true;
      pos++;
    }
    else if (strcmp(arg, "--max-errors") == 0 || strcmp(arg, "-me")== 0){
      pos++;
      if (pos == argc){ print_usage(); exit(0); }
      if(isdigit(argv[pos][0])){
	setting.max_errors = atoi(argv[pos]);
	pos++;
      }
    }
    else if (strcmp(arg, "--stateful") == 0 || strcmp(arg,"-s") == 0){
      pos++;
      setting.stateful_flag = true;
    }
    else if (strcmp(arg, "--maxthreads") == 0){
      pos++;
      arg1 = argv[pos];
      setting.max_threads = atoi(arg1);
      pos++;
    }
    else if (strcmp(arg, "--timeout") == 0){
      pos++;
      arg1 = argv[pos];
      setting.timeout_val = atoi(arg1);
      pos++;
    }
    else if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0){
      print_usage();  exit(0);
    }
    else if (strcmp(arg, "--verbose") == 0 || strcmp(arg, "-v") == 0){
      pos++;
      if (pos == argc) { print_usage(); exit(0); }
      if (isdigit(argv[pos][0])){
	verboseLevel = atoi(argv[pos]);
	pos++;
      }

    }
    else if (strncmp(arg, "-", 1) == 0){
      cout << "Incorrect usage of Inspect: "
	   << "argument: \'" << arg <<"\' is unknown.\n";
      print_usage(); exit(0);
    }
    else  break;
  }

  if (setting.standalone_flag) return true;

  setting.target_argc = argc - pos;
  setting.target_argv = &argv[pos];
  setting.target = argv[pos];
  return true;
}


void print_time(struct timeval * start_time, struct timeval * end_time)
{
  int sec;
  int usec;

  sec = end_time->tv_sec - start_time->tv_sec;
  usec = end_time->tv_usec - start_time->tv_usec;

  if (usec < 0){
    usec += 1000000;
    sec--;
  }

  cout << "Used time (seconds): " << sec <<"." << usec << "\n" ;
}


int main(int argc, char* argv[])
{
//   int cur_pos;
  bool success_flag;
  SchedulerSetting  setting;
  struct timeval start_time, end_time;

  success_flag = parsing_command_line(argc, argv);

  if (! success_flag) return -1;
  bool contextbound; int bound;
  cout << "ContextBound(1) or SDPOR(0): ";
  cin >> contextbound;

  if(contextbound) {
    cout << "Bound : ";
    cin >> bound;
  }


  g_scheduler = new Scheduler();
  g_scheduler->init();
  g_scheduler->bound = bound;
  g_scheduler->context_bound = contextbound;
  gettimeofday(&start_time, NULL);
  g_scheduler->run();
  gettimeofday(&end_time, NULL);

  print_time(&start_time, &end_time);

  delete g_scheduler;
  return 0;
}


/** \mainpage Inspect -- A Runtime Model Checker for Multithreaded C/C++ Programs
 *
 * \section intro_sec Introduction
 *
 *
 * \section install_sec Installation
 *
 * \subsection step1 Step 1: Download the source
 *
 *
 */




