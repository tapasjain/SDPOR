#include <cassert>
#include <cstring>

#include <utility>
#include <string>
#include <map>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>

#include <signal.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include "scheduler.hh"
#include "next_state.hh"

using namespace std;

extern Scheduler* g_scheduler;
extern int verboseLevel;

const int CONTEXT_BOUND = 2;
int reached = 0;
int reaching = 0;

inline void verbose(int level, string str) {
	if (verboseLevel >= level)
		cout << str << endl;
}

/////////////////////////////////////////////////////////////////////////////
//
// some signal handlers for cleaning up
// 
/////////////////////////////////////////////////////////////////////////////

void sig_alarm(int signo) {
	//g_scheduler->resetReplayStatus();
}

void sig_int(int signo) {
	cout << "sig_int is fired .... !\n";
	g_scheduler->stop();

	cout << "Total number of runs:  " << g_scheduler->run_counter << endl;
	if (setting.stateful_flag) {
		cout << "Total number of states: " << g_scheduler->num_of_states
				<< endl;
		cout << "Truncated Branches: " << g_scheduler->num_of_truncated_branches
				<< endl;
	}

	cout << "Transitions explored: " << g_scheduler->num_of_transitions << endl;

	cout << g_scheduler->state_stack.toString() << endl;
	exit(0);
}

void sig_abort(int signo) {
	cout << "sig_abort is fired .... !\n";
	g_scheduler->stop();
	//cout << g_scheduler->state_stack.toString() << endl;
	exit(-1);
}

void sig_pipe(int signo) {
	throw SocketException("unknow");
}

//////////////////////////////////////////////////////////////////////////////
//
//     The implementation of Scheduler
// 
//////////////////////////////////////////////////////////////////////////////

Scheduler::Scheduler() :
		max_tolerated_errors(1), num_of_errors_detected(0), num_of_transitions(
				0), num_of_states(0), num_of_truncated_branches(0), num_killed(
				0), sut_pid(-1), run_counter(1) {
	sigset_t sst;
	int retval;

	sigemptyset(&sst);
	sigaddset(&sst, SIGPIPE);
	retval = sigprocmask(SIG_BLOCK, &sst, NULL);
	assert(retval != -1);

	retval = pthread_sigmask(SIG_BLOCK, &sst, NULL);
	assert(retval != -1);

	signal(SIGINT, sig_int);
	signal(SIGABRT, sig_abort);
}

Scheduler::~Scheduler() {
}

/**
 *  set up the server socket for listening 
 * 
 */
bool Scheduler::init() {
	int retval;
	char buf[64];

	setenv("INSPECT_SOCKET_FILE", setting.socket_file.c_str(), 1);

	memset(buf, 0, sizeof(buf));
	sprintf(buf, "%d", setting.timeout_val);
	setenv("INSPECT_TMEOUT_VAL", buf, 1);

	memset(buf, 0, sizeof(buf));
	sprintf(buf, "%d", setting.max_threads);
	setenv("INSPECT_BACKLOG_NUM", buf, 1);

	memset(buf, 0, sizeof(buf));
	sprintf(buf, "%d", (int) setting.stateful_flag);
	setenv("INSPECT_STATEFUL_FLAG", buf, 1);

	max_tolerated_errors = setting.max_errors;

	retval = event_buffer.init(setting.socket_file, setting.timeout_val,
			setting.max_threads);

	assert(retval);
	return true;
}

void Scheduler::stop() {
	event_buffer.close();
}

/** 
 *  Restart the system under test
 */
void Scheduler::exec_test_target(const char* path) {
	int retval;
	int pid = fork();
	if (pid == 0) {
		int sub_pid = fork();
		if (sub_pid == 0) {
			//retval = ::execl(path, path, NULL);
			retval = ::execvp(setting.target_argv[0], setting.target_argv);
			assert(retval != -1);
		} else {
			ofstream os(".subpid");
			os << sub_pid << endl;
			os.close();
			exit(0);
		}
	} else {
		waitpid(pid, NULL, 0);
		ifstream is(".subpid");
		int sub_pid;
		is >> sub_pid;
		//cout << " sub_pid = " << sub_pid <<endl;
		system("rm -f .subpid");
		is.close();

		sut_pid = sub_pid;
	}
}
int inserted_in_backtrack = 0;

// The main function that is called after initialisation
void Scheduler::run() {
	cout << " === run " << run_counter << " ===\n";
	this->monitor_first_run();

	verbose(1, state_stack.toString2());

	while (state_stack.has_backtrack_points()) {
		run_counter++;

		if (num_of_errors_detected >= max_tolerated_errors)
			break;

		//if (run_counter % 10 == 0)
		cout << " === run " << run_counter << " ===\n";
		//	this->backtrack_checking();
		this->context_bound_SourceDPOR();
	}

	cout << "Total number of runs:  " << run_counter
			<< ",   killed-in-the-middle runs: " << num_killed << endl;
	cout << "Transitions explored: " << num_of_transitions << endl;
	cout << "Total No of times backtrack updated " << inserted_in_backtrack
			<< endl;
	cout << "Reached " << reached << " " << "reaching "  << reaching << endl;
}

void Scheduler::exec_transition(InspectEvent & event) {
	//this->approve_event(event);
	//cout << event.toString() << endl;
	event_buffer.approve(event);
}

State * Scheduler::get_initial_state() {
	State * init_state;

	InspectEvent event;

	init_state = new State();
	event = event_buffer.get_the_first_event();
	assert(event.type == THREAD_START);
	assert(event.thread_id == 0);

	init_state->add_to_enabled(event);
	init_state->clock_vectors.add_the_first_thread();

	return init_state;
}

InspectEvent Scheduler::get_latest_executed_event_of_thread(State * state,
		int thread_id) {
	State * ptr;
	InspectEvent dummy;

	ptr = state;
	while (ptr != NULL) {
		if (ptr->sel_event.thread_id == thread_id)
			return ptr->sel_event;
		ptr = ptr->prev;
	}

	return dummy;
}

void Scheduler::check_race(State * state) {
	if (state->check_race() && !setting.deadlock_only_flag) {
		//num_of_errors_detected++;
		cout << state_stack.toString3() << endl;

	}
}

// tapas, vansh -> changed the first run according to our implemented SourceDPOR.
void Scheduler::monitor_first_run() {
	// int thread_counter = 0;

	// int step_counter = 0;
	InspectEvent event;
	State * init_state, *new_state, *current_state;
	TransitionSet::iterator tit;

	/* Not exactly sure what this is doing */
	this->exec_test_target(setting.target.c_str());

	/* Gets the initial state from the event buffer
	 Event is received from the socket buffer
	 The following classes are implemented:
	 1.EventBuffer - Has a mapping of events to integer. Basically it is the class responsible for listening events from
	 2.Socket - Implements operators and various functions to receive the event
	 3.SocketIndex - A mapping between the threadId and the sockets
	 4.InspectEvent
	 An event has the following important fields :
	 type,
	 location(pc),
	 local_state_changed_flag, : Gives information when the type is local_info
	 point_no_of_the_symetry_point
	 obj_id of the object being read or written to, obj_id is mapped to the address(another class)

	 Important Points:
	 1. See inspect_event.def file

	 These are the various inspect_types
	 1.1 (State_Capturing_Thread_Start)
	 1.2 OBJ_Read/OBJ_WRITE
	 1.3 SYM_POINT
	 1.4 STATE_REQ  (Global)
	 1.5 STATE_ACK  (Global)
	 1.6 Related to Local_Info/Local_State_Begin/Local_State_End....

	 These are the various inspect_categories
	 1.1 EC_GLOBAL 1.2 EC_LOCAL 1.3 EC_OBJECT 1.4 EC_SYMMETRY ......

	 Thus on input of an event(Via Operator Socket Defined)..the following information is passed
	 1. location_id 2. wether the local state has been changed or not(delta information is not pased)
	 3. event type

	 Unused fields/functions wrt to the state changes
	 1. may_have_side_effects()
	 2. old_val
	 3. is_modification_by_const()
	 4.


	 5. Note dummy event defines that the events of that thread are over.

	 Get the initial state, gets the first event and then updates accordingly..
	 */
	init_state = this->get_initial_state();

	state_stack.push(init_state);

	//cout << " num of threads = " << thread_counter << flush << endl;

	try {
		current_state = state_stack.top();
		while (!current_state->is_enabled_empty()) {

			// returns the first event in the enabled set
			event = current_state->get_transition();

			// is a method of just updating the backtrack set.
			update_backtrack_SourceDPOR(current_state, event);
			cout << "Transition done : ";
			verbose(-1e9, event.toString());
			//	this->check_race(current_state);
			if (found_enough_error()) {
				kill(sut_pid, SIGTERM);
				return;
			}
			assert(current_state->is_enabled(event));

			/* next_state() does the following functions:
			 1.removes the current event from enabled.
			 2.exec(event) -- only does something in case of various types of locks
			 3.updates the symetry (this is not clear)
			 4.updates the sleepset, enabledset, disabledset as expected
			 5.approves the event some basic checks
			 6.sets the selected event of the present state
			 7.finds the next event of that thread id
			 7a.if the event has the type LOCAL_INFO
			 7b.handle symetry event...no idea what it does

			 */
			new_state = next_state(current_state, event, event_buffer);
			num_of_transitions++;
			state_stack.push(new_state);
			current_state = new_state;
		}

		// old code
		//		current_state = state_stack.top();
		//		    while (!current_state->is_enabled_empty()){
		//		      for (tit = current_state->prog_state->enabled.begin();
		//			   tit != current_state->prog_state->enabled.end();
		//			   tit++)
		//				{
		//				  event = tit->second;
		//				  update_backtrack_info(current_state, event);
		//				}
		//
		//		      event = current_state->prog_state->enabled.get_transition();
		//
		//		      assert(current_state->clock_vectors.size() > 0);
		//
		//				//       if (current_state->check_race()){
		//				// 	cout << state_stack.toString3() << endl;
		//				//       }
		////		      this->check_race(current_state);
		//		      if (found_enough_error()){
		//					kill(sut_pid, SIGTERM);
		//					return;
		//		      }
		//
		//		      new_state = next_state(current_state, event, event_buffer);
		//
		//		      // added about event graph
		//		      num_of_transitions++;
		//
		//		      if (event.type == THREAD_START) thread_counter++;
		//
		//				//       verbose(0, current_state->sel_event.toString());
		//				//       verbose(2, current_state->toString());
		//
		//				//       cout << "--------------- " << step_counter++ << ": " << event.toString() << " ---------\n"
		//				// 	   << state_stack.toString2() << endl;
		//
		//		      state_stack.push(new_state);
		//		      assert(new_state->prev == current_state);
		//		      current_state = new_state;
		//		    }

	} catch (DeadlockException & e) {
		kill(sut_pid, SIGTERM);
		sut_pid = -1;
	}

	//cout << state_stack.toString() << endl;

	// cout << " num of threads = " << thread_counter << flush << endl;
}

bool Scheduler::examine_state(State * old_state, State * new_state) {
	return false;
}

void Scheduler::filter_auxiliary_events(InspectEvent &event) {
	InspectEvent new_event;

	if (event.type == SYMM_POINT) {
		exec_transition(event);
		new_event = event_buffer.get_event(event.thread_id);
	} else
		new_event = event;

	event = new_event;
}

/* ----tapas, vansh ------
 This function return the set I(E,w) which
 contains all those processes(p) in w for which there is no event e in dom(state, w) such that e happens before next(state, p)
 The variable ans passed by reference contains the thread-ids.
 */
void Scheduler::return_min(State * state, vector<InspectEvent> &w,
		set<int> &ans) {
	for (vector<InspectEvent>::iterator it = w.begin(); it != w.end(); ++it) {
		InspectEvent nextEp = state->prog_state->enabled.get_transition(
				it->thread_id);
		if (nextEp.valid()) {
			//			cout << "Minimal check for ";
			//			verbose(-1e9, nextEp.toString());
			//			cout << "Checking against \n";
			int tobeadded = 1;
			for (vector<InspectEvent>::iterator it2 = w.begin(); it2 != w.end();
					++it2) {
				if (nextEp.thread_id == it2->thread_id)
					continue;
				InspectEvent e = *it2;
				assert(e.valid());

				//				verbose(-1e9, e.toString());

				int occured_before = 0;
				State* new_state = state;
				while (1) {
					if (new_state->sel_event.thread_id == e.thread_id) {
						occured_before = 1;
						break;
					}
					if (new_state->sel_event.thread_id == nextEp.thread_id) {
						occured_before = 0;
						break;
					}
					new_state = new_state->next;
				}

				if (occured_before
						&& (e.dependent(nextEp)
								|| e.thread_id == nextEp.thread_id)) {
					tobeadded = 0;
					break;
				}
			}

			//			cout << "Minimal check ans " << tobeadded << endl;
			if (tobeadded)
				ans.insert(it->thread_id);
		}
	}
}

// tapas, vansh
// This function find the nondependent set corresponding to event at E' & E (v = notdep(e,E)) & then calls the return_min function
// from the result it finds out if there is an event which is there in I(E', v) and also backtrack(E') then it doesn't do anything, 
// else it adds any one event from I(E', v) to the backtrack(E')
void Scheduler::backtrack_SourceDPOR_helper(State *E, State * old_state,
		InspectEvent &event, InspectEvent *event_at_E_) {

	vector<InspectEvent> thread_ids_notdep;
	State *new_state = old_state->next;
	while (new_state != E) {
		if (not (old_state->sel_event.dependent(new_state->sel_event)
				|| old_state->sel_event.thread_id
						== new_state->sel_event.thread_id)) {
			thread_ids_notdep.push_back(new_state->sel_event);
		}
		new_state = new_state->next;
	}
	thread_ids_notdep.push_back(event);

	set<int> minimal_processes;
	return_min(old_state, thread_ids_notdep, minimal_processes);
	InspectEvent to_insert;
	bool found = false;

	for (set<int>::iterator it = minimal_processes.begin();
			it != minimal_processes.end(); ++it) {
		InspectEvent nextEp = old_state->prog_state->enabled.get_transition(
				*it);
		assert(nextEp.valid());
		assert(old_state->is_enabled(nextEp));
		if (old_state->backtrack.has_member(nextEp)) {
			return;
		}
		if (old_state->done.has_member(nextEp)) {
			return;
		}
		if (nextEp.thread_id == old_state->sel_event.thread_id)
			continue;
		if (old_state->sleepset.has_member(nextEp))
			continue;
		Lockset * lockset1, *lockset2;
		lockset1 = old_state->get_lockset(old_state->sel_event.thread_id);
		lockset2 = E->get_lockset(event.thread_id);
		if (is_mutex_exclusive_locksets(lockset1, lockset2)) {
			continue;
		}
		if (not found) {
			to_insert = nextEp;
			found = true;
		}

	}
	if (found) {
		assert(to_insert.valid());
		assert(old_state->is_enabled(to_insert));
		assert(not old_state->backtrack.has_member(to_insert));
		assert(not old_state->is_disabled(to_insert));
		assert(not old_state->sleepset.has_member(to_insert));
		inserted_in_backtrack++;
		old_state->backtrack.insert(to_insert);
	}
}

// ------tapas, vansh-----
//This function checks for all events, e in dom(E), such that if in some other event sequence if event occurs just before e
//then e is not blocked before the occurrence of event.(This is done be checking no other event occurs which lies in b/w 
// these 2 in terms of happens-before transition & race reversal) If the above condition is true, it calls the  function backtrack_SDPOR_helper 

void Scheduler::update_backtrack_SourceDPOR(State *E, InspectEvent &event) {
	State *old_state = E->prev;
	while (old_state != NULL) {
		// checking happens before
		if (event.thread_id != old_state->sel_event.thread_id
				&& old_state->sel_event.dependent(event)) {

			State *new_state = old_state->next;
			int incondition = 1;
			while (new_state != E) {
				if ((old_state->sel_event.dependent(new_state->sel_event)
						|| old_state->sel_event.thread_id
								== new_state->sel_event.thread_id)
						&& (new_state->sel_event.dependent(event)
								|| new_state->sel_event.thread_id
										== event.thread_id)) {
					incondition = 0;
					break;
				}
				new_state = new_state->next;
			}

			// now to check if race can be reversed
			if (incondition) {
				InspectEvent alt_event =
						old_state->prog_state->enabled.get_transition(
								event.thread_id);
				if (alt_event.invalid()) {
					alt_event = old_state->prog_state->disabled.get_transition(
							event.thread_id);
					if (alt_event.invalid()) {
						//						cout << "Invalid encountered \n";
						old_state = old_state->prev;
						continue;
					}
				}

				assert(alt_event.thread_id != old_state->sel_event.thread_id);
				if (old_state->sleepset.has_member(alt_event)) {
					old_state = old_state->prev;
					continue;
				}

				ClockVector *vec1, *vec2;
				vec1 = old_state->get_clock_vector(
						old_state->sel_event.thread_id);
				vec2 = E->get_clock_vector(event.thread_id);

				if (vec1->is_concurrent_with(*vec2)) {
					backtrack_SourceDPOR_helper(E, old_state, event,
							&old_state->sel_event);
				}
			}
		}
		old_state = old_state->prev;
	}
}

/*

 bool Scheduler::check_state_exists_in_hash(State* state)
 {
 // Note at present prog_state has the same structure as that of the arbitary state....

 if (h_table.has_member(state->prog_state))
 {
 vector<InspectEvent> reachable;
 TransitionSet::iterator it;
 for (it = state->prog_state->enabled.begin();
 it != state->prog_state->enabled.end(); it++)
 {
 InspectEvent event_x = it->second;
 // updates the reachable vector for every enabled event
 g_visible_operation.get_following_events(event_x, reachable);
 }
 vector<InspectEvent>::iterator it2;
 for (it2 = reachable.begin(); it2 != reachable.end(); ++it2)
 {
 // check if calling this instead of backtrack checking is ok...
 loopmain(state, *it2);
 }
 return true;
 }
 return false;
 }

 // Tapas, Vansh : This function is the incomplete implementation of the StatefulDPOR() with the backtracking as SourceSet DPOR().
 // Please check code comments to know the parts required for completion

 void Scheduler::StatefulDPOR()
 {
 State * state = NULL, *current_state = NULL, * new_state = NULL;
 InspectEvent event,event2;


 int depth, i;

 event_buffer.reset();
 thread_table.reset();

 state = state_stack.top();
 


 while (state != NULL && state->backtrack.empty()) {
 state_stack.pop();

 //cout << "Printing states to delete ";
 //verbose(-1e9, state->sel_event.toString());

 delete state;
 state = state_stack.top();
 }

 depth = state_stack.depth();

 this->exec_test_target(setting.target.c_str());
 event = event_buffer.get_the_first_event();
 exec_transition(event);

 // running previous iterations -- vansh
 for (i = 1; i < depth-1; i++) {
 state = state_stack[i];
 cout << "OLDER ";
 if (state->sel_event.type == THREAD_CREATE) {
 event = state->sel_event;
 verbose(-1e9, event.toString());
 InspectEvent pre, post, first;
 pre = event_buffer.get_event(event.thread_id);
 verbose(-1e9, pre.toString());
 exec_transition(pre);

 post = event_buffer.get_event(event.thread_id);
 verbose(-1e9, post.toString());
 exec_transition(post);

 first = event_buffer.get_event(event.child_id);
 verbose(-1e9, first.toString());
 string thread_nm;

 filter_auxiliary_events(first);

 exec_transition(first);

 thread_table.add_thread(post.child_id, thread_nm, post.thread_arg);
 } else {
 event = event_buffer.get_event(state->sel_event.thread_id);
 filter_auxiliary_events(event);
 verbose(-1e9, event.toString());

 assert(event.valid());
 assert(event == state->sel_event);
 exec_transition(event);
 }
 }

 assert(state_stack[depth - 1] == state_stack.top());

 state = state_stack.top();

 TransitionSet::iterator it;
 for (it = state->prog_state->enabled.begin();
 it != state->prog_state->enabled.end(); it++) {
 event = it->second;
 event2 = event_buffer.get_event(event.thread_id);

 filter_auxiliary_events(event2);

 assert(event == event2);
 }


 for (it = state->prog_state->disabled.begin();
 it != state->prog_state->disabled.end(); it++) {
 event = it->second;
 event2 = event_buffer.get_event(event.thread_id);

 filter_auxiliary_events(event2);
 assert(event == event2);
 }

 // state is the top of the state_stack

 // checks if the hash_table has the same program state
 // may be the h_table implementation is not and can't be checked in this way


 // ________CHECK THIS____________________



 // Confirm from Sir ->>
 // We wont be checking wether the state at which we start again the backtrack is in the hash table...
 // Reason is : This state has already been checked and would definitely belong to the hash table...We have updated the backtrack set due to it and for it.



 event = state->backtrack.get_transition();
 this->check_race(state);
 if (found_enough_error()) {
 kill(sut_pid, SIGTERM);
 return;
 }

 assert(state->is_enabled(event));
 current_state = next_state(state, event, event_buffer);
 state->backtrack.remove(event);

 cout << "Transition done of backtrack : ";
 verbose(-1e9, event.toString());
 num_of_transitions++;

 state_stack.push(current_state);




 while (current_state->has_executable_transition()) {


 // first check if current state belongs to the hash
 bool repeated_state = check_state_exists_in_hash(current_state);
 if (repeated_state) break;

 // insert the program state in the hash table
 h_table.insert(current_state->prog_state);

 event = current_state->get_transition();
 // update_backtrack(current_state, event);
 cout << "Transition done : ";
 verbose(-1e9, event.toString());
 if (found_enough_error()) {
 kill(sut_pid, SIGTERM);
 return;
 }
 assert(current_state->is_enabled(event));
 new_state = next_state(current_state, event, event_buffer);
 num_of_transitions++;



 // get delta_h
 // get x = NextLocalState(....)
 // get arbitary state and include it in the structure of the state...we ll add an arbitary state class in the state class itself...


 int current_state_index = state_stack.depth()-1;
 int prev_state_index = state_stack.depth()-2;

 // prev_state -u-> current_state -v-> new_state
 if (prev_state_index >= 0)
 {
 InspectEvent u = state_stack[prev_state_index]->sel_event;
 InspectEvent v = state_stack[current_state_index]->sel_event;
 assert(event == v);
 g_visible_operation.add_edge(u, v);
 }

 // note at present the program_state is used instead of the arbitary state.. this is to change..moreover state itself contains the arbitary state
 state_stack.push(new_state);
 }    current_state = new_state;


 // Copied from SourceDPOR
 if (!current_state->prog_state->disabled.empty()
 && current_state->sleepset.empty()) {

 if (!setting.race_only_flag) {
 num_of_errors_detected++;

 cout << "Found a deadlock!!\n";
 cout << state_stack.toString3() << endl;
 }

 }

 if (!current_state->has_been_end()) {
 kill(sut_pid, SIGTERM);
 cout << "Kill  " << sut_pid << flush << endl;
 num_killed++;
 }
 }

 */

// tapas, vansh
// This function is the core of SPOR
// It is called to go to each new interleaving and add the correct events to the backtrack set.
void Scheduler::SourceDPOR() {
	State * state = NULL, *current_state = NULL, *new_state = NULL;
	InspectEvent event, event2;
	int depth, i;

	event_buffer.reset();
	thread_table.reset();

	state = state_stack.top();
	while (state != NULL && state->backtrack.empty()) {
		state_stack.pop();
		//cout << "Printing states to delete ";
		//verbose(-1e9, state->sel_event.toString());
		delete state;
		state = state_stack.top();
	}

	depth = state_stack.depth();
	//	cout << "Depth " << depth << endl;

	this->exec_test_target(setting.target.c_str());
	event = event_buffer.get_the_first_event();
	exec_transition(event);

	//	verbose(-1e9, event.toString());

	// running previous iterations -- vansh
	for (i = 1; i < depth - 1; i++) {
		state = state_stack[i];
		cout << "OLDER ";
		if (state->sel_event.type == THREAD_CREATE) {
			event = state->sel_event;
			verbose(-1e9, event.toString());
			InspectEvent pre, post, first;
			pre = event_buffer.get_event(event.thread_id);
			verbose(-1e9, pre.toString());
			exec_transition(pre);

			post = event_buffer.get_event(event.thread_id);
			verbose(-1e9, post.toString());
			exec_transition(post);

			first = event_buffer.get_event(event.child_id);
			verbose(-1e9, first.toString());
			string thread_nm;

			filter_auxiliary_events(first);

			exec_transition(first);

			thread_table.add_thread(post.child_id, thread_nm, post.thread_arg);
		} else {
			event = event_buffer.get_event(state->sel_event.thread_id);
			filter_auxiliary_events(event);
			verbose(-1e9, event.toString());

			assert(event.valid());
			assert(event == state->sel_event);
			exec_transition(event);
		}
	}

	assert(state_stack[depth - 1] == state_stack.top());

	state = state_stack.top();

	TransitionSet::iterator it;
	for (it = state->prog_state->enabled.begin();
			it != state->prog_state->enabled.end(); it++) {
		event = it->second;
		event2 = event_buffer.get_event(event.thread_id);

		filter_auxiliary_events(event2);

		assert(event == event2);
		//cout << " ++++ " << event2.toString() << endl;
	}

	for (it = state->prog_state->disabled.begin();
			it != state->prog_state->disabled.end(); it++) {
		event = it->second;
		event2 = event_buffer.get_event(event.thread_id);

		filter_auxiliary_events(event2);
		assert(event == event2);
	}

	//	event = InspectEvent::dummyEvent;
	//	for(hash_map<int, InspectEvent, hash<int> >::iterator it = state->backtrack.events.begin(); it!=state->backtrack.events.end(); ++it) {
	//		event = it->second;
	//		assert(event.valid());
	//		if(not state->sleepset.has_member(event)) {
	//			break;
	//		}
	//	}

	//	assert(event.valid());
	//
	event = state->backtrack.get_transition();
	//	while(state->sleepset.has_member(event)) {
	//
	//	}
	//	assert(event.valid());
	// state->backtrack.remove(event);

	this->check_race(state);
	if (found_enough_error()) {
		kill(sut_pid, SIGTERM);
		return;
	}

	assert(state->is_enabled(event));

	// next_state function takes care of the updates to the enabled state, the backtrack state..etc
	current_state = next_state(state, event, event_buffer);

	// Imp Step to remove the event from the backtrack
	state->backtrack.remove(event);

	cout << "Transition done of backtrack : ";
	verbose(-1e9, event.toString());
	num_of_transitions++;

	//
	//   important!! we need to be careful here,
	//   to put backtrack transition into
	//  state->sleepset.remove(event); // why is this happening??
	state_stack.push(current_state);

	while (current_state->has_executable_transition()) {

		event = current_state->get_transition();
		update_backtrack_SourceDPOR(current_state, event);
		cout << "Transition done : ";
		verbose(-1e9, event.toString());
		//			this->check_race(state);
		if (found_enough_error()) {
			kill(sut_pid, SIGTERM);
			return;
		}
		assert(current_state->is_enabled(event));
		new_state = next_state(current_state, event, event_buffer);
		num_of_transitions++;
		state_stack.push(new_state);
		current_state = new_state;
	}

	//	cout << "******************Out of loop*****************" << endl;

	//verbose(1, state_stack.toString2());
	if (!current_state->prog_state->disabled.empty()
			&& current_state->sleepset.empty()) {

		//		cout << "Well I am here \n";
		if (!setting.race_only_flag) {
			num_of_errors_detected++;

			cout << "Found a deadlock!!\n";
			cout << state_stack.toString3() << endl;
		}

	}
	//	cout << "************* " << current_state->prog_state->enabled.size() << " " << current_state->prog_state->disabled.size() << endl;
	if (!current_state->has_been_end()) {
		kill(sut_pid, SIGTERM);
		cout << "Kill  " << sut_pid << flush << endl;
		num_killed++;
	}
	//	cout << "Completed transition again \n";
}

void Scheduler::backtrack_context_bound_SourceDPOR_helper(State *E,
		State * old_state, InspectEvent &event, InspectEvent *event_at_E_) {

	vector<InspectEvent> thread_ids_notdep;
	State *new_state = old_state->next;
	while (new_state != E) {
		if (not (old_state->sel_event.dependent(new_state->sel_event)
				|| old_state->sel_event.thread_id
						== new_state->sel_event.thread_id)) {
			thread_ids_notdep.push_back(new_state->sel_event);
		}
		new_state = new_state->next;
	}
	thread_ids_notdep.push_back(event);

	set<int> minimal_processes;
	return_min(old_state, thread_ids_notdep, minimal_processes);
	InspectEvent to_insert;
	bool found = false;

	for (set<int>::iterator it = minimal_processes.begin();
			it != minimal_processes.end(); ++it) {
		InspectEvent nextEp = old_state->prog_state->enabled.get_transition(
				*it);
		assert(nextEp.valid());
		assert(old_state->is_enabled(nextEp));
		if (old_state->backtrack.has_member(nextEp)) {
			return;
		}
		if (old_state->done.has_member(nextEp)) {
			return;
		}
		if (nextEp.thread_id == old_state->sel_event.thread_id)
			continue;
		if (old_state->sleepset.has_member(nextEp))
			continue;
		Lockset * lockset1, *lockset2;
		lockset1 = old_state->get_lockset(old_state->sel_event.thread_id);
		lockset2 = E->get_lockset(event.thread_id);
		if (is_mutex_exclusive_locksets(lockset1, lockset2)) {
			continue;
		}
		if (not found) {
			to_insert = nextEp;
			found = true;
		}

	}
	if (found) {
		assert(to_insert.valid());
		assert(old_state->is_enabled(to_insert));
		assert(not old_state->backtrack.has_member(to_insert));
		assert(not old_state->is_disabled(to_insert));
		assert(not old_state->sleepset.has_member(to_insert));
		inserted_in_backtrack++;

		if (old_state->context_switches
				< CONTEXT_BOUND|| old_state->prev == NULL) {
			old_state->backtrack.insert(to_insert);
		}
		else if (old_state->prev != NULL) {
			State* temp = old_state->prev;
			while (temp->prev != NULL) {
				if (temp->context_switches < CONTEXT_BOUND)
					break;
				temp = temp->prev;
			}

			Lockset * lockset1, *lockset2;
			lockset1 = temp->get_lockset(temp->sel_event.thread_id);
			lockset2 = E->get_lockset(event.thread_id);
			if (is_mutex_exclusive_locksets(lockset1, lockset2)
					|| temp->sleepset.has_member(to_insert)
					|| temp->backtrack.has_member(to_insert)
					|| temp->done.has_member(to_insert)
					|| to_insert.thread_id == temp->sel_event.thread_id) {
				cout << "I am reachin here again.\n";
				reaching++;
				return;
			}

			if (temp->is_enabled(to_insert)) {
				assert(not temp->backtrack.has_member(to_insert));
				assert(not temp->is_disabled(to_insert));
				assert(not temp->sleepset.has_member(to_insert));
				cout << "I have reacheded here.\n";
				reached++;
				temp->backtrack.insert(to_insert);
			} else {
				cout << "I do not want to reachikj here.\n";
				for(TransitionSet::iterator it=temp->prog_state->enabled.events.begin(); it!=temp->prog_state->enabled.events.end(); ++it) {
					if( temp->sleepset.has_member(it->second)
							|| temp->backtrack.has_member(it->second)
							|| temp->done.has_member(it->second)
							|| !(it->second.can_enable(temp->sel_event)))
						continue;
					temp->backtrack.insert(it->second);
				}
			}

		}
	}
}

void Scheduler::update_backtrack_context_bound_SourceDPOR(State *E,
		InspectEvent &event) {
	State *old_state = E->prev;
	while (old_state != NULL) {
		// checking happens before
		if (event.thread_id != old_state->sel_event.thread_id
				&& old_state->sel_event.dependent(event)) {

			State *new_state = old_state->next;
			int incondition = 1;
			while (new_state != E) {
				if ((old_state->sel_event.dependent(new_state->sel_event)
						|| old_state->sel_event.thread_id
								== new_state->sel_event.thread_id)
						&& (new_state->sel_event.dependent(event)
								|| new_state->sel_event.thread_id
										== event.thread_id)) {
					incondition = 0;
					break;
				}
				new_state = new_state->next;
			}

			// now to check if race can be reversed
			if (incondition) {
				InspectEvent alt_event =
						old_state->prog_state->enabled.get_transition(
								event.thread_id);
				if (alt_event.invalid()) {
					alt_event = old_state->prog_state->disabled.get_transition(
							event.thread_id);
					if (alt_event.invalid()) {
						//						cout << "Invalid encountered \n";
						old_state = old_state->prev;
						continue;
					}
				}

				assert(alt_event.thread_id != old_state->sel_event.thread_id);
				if (old_state->sleepset.has_member(alt_event)) {
					old_state = old_state->prev;
					continue;
				}

				ClockVector *vec1, *vec2;
				vec1 = old_state->get_clock_vector(
						old_state->sel_event.thread_id);
				vec2 = E->get_clock_vector(event.thread_id);

				if (vec1->is_concurrent_with(*vec2)) {
					backtrack_context_bound_SourceDPOR_helper(E, old_state,
							event, &old_state->sel_event);
				}
			}
		}
		old_state = old_state->prev;
	}
}

void Scheduler::context_bound_SourceDPOR() {
	State * state = NULL, *current_state = NULL, *new_state = NULL;
	InspectEvent event, event2;
	int depth, i;

	event_buffer.reset();
	thread_table.reset();

	state = state_stack.top();
	while (state != NULL && state->backtrack.empty()) {
		state_stack.pop();
		//cout << "Printing states to delete ";
		//verbose(-1e9, state->sel_event.toString());
		delete state;
		state = state_stack.top();
	}

	depth = state_stack.depth();
	//	cout << "Depth " << depth << endl;

	this->exec_test_target(setting.target.c_str());
	event = event_buffer.get_the_first_event();
	exec_transition(event);

	//	verbose(-1e9, event.toString());

	// running previous iterations -- vansh
	for (i = 1; i < depth - 1; i++) {
		state = state_stack[i];
		cout << "OLDER ";
		if (state->sel_event.type == THREAD_CREATE) {
			event = state->sel_event;
			verbose(-1e9, event.toString());
			InspectEvent pre, post, first;
			pre = event_buffer.get_event(event.thread_id);
			verbose(-1e9, pre.toString());
			exec_transition(pre);

			post = event_buffer.get_event(event.thread_id);
			verbose(-1e9, post.toString());
			exec_transition(post);

			first = event_buffer.get_event(event.child_id);
			verbose(-1e9, first.toString());
			string thread_nm;

			filter_auxiliary_events(first);

			exec_transition(first);

			thread_table.add_thread(post.child_id, thread_nm, post.thread_arg);
		} else {
			event = event_buffer.get_event(state->sel_event.thread_id);
			filter_auxiliary_events(event);
			verbose(-1e9, event.toString());

			assert(event.valid());
			assert(event == state->sel_event);
			exec_transition(event);
		}
	}

	assert(state_stack[depth - 1] == state_stack.top());

	state = state_stack.top();

	TransitionSet::iterator it;
	for (it = state->prog_state->enabled.begin();
			it != state->prog_state->enabled.end(); it++) {
		event = it->second;
		event2 = event_buffer.get_event(event.thread_id);

		filter_auxiliary_events(event2);

		assert(event == event2);
		//cout << " ++++ " << event2.toString() << endl;
	}

	for (it = state->prog_state->disabled.begin();
			it != state->prog_state->disabled.end(); it++) {
		event = it->second;
		event2 = event_buffer.get_event(event.thread_id);

		filter_auxiliary_events(event2);
		assert(event == event2);
	}

	//	event = InspectEvent::dummyEvent;
	//	for(hash_map<int, InspectEvent, hash<int> >::iterator it = state->backtrack.events.begin(); it!=state->backtrack.events.end(); ++it) {
	//		event = it->second;
	//		assert(event.valid());
	//		if(not state->sleepset.has_member(event)) {
	//			break;
	//		}
	//	}

	//	assert(event.valid());
	//
	event = state->backtrack.get_transition();
	//	while(state->sleepset.has_member(event)) {
	//
	//	}
	//	assert(event.valid());
	// state->backtrack.remove(event);

	this->check_race(state);
	if (found_enough_error()) {
		kill(sut_pid, SIGTERM);
		return;
	}

	assert(state->is_enabled(event));

	// next_state function takes care of the updates to the enabled state, the backtrack state..etc
	current_state = next_state(state, event, event_buffer);

	// Imp Step to remove the event from the backtrack
	state->backtrack.remove(event);

	cout << "Transition done of backtrack : ";
	verbose(-1e9, event.toString());
	num_of_transitions++;

	//
	//   important!! we need to be careful here,
	//   to put backtrack transition into
	//  state->sleepset.remove(event); // why is this happening??
	state_stack.push(current_state);

	while (current_state->has_executable_transition()) {
		if (current_state->prev != NULL) {
			int id1 = current_state->prev->sel_event.thread_id;
			event = current_state->prog_state->enabled.get_transition(id1);
			if (event.invalid() || current_state->sleepset.has_member(event))
				event = current_state->get_transition();
		} else {
			event = current_state->get_transition();
		}

		update_backtrack_context_bound_SourceDPOR(current_state, event);
		cout << "Transition done : ";
		verbose(-1e9, event.toString());
		//			this->check_race(state);
		if (found_enough_error()) {
			kill(sut_pid, SIGTERM);
			return;
		}
		assert(current_state->is_enabled(event));
		new_state = next_state(current_state, event, event_buffer);

		assert(current_state->context_switches == new_state->context_switches);

		num_of_transitions++;
		state_stack.push(new_state);
		current_state = new_state;
	}

	//	cout << "******************Out of loop*****************" << endl;

	//verbose(1, state_stack.toString2());
	if (!current_state->prog_state->disabled.empty()
			&& current_state->sleepset.empty()) {

		//		cout << "Well I am here \n";
		if (!setting.race_only_flag) {
			num_of_errors_detected++;

			cout << "Found a deadlock!!\n";
			cout << state_stack.toString3() << endl;
		}

	}
	//	cout << "************* " << current_state->prog_state->enabled.size() << " " << current_state->prog_state->disabled.size() << endl;
	if (!current_state->has_been_end()) {
		kill(sut_pid, SIGTERM);
		cout << "Kill  " << sut_pid << flush << endl;
		num_killed++;
	}
	//	cout << "Completed transition again \n";

}

void Scheduler::backtrack_checking() {
	State * state = NULL, *current_state = NULL, *new_state = NULL;
	InspectEvent event, event2;
	int depth, i;

	event_buffer.reset();
	thread_table.reset();

	state = state_stack.top();
	while (state != NULL && state->backtrack.empty()) {
		state_stack.pop();
		delete state;
		state = state_stack.top();
	}

	depth = state_stack.depth();

	this->exec_test_target(setting.target.c_str());
	event = event_buffer.get_the_first_event();
	exec_transition(event);

	// running previous iterations -- vansh
	for (i = 1; i < depth - 1; i++) {
		state = state_stack[i];
		verbose(-1e9, event.toString());
		if (state->sel_event.type == THREAD_CREATE) {
			event = state->sel_event;
			InspectEvent pre, post, first;
			pre = event_buffer.get_event(event.thread_id);
			exec_transition(pre);

			post = event_buffer.get_event(event.thread_id);
			exec_transition(post);

			first = event_buffer.get_event(event.child_id);
			string thread_nm;

			filter_auxiliary_events(first);

			exec_transition(first);

			thread_table.add_thread(post.child_id, thread_nm, post.thread_arg);
		} else {
			event = event_buffer.get_event(state->sel_event.thread_id);
			filter_auxiliary_events(event);

			assert(event.valid());
			assert(event == state->sel_event);
			exec_transition(event);
		}
	}

	assert(state_stack[depth - 1] == state_stack.top());

	state = state_stack.top();

	TransitionSet::iterator it;
	for (it = state->prog_state->enabled.begin();
			it != state->prog_state->enabled.end(); it++) {
		event = it->second;
		event2 = event_buffer.get_event(event.thread_id);

		filter_auxiliary_events(event2);

		assert(event == event2);
		//cout << " ++++ " << event2.toString() << endl;
	}

	for (it = state->prog_state->disabled.begin();
			it != state->prog_state->disabled.end(); it++) {
		event = it->second;
		event2 = event_buffer.get_event(event.thread_id);

		filter_auxiliary_events(event2);
		assert(event == event2);
	}

	event = state->backtrack.get_transition();
	assert(event.valid());
	state->backtrack.remove(event);

	//   verbose(0, event.toString());

	//	this->check_race(state);
	//	if (found_enough_error()) {
	//		kill(sut_pid, SIGTERM);
	//		return;
	//	}

	current_state = next_state(state, event, event_buffer);

	num_of_transitions++;
	//   important!! we need to be careful here,
	//   to put backtrack transition into
	state->sleepset.remove(event);
	state_stack.push(current_state);

	while (current_state->has_executable_transition()) {
		TransitionSet::iterator tit;
		for (tit = current_state->prog_state->enabled.begin();
				tit != current_state->prog_state->enabled.end(); tit++) {
			event = tit->second;
			update_backtrack_info(current_state, event);
		}

		event = current_state->get_transition();
		//     verbose(0, event.toString());

		//		cout << current_state->sel_event.thread_id << endl;

		//		this->check_race(state);
		if (found_enough_error()) {
			kill(sut_pid, SIGTERM);
			return;
		}

		new_state = next_state(current_state, event, event_buffer);
		num_of_transitions++;
		state_stack.push(new_state);
		current_state = new_state;
	}

	//cout << state_stack.toString() << endl;

	verbose(1, state_stack.toString2());

	if (!current_state->prog_state->disabled.empty()
			&& current_state->sleepset.empty()) {

		if (!setting.race_only_flag) {
			num_of_errors_detected++;

			cout << "Found a deadlock!!\n";
			cout << state_stack.toString3() << endl;
		}

	}

	if (!current_state->has_been_end()) {
		kill(sut_pid, SIGTERM);
		//cout << "Kill  " << sut_pid << flush << endl;
		num_killed++;
	}
}

void Scheduler::get_mutex_protected_events(State * state1, State * state2,
		int tid, vector<InspectEvent> & events) {
	State * state = NULL;

	assert(state1 != NULL);
	assert(state2 != NULL);

	events.clear();
	state = state1->next;
	while (state != state2) {
		if (state->sel_event.thread_id == tid)
			events.push_back(state->sel_event);
		state = state->next;
	}

	assert(state == state2);
}

void Scheduler::update_backtrack_info(State * state, InspectEvent &event) {
	standard_update_backtrack_info(state, event);
}

bool Scheduler::is_mutex_exclusive_locksets(Lockset * lockset1,
		Lockset * lockset2) {
	if (lockset1 == NULL || lockset2 == NULL)
		return false;
	return lockset1->mutual_exclusive(*lockset2);
}

void Scheduler::standard_update_backtrack_info(State * state,
		InspectEvent &event) {
	State * old_state;
	InspectEvent alt_event;
	Lockset * lockset1, *lockset2;
	ClockVector * vec1, *vec2;

	old_state = state->prev;
	while (old_state != NULL) {
		if (dependent(old_state->sel_event, event))
			break;
		old_state = old_state->prev;
	}

	if (old_state == NULL)
		return;

	alt_event = old_state->prog_state->enabled.get_transition(event.thread_id);
	if (alt_event.invalid()) {
		cout << "Invalid alternate event in backtrack" << endl;
		lockset1 = old_state->get_lockset(old_state->sel_event.thread_id);
		lockset2 = state->get_lockset(event.thread_id);
		if (is_mutex_exclusive_locksets(lockset1, lockset2))
			return;

		vec1 = old_state->get_clock_vector(old_state->sel_event.thread_id);
		vec2 = state->get_clock_vector(event.thread_id);
		if (!vec1->is_concurrent_with(*vec2))
			return;

		TransitionSet::iterator tit;
		InspectEvent tmp_ev;

		for (tit = old_state->prog_state->enabled.begin();
				tit != old_state->prog_state->enabled.end(); tit++) {
			tmp_ev = tit->second;
			if (tmp_ev.invalid())
				continue;
			if (old_state->done.has_member(tmp_ev))
				continue;
			if (old_state->sleepset.has_member(tmp_ev))
				continue;
			old_state->backtrack.insert(tmp_ev);

		}
		return;
	}

	if (alt_event.thread_id == old_state->sel_event.thread_id)
		return;

	if (old_state->sleepset.has_member(alt_event))
		return;
	if (old_state->done.has_member(alt_event)
			|| old_state->backtrack.has_member(alt_event))
		return;

	lockset1 = old_state->get_lockset(old_state->sel_event.thread_id);
	lockset2 = state->get_lockset(event.thread_id);
	if (is_mutex_exclusive_locksets(lockset1, lockset2))
		return;

	vec1 = old_state->get_clock_vector(old_state->sel_event.thread_id);
	vec2 = state->get_clock_vector(event.thread_id);
	if (!vec1->is_concurrent_with(*vec2))
		return;

	if (setting.symmetry_flag) {
		if (thread_table.is_symmetric_threads(alt_event.thread_id,
				old_state->sel_event.thread_id)) {
			InspectEvent symm1, symm2;
			symm1 = old_state->symmetry.get_transition(alt_event.thread_id);
			symm2 = old_state->symmetry.get_transition(
					old_state->sel_event.thread_id);
			if (symm1.valid() && symm2.valid() && symm1.obj_id == symm2.obj_id)
				return;
		}
	}

	old_state->backtrack.insert(alt_event);
}

bool Scheduler::dependent(InspectEvent &e1, InspectEvent &e2) {
	bool retval = false;

	retval = e1.dependent(e2);

	//   cout << "e1: " << e1.toString() << "  "
	//        << "e2: " << e2.toString() << "  " << (retval? "true" : "false") << endl;

	return retval;
}

