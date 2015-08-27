#ifndef PIPE_HPP
#define PIPE_HPP
#include <atomic>

namespace MYPIPE {

static const unsigned CHUNK_SIZE = 1024 * 1024;
struct Chunk
{
  // atomic initialization is not atomic
  Chunk(unsigned n=CHUNK_SIZE) : head(0), tail(0), capacity(std::max(n, CHUNK_SIZE)), data((char*)malloc(capacity)), next(NULL) {} 
  void reset() { head = 0; tail.store(0, std::memory_order_relaxed); next = NULL; }
  void resize(unsigned n) { capacity = n; free(data); data = (char*)malloc(n); }
  uint32_t head;
  std::atomic<uint32_t> tail; // because gcc<4.6 does not support std::atomic_thread_fence, so we use atomic for std fence support
  uint32_t capacity;
  char* data;
  Chunk* next;
};

class Pipe
{
public:
  Pipe() { head_ = tail_ = new Chunk; spared_ = NULL; }
  void reset() { while (head_ != tail_) { auto p = head_->next; delete head_; head_ = p; }; head_->reset(); }

  void push(const char* str, size_t n)
  {
    uint32_t tail = tail_->tail.load(std::memory_order_relaxed);
    size_t remained = tail_->capacity - tail;
    if (remained > n) {
      memcpy(tail_->data + tail, str, n);
      //std::atomic_thread_fence(std::memory_order_release);
      //tail += n;
      tail_->tail.fetch_add(n, std::memory_order_release);
      assert(tail_->tail.load(std::memory_order_relaxed) < tail_->capacity);
    } else {
      assert(remained > 0);
      memcpy(tail_->data + tail, str, remained);
      str += remained;
      n -= remained;
      auto t = spared_.exchange(NULL); //, std::memory_order_relaxed); // not safe with std::memory_order_relaxed
      if (!t)
        t = new Chunk(n*2);
      else {
        t->reset();
        if (t->capacity <= n) t->resize(n*2);
      }
      if (n) {
        memcpy(t->data, str, n);
        t->tail.store(n, std::memory_order_relaxed);
      }
      tail_->next = t;
      //std::atomic_thread_fence(std::memory_order_release);
      //tail->tail = t->capacity;
      tail_->tail.store(tail_->capacity, std::memory_order_release);
      tail_ = t;
    }
  }

  bool data(const char*& str, size_t& n)
  {
    // auto tail = head_->tail;
    // std::atomic_thread_fence(std::memory_order_acquire);
    auto tail = head_->tail.load(std::memory_order_acquire);
    assert(tail >= head_->head);
    if (tail == head_->head) return false;
    str = head_->data + head_->head;
    n = tail - head_->head;
    return true;
  }

  void pop(size_t n)
  {
    assert(n <= head_->tail.load(std::memory_order_relaxed) - head_->head);
    head_->head += n;
    if (head_->head == head_->capacity) {
      auto next = head_->next;
      assert(next);
      auto s = spared_.exchange(head_); //,std::memory_order_relaxed);
      if (s)
        delete s;
      head_ = next;
    }
  }

private:
  Chunk* head_;
  Chunk* tail_;
  std::atomic<Chunk*> spared_; // zmq does this way? is using free list better?
};

}
#endif
