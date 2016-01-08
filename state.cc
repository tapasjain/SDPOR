
#include "state.hh"
#include <sstream>
#include <iomanip>
#include <cassert>
#include <functional>
#include "scheduler_setting.hh"

using namespace std;


////////////////////////////////////////////////////////////////////////////
//
//   the implementation of State
//
////////////////////////////////////////////////////////////////////////////

State::State()
  : prev(NULL), next(NULL)    
{ 
  prog_state = new ProgramState();
  context_switches = 0;
}


State::State(State &another)
{
  prog_state = new ProgramState();
  *prog_state = *another.prog_state;

  symmetry = another.symmetry;
  backtrack = another.backtrack;
  done = another.done;
  sleepset = another.sleepset;

  context_switches = another.context_switches;

  sel_event = prog_state->enabled.get_transition(another.sel_event.thread_id);
  clock_vectors = another.clock_vectors;
  
  prev = next = NULL;

  remove_from_sleep = another.remove_from_sleep;
}


State::~State()
{ 
    delete prog_state;
}


State & State::operator = (State& another)
{
  prog_state = new ProgramState();
  *prog_state = *another.prog_state;

  backtrack = another.backtrack;
  done = another.done;
  sleepset = another.sleepset;

  remove_from_sleep = another.remove_from_sleep;

  context_switches = another.context_switches;

  if (another.sel_event.valid())
    sel_event = prog_state->enabled.get_transition(another.sel_event.thread_id);

  clock_vectors = another.clock_vectors;

  prev = next = NULL;
  return *this;
}

bool State::must_happen_before(InspectEvent * e1, InspectEvent * e2) {
	return 1;
}

State * State::duplicate()
{
  State * state;
  state = new State(); 
  *state = *this;
  return state;
}


void State::add_transition(InspectEvent &event)
{
  prog_state->add_transition(event);
}



/**
 *  move all events that are in enabled set into disabled sets
 * 
 */
void State::update_enabled_set(InspectEvent &event)
{
  prog_state->update_enabled_set(event);
}


/** either release a disabled RWLOCK_WRLOCK or all disabled RWLOCK_RDLOCK;
 *
 */

void State::update_disabled_set(InspectEvent &event)
{
  prog_state->update_disabled_set(event);
}


void State::update_sleepset(InspectEvent &event)
{
  TransitionSet::iterator it;
  vector<InspectEvent> tobe_removed;
  vector<InspectEvent>::iterator vit;
  InspectEvent tmpev;

  assert( !prog_state->enabled.has_member(event));
  
  for (it = sleepset.begin(); it != sleepset.end(); it++){
    tmpev = it->second;
    if (event.dependent(tmpev))
      tobe_removed.push_back(tmpev);
  }

  for (vit = tobe_removed.begin(); vit != tobe_removed.end(); vit++){
    tmpev = *vit;
    sleepset.remove(tmpev);
  }  
}


bool State::is_in_backtrack(InspectEvent event)
{
  return backtrack.has_member(event);
}


bool  State::is_in_the_backtrack_set_of_previous_state(InspectEvent &event)
{
  State * prev_state = prev;

  while (prev_state != NULL){
    if (prev_state->is_in_backtrack(event)) return true;
    prev_state = prev_state->prev;
  }
  
  return false;
}



bool State::is_executed_before_bound(State * bound, InspectEvent &event)
{
  State * ptr;

  ptr = this;
  while (ptr != NULL && ptr != bound){
    if (ptr->sel_event == event) return true;
    ptr = ptr->next;
  }
  
  return false;  
}


bool State::is_in_some_backtrack_set_before_bound(State * bound, InspectEvent &event)
{
  State * ptr;
  ptr = this;

  while (ptr != NULL && ptr != bound){
    if (ptr->backtrack.has_member(event)) return true;
    ptr = ptr->next;
  }

  return false;
}


bool State::is_disabled_before_bound(State * bound, InspectEvent &event)
{
  State * ptr;
  ptr = this;
  while (ptr != NULL && ptr != bound){
    if (ptr->prog_state->disabled.has_member(event)) return true;	
    ptr = ptr->next;
  }

  return false;
}


bool State::is_accessed_by_the_same_thread_again_before_bound(State * bound, 
							      InspectEvent &event)
{
  State *ptr;
  InspectEvent event2;

  ptr = this->next;
  
  while (ptr != NULL && ptr != bound){
    event2 = ptr->sel_event;
    if (event2.thread_id == event.thread_id && event2.type == event.type){
      switch(event.type){
      case MUTEX_LOCK:  
	if (event.mutex_id == event2.mutex_id) return true; break;

      case OBJ_READ: 
      case OBJ_WRITE:
	if (event.obj_id == event2.obj_id) return true; break;

      default:  break;
      }
    }
    
    ptr = ptr->next;
  }
  return false;
}


void State::update_backtrack_set(State * bound)
{
  TransitionSet::iterator it;
  InspectEvent event;

  assert( bound != NULL);
  //cout << this->toString() << endl;
  assert(prog_state != NULL);
  if (sel_event.type == MUTEX_UNLOCK) return;
  if (sel_event.type == THREAD_CREATE) return;
  if (is_accessed_by_the_same_thread_again_before_bound(bound, sel_event)) return;
  
  for (it = prog_state->enabled.begin(); 
       it != prog_state->enabled.end(); it++){
    event = it->second;    
    if (event.type == MUTEX_UNLOCK)  continue;
    if (backtrack.has_member(event)) continue;
    if (done.has_member(event)) continue;
    if (sleepset.has_member(event)) continue;
    //if (is_in_the_backtrack_set_of_previous_state(event)) continue;
    if (event.dependent(sel_event) && is_executed_before_bound(bound, event)) continue;
    // if (is_in_some_backtrack_set_before_bound(bound, event)) continue;
    // if (is_disabled_before_bound(bound, event)) continue;
    
    backtrack.insert(event);
  }
}

void State::execute(InspectEvent &event)
{
  switch(event.type){
  case MUTEX_INIT:   
  case MUTEX_DESTROY: 
  case MUTEX_LOCK:
  case MUTEX_UNLOCK:
  case RWLOCK_INIT:   
  case RWLOCK_DESTROY: 
  case RWLOCK_RDLOCK:
  case RWLOCK_WRLOCK:
  case RWLOCK_UNLOCK:    prog_state->execute_transition(event);    
  default:  break;
  }

  switch(event.type){
  case THREAD_START:  locksets.add_thread(event.thread_id);  break;
  case THREAD_END:    locksets.remove_thread(event.thread_id); break;

  case MUTEX_LOCK:    locksets.acquire(event.thread_id, event.mutex_id); break;

  case MUTEX_UNLOCK:  locksets.release(event.thread_id, event.mutex_id); break;    
  default:  break;
  }
}



bool State::is_enabled_empty()
{
  assert( prog_state != NULL);
  return  prog_state->enabled.empty();
}

bool State::change_preemption_bound(InspectEvent &event) {
	if(prev != NULL) {
		int id1 = prev->sel_event.thread_id;
		InspectEvent alt_event = prog_state->enabled.get_transition(id1);
		if(id1 != event.thread_id && alt_event.valid()) return true;
	}
	return false;
}

/**
 *  get the new state by executing the event 
 */
State * State::apply(InspectEvent &event)
{
  State * new_state = NULL;
  TransitionSet::iterator it;
  InspectEvent  tmpev;
  ClockVector * clock_vector = NULL;

  assert( prog_state->is_enabled(event) );  

  this->sel_event = event;
  
  new_state = new State();
  new_state->sleepset = sleepset;
  new_state->locksets = locksets;  
  new_state->clock_vectors = clock_vectors;

  new_state->context_switches = context_switches;

  if(change_preemption_bound(event)) new_state->context_switches++;

  clock_vector = new_state->clock_vectors.get_clock_vector(event.thread_id);  
  assert(clock_vector != NULL);
  clock_vector->timestamps[event.thread_id]++;

  new_state->execute(event);

  // to be added
  if (! this->symmetry.has_member(event.thread_id)){
    new_state->symmetry = symmetry;
  }

  // move some enabled transitions to disable transitions 
  new_state->update_enabled_set(event);
  new_state->update_disabled_set(event);
  new_state->update_sleepset(event);

  if (! this->sleepset.has_member(event) )
    this->sleepset.insert(event);

  if( !this->done.has_member(event))
    this->done.insert(event);  

  return new_state;
}


/**
 *  testing whether an thread is alive
 */
bool State::alive(int tid)
{
  return prog_state->alive(tid);
}


bool  State::has_executable_transition()
{
  TransitionSet::iterator it;
  InspectEvent event;

  for (it = prog_state->enabled.begin(); it != prog_state->enabled.end(); it++){
    event = it->second;
    if (!sleepset.has_member(event)) return true;
  }
  return false;
}


bool State::has_been_end()
{
  return (prog_state->enabled.size() == 0 && prog_state->disabled.size() == 0);
}


InspectEvent State::get_transition()
{
  TransitionSet::iterator it;
  InspectEvent event;

  for (it = prog_state->enabled.begin(); it != prog_state->enabled.end(); it++){
    event = it->second;
    if (!sleepset.has_member(event)) return event;
  }

  return InspectEvent::dummyEvent;
}


bool State::operator == (State &st)
{
//   return (enabled == st.enabled  &&
// 	  disable == st.disable  &&
// 	  sel_event == st.sel_event);	  
  assert(false);
  return false;
}


bool State::is_enabled(InspectEvent &event)
{
  return  prog_state->is_enabled(event);
}

bool State::is_disabled(InspectEvent &event)
{
  return prog_state->is_disabled(event);
}


bool State::check_race()
{
  return  prog_state->check_race();
}



Lockset * State::get_lockset(int tid)
{
  Lockset * lockset = NULL;
  lockset = locksets.get_lockset(tid);
  return lockset;
}


ClockVector * State::get_clock_vector(int tid)
{
  ClockVector * clock_vec = NULL;
  clock_vec = clock_vectors.get_clock_vector(tid);
  return clock_vec;
}


ProgramState * State::get_program_state(EventBuffer & event_buffer)
{
  return prog_state;
}


void State::update_clock_vector(int tid)
{
  ClockVector * clock_vector;

  clock_vector = clock_vectors.get_clock_vector(tid);
  assert(clock_vector != NULL);
  clock_vector->timestamps[tid]++;
}
 

string State::toString()
{
  stringstream  ss;
  TransitionSet::iterator it;

  ss << "state: ---------------------------\n";
  ss << "       sel:   : " << sel_event.toString() << "\n";

  ss << "       enabled: \n";
  for (it = prog_state->enabled.begin(); it != prog_state->enabled.end(); it++)
    ss << "            " << it->second.toString() << "\n";
    
  ss << "       disabled: \n";
  for (it = prog_state->disabled.begin(); it != prog_state->disabled.end(); it++)
    ss << "            " << it->second.toString() << "\n";

  ss << "       backtrack: \n";
  for (it = backtrack.begin(); it != backtrack.end(); it++)
    ss << "            " << it->second.toString() << "\n";

  ss << "       done: \n";
  for (it = done.begin(); it != done.end(); it++)
    ss << "            " << it->second.toString() << "\n";


  ss << "       sleepset: \n";
  for (it = sleepset.begin(); it != sleepset.end(); it++)
    ss << "            " << it->second.toString() << "\n";

  ss << "-----------------------------------\n";

  ss << "		context switches:	";
  ss << context_switches << "\n";

  return ss.str();
}



string State::toSimpleString()
{
  stringstream  ss;
  TransitionSet::iterator it;

  ss << setw(15) << sel_event.toString() << ";";
  ss << setw(30) <<"{";

  
  for (it = backtrack.begin(); it != backtrack.end(); it++)
    ss << "  " << it->second.toString() << "  ";
  ss <<"}  ";


  
  ss << setw(20) << " done = {";
  for (it = done.begin(); it != done.end(); it++)
    ss <<  it->second.toString() << "  ";
  ss <<"}  ";

  ss << setw(20) 
     << " sleepset = {";
  for (it = sleepset.begin(); it != sleepset.end(); it++)
    ss <<  it->second.toString() << "  ";
  ss <<"} \n";

  return ss.str();
}


void State::add_to_enabled(InspectEvent &event)
{
  assert(prog_state != NULL);
  prog_state->enabled.insert(event);
}


void State::remove_from_enabled(InspectEvent &event)
{
  assert(prog_state != NULL);
  prog_state->enabled.remove(event);
}





