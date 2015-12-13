#ifndef __THREAD_SCHEDULER_H__
#define __THREAD_SCHEDULER_H__


#include <vector>
#include <set>
#include <ext/hash_set>
#include <utility>

#include "inspect_util.hh"
#include "inspect_event.hh"
#include "scheduler_object_table.hh"
#include "inspect_exception.hh"

#include "state.hh"
#include "state_stack.hh"
#include "event_buffer.hh"
#include "thread_info.hh"
#include "scheduler_setting.hh"
#include "event_relation_graph.hh"


using std::vector;
using std::set;
using std::map;
using std::pair;
using __gnu_cxx::hash_map;
using __gnu_cxx::hash;



class Scheduler : public Thread
{
public:
  Scheduler();
  ~Scheduler();

  bool init();
  InspectEvent receive_event( Socket * socket ); 
  InspectEvent receive_main_start();
  void approve_event(InspectEvent &event);

  void run(); 
  void stop();
  
  inline bool is_listening_socket(int fd); 
  void print_out_trace();
  pair<int, int> set_read_fds();

  void exec_test_target(const char * path);
  void update_backtrack_info(State * state, InspectEvent &event);
  void standard_update_backtrack_info(State * state, InspectEvent &event);
  void lazy_update_backtrack_info(State * state, InspectEvent &event, bool allow_communicative=true);

  void lazy_update_locks(State*, InspectEvent&);
  void lazy_update_internal(State*, State*, InspectEvent&, InspectEvent&, InspectEvent&);

  bool dependent(InspectEvent &e1, InspectEvent &e2);
  bool is_mutex_exclusive_locksets(Lockset *, Lockset*);
  void report();
  void utility(State * state, InspectEvent &event);

  bool examine_state(State*, State*);
public:
  InspectEvent & get_event_from_buffer(int tid);
  void exec_transition(InspectEvent &event);

  void monitor_first_run();
  void utility();

  void return_min(State * E_, vector<InspectEvent> &v, set<int> &ts);
  void backtrack_SourceDPOR_helper(State *E, State * E_,InspectEvent &p, InspectEvent *event_at_E_);
  void update_backtrack_SourceDPOR(State *E, InspectEvent &p);
  void SourceDPOR();

  void backtrack_context_bound_SourceDPOR_helper(State *E, State * E_,InspectEvent &p, InspectEvent *event_at_E_);
  void update_backtrack_context_bound_SourceDPOR(State *E, InspectEvent &p);
  void context_bound_SourceDPOR();

  void backtrack_checking();
  void StatefulDPOR();
  bool check_state_exists_in_hash(State* state);
//   void monitor();
  State * execute_one_thread_until(State * state, int tid, InspectEvent);
  //State * next_state(State *state, InspectEvent &event, LocalStateTable * ls_table);

public:
  void get_mutex_protected_events(State * state1, State * state2, int tid, vector<InspectEvent>& events);

  State * get_initial_state();

  void filter_auxiliary_events(InspectEvent &event);
  InspectEvent get_latest_executed_event_of_thread(State * state, int thread_id);

  void check_race(State * state);
  
  bool found_enough_error() { return (num_of_errors_detected >= max_tolerated_errors); }
public:
  EventBuffer event_buffer;
  EventGraph  event_graph;
  
public:
  int max_tolerated_errors;
  int num_of_errors_detected;
  int num_of_transitions;
  int num_of_states;
  int num_of_truncated_branches;
  int num_killed;
  int sut_pid;  
  int run_counter;
  bool found_bug; 
  StateStack state_stack;
};




#endif



