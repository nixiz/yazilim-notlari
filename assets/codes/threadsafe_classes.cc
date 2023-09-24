#include <assert.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <exception>

template <typename T>
struct auto_locker_t {
  auto_locker_t(const T* p) : ptr(const_cast<T*>(p)) {
    if (ptr != 0) {
      ptr->lock();
    }
  }

  ~auto_locker_t() {
    if (ptr != 0) {
      ptr->unlock();
    }
  }
  // type cast operator overload
  operator T* () { return ptr; }

  // class member access operator overload
  T* operator->() { return ptr; }

private:
  auto_locker_t() = delete;
  // cannot delete copy ctor because of thread safety policy
  // classes are returning copy of this class (before C++17)
  // auto_locker_t(const auto_locker_t&) = delete;
  auto_locker_t& operator=(const auto_locker_t&) = delete;
  T* ptr;
};

template <typename T>
class thread_safe 
{
public:
  template <typename... Args>
  thread_safe(Args&&... args) 
    : ptr{ new lockable_T{std::forward<Args>(args)...} } { }
  
  // ~dtor may throw if the T class has lock somewhere else
  ~thread_safe() noexcept(false) {
    ptr.reset();
  }

  // typedef auto_locker_t<lockable_T> AccessType;
  auto operator->() { 
    return auto_locker_t<lockable_T>(ptr.get()); 
  }

  auto operator->() const { 
    return auto_locker_t<lockable_T>(ptr.get());
  }

private:
  // lock/unlock methods are inserted into T class using mixin pattern
  class lockable_T : public T
  {
  public:
    using T::T;

    ~lockable_T() noexcept(false)
    { 
      assert(!locked);
      if (locked) {
        throw std::runtime_error("the resourse is still locked! cannot be deleted!");
      }
    }

    void lock() const {
      guard.lock();
      locked = true;
    }

    void unlock() const noexcept {
      assert(locked);
      guard.unlock();
      locked = false;
    }

  private:
    mutable std::mutex guard;
    mutable std::atomic_bool locked{false};
  };
  std::unique_ptr<lockable_T> ptr;
};

class Counter {
public:
  explicit Counter(int default_value) 
    : count(default_value) {}
  void increment() {
    count++;
  }
  auto getCount() const {
    return count;
  }
private:
  int count = 0;
};

int main()
{
  thread_safe<Counter> counter(5);
  std::thread t1([&] {
    for (int i = 0; i < 10; i++) {
        counter->increment();
    }
  });
  std::thread t2([&] {
    for (int i = 0; i < 10; i++) {
        counter->increment();
    }
  });

  t1.join();
  t2.join();

  std::cout << "Final Count: " << counter->getCount() << std::endl;
  return 0;
}