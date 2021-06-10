#pragma once

#include <functional>
#include <memory>

#include <unistd.h>


namespace speedex {

// Throw an exception based on the current errno (like perror).
[[noreturn]] void threrror(const char *msg);
[[noreturn]] void threrror(const std::string msg);

//! Self-closing file descriptor will be closed as soon as it goes out
//! of scope.
class unique_fd {
  int fd_{-1};
public:
  constexpr unique_fd() = default;
  constexpr explicit unique_fd(int fd) : fd_(fd) {}
  unique_fd(unique_fd &&uf) : fd_(uf.release()) {}
  ~unique_fd() { clear(); }
  unique_fd &operator=(unique_fd &&uf) { reset(uf.release()); return *this; }

  //! Return the file descriptor number, but maintain ownership.
  int get() const { return fd_; }
  //! Return the file descriptor.
  operator int() const { return fd_; }
  //! True if the file descriptor is not -1.
  explicit operator bool() const { return fd_ != -1; }
  //! Return the file descriptor number, relinquishing ownership of
  //! it.  The \c unique_fd will have file descriptor -1 after this
  //! method returns.
  int release() {
    int ret = fd_;
    fd_ = -1;
    return ret;
  }
  void clear() {
    if (fd_ != -1) {
      close(fd_);
      fd_ = -1;
    }
  }
  void reset(int fd) { clear(); fd_ = fd; }
};


namespace detail {
template<auto Fn> struct deleter;
template<typename R, typename T, R(*Fn)(T *)> struct deleter<Fn> {
  constexpr deleter() {}
  void operator()(T *t) const { Fn(t); }
  using unique_ptr_type = std::unique_ptr<T, deleter>;
};
}
//! unique_destructor_t(destructor)--where destructor is a function
//! taking a T*--becomes a unique_ptr<T> but where destructor is
//! invoked on cleanup instead of delete.
template<auto destructor> using unique_destructor_t =
  typename detail::deleter<destructor>::unique_ptr_type;


//! Container for a cleanup action.
class cleanup {
  static std::function<void()> &&voidify(std::function<void()> &&f) {
    return std::move(f);
  }
  static const std::function<void()> &voidify(const std::function<void()> &f) {
    return f;
  }
  template<typename F> static std::function<void()> voidify(F &&f) {
    return [f{std::move(f)}]() { f(); };
  }

  std::function<void()> action_;
  static void nop() {}
public:
  cleanup() : action_(nop) {}
  cleanup(const cleanup &) = delete;
  cleanup(cleanup &&c) : action_(std::move(c.action_)) { c.release(); }
  template<typename F> cleanup(F &&f) : action_(std::forward<F>(f)) {}
  template<typename... Args> cleanup(Args... args)
    : action_(voidify(std::bind(args...))) {}
  ~cleanup() { action_(); }
  cleanup &operator=(cleanup &&c) { action_.swap(c.action_); return *this; }
  void reset() {
    std::function<void()> old(std::move(action_));
    release();
    old();
  }
  template<typename F> void reset(F &&f) {
    std::function<void()> old(std::move(action_));
    action_ = std::forward<F>(f);
    old();
  }
  template<typename... Args> void reset(Args... args) {
    std::function<void()> old(action_);
    action_ = std::bind(args...);
    old();
  }
  void release() { action_ = nop; }
};

}
