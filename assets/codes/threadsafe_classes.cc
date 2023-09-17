#include <assert.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <exception>

/*

From the book "Modern C++ Design: Generic Programming and Design Patterns Applied" Andrei
Alexandrescu

There are interesting applications of smart pointer layering, mainly because of the mechanics of
operator->. When you apply operator-> to a type that's not a built-in pointer, the compiler does an
interesting thing. After looking up and applying the user-defined operator-> to that type, it
applies operator-> again to the result. The compiler keeps doing this recursively until it reaches a
pointer to a built-in type, and only then proceeds with member access. It follows that a smart
pointer's operator-> does not have to return a pointer. It can return an object that in turn
implements operator->, without changing the use syntax.

This leads to a very interesting idiom: pre-and postfunction calls (Stroustrup 2000). If you return
an object of type PointerType by value from operator->, the sequence of execution is as follows:
  1. Constructor of PointerType
  2. PointerType::operator-> called; likely returns a pointer to an object of type
  PointeeType
  3. Member access for PointeeType?likely a function call
  4. Destructor of PointerType

In a nutshell, you have a nifty way of implementing locked function calls. This idiom has broad uses
with multithreading and locked resource access. You can have PointerType's constructor lock the
resource, and then you can access the resource; finally, Pointer Type's destructor unlocks the
resource.


*/

template <typename T>
struct auto_locker_t {
  auto_locker_t(const T* p) : ptr(const_cast<T*>(p)) {
    if (ptr != 0)
      ptr->lock();
  }

  ~auto_locker_t() {
    if (ptr != 0)
      ptr->unlock();
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
    template <typename... Args>
    explicit lockable_T(Args&&... args) noexcept(noexcept(T(std::forward<Args>(args)...)))
      : T(std::forward<Args>(args)...), locked(false) {}

    ~lockable_T() noexcept(false) 
    { 
      assert(!locked);
      if (!locked) {
        throw std::exception("the resourse is still locked! cannot be deleted!");
      }
    }

    void lock() const {
      std::cout << "lockable_T::lock()\n";
      guard.lock();
      locked = true;
    }

    void unlock() const noexcept {
      std::cout << "lockable_T::unlock()\n";
      assert(locked);
      guard.unlock();
      locked = false;
    }

  private:
    mutable std::mutex guard;
    mutable std::atomic_bool locked;
  };
  std::unique_ptr<lockable_T> ptr;
};

class DummyClass {
  int val;

public:
  DummyClass(int x) : val{ x } {}

  void foo(int x) {
    std::cout << "foo()\n";
    val = x;
  }

  int bar() {
    std::cout << "bar()::val=" << val << "\n";
    return val;
  }
};

int main() 
{
  thread_safe<DummyClass> tsafe{ 5 };
  tsafe->foo(12);
  auto x = tsafe->bar();

  std::thread th([&] {
    (void)tsafe->bar();
    tsafe->foo(20);
    (void)tsafe->bar();
  });
  th.join();
  return x;
}