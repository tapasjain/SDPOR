#ifndef __EVENT_BUFFER_H__
#define __EVENT_BUFFER_H__


#ifdef __GNUC__
  #include <ext/hash_map>
  using __gnu_cxx::hash_map;
  using __gnu_cxx::hash;
#else
  #include <hash_map>
#endif

#include <utility>
#include <string>
#include "inspect_util.hh"
#include "inspect_event.hh"
#include "program_state.hh"


using std::pair;
using std::string;

/**
 *  the socket index at the scheduler's side
 */
class SchedulerSocketIndex
{
public:
  SchedulerSocketIndex();
  ~SchedulerSocketIndex();
  
  typedef hash_map<int, Socket*, hash<int> >::iterator iterator;
  
  inline iterator begin() { return sockets.begin(); }
  inline iterator end()   { return sockets.end();   }
  
  Socket * get_socket( int fd );
  Socket * get_socket_by_thread_id(int tid);
  Socket * get_socket(InspectEvent &);
  
  void add(Socket * socket);
  void update(int thread_id,  Socket*);

  void remove(int fd);
  void remove_sc_fd(int fd);
  void remove(Socket * socket);
  void clear();

  bool has_fd(int fd);
public:  
  hash_map<int, Socket*, hash<int> > sockets;
  hash_map<int, Socket*, hash<int> > tid2socket;
};




class EventBuffer
{
public:
  typedef hash_map<int, InspectEvent>::iterator iterator;
  
public:
  EventBuffer();
  ~EventBuffer();

  bool init(string socket_file, int time_out, int backlog);
	    
  void reset();
  void close() {  server_socket.close(); } 

  void collect_events();
  inline bool has_event(int tid);
  
  InspectEvent  get_the_first_event();
  InspectEvent  get_event(int tid);
  InspectEvent  receive_event(Socket * socket);
  pair<int,int> set_read_fds();

  void approve(InspectEvent & event);
  inline bool is_listening_socket(int fd); 

  string toString();
  
public:
  ServerSocket server_socket;  
  SchedulerSocketIndex  socket_index;
  Socket * sc_socket;  // the socket to connect with the state-capturing thread
  fd_set  readfds;
  hash_map<int, InspectEvent>  buffer;
};



#endif


