#ifndef __INSPECT_EXCEPTION_H__
#define __INSPECT_EXCEPTION_H__


/**
 * 
 */
class BacktrackChoiceUnfoundException
{ };


/**
 *  will be thrown out by the scheduler when a deadlock is found!!
 */
class DeadlockException 
{ };


class NullObjectException
{ };


class SocketException
{
public:
  SocketException(const char * reason) : detail(reason) { } 

public:
  string detail;
};


#endif

