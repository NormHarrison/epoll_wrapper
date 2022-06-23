#include <span>
#include <system_error>
#include <cerrno>
#include <cstdlib>
#include <atomic>

#include <unistd.h>

#include <sys/epoll.h>
#include <sys/socket.h>

#include "epoll_wrap.hpp"



EpollWrap::EpollWrap(bool a_close_on_exec)
{
  m_ep_fd = epoll_create1(a_close_on_exec ? EPOLL_CLOEXEC : 0);
  if (m_ep_fd == -1)
    throw std::system_error(errno, std::system_category());

  if (socketpair(AF_UNIX, SOCK_RAW, PF_UNIX, m_abort_pair) == -1)
    throw std::system_error(errno, std::system_category());

  struct epoll_event event = {
    .events = EPOLLIN,
    .data   = {.fd = m_abort_pair[M_AP_READ]}
  };

  this->control(Operation::ADD, event.data.fd, &event);
}


void EpollWrap::control(Operation a_op, int a_fd, struct epoll_event* a_event)
{
  switch (a_op) {
    case Operation::ADD:
      m_interest_count += 1;
      break;
    case Operation::REMOVE:
      m_interest_count -= 1;
      break;
    case Operation::MODIFY:
      break;
  }

  if (epoll_ctl(m_ep_fd, static_cast<int>(a_op), a_fd, a_event) == -1)
    throw std::system_error(errno, std::system_category());
}


std::span<struct epoll_event> EpollWrap::wait(int a_timeout_mili)
{
  if (m_interest_count > m_result_events_size) {

    m_result_events_size = m_interest_count;
    const size_t new_size = sizeof(struct epoll_event) * m_interest_count;
    m_result_events = (struct epoll_event*) realloc(m_result_events, new_size);
  }

  m_is_waiting = true;

  int ready_count = epoll_wait(m_ep_fd, m_result_events, m_interest_count,
                               a_timeout_mili);
  m_is_waiting = false;

  if (ready_count == -1)
    throw std::system_error(errno, std::system_category());

  while (m_is_being_aborted);

  if (m_was_aborted) {

    char dummy;
    const size_t bytes_read = recv(m_abort_pair[M_AP_READ], &dummy, 1, 0);
    //  ! Read until the buffer is empty instead?

    if (bytes_read not_eq 1) {
      m_was_aborted = false;
      throw std::system_error(errno, std::system_category());
    }

    ready_count = 0;
    m_was_aborted = false;
  }

  return std::span<struct epoll_event>(m_result_events, ready_count);
}


void EpollWrap::abort_wait()
{
  if (m_is_waiting) {

    m_is_being_aborted = true;
    m_was_aborted = true;

    const char dummy = 0;
    const size_t bytes_written = send(m_abort_pair[M_AP_WRITE], &dummy, 1, 0);

    if (bytes_written not_eq 1) {
      m_is_being_aborted = false;
      throw std::system_error(errno, std::system_category());
    }

    m_is_being_aborted = false;
    while (m_was_aborted);

  }
}


EpollWrap::~EpollWrap()
{
  if (m_ep_fd == -1)
    return;

  close(m_ep_fd);
  m_ep_fd = -1;

  close(m_abort_pair[M_AP_READ]);
  close(m_abort_pair[M_AP_WRITE]);

  if (m_result_events) {
    delete m_result_events;
    m_result_events = nullptr;
  }
}
