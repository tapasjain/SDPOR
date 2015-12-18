
#ifdef __GNUC__
  #include <signal.h>
  #include <netinet/in.h>
  #include <sys/time.h>
  #include <sys/ioctl.h>
  #include <sys/wait.h>
  #include <unistd.h>
#else
  
#endif


#include <cassert>
#include <string>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>

#include "event_buffer.hh"
#include "inspect_exception.hh"
#include "inspect_pthread.hh"

using namespace std;
using namespace __gnu_cxx;


//////////////////////////////////////////////////////////////////////////////
//
//  the implementation of SchedulerSocketIndex 
//
//////////////////////////////////////////////////////////////////////////////

SchedulerSocketIndex::SchedulerSocketIndex()
{
  sockets.clear();
}


SchedulerSocketIndex::~SchedulerSocketIndex()
{ 
  this->clear();
}


Socket * SchedulerSocketIndex::get_socket(int fd)
{
  hash_map<int, Socket*, hash<int> >::iterator it;
  it = sockets.find( fd );
  assert( it != sockets.end() );
  return it->second;
}


Socket * SchedulerSocketIndex::get_socket_by_thread_id(int tid)
{
  hash_map<int,Socket*, hash<int> >::iterator it;
  
  it = tid2socket.find( tid );
  assert( it != tid2socket.end() );
  return it->second;
}


Socket * SchedulerSocketIndex::get_socket(InspectEvent &ev)
{
  hash_map<int,Socket*, hash<int> >::iterator it;
  
  it = tid2socket.find( ev.thread_id );
  assert( it != tid2socket.end() );
  return it->second;
}


void SchedulerSocketIndex::add(Socket * socket)
{
  int fd;
  assert( socket != NULL );
  fd = socket->get_fd();
  sockets.insert( make_pair( fd, socket) );
}


void SchedulerSocketIndex::update(int thread_id,  Socket * socket)
{
  assert( socket != NULL );
  if (tid2socket.find(thread_id) == tid2socket.end())
    tid2socket.insert( make_pair(thread_id, socket) );
}


void SchedulerSocketIndex::remove(int fd)
{
  hash_map<int, Socket*, hash<int> >::iterator it;
  Socket * socket;

  assert( fd > 0);
  it = sockets.find( fd );
  socket = it->second;
  socket->close();
  delete socket;
  sockets.erase(it);
}

void SchedulerSocketIndex::remove_sc_fd(int fd)
{
  hash_map<int, Socket*, hash<int> >::iterator it;
  Socket * socket;

  assert( fd > 0);
  it = sockets.find( fd );
  socket = it->second;
  sockets.erase(it);
}


/**
 * close all other sockets except the server socket  
 */
void SchedulerSocketIndex::clear()
{
  hash_map<int, Socket*, hash<int> >::iterator it;
  Socket * socket;
  for (it = sockets.begin(); it != sockets.end(); it++){
    socket = it->second;
    socket->close();
    delete socket;
  }

  sockets.clear();
  tid2socket.clear();
}


bool SchedulerSocketIndex::has_fd(int fd)
{
  iterator it;
  it = sockets.find(fd);
  return (it != sockets.end());
}


////////////////////////////////////////////////////////////////////////////////////
//
//  the implementation of EventBuffer
//
////////////////////////////////////////////////////////////////////////////////////

EventBuffer::EventBuffer()
  : server_socket(SOCK_UNIX),
    sc_socket(NULL)
{ }
  

EventBuffer::~EventBuffer()
{
  server_socket.close();
  if (sc_socket != NULL){
    sc_socket->close();
    delete sc_socket;
  }
}


bool EventBuffer::init(string socket_file, int time_out, int backlog)
{
  int retval;

  server_socket.set_timeout_val( time_out );

  retval = server_socket.bind( socket_file );

  if (retval == -1){
    cerr<<"Inspect initialization error: fail to bind the socket " 
	<< socket_file << endl;
    return false;
  }

  retval = server_socket.listen(backlog);
  if (retval == -1){
    cerr<<"Inspect initialization error: fail to listen to the socket " << endl;
    return false;
  }

  return true;
}


void EventBuffer::reset()
{
  socket_index.clear();
  buffer.clear();
  if (sc_socket != NULL){
    sc_socket->close();
    delete sc_socket;
    sc_socket = NULL;
  }
}


/**
 *   the first event must be MAIN_START 
 */
InspectEvent EventBuffer::get_the_first_event()
{
  Socket * socket;
  InspectEvent event;
  InspectEvent sc_ev1, sc_ev2; 

  socket = server_socket.accept();
  assert( socket != NULL );

  (*socket) >> event;
  socket_index.add(socket);

  socket_index.update(event.thread_id, socket);

  assert( event.type == THREAD_START );    
  return event;
}


bool  EventBuffer::has_event(int tid)
{
  EventBuffer::iterator it;
  it = buffer.find(tid);
  if (it == buffer.end()) return false;
  return it->second.valid();
}


InspectEvent EventBuffer::get_event(int tid)
{
  EventBuffer::iterator it;
  InspectEvent event;

  while (! has_event(tid) ) collect_events();

  it = buffer.find(tid);
  assert(it != buffer.end());

  event = it->second;
  it->second.clear();
  
  if (event.type == LOCAL_INFO){
    int pos = event.location_id;
    bool flag = event.local_state_changed_flag;

    approve(event);
    
    while (! has_event(tid) ) collect_events();
    it = buffer.find(tid);
    assert(it != buffer.end());
    event = it->second;
    it->second.clear();

    event.location_id = pos;
    event.local_state_changed_flag = flag;
  }
  
  return event;
}


/**
 *  receive the content of a thread event 
 * 
 *  Handling the local binary datais a specical case of 
 *  receiving event
 */
InspectEvent EventBuffer::receive_event(Socket * socket)
{
  InspectEvent event;
  
  assert(socket != NULL);
  (*socket) >> event;

  if (event.type == LOCAL_BINARY){
    int size;
    void * local_binary;

    socket->recv_int(&size);
    local_binary = new char[size];
    event.thread_arg = local_binary;
    event.obj_id = size;    
  }
  else if (event.type == THREAD_START){
    string str;
    socket->recv_string(str);
    
    //cout << " getting thread name: " << str << endl;
  }
  return event;
}




/**
 *  set the readfs set that is going to be monitored by the 
 *  ::select system call 
 */
pair<int,int> EventBuffer::set_read_fds()
{
  SchedulerSocketIndex::iterator it;
  int max_fd, min_fd, server_fd;

  FD_ZERO( &readfds );

  max_fd = -1;
  min_fd = 9999999;

  server_fd = server_socket.get_fd();
  FD_SET( server_fd, &readfds );
  max_fd = ( server_fd > max_fd ) ? server_fd : max_fd; 
  min_fd = ( server_fd < min_fd ) ? server_fd : min_fd; 
  
  for ( it = socket_index.begin(); it != socket_index.end(); it++ ){
    FD_SET( it->first, &readfds );
    max_fd = (it->first > max_fd)? it->first : max_fd; 
    min_fd = (it->first < min_fd)? it->first : min_fd; 
  }

  return  pair<int, int>(min_fd, max_fd);
}



/**
 *   monitor the sockets, and collect the coming events from them. 
 *   the porblem is that the events may come in different order. 
 *
 */
void EventBuffer::collect_events()
{
  pair<int, int> min_max;
  int min_fd, max_fd, num_valid, num_readable;
  InspectEvent event;
  Socket * socket;
  hash_map<int, InspectEvent, hash<int> >::iterator ebit;

  min_max = this->set_read_fds();
  min_fd = min_max.first;
  max_fd = min_max.second;
  num_valid = ::select(max_fd + 1, &readfds, NULL, NULL, NULL);

  for (int i = min_fd; i <= max_fd; i++){
    if ( ! FD_ISSET(i, &readfds) ) continue;
    if ( is_listening_socket(i) ){
      socket = server_socket.accept();
      socket_index.add( socket );
      continue;
    }
    
    ::ioctl(i, FIONREAD, &num_readable);
    if (num_readable == 0){      
      socket_index.remove( i );
      continue;
    }
    
    socket = socket_index.get_socket(i);
    event = this->receive_event(socket);

    //cout << event.toString () << endl;

    if (event.thread_id == SC_THREAD_ID){
      assert(event.type == STATE_CAPTURING_THREAD_START);
      socket_index.remove_sc_fd(i);
      sc_socket = socket;
      assert(! socket_index.has_fd(i));
      
      (*sc_socket ) << (int)STATE_CAPTURING_THREAD_START_PERM;
    }
    else{
      socket_index.update(event.thread_id, socket);   
      assert( !has_event(event.thread_id) );
      buffer[event.thread_id] = event;
    }
  }
}



void EventBuffer::approve(InspectEvent & event)
{ 
  Socket * socket = NULL;
  
  assert( event.valid() );
  socket = socket_index.get_socket(event);
  assert( socket != NULL );

  EventPermitType  permit;
  permit = permit_type(event.type);
  
  *socket << (int)permit;
}



bool EventBuffer::is_listening_socket(int fd)
{
  return (fd == server_socket.sock_fd);
}


string EventBuffer::toString()
{
  stringstream ss;
  iterator it;

  ss << "{";
  for (it = buffer.begin(); it != buffer.end(); it++){
    if (it->second.valid())
      ss << it->second.toString() << "  ";
  }
  ss <<"}";
  return ss.str();
}

