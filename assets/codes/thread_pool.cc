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

int main()
{
  using namespace std::chrono_literals;
  {
    thread_pool pool {2};
    std::promise<void> promise;
    auto waiter = promise.get_future();
    post(pool, [&] 
         {
           std::this_thread::sleep_for(10s);
           promise.set_value();
         });

    auto test_lmbda = [] {
      thread_local int count = 1;
      std::cout 
        << "working thread: " << std::this_thread::get_id()
        << "\tcount: " << count++ << std::endl;
      std::this_thread::sleep_for(1s);
    };
    for (size_t i = 0; i < 10; i++)
    {
      post(pool, test_lmbda);
    }
    waiter.wait();
  }

  {
    thread_pool test_guard;
    auto guard =
      post(test_guard,
           use_future([]
                      {
                        std::cout << "enter any key to quit app..\n";
                        std::cin.get();
                      }));

    thread_pool pool {1};
    auto test_lmbda = [&] {
      post(pool, [] 
           {
             thread_local int count = 1;
             std::cout
               << "working thread: " << std::this_thread::get_id()
               << "\tcount: " << count++ << std::endl;
             std::this_thread::sleep_for(1s);
           });
    };

    for (size_t i = 0; i < 10; i++) {
      std::thread(test_lmbda).detach();
    }
    guard.wait();
  }
}