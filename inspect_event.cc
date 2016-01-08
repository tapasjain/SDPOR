#include "inspect_event.hh"
#include "inspect_exception.hh"
#include <sstream>
#include <string> 
#include <iostream>
#include <fstream>
#include <iomanip>
#include <utility>

#include "scheduler_setting.hh"

using namespace std;

const int DummyLocation = -1;


#define DEF_EVENT_CODE(EV, STR, EV_PERM, CAT)   CAT,

EventCategory  events_cat[] = {
  #include "inspect_event.def"
  EC_ERROR
};

#undef DEF_EVENT_CODE


EventCategory  get_event_category(EventType  et)
{
  int idx;
  idx = (int)et;
  return events_cat[idx];
}



#define DEF_EVENT_CODE(EV, STR, EV_PERM, CAT)   STR, 
const char * eventstr[] = { 
  #include "inspect_event.def"
  "unknown"
};
#undef DEF_EVENT_CODE


string  event_type_to_string(EventType et)
{
  int idx;
  idx = (int)et;
  return eventstr[idx];
}


EventPermitType  permit_type(enum EventType type)
{
  EventPermitType permit;
  permit = (EventPermitType)(type);
  return permit;
}


InspectEvent  InspectEvent::dummyEvent;


InspectEvent::InspectEvent()
{ 
  this->init(); 
}

void InspectEvent::init()
{
  type = UNKNOWN;
  thread_id = -1;
  mutex_id = -1;
  cond_id = -1;
  location_id = -1;
  local_state_changed_flag = true;
  event_toremove = NULL;
}

void InspectEvent::init_mutex_init(int tid, int mid)
{
  type = MUTEX_INIT;
  thread_id = tid;
  mutex_id = mid;
}


void InspectEvent::init_mutex_destroy(int tid, int mid)
{
  type = MUTEX_DESTROY;
  thread_id = tid;
  mutex_id = mid;
}


void InspectEvent::init_mutex_lock(int tid, int mid)
{
  type = MUTEX_LOCK;
  thread_id = tid;
  mutex_id = mid;
}


void InspectEvent::init_mutex_trylock(int tid, int mid)
{
  type = MUTEX_TRYLOCK;
  thread_id = tid;
  mutex_id = mid;
}



void InspectEvent::init_mutex_unlock(int tid, int mid)
{
  type = MUTEX_UNLOCK;
  thread_id = tid;
  mutex_id = mid;  
}


void InspectEvent::init_cond_init(int tid, int cid)
{
  type = COND_INIT;
  thread_id = tid;
  cond_id = cid;
}


void InspectEvent::init_cond_destroy(int tid, int cid)
{
  type = COND_DESTROY;
  thread_id = tid;
  cond_id = cid;
}


void InspectEvent::init_cond_wait(int tid, int cid, int mid)
{
  type = COND_WAIT;
  thread_id = tid;
  cond_id = cid;
  mutex_id = mid;
}


void InspectEvent::init_cond_signal(int tid, int cid)
{
  type = COND_SIGNAL;
  thread_id = tid;
  cond_id = cid;
}


void InspectEvent::init_cond_broadcast(int tid, int cid)
{
  type = COND_BROADCAST;
  thread_id = tid;
  cond_id = cid;
}


void InspectEvent::init_cond_timedwait(int tid, int cid)
{
  type = COND_WAIT;
  thread_id = tid;
  cond_id = cid;
}


/*
 *  a pthread_cond_wait operation is split into three parts:
 *   
 */
void InspectEvent::init_cond_wait_lock(int tid, int cid, int mid)
{
  type = COND_WAIT_LOCK;
  thread_id = tid;
  cond_id = cid;
  mutex_id = mid;
}


void InspectEvent::init_cond_wait_unlock(int tid, int cid, int mid)
{
  type = COND_WAIT_UNLOCK;
  thread_id = tid;
  cond_id = cid;
  mutex_id = mid;
}


void InspectEvent::init_rwlock_init(int tid, int rwid)
{
  type = RWLOCK_INIT;
  thread_id = tid;
  rwlock_id = rwid;
}


void InspectEvent::init_rwlock_destroy(int tid,int rwlid)
{
  type = RWLOCK_DESTROY;
  thread_id = tid;
  rwlock_id= rwlid;
}


void InspectEvent::init_rwlock_rdlock(int tid, int rwlid)
{
  type = RWLOCK_RDLOCK;
  thread_id = tid;
  rwlock_id = rwlid;
}


void InspectEvent::init_rwlock_wrlock(int tid, int rwlid)
{
  type = RWLOCK_WRLOCK;
  thread_id = tid;
  rwlock_id = rwlid;
}


void InspectEvent::init_rwlock_unlock(int tid, int rwlid)
{
  type = RWLOCK_UNLOCK;
  thread_id = tid;
  rwlock_id = rwlid;
}

 
void InspectEvent::init_thread_start(int tid)
{
  type = THREAD_START;
  thread_id = tid;
}


void InspectEvent::init_thread_end(int tid)
{
  type = THREAD_END;
  thread_id = tid;
}


void InspectEvent::init_thread_create(int tid, int cid)
{
  type = THREAD_CREATE;
  thread_id = tid;
  child_id = cid;
}


void InspectEvent::init_thread_pre_create(int tid)
{
  type = THREAD_PRE_CREATE;
  thread_id = tid;
}


void InspectEvent::init_thread_post_create(int tid, int cid)
{
  type = THREAD_POST_CREATE;
  thread_id = tid;
  child_id = cid;
}


void InspectEvent::init_thread_join(int tid, int cid)
{
  type = THREAD_JOIN;
  thread_id = tid;
  child_id = cid;
}


void InspectEvent::init_state_capturing_thread_start(int tid)
{
  type = STATE_CAPTURING_THREAD_START;
  thread_id = tid;
}


void InspectEvent::init_state_capturing_thread_pre_create(int tid)
{
  type = STATE_CAPTURING_THREAD_PRE_CREATE;
  thread_id = tid;
}


void InspectEvent::init_state_capturing_thread_post_create(int tid, int cid)
{
  type = STATE_CAPTURING_THREAD_POST_CREATE;
  thread_id = tid;  
  child_id = cid;
}



void InspectEvent::init_obj_register(int tid, int oid)
{
  type = OBJ_REGISTER;
  thread_id = tid;
  obj_id = oid;
}


void InspectEvent::init_obj_read(int tid, int oid)
{
  type = OBJ_READ;
  thread_id = tid;
  obj_id = oid;
}


void InspectEvent::init_obj_write(int tid, int oid)
{
  type = OBJ_WRITE;
  thread_id = tid;
  obj_id = oid;
}
  
  
void InspectEvent::init_symm_point(int tid, int ptno)
{
  thread_id = tid;
  point_no = ptno;
}


void InspectEvent::init_state_req(int tid)
{
  type = STATE_REQ;
  thread_id = tid;
}

void InspectEvent::init_state_ack(int tid)
{
  type = STATE_ACK;
  thread_id = tid;
}



void InspectEvent::init_local_info(int tid, int pos, int flag)
{
  type = LOCAL_INFO;
  thread_id = tid;
  location_id = pos;
  local_state_changed_flag = flag;
}


void InspectEvent::init_local_state_begin(int tid)
{
  type = LOCAL_STATE_BEGIN;
  thread_id = tid;
}


void InspectEvent::init_local_state_end(int tid)
{
  type = LOCAL_STATE_END;
  thread_id = tid;
}


void InspectEvent::init_local_int(int tid)
{
  type = LOCAL_INT;
  thread_id = tid;
}


void InspectEvent::init_local_double(int tid)
{
  type = LOCAL_DOUBLE;
  thread_id = tid;
}


void InspectEvent::init_local_binary(int tid)
{
  type = LOCAL_BINARY;
  thread_id = tid;
}



void InspectEvent::init_cas_instr(int tid, int oid)
{
  type = CAS_INSTR;
  thread_id = tid;
  obj_id = oid;
}


void InspectEvent::init_cas2_instr(int tid, int oid)
{
  type = CAS2_INSTR;
  thread_id = tid;
  obj_id = oid;
}



string  InspectEvent::toString()
{
  stringstream ss;
  EventCategory cat;

  cat = get_event_category(type);

  ss << "("  << thread_id << ", " 
     << event_type_to_string(type) ;
  
  switch(cat){
  case EC_MUTEX:   
    ss <<", "<< mutex_id;  break;
    
  case EC_COND:     
    ss <<", "<< cond_id;
    if (type == COND_WAIT) ss << ", " << mutex_id;
    break;
    
  case EC_RWLOCK:   ss <<", "<< rwlock_id; break;
  case EC_OBJECT:   ss <<", "<< obj_id;    break;

  case EC_THREAD:   
    if (type == THREAD_CREATE)
      ss <<", "<< child_id;
    else if (type == THREAD_POST_CREATE || type == THREAD_JOIN)
      ss <<", " << child_id;  
    else if (type == THREAD_START ) 
      // ss <<", "<< thread_nm;
      ;
    break;

  case EC_SEMAPHORE: ss <<", "<< sem_id;    break;
    
  case EC_SYMMETRY:  ss << ", " << obj_id; break;
  default: break;
    //assert(false); break;
  }

  ss << ")   ";

//   if (setting.stateful_flag){
//     ss << "(" << location_id;    
//     if (local_state_changed_flag)  ss << ", true";  
//     else  ss <<", false";    
//     ss << ")";
//   }

  return ss.str();
}



bool InspectEvent::communicative(InspectEvent & event)
{
  EventCategory my_cat, cat;
  bool retval = false;

  my_cat = get_event_category(type);
  cat = get_event_category(event.type);

  if (this->invalid() || event.invalid()) return false;
  if ( (my_cat != cat) &&
       !( (my_cat == EC_MUTEX && cat == EC_COND) ||
	  (my_cat == EC_COND  && cat == EC_MUTEX) ) )
    return false;
  
//   switch(my_cat){    
//   case EC_OBJECT:
//     retval = this->is_modification_by_const() && event.is_modification_by_const();
//     break;
      
//   default:  break;
//   }  
  
  return retval;
}



bool InspectEvent::dependent(InspectEvent & event)
{
  EventCategory  my_cat, cat;
  my_cat = get_event_category(type);
  cat = get_event_category(event.type);

  if (this->invalid() || event.invalid()) return false;

  if (! (my_cat == cat  ||
	 (my_cat == EC_MUTEX && cat == EC_COND) ||
	 (my_cat == EC_COND  && cat == EC_MUTEX) ) )
    return false;
  
  switch (my_cat){
  case EC_MUTEX:  
  case EC_COND:
    if (event.mutex_id != this->mutex_id) return false;
    if ( (event.type == MUTEX_LOCK || event.type == COND_WAIT_LOCK) && 
         (type == MUTEX_LOCK || type == COND_WAIT_LOCK) ) return true;
    break;
    
  case EC_RWLOCK: 
    if (event.rwlock_id != this->rwlock_id) return false;
    if (event.type == RWLOCK_UNLOCK || this->type == RWLOCK_UNLOCK) return false;
    if (event.type == RWLOCK_WRLOCK || this->type == RWLOCK_WRLOCK)  return true;
    break;

  case EC_OBJECT:    
    if (event.obj_id != this->obj_id) return  false;
    if (event.type == OBJ_WRITE || this->type == OBJ_WRITE)
      return true;
    
    break;

  case EC_THREAD:    break;

  case EC_SEMAPHORE:  assert(false);   break;

  case EC_SYMMETRY:  break;
    
  default: assert(false);
  }
  
  return false;
}



bool InspectEvent::can_disable(InspectEvent &event)
{
  bool retval = false;

  if (thread_id == event.thread_id) return false;
  
  switch(event.type){
  case MUTEX_LOCK:
    if (type == MUTEX_LOCK && mutex_id == event.mutex_id)  retval = true;
    break;
    
  case RWLOCK_RDLOCK:
    if (type == RWLOCK_WRLOCK  && rwlock_id == event.rwlock_id)  retval = true;
    break;

  case RWLOCK_WRLOCK:
    if ((type == RWLOCK_RDLOCK || type == RWLOCK_WRLOCK) && rwlock_id == event.rwlock_id)
      retval = true;
    break;

  default: break;
  }

  return retval;
}



bool InspectEvent::can_enable(InspectEvent &event)
{
  bool retval = false;
  
  if (thread_id == event.thread_id) return false;

  switch(type){
  case MUTEX_UNLOCK:
    if (event.type == MUTEX_LOCK  && mutex_id == event.mutex_id)
      retval = true;
    break;

  case THREAD_END:
    if (event.type == THREAD_JOIN && event.child_id  == thread_id)  retval = true;
    
  default: break;
  }
  
  return retval;
}


bool InspectEvent::must_happen_before(InspectEvent * another) {
	if(thread_id == another->thread_id)
		return true;

	bool retval = false;


	return retval;
}

bool InspectEvent::is_concurrent_with(InspectEvent * another) {
	if(thread_id == another->thread_id)
		return false;

	bool retval = true;

	return retval;
}


bool InspectEvent::operator == (InspectEvent & event)
{
  if (type != event.type) return false;
  if (thread_id  != event.thread_id) return false;
  if (location_id != event.location_id) return false;
  if (local_state_changed_flag != event.local_state_changed_flag) return false;
  return this->semantic_equal(event);
}


bool InspectEvent::operator != (InspectEvent & event)
{
  return ! (*this == event);
}


InspectEvent& InspectEvent::operator =(InspectEvent ev2)
{
  type = ev2.type;

  thread_id = ev2.thread_id;
  location_id = ev2.location_id;
  thread_arg = ev2.thread_arg;

  mutex_id = ev2.mutex_id;
  cond_id = ev2.cond_id;
  obj_id = ev2.obj_id;
  rwlock_id = ev2.rwlock_id;
  sem_id = ev2.sem_id;
  child_id = ev2.child_id;
  
  local_state_changed_flag = ev2.local_state_changed_flag;

  if(ev2.event_toremove == NULL)
    event_toremove = NULL;
  else {
    event_toremove = new InspectEvent();
    *event_toremove = *(ev2.event_toremove);
  }
  
  return *this;
}


/**
 *  test whether two events are equal when the thread id is ignored.
 */
bool InspectEvent::semantic_equal(InspectEvent &event)
{
  EventCategory ecat;
  bool retval = false;

  ecat = get_event_category(type);
  switch (ecat){
  case EC_MUTEX:     retval = mutex_id == event.mutex_id;  break;
  case EC_COND:      retval = (mutex_id == event.mutex_id) && (cond_id == event.cond_id); break;
  case EC_RWLOCK:    retval = (rwlock_id == event.rwlock_id); break;
  case EC_OBJECT:    retval =  obj_id == event.obj_id;  break;
  case EC_THREAD:    retval =  child_id == event.child_id; break;
  case EC_SEMAPHORE: retval = sem_id == event.sem_id;  break;    
  case EC_SYMMETRY:  retval = (obj_id == event.obj_id); break;
  default: assert(false);
  }

  return retval;
}





istream & operator >>(istream &is, EventType &etype)
{ 
  int val;
  is >> val;
  etype = static_cast<EventType>(val);
  return is;  
}


ostream & operator <<(ostream &os, EventType etype)
{
  int val;
  val = static_cast<int>(etype);
  os << val;
  return os;  
}


Socket& operator << (Socket& sock, EventType etype)
{
  int val;
  val = static_cast<int>(etype);
  sock << val;
  return sock;
}


Socket & operator >> (Socket& sock, EventType & type)
{
  int val;
  sock >> val;
  type = static_cast<EventType>(val);
  return sock;
}


Socket & operator << (Socket &sock, EventPermitType perm)
{
  int val;
  val = static_cast<int>(perm);
  sock << val;

  return sock;  
}


Socket & operator >> (Socket& sock, EventPermitType & perm)
{
  int val;
  sock >> val;
  perm = static_cast<EventPermitType>(val);
  return sock;
}


Socket& operator << (Socket& sock, InspectEvent &event)
{
  int data[6];
  data[0] = (int)event.type;
  data[1] = (int)event.thread_id;
  
  data[2] = (int)event.mutex_id;
  data[3] = (int)event.cond_id;

  data[4] = (int)event.location_id;
  data[5] = (int)event.local_state_changed_flag;

  sock.send_binary(data, sizeof(data));
  return sock;
}


Socket& operator >> (Socket& sock, InspectEvent &event)
{
  int data[6];
  
  sock.recv_binary(data);

  event.type = (EventType)data[0];
  event.thread_id = data[1];
  event.mutex_id = data[2];
  event.cond_id = data[3];
  event.location_id = data[4];
  event.local_state_changed_flag = (bool)data[5];
  
  return sock;
}

