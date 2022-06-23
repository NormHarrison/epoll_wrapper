#include <cstdio>
#include <thread>
#include <chrono>
#include <functional>

#include <sys/epoll.h>

#include "epoll_wrap.hpp"


const int STDIN_FD = 0;
const int WAIT_TIMEOUT_MILI = 10 * 1000;
const int ABORT_TIMEOUT_SECS = 10;


void abort_thread(EpollWrap &ep)
{
  while (true) {

    std::printf("Aborting `EpollWrap::Wait()` in %d seconds.\n",
                ABORT_TIMEOUT_SECS);

    std::this_thread::sleep_for(std::chrono::seconds(ABORT_TIMEOUT_SECS));
    ep.abort_wait();
  }
}


void empty_stdin(void)
{
  char line[255];

  std::fgets(line, 255, stdin);
  std::printf("Data from stdin: \"%s\"\n\n", line);
}


int main(int argc, char* argv[])
{
  EpollWrap ep(false);

  std::thread abort_thread_handle(abort_thread, std::ref(ep));

  struct epoll_event stdin_event = {
    .events = EPOLLIN,
    .data   = {.fd = STDIN_FD}
  };

  ep.control(EpollWrap::Operation::ADD, STDIN_FD, &stdin_event);

  while (true) {

    std::printf("Waiting %d second(s) for events...\n",
                WAIT_TIMEOUT_MILI / 1000);

    auto events = ep.wait(WAIT_TIMEOUT_MILI);
    std::printf("%ld descriptor(s) had events.\n", events.size());

    for (const auto &event : events) {

      std::printf("Event occurred on file descriptor %d.\n", event.data.fd);
      if (event.data.fd == STDIN_FD)
        empty_stdin();
    }

  }

  abort_thread_handle.join();
}

