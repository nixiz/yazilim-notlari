---
layout: post
title: Building a Thread Pool with C++ and STL
subtitle: Exploring Thread Pools, Boost asio::thread_pool, and Creating a Pure C++ Solution from Scratch
thumbnail-img: /assets/img/thread_pool_banner.png
share-img: /assets/img/thread_pool_banner.png
nav-short: true
comments: true
readtime: true
show-avatar: false
language: tr
gh-repo: nixiz
gh-badge: [star, follow]
tags: [C++, threadpool, template, modern-cpp, OOP]
---

## Thread Pool Implementation in C++

Thread pools are a fundamental concept in concurrent programming, providing a way to efficiently manage and execute a large number of tasks concurrently. Thread pools are a group of worker threads that can be used to execute tasks concurrently. Instead of creating a new thread for each task, a thread pool reuses existing threads, which can significantly improve performance and resource utilization. Boost's [asio::thread_pool](https://www.boost.org/doc/libs/1_83_0/doc/html/boost_asio/reference/thread_pool.html) is a powerful tool in the Boost library that makes asynchronous I/O and concurrency easier. Inspired by Boost, this article carefully shows how to create a thread pool using only the STL and C++. The article also includes code breakdowns and PlantUML diagrams to help readers understand the implementation better. The complete example can be found at the end of the article or [here](https://gist.github.com/nixiz/12bdf1c267c45fab9468c6cff1617092)

### Boost Library

[Boost](https://www.boost.org/) is a set of high-quality, peer-reviewed C++ libraries that are designed to work well with the C++ Standard Library. These libraries cover a wide range of functionalities, including data structures, algorithms, I/O, concurrency, and more. Boost aims to provide portable, efficient, and reusable code for C++ developers.

Boost libraries are often considered for inclusion in the C++ Standard Library, and many Boost components have eventually made their way into the standard.

### Boost Asio Library

[Boost Asio](https://www.boost.org/doc/libs/1_83_0/doc/html/boost_asio.html) is a part of the Boost library that provides support for asynchronous I/O and concurrency. It's commonly used for building network and communication applications. Asynchronous I/O allows the program to perform other tasks while waiting for I/O operations to complete, improving efficiency and responsiveness.

### `boost::asio::thread_pool`

The Asio library in Boost includes a thread pool implementation starting with 1.66 version. `boost::asio::thread_pool` is a component within `boost::asio` that provides a simple way to manage a pool of threads. Thread pools are a common concurrency pattern where a fixed number of worker threads are created and used to execute tasks concurrently. This is especially useful in scenarios where creating and destroying threads can be expensive.

Here's a basic overview of using `boost::asio::thread_pool`:

```cpp
#include <iostream>
#include <boost/asio/thread_pool.hpp>

int main() {
    boost::asio::thread_pool pool;

    for (int i = 0; i < 5; ++i) {
        boost::asio::post(pool, [i]() {
            std::cout << "Task " << i << " executed by thread " << std::this_thread::get_id() << std::endl;
        });
    }

    std::future<int> r1 = boost::asio::post(pool, asio::use_future([]() { return 2; }));
    std::cout << "Result = " << r1.get() << '\n';

    // Optional: Wait for all tasks to complete
    pool.join();

    return 0;
}
```

## Rewrite It with STL

When I saw the new `thread_pool` usage and the new [`executor`](https://www.boost.org/doc/libs/1_83_0/doc/html/boost_asio/std_executors.html) definition in the Asio library, I was impressed. To understand how they were able to get return values from asynchronous operations using a single wrapper function and to improve my skills, I decided to rewrite the thread pool implementation in plain C++ using only the STL. I set a requirement for myself to replicate the `boost::asio::thread_pool` implementation as closely as possible, including the function calls and [`use_future`](https://www.boost.org/doc/libs/1_83_0/boost/asio/use_future.hpp) wrapper. When I finished my implementation, it looked like this:

```cpp
class thread_pool
{
private:
  std::atomic_bool is_active {true};
  std::vector<std::thread> pool;
  std::condition_variable cv;
  std::mutex guard;
  std::deque<std::packaged_task<void()>> pending_jobs;
public:
  explicit thread_pool(int num_threads = 1) 
  {
    for (int i = 0; i < num_threads; i++) {
      pool.emplace_back(&thread_pool::run, this);
    }
  }
  ~thread_pool() {
    is_active = false;
    cv.notify_all();
    for (auto& th : pool) {
      th.join();
    }
  }

  void post(std::packaged_task<void()> job) {
    std::unique_lock lock(guard);
    pending_jobs.emplace_back(std::move(job));
    cv.notify_one();
  }
private:
  void run() noexcept 
  {
    while (is_active)
    {
      thread_local std::packaged_task<void()> job;
      {
        std::unique_lock lock(guard);
        cv.wait(lock, [&]{ return !pending_jobs.empty() || !is_active; });
        if (!is_active) break;
        job.swap(pending_jobs.front());
        pending_jobs.pop_front();
      }
      job();
    }
  }
};
```

Let's break down the key components of the implementation:

- **`thread_pool` Class:** This class encapsulates the entire thread pool. It maintains a set of threads, a condition variable (`cv`), and a mutex (`guard`) to synchronize access to the task queue.

- **`post` Method:** Enqueues a new task into the task queue and notifies a waiting thread.

- **`run` Method:** The actual function executed by each worker thread. It continuously waits for tasks in the queue and executes them. In this function, thread_pool doesn't know and does not care whether the executing function has return value or not. This will be handled by the global `post` function by using `use_future` wrapper function.
    {: .box-note}
    **Note:** where the line `thread_local std::packaged_task<void()> job;` I used [`thread_local`](https://en.cppreference.com/w/cpp/language/storage_duration) to make sure that the ownership of the executing task is moved to the thread where it will be called.

Let's write the global `post` and `use_future` functions to complete the thread pool implementation:

```cpp
template <class Executor, class Fn>
void post(Executor& exec, Fn&& func)
{
  using return_type = decltype(func());
  static_assert(std::is_void_v<return_type>, "posting functions with return types must be used with \"use_future\" tag.");
  std::packaged_task<void()> task(std::forward<Fn>(func));
  exec.post(std::move(task));
}

struct use_future_tag {};
template <class Fn>
constexpr auto use_future(Fn&& func) {
  return std::make_tuple(use_future_tag{}, std::forward<Fn>(func));
}

template <class Executor, class Fn>
[[nodiscard]] decltype(auto) 
post(Executor& exec, std::tuple<use_future_tag, Fn>&& tpl)
{
  using return_type = std::invoke_result_t<Fn>;
  auto&& [_, func] = tpl;
  if constexpr (std::is_void_v<return_type>) 
  {
    std::packaged_task<void()> tsk(std::move(func));
    auto ret_future = tsk.get_future();
    exec.post(std::move(tsk));
    return ret_future;
  }
  else
  {
    struct forwarder_t {
      forwarder_t(Fn&& fn) : tsk(std::forward<Fn>(fn)) {}
      void operator()(std::shared_ptr<std::promise<return_type>> promise) noexcept
      {
        promise->set_value(tsk());
      }
    private:
      std::decay_t<Fn> tsk;
    } forwarder(std::forward<Fn>(func));

    auto promise = std::make_shared<std::promise<return_type>>();
    auto ret_future = promise->get_future();
    std::packaged_task<void()> tsk([promise = std::move(promise), forwarder = std::move(forwarder)] () mutable {
      forwarder(promise);
    });
    exec.post(std::move(tsk));
    return ret_future;
  }
}
```

- **`post` Function:** this is the plain function which only creates a [`packaged_task`](https://en.cppreference.com/w/cpp/thread/packaged_task) from the given lambda or function pointer and puts into queue of the provided `Executor` class, which is our `thread_pool` in this instance.

- **`use_future` Function:** Instead of Boost implementation, which they have created an allocator aware wrapper class that holds the callable, what I did here is I simply moved the callable object into a tuple with [`use_future_tag`](https://en.wikibooks.org/wiki/More_C%2B%2B_Idioms/Tag_Dispatching) struct. Therefore I can create an overload version of `post` function returns a `std::future<>` object with the return type of the callable object.

{: .box-note}
**Quote:** *"All problems in computer science can be solved by another level of indirection",  Butler Lampson, 1972*

- **Second `post` Function:** As the `thread_pool` class only accepts `std::packaged_task<void()>` type of callable into its queue, I had to create a higher order function, a local callable class `forwarder_t` which makes the actual function call, stores the return value into a promise object and returns void. When the `thread_pool` run the task which has return value, the following sequence will be executed:
    {% plantuml %}
    @startuml
    participant thread_pool as tp
    participant forwarder_t as fwd
    participant Task as task

    tp -> tp: run()
    activate tp
    tp -> fwd: job()
    activate fwd
    fwd -> fwd: operator(promise<return_type>)
    activate fwd
    fwd -> task: task()
    activate task
    task --> fwd: return value
    deactivate fwd
    deactivate task
    fwd -> fwd: promise.set_value()
    fwd --> tp: return;
    deactivate fwd
    @enduml
    {% endplantuml %}

## Complete Example

I learned a lot by studying how thread pools work, exploring the Boost asio::thread_pool library, and writing my own thread pool implementation in plain C++. I think this experience has helped me to understand concurrent programming better, especially how thread pools work. I hope this article has been helpful to you, whether you are using Boost's thread pool or are looking for a simpler, dependency-free solution. Happy coding

[Compiler Explorer](https://godbolt.org/z/6oTco3Tjz)

```cpp
#include <iostream>
#include <memory>
#include <thread>
#include <mutex>
#include <future>
#include <condition_variable>
#include <functional>
#include <vector>
#include <deque>
#include <type_traits>

class thread_pool
{
private:
  std::atomic_bool is_active {true};
  std::vector<std::thread> pool;
  std::condition_variable cv;
  std::mutex guard;
  std::deque<std::packaged_task<void()>> pending_jobs;
public:
  explicit thread_pool(int num_threads = 1) 
  {
    for (int i = 0; i < num_threads; i++) {
      pool.emplace_back(&thread_pool::run, this);
    }
  }
  ~thread_pool() {
    is_active = false;
    cv.notify_all();
    for (auto& th : pool) {
      th.join();
    }
  }

  void post(std::packaged_task<void()> job) {
    std::unique_lock lock(guard);
    pending_jobs.emplace_back(std::move(job));
    cv.notify_one();
  }
private:
  void run() noexcept 
  {
    while (is_active)
    {
      thread_local std::packaged_task<void()> job;
      {
        std::unique_lock lock(guard);
        cv.wait(lock, [&]{ return !pending_jobs.empty() || !is_active; });
        if (!is_active) break;
        job.swap(pending_jobs.front());
        pending_jobs.pop_front();
      }
      job();
    }
  }
};

struct use_future_tag {};

template <class Fn>
constexpr auto use_future(Fn&& func) {
  return std::make_tuple(use_future_tag {}, std::forward<Fn>(func));
}

template <class Executor, class Fn>
void post(Executor& exec, Fn&& func)
{
  using return_type = decltype(func());
  static_assert(std::is_void_v<return_type>, "posting functions with return types must be used with \"use_future\" tag.");
  std::packaged_task<void()> task(std::forward<Fn>(func));
  exec.post(std::move(task));
}

template <class Executor, class Fn>
[[nodiscard]] decltype(auto) 
post(Executor& exec, std::tuple<use_future_tag, Fn>&& tpl)
{
  using return_type = std::invoke_result_t<Fn>;
  auto&& [_, func] = tpl;
  if constexpr (std::is_void_v<return_type>) 
  {
    std::packaged_task<void()> tsk(std::move(func));
    auto ret_future = tsk.get_future();
    exec.post(std::move(tsk));
    return ret_future;
  }
  else
  {
    struct forwarder_t {
      forwarder_t(Fn&& fn) : tsk(std::forward<Fn>(fn)) {}
      void operator()(std::shared_ptr<std::promise<return_type>> promise) noexcept
      {
        promise->set_value(tsk());
      }
    private:
      std::decay_t<Fn> tsk;
    } forwarder(std::forward<Fn>(func));

    auto promise = std::make_shared<std::promise<return_type>>();
    auto ret_future = promise->get_future();
    std::packaged_task<void()> tsk([promise = std::move(promise), forwarder = std::move(forwarder)] () mutable {
      forwarder(promise);
    });
    exec.post(std::move(tsk));
    return ret_future;
  }
} 

using namespace std::chrono_literals;
int main()
{
    thread_pool pool {2};
    auto waiter = 
        post(pool, use_future([] 
        {
            std::this_thread::sleep_for(1s);
            return 2;
        }));

    auto test_lmbda = [] {
        thread_local int count = 1;
        std::cout 
        << "working thread: " << std::this_thread::get_id()
        << "\tcount: " << count++ << std::endl;
        std::this_thread::sleep_for(50ms);
    };
    for (size_t i = 0; i < 10; i++)
    {
        post(pool, test_lmbda);
    }
    return waiter.get();
}
```
