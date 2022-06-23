#ifndef EPOLLWRAPPER_EPOLLWRAP_H_
#define EPOLLWRAPPER_EPOLLWRAP_H_

#include <span>
#include <atomic>

#include <sys/epoll.h>


//  `EpollWrap` wraps a traditional epoll instance inside the Linux kernel.
//  The behavior of each member function and its parameters are identical
//  original `epoll_*()` functions, except for the added functionality of
//  aborting a call to `wait()`. Additionally, much like raw epoll instances,
//  the member functions of this class are not thread safe, with the exception
//  of `EpollWrap::abort_wait()`, which may be called during a call to
//  `EpollWrap::wait()` occurring in another thread.

class EpollWrap
{
public:
  enum class Operation {ADD = 1, MODIFY, REMOVE};

  //  Refer to `man epoll_create1`.
  //  Throws `std::system_error` on failure to create new epoll instance
  //  or failure to create a pair of sockets.
  EpollWrap(bool a_close_on_exec);


  //  Refer to `man epoll_ctl`.
  //  Throws `std::system_error` on failure to add, remove or modify
  //  the file descriptor `a_fd` in relation to the underlying epoll instance.
  void control(Operation a_op, int a_fd, struct epoll_event* a_event);


  //  Refer to `man epoll_wait`.
  //  The value for the `maxevents` parameter is automatically determined based
  //  off of previous calls to `EpollWrap::control`. The size of the returned
  //  `std::span` instance will be 0 if no activity occurred before the timeout,
  //  or if a call to `EpollWrap::abort_wait()` was made in another thread.
  //  Throws `std::system_error` on failure to wait for activity on the
  //  underlying epoll instance, or if reading from the underlying socket pair
  //  failed.
  std::span<struct epoll_event> wait(int a_timeout_mili);


  //  Aborts (causes to return immediately) a simultaneous call to
  //  `EpollWrap::wait()` occurring in another thread. Does nothing if
  //  the instance is not currently waiting for activity.
  //  Throws `std::system_error` on failure to write to the underlying
  //  socket pair.
  void abort_wait();


  //  Closes the underlying epoll instance and all other kernel resources.
  //  After the instance is destructed, every member function will throw
  //  `std::system_error` with a "bad file descriptor" message.
  ~EpollWrap();

private:
  static const int M_AP_READ = 0;
  static const int M_AP_WRITE = 1;
  int m_abort_pair[2];

  struct epoll_event* m_result_events = nullptr;
  //  TODO ^ Use a smart pointer?
  int m_result_events_size = 0;

  int m_ep_fd;
  int m_interest_count = 0;

  std::atomic_bool m_was_aborted = false;
  std::atomic_bool m_is_being_aborted = false;
  std::atomic_bool m_is_waiting = false;
};

#endif  // EPOLLWRAPPER_EPOLLWRAP_H_
