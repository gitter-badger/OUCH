#ifndef __EPOLL_HPP_INCLUDED__
#define __EPOLL_HPP_INCLUDED__

// borrow from zmq

#include <vector>
#include <atomic>
#include <sys/epoll.h>

struct i_poll_events
{   
  virtual ~i_poll_events () {}

  // Called by I/O thread when file descriptor is ready for reading.
  virtual void in_event (int) {}

  // Called by I/O thread when file descriptor is ready for writing.
  virtual void out_event (int) {}
};  

//  This class implements socket polling mechanism using the Linux-specific
//  epoll mechanism.
class epoll_t
{
  public:
    struct poll_entry_t
    {
      int fd;
      epoll_event ev;
      i_poll_events *events;
    };
    typedef poll_entry_t* handle_t;

    epoll_t ();
    ~epoll_t ();

    //  "poller" concept.
    handle_t add_fd (int fd_, i_poll_events *events_);
    void rm_fd (handle_t handle_);
    void set_pollin (handle_t handle_);
    void reset_pollin (handle_t handle_);
    void set_pollout (handle_t handle_);
    void reset_pollout (handle_t handle_);
    void stop ();
    //  Main event loop.
    void loop ();
    int load() { return load_; }

  private:
    //  Main epoll file descriptor
    int epoll_fd;
    std::atomic<int> load_;

    //  List of retired event sources.
    typedef std::vector <poll_entry_t*> retired_t;
    retired_t retired;

    //  If true, thread is in the process of shutting down.
    bool stopping;

    epoll_t (const epoll_t&);
    const epoll_t &operator = (const epoll_t&);
};

#endif
