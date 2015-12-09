#include "inspect_event.hh"
#include "state.hh"
#include "event_buffer.hh"
#include "scheduler_setting.hh"
#include "scheduler.hh"
#include "inspect_exception.hh"
#include "thread_info.hh"

using namespace std;



void handle_symmetry_event(State *state, InspectEvent &event, EventBuffer &event_buffer)
{
  assert(state != NULL);
  if (event.type != SYMM_POINT) return;
  
  event_buffer.approve(event);
  state->symmetry.insert(event);
 
  InspectEvent new_event;
  new_event = event_buffer.get_event(event.thread_id);
  event = new_event;  
}



/**  Given a state, compute the next state 
 * 
 *  Here we need to take care of thread creation specially.
 *  
 *  A state is composed of:
 *  (1) enabled set  -- the set of transitions that are enabled at the state                 
 *  (2) disabled set -- the set of transitions that are currently disabled, but              
 *  (3) the state of global objects                                                        
 *        -- the state of mutexes
 *        -- the state of reader/writer locks
 *        -- the snapshot of global objects
 *  (4) the local states of threads
 *  (5) 
 *  (6)  
 * 
 */
State * next_state(State* state, 
		   InspectEvent &event, 
		   EventBuffer &event_buffer)
{
  State * new_state = NULL;
  InspectEvent new_event;
  InspectEvent  symmetry_event;
  InspectEvent pre_create, post_create, after_post, thread_start, first_ev;
  int prev_sid = -1;    
  bool exception_flag = false;

  assert( state != NULL );
  assert( event.valid() );  
  assert( state->is_enabled(event) );
  
  // state->check_race();

  new_state = new State();

  //*new_state = *state;

  *(new_state->prog_state) = *(state->prog_state);
  new_state->remove_from_enabled(event);

  new_state->sleepset = state->sleepset;
  new_state->locksets = state->locksets;  
  new_state->clock_vectors = state->clock_vectors;
  new_state->update_clock_vector(event.thread_id);  // increase the clock counter of the correspondent thread by 1

  new_state->execute(event);

  if (! state->symmetry.has_member(event.thread_id)){
    new_state->symmetry = state->symmetry;
  }

  new_state->update_enabled_set(event);
  new_state->update_disabled_set(event);
  new_state->update_sleepset(event);

  if (! state->sleepset.has_member(event) )
    state->sleepset.insert(event);

  if( !state->done.has_member(event))
    state->done.insert(event);  

  try{    
    event_buffer.approve(event);
  }
  catch(SocketException &e){
    exception_flag = true;
  }

  if (exception_flag) return NULL;
  
  state->sel_event = event;
  
  if (event.type == THREAD_END)  return new_state;
  
  // here we need to handle multiple situtation here
  // need a better style 
  
  bool found_next_event = false;
  bool received_delta = false;
  
  while (! found_next_event){
    new_event = event_buffer.get_event(event.thread_id);
    
    handle_symmetry_event(new_state, new_event, event_buffer);

    switch(new_event.type){
    case THREAD_POST_CREATE:
      {
	assert(event.type == THREAD_PRE_CREATE);
	post_create = new_event;	
	event_buffer.approve(post_create);
	
	after_post = event_buffer.get_event(post_create.thread_id);
	thread_start = event_buffer.get_event(post_create.child_id);
	assert(thread_start.type == THREAD_START);
	event_buffer.approve(thread_start);
	first_ev = event_buffer.get_event(post_create.child_id);
	
	//ls_tables.add_thread(post_create.child_id);
	
	handle_symmetry_event(new_state, first_ev, event_buffer);

	InspectEvent  thrd_ev;
	thrd_ev.init_thread_create(post_create.thread_id, post_create.child_id);
	
	new_state->execute(post_create);
	new_state->execute(thread_start);      
	state->sel_event = thrd_ev;
	
	new_state->remove_from_enabled(post_create);
	new_state->add_transition(first_ev);
	//new_state->add_transition(after_post);
	
	new_event = after_post;
	
	new_state->clock_vectors.add_thread(post_create.child_id, post_create.thread_id);	

	thread_table.add_thread(post_create.child_id, ""// thread_start.thread_nm
				, post_create.thread_arg);

	found_next_event = true;
	break;
      }
      
    default:  found_next_event = true; break;
    }
    
  }
  
  assert(new_event.thread_id == event.thread_id);

  new_state->add_transition(new_event);
  
  return new_state;
}
	


