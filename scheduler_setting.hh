#ifndef __INSPECT_SCHEDULER__SETTING_HH__
#define __INSPECT_SCHEDULER__SETTING_HH__

#include <string>

using std::string;

class SchedulerSetting
{
public:
  SchedulerSetting();

public:
  bool standalone_flag;
  bool lazy_flag;
  bool stateful_flag;
  bool symmetry_flag;

  bool deadlock_only_flag;
  bool race_only_flag;

  int  max_threads;
  int  max_errors;
  int  timeout_val;
  string  socket_file;

  string  target;
  int target_argc;
  char ** target_argv; 
};

extern SchedulerSetting  setting;

void set_stateful_flag();
void clear_stateful_flag();

void set_lazy_flag();
void clear_lazy_flag();

void set_standalone_flag();
void clear_standalone_flag();

void set_symmetry_flag();
void clear_symmetry_flag();

#endif


