---
layout: post
title: The Thread Safety Challenge Rust, Java vs. C++
subtitle: Simplify Concurrent Programming and Protect Shared Resources in C++ with Ease
thumbnail-img: /assets/img/async_cb_banner.png
share-img: /assets/img/async_cb_banner.png
nav-short: true
comments: true
readtime: true
show-avatar: false
language: tr
gh-repo: nixiz/async-call-helper
gh-badge: [star, follow]
tags: [C++, templates, async, callback, modern-cpp, OOP]
---

## TL;DR

In modern C++ programming, thread safety is essential to avoid data corruption and unexpected behavior in concurrent applications. However, writing thread-safe code in C++ can be challenging, often requiring complex synchronization mechanisms.  
In this article, I introduced the `thread_safe` helper class, which is designed to make it easier to write thread-safe C++ code.  
`thread_safe` provides a simple and easy-to-use interface for protecting shared resources, allowing developers to focus on their core logic without worrying about the details of low-level thread management.

```cpp
class Counter {
public:
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
  thread_safe<Counter> counter;
  std::thread t1([&] {
    for (int i = 0; i < 1000000; i++) {
      counter->increment();
    }
  });
  std::thread t2([&] {
    for (int i = 0; i < 1000000; i++) {
      counter->increment();
    }
  });
  t1.join(); t2.join();
  std::cout << "Final Count: " << counter->getCount() << std::endl;
}
```

The `thread_safe` helper class will take care of all the necessary synchronization to ensure that the `counter` object is thread-safe. This means that you can safely access and modify the counter object from multiple threads without having to worry about data corruption or unexpected behavior.
Complete example can be found in [here](https://gist.github.com/nixiz/8fbc4126d50e33a9802aa5198d87ba74).

## Threadsafe Classes

C++ is often compared to other languages, such as Java, for its system-level programming capabilities and performance.

C++ is better for system-level programming because it gives you more control over memory and has better performance. This makes it ideal for resource-intensive tasks. However, C++ doesn't have built-in features for concurrent programming, so you have to be careful to avoid data races and complex code.

On the other hand, Java is better for concurrent programming because it has built-in features like synchronized blocks that make it easier to write thread-safe code. This makes Java a good choice for scenarios where ease of use and thread safety are more important than performance.

In Java, synchronized blocks are a powerful mechanism for ensuring thread safety in concurrent programming. When multiple threads access shared resources, synchronization is crucial to prevent race conditions and data corruption. Synchronized blocks allow you to protect critical sections of code by ensuring that only one thread can execute them at a time. This is achieved by acquiring an object's intrinsic lock before entering the synchronized block and releasing it when exiting.

```java
public class Counter {
    private int count = 0;

    public synchronized void increment() {
        count++;
    }
}
```

```java
public class Main {
    public static void main(String[] args) {
        // Create a shared Counter object
        Counter counter = new Counter();

        // Create multiple threads to increment the counter
        Thread thread1 = new Thread(() -> {
            for (int i = 0; i < 1000000; i++) {
                counter.increment();
            }
        });

        Thread thread2 = new Thread(() -> {
            for (int i = 0; i < 1000000; i++) {
                counter.increment();
            }
        });

        // Start the threads
        thread1.start();
        thread2.start();

        // Wait for both threads to finish
        try {
            thread1.join();
            thread2.join();
        } catch (InterruptedException e) {
            e.printStackTrace();
        }

        // Print the final count
        System.out.println("Final Count: " + counter.getCount());
    }
}
```

In this example, we create two threads (`thread1` and `thread2`) that concurrently increment the Counter object using synchronized `increment` method. Having synchronization enabled for `increment` method, we ensure that only one thread can increment the count variable at a time, making it thread-safe. However, in C++ it is not that easy to make a code thread-safe. To achieve same thread safety in C++ implementation, we have to create and manage the mutex object exclusively. This is a more complex operation, and it is important to note that this was just a simple example. Real-world requirements for thread safety are often much more complex.

## Lock Concept in C++

In C++, a lock is a synchronization mechanism used to control access to shared resources among multiple threads. It prevents data races and ensures that only one thread can access a particular resource at a time. C++ provides several types of locks, including std::mutex, std::unique_lock, and std::lock_guard, among others. Developers can choose the appropriate lock type based on their specific requirements. Here's a simple example of using std::mutex to protect a shared resource:

```cpp
#include <iostream>
#include <thread>
#include <mutex>

class Counter {
public:
    void increment() {
        std::unique_lock<std::mutex> lock(mtx);
        count++;
    }
    auto getCount() const {
        return count;
    }
private:
    std::mutex mtx;
    int count = 0;
};

int main() 
{
    Counter counter;
    std::thread t1([&] {
        for (int i = 0; i < 1000000; i++) {
            counter.increment();
        }
    });
    std::thread t2([&] {
        for (int i = 0; i < 1000000; i++) {
            counter.increment();
        }
    });

    t1.join();
    t2.join();

    std::cout << "Final Count: " << counter.getCount() << std::endl;
    return 0;
}
```

## Updates to New C++ Standards

C++ standards are constantly evolving to improve concurrency and thread safety. Subsequent iterations of the standard have introduced new features and enhancements to the C++ standard library, particularly in the area of concurrent programming.

For example, C++17 introduced the std::shared_mutex class, which allows multiple threads to read a shared resource concurrently, but only one thread to write to it at a time. This is useful for situations where multiple threads need to read the same data, but only one thread needs to modify it.

C++20 introduced the std::jthread class, which simplifies thread management and resource cleanup. Unlike std::thread, which requires explicit calls to join() or detach(), std::jthread automatically joins the thread upon destruction. This makes it easier to write clean and safe multithreaded code.

However, despite these improvements, there is still a lack of libraries that make it easy to synchronize access to classes and their resources in C++. This is in contrast to Java, which has a number of built-in libraries for concurrent programming, such as the `synchronized` keyword and the `java.util.concurrent` package.

## Lockable Class Definition by Andrei Alexandrescu

![Modern C++ Design: Generic Programming and Design Patterns Applied](/yazilim-notlari/assets/img/ModernCppDesignBookCover.jpg)

If you haven't read it yet, 'Modern C++ Design: Generic Programming and Design Patterns Applied' is undeniably one of the most crucial books for those seeking to gain a profound understanding of the C++ language. Authored by the Andrei Alexandrescu, this book goes beyond the boundaries of conventional C++ programming and teaches you how to use generic programming and design patterns to write elegant, efficient, and reusable code. From the book, in Chapter 7 where he explains about smart pointers, he also mention about pre-and postfunction calls (Stroustrup 2000) idiom when returning a PointerType object by value from operator->, the sequence of execution is: constructor, operator->, member access, destructor.
Which means, If you are returning a value type of a proxy type from the `operator->()` operator on your smart pointer, than your code will be compiled into steps defined below:

  1. Constructor of ProxyType
  2. ProxyType::operator-> called; likely returns a pointer to an object of type what smart pointer holds
  3. Member access for actual type; likely a function call
  4. Destructor of ProxyType

In a nutshell, this idiom provides a convenient way to implement locked function calls, and can be used in a variety of scenarios involving multithreading and locked resource access.

## The Implementation of the Lockable Classes in C++

While I was working on this book, I was trying to improve my C++ knowledge by trying to implement the topics in the book myself. While reading the chapter I mentioned above, I started to try if I could write my own thread-safe class RAII helper with the things explained here. Afterwards, I managed to create a C++ version of synchronized class in Java. To make my classes more extensible and allow developers to choose whether or not to make a class thread-safe, I designed the `thread_safe` helper class using aggregation instead of inheritance. This means that if you want to make your class `Counter` thread-safe, all you need to do is define it with the thread_safe helper class, like this: `thread_safe<Counter>`. Returning the same example we made in Java, C++ code will be as follows:

```cpp
class Counter {
public:
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
  thread_safe<Counter> counter;
  std::thread t1([&] {
    for (int i = 0; i < 1000000; i++) {
        counter->increment();
    }
  });
  std::thread t2([&] {
    for (int i = 0; i < 1000000; i++) {
        counter->increment();
    }
  });

  t1.join();
  t2.join();

  std::cout << "Final Count: " << counter->getCount() << std::endl;
  return 0;
}
```

If look at the example, you will notice that the `Counter` class doesn't require any additional variables or logic to ensure the thread safety of `count` increments. Surprisingly, the main code remains unchanged, except for a single modification: encapsulating the `counter` variable using the `thread_safe` helper. To run this code successfully, you will need to include the `thread_safe` implementation:

```cpp
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
```

Let's break down the code part by part, starting with the `thread_safe` helper class and its associated components. `thread_safe` class has basically 3 main components:

1. `auto_locker_t` class: this class is responsible for locking and unlocking between the actual function call is made from the `thread_safe` implementation. Thanks to the "pre-and postfunction calls" idiom mentioned above, following steps will be executed when the `counter->increment();` function call is made:
   * `auto_locker_t` class will be instantiated before the actual function call
   * `auto_locker_t` instance will call `lock()` to ensure the function call will execute thread safe
   * `T* auto_locker_t::operator->()` will forward to actual function call
   * `auto_locker_t` instance will call `unlock()` to release the mutex object
   * `~auto_locker_t()` object will destructed

2. `lockable_T` class: this class will represents the `T` type, despite it will add `lock` and `unlock` functionality into `T` type to enable thread safety for that class. This idiom is also called [Mixin](https://en.wikipedia.org/wiki/Mixin) pattern, which is basically a class that contains methods for use by other classes without having to be the parent class of those other classes.

3. `thread_safe` class itself looks like one another smart pointers which came with C++11. Main difference of this class to smart pointers is `thread_safe` class returns a `auto_locker_t` object instead of the reference of the `T` type. Which this allows to call the functions of the `T` class by having thread safety on top them.

## Benefits to have `thread_safe` helper

In modern C++ programming, thread safety is essential to avoid data corruption and unexpected behavior in concurrent applications. However, writing thread-safe code in C++ can be challenging, as it lacks built-in concurrency mechanisms. This is in contrast to languages like Rust, which offer more integrated solutions for thread safety.

The `thread_safe` helper class simplifies thread safety in C++ by providing an easy-to-use interface for protecting shared resources. Its benefits include:

1. **Simplified Thread Safety:** With `thread_safe`, you can ensure thread safety without complex synchronization mechanisms, making it easier to write concurrent C++ code.
2. **Focus on Core Logic:** Developers can focus on their core logic and algorithms without getting bogged down in low-level thread management details.
3. **Prevent Data Corruption:** `thread_safe` takes care of necessary synchronization, preventing data races and ensuring that shared resources are accessed safely.
4. **Reusable Code:** You can use `thread_safe` with different classes, promoting code reusability and reducing the need for custom thread safety implementations.
5. **Improved Readability:** The code remains concise and readable, making it easier for developers to understand and maintain.

By encapsulating thread safety concerns within the `thread_safe` helper class, you can write cleaner, more maintainable, and more reliable concurrent C++ code.
