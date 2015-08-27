#include "epoll.hpp"

#include <sys/epoll.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <algorithm>
#include <new>
#include <cassert>

#include "epoll.hpp"
static const size_t max_io_events = 256;
enum {retired_fd = -1};

#define errno_assert(x) \
    do {\
        if ((!(x))) {\
            const char *errstr = strerror (errno);\
            fprintf (stderr, "%s (%s:%d)\n", errstr, __FILE__, __LINE__);\
        }\
    } while (false)

epoll_t::epoll_t () :
  stopping (false)
{
  load_ = 0;
  epoll_fd = epoll_create (1);
  assert (epoll_fd != -1);
}

epoll_t::~epoll_t ()
{
  close (epoll_fd);
  for (retired_t::iterator it = retired.begin (); it != retired.end (); ++it)
    delete *it;
}

// epoll is thread-safe per below
// http://man7.org/linux/man-pages/man2/epoll_wait.2.html
// https://source.ridgerun.net/svn/leopardboarddm365/sdk/trunk/fs/apps/cherokee-0.99/src/cherokee/fdpoll-epoll.c
epoll_t::handle_t epoll_t::add_fd (int fd_, i_poll_events *events_)
{
  poll_entry_t *pe = new (std::nothrow) poll_entry_t;
  assert (pe);

  //  The memset is not actually needed. It's here to prevent debugging
  //  tools to complain about using uninitialised memory.
  memset (pe, 0, sizeof (poll_entry_t));

  pe->fd = fd_;
  pe->ev.events = 0;
  pe->ev.data.ptr = pe;
  pe->events = events_;

  int rc = epoll_ctl (epoll_fd, EPOLL_CTL_ADD, fd_, &pe->ev);
  errno_assert (rc != -1);

  //  Increase the load metric of the thread.
  load_++;

  return pe;
}

void epoll_t::rm_fd (handle_t handle_)
{
  poll_entry_t *pe = (poll_entry_t*) handle_;
  int rc = epoll_ctl (epoll_fd, EPOLL_CTL_DEL, pe->fd, &pe->ev);
  errno_assert (rc != -1);
  pe->fd = retired_fd;

  //  Decrease the load metric of the thread.
  load_--;
}

void epoll_t::set_pollin (handle_t handle_)
{
  poll_entry_t *pe = (poll_entry_t*) handle_;
  pe->ev.events |= EPOLLIN;
  int rc = epoll_ctl (epoll_fd, EPOLL_CTL_MOD, pe->fd, &pe->ev);
  errno_assert (rc != -1);
}

void epoll_t::reset_pollin (handle_t handle_)
{
  poll_entry_t *pe = (poll_entry_t*) handle_;
  pe->ev.events &= ~((short) EPOLLIN);
  int rc = epoll_ctl (epoll_fd, EPOLL_CTL_MOD, pe->fd, &pe->ev);
  errno_assert (rc != -1);
}

void epoll_t::set_pollout (handle_t handle_)
{
  poll_entry_t *pe = (poll_entry_t*) handle_;
  pe->ev.events |= EPOLLOUT;
  int rc = epoll_ctl (epoll_fd, EPOLL_CTL_MOD, pe->fd, &pe->ev);
  errno_assert (rc != -1);
}

void epoll_t::reset_pollout (handle_t handle_)
{
  poll_entry_t *pe = (poll_entry_t*) handle_;
  pe->ev.events &= ~((short) EPOLLOUT);
  int rc = epoll_ctl (epoll_fd, EPOLL_CTL_MOD, pe->fd, &pe->ev);
  errno_assert (rc != -1);
}

void epoll_t::stop ()
{
  stopping = true;
}

void epoll_t::loop ()
{
  epoll_event ev_buf [max_io_events];

  while (!stopping) {
    //  Wait for events.
    int n = epoll_wait (epoll_fd, &ev_buf [0], max_io_events, 100); // timeout in milliseconds
    if (n == -1) {
      assert (errno == EINTR);
      continue;
    }

    for (int i = 0; i < n; i ++) {
      poll_entry_t *pe = ((poll_entry_t*) ev_buf [i].data.ptr);

      if (ev_buf [i].events & EPOLLOUT)
        pe->events->out_event (pe->fd);
      if (pe->fd == retired_fd) {
        retired.push_back (pe);
        continue;
      }
      if (ev_buf [i].events & EPOLLIN)
        pe->events->in_event (pe->fd);
      if (pe->fd == retired_fd) {
        retired.push_back (pe);
        continue;
      }
    }

    //  Destroy retired event sources.
    for (retired_t::iterator it = retired.begin (); it != retired.end ();
        ++it) {
       delete *it;
    }
    retired.clear ();
  }
}

