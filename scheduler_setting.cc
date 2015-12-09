#include "scheduler_setting.hh"
#include <cstdlib>

SchedulerSetting  setting;


SchedulerSetting::SchedulerSetting()
  : standalone_flag(false),
    lazy_flag(false),
    stateful_flag(false),
    symmetry_flag(false), 
    
    deadlock_only_flag(false), 
    race_only_flag(false),
    
    max_threads(32), 
    max_errors(1),
    timeout_val(150),
    socket_file(""),  
    target(""), 
    target_argc(-1), 
    target_argv(NULL) 
{
  socket_file = string("/tmp/_") + string(getenv("USER")) + "_inspect_socket";
}


void set_stateful_flag()
{
  setting.stateful_flag = true;
}


void clear_stateful_flag()
{
  setting.stateful_flag = false;
}


void set_lazy_flag()
{
  setting.lazy_flag = true;
}


void clear_lazy_flag()
{
  setting.lazy_flag = false;
}

void set_standalone_flag()
{
  setting.standalone_flag = true;
}


void clear_standalone_flag()
{
  setting.standalone_flag = false;
}


void set_symmetry_flag()
{
  setting.symmetry_flag = true;
}

void clear_symmetry_flag()
{
  setting.symmetry_flag = false;
}
