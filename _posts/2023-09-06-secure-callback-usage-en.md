---
layout: post
title: Safe Asynchronous Calls in C++
subtitle: Simplify asynchronous call handling and object lifetime management in C++ projects.
thumbnail-img: /assets/img/async_cb_banner.png
share-img: /assets/img/async_cb_banner.png
nav-short: true
comments: true
readtime: true
show-avatar: false
language: en
gh-repo: nixiz/async-call-helper
gh-badge: [star, follow]
tags: [C++, templates, async, callback, modern-cpp, OOP]
---

Managing the lifetimes of objects in C and C++ projects can be tricky, especially in places where asynchronous calls are made. This is because the lifetimes of objects that will receive feedback are not automatically extended in these places. This can lead to errors and crashes, so it's important to understand how to manage object lifetimes in these situations.

![C++ Road Map](/yazilim-notlari/assets/img/async_cb_banner.png){: .mx-auto.d-block :}

---

## TL;DR

- **GitHub Repository:** You can access the complete code and project details on [GitHub](https://github.com/nixiz/async-call-helper).
- **Summary:** This blog post explores the difficulties of managing object lifetimes in projects that use both C and C++ languages, with a focus on asynchronous calls. We discuss when the object lifetimes are not automatically extended in asynchronous scenarios, which can cause problems. The article also introduces an innovative solution using the `async_call_helper` class, which enhances code readability and ensures safe execution of asynchronous call responses, even provides extensions in modern C++ with lambda expressions. Dive deeper into this intriguing topic and discover how to create robust and reliable software in mixed-language environments.

---

For example, when we look at the code below, if the `service` object in the C library is deleted before the asynchronous call is completed, it is impossible for it to know whether it has been deleted in the feedback of the call. Since the `unsafe_service` object that is attempted to be accessed on line 3 has been deleted, the application will crash.

```cpp
1. static inline void response_cb(void* context, int response) {
2.  auto srv_ptr = static_cast<unsafe_service*>(context);
3.  srv_ptr->response(response);
4. }
5. 
6. void unsafe_service::execute() {
7.   c_long_async_function((void*)this, response_cb, in_param);
8.   delete this;
9. }
```

### Using `std::enable_shared_from_this<T>`

The `unsafe_service` class in the above example was deleted before the asynchronous call completed. This is because the lifetime of the `unsafe_service` class was not extended when the asynchronous call was made. To address the issue encountered in the example above, the `unsafe_service` class should derive from the [`std::enable_shared_from_this<unsafe_service>`][enable-shared-this-link] helper class and be created with a [`shared_ptr<unsafe_service>`][shared-ptr-link].  
However, this alone is not sufficient; another helper object that holds a [`weak_ptr`][weak-ptr-link] reference should also be sent instead of its own reference directly to the asynchronous call. This way, when the response to the asynchronous call is received, the context argument will be converted to this helper object type, and the [`weak_ptr<unsafe_service>`][weak-ptr-link] within it can be used to check whether the service object is still alive.

```cpp
class unsafe_service 
  : public std::enable_shared_from_this<unsafe_service> {
// ...
};

struct async_callback_token {
  std::weak_ptr<unsafe_service> caller;
};

static inline void response_cb(void* context, int out_param) {
  std::unique_ptr<async_callback_token> act_handle(
      static_cast<async_callback_token*>(context));
  auto srv_ptr_weak = act_handle->caller;
  if (auto srv_ptr = srv_ptr_weak.lock()) {
    srv_ptr->response(out_param);
  }
  else {
    std::cerr << "caller instance is deleted!!\n";
  }
}

void unsafe_service::execute() {
  auto* context = new async_callback_token{
    this->weak_from_this()
  };
  c_long_async_function((void*)context, response_cb, 300, 300);
}
```

{: .box-note}
**Note:** You can access the complete code for the above example [here][godbolt-1].

The solution we have found works, but it requires changes to be made to the developed class and the lifecycles of the classes wanting to use the method to be managed by creating [`shared_ptr<T>`][shared-ptr-link]. Even if we are developing the classes we are working on, it is not always possible to change their usage. For example, if we are using a framework and the framework creates the `service` class in a certain way, we have to use it in the same way. Moreover, applying the same method repeatedly for classes making asynchronous calls would be both difficult and contrary to the [SOLID][solid-link-wiki] principles.

A better approach to handling asynchronous call responses is to have the classes that handle the calls do it automatically. This is more reliable and less error-prone than doing it manually. Additionally, it is important to use a secure structure that will not crash the application if the objects that made the calls are deleted.

### Developing the `async_call_helper` Class

We have identified the problem above and made a simple design for a solution. To generate a general solution based on our example, we have to determine our requirements.  
Our `async_call_helper` class should have at least the following features:

* It should not change how the class to be used is created or its lifetime.
* It should cause minimal or no changes to the class being used.
* When the class object used is deleted from the system, asynchronous call responses should not cause the application to crash.
* **Bonus:** Asynchronous call responses should preferably be handled via function pointers such as [`lambda`][lambda-link] or [`std::bind`][bind-link].

Let's start improving our `async_call_helper` class to meet these requirements. First, we need to decide how the class will be used. The `async_call_helper` class should be used via inheritance to make it easier for target classes to use its features. In addition to that, to allow access to the type and reference of the service classes, the `async_call_helper` class should also use the [CRTP][crtp-link] technique.

```cpp
template <typename Caller>
class async_call_helper
{
public:
  // ...
  void* get_context() const;
};

class safe_service 
  : public async_call_helper<safe_service> {
public:
  void execute();
};

void safe_service::execute() {
  c_long_async_function(get_context(), response_cb, 300, 300);
}
```

Using the [CRTP][crtp-link] technique, while the `safe_service` class can use the `get_context()` method as if it were its own method, the `async_call_helper` class can access the `safe_service` reference whenever it needs to. We should also note that we need the [`shared_ptr`][shared-ptr-link] and [`weak_ptr`][weak-ptr-link] pair that we used in our sample solution here as well. Since we cannot change how the `async_call_helper` class objects are created, we need a [`shared_ptr`][shared-ptr-link] object inside the `async_call_helper` class.

```cpp
template <typename Caller>
class async_call_helper
{
public:
  void* get_context() const;
  // ...
private:
  struct auto_ref_holder {
    // ...
  };
  std::shared_ptr<auto_ref_holder> lifetime_ref;
};
```

{: .box-note}
**Note:** Since the `async_call_helper` creates the `auto_ref_holder` class as `shared_ptr` type, the `auto_ref_holder` class does no more need to inherit from the `std::enable_shared_from_this<auto_ref_holder>` class anymore.

If we look back at the sample code above, we can see that an intermediate object named `async_callback_token` is sent instead of the service class itself in the asynchronous call. Thanks to this helper class, we can safely check whether the service object is alive and protect the application from crashes when processing the response of the asynchronous call. We should also use the same class inside the newly created `async_call_helper` class. The `get_context()` function will return this helper object.

```cpp
template <typename Caller>
class async_call_helper
{
public:
  async_callback_token* get_context() const;
  // ...
};
```

When the asynchronous call invokes the `response_cb(void *context, ...)` function, the `context` object is actually holds the `asyn_call_token` object, so we first need to cast it to the `asyn_call_token` type. Then, by calling the `get_caller()` virtual method of this class, we can access the reference of our service object or `nullptr` if this object has been deleted. This way, we will have a safe way that can run safely without causing a crash in the application.

```cpp
struct asyn_call_token
{
  virtual ~asyn_call_token() = default;
  template <typename Cast> 
  static Cast* from_context(void* context) noexcept
  {
    std::unique_ptr<asyn_call_token> handle(
      reinterpret_cast<asyn_call_token*>(context));
    auto *cast_ptr = static_cast<Cast*>(handle->get_caller());
    return cast_ptr;    
  }
protected:
  virtual void* get_caller() = 0;
};

static inline void response_cb(void* context, int out_param) {
  auto srv_ptr = asyn_call_token::from_context<safe_service>(context);
  if (srv_ptr) {
    srv_ptr->response(out_param);
  } else {
    std::cerr << "service instance has already been deleted\n";
  }
}
```

Above, we wrote how to use the `context` object to be used in the response of the asynchronous call is handled. We still need to implement the `get_context()` method to make the code we wrote working. The `asyn_call_token` object returned from this method should hold a weak reference to the `auto_ref_holder` class and return either the reference to the service class or `nullptr` if it has been deleted.

```cpp
template <typename Caller>
class async_call_helper
{
public:
  async_call_helper() {
    lifetime_ref = std::make_shared<auto_ref_holder>(
      static_cast<Caller*>(this));
  }

  void* get_context() const 
  {
    struct special_token final : public asyn_call_token
    {
      special_token(std::weak_ptr<auto_ref_holder> ref_) 
        : ref(ref_) {}
      ~special_token() = default;

      void* get_caller() override {
        auto sref = ref.lock();
        return (sref) ? sref->get_parent() : nullptr;
      }
    private:
      std::weak_ptr<auto_ref_holder> ref;
    };
    return new special_token(weak_ref());
  }
  // ...
private:
  struct auto_ref_holder
    : public std::enable_shared_from_this<auto_ref_holder> {
    explicit auto_ref_holder(Caller* caller_) : caller(caller_) {}
    Caller* get_parent() { return caller; }
  };
  std::weak_ptr<auto_ref_holder> weak_ref() const noexcept {
    return lifetime_ref;
  }
  std::shared_ptr<auto_ref_holder> lifetime_ref;
};
```

{: .box-note}
**Note:** By making the `asyn_call_token` class polymorphic, we have created a flexible structure that can be adapted to meet future needs. For example, we could add timeout counters to our asynchronous calls in the future, so that we would know if a call has not been answered after a certain period of time.

### We ❤️ Modern C++

Based on the work we have completed so far, we are on track to meet our original requirements. However, we can use the `async_call_helper` class not only for C functions but also for C++ asynchronous calls. Similarly, thanks to the [lambda expressions][lambda-link] introduced with Modern C++, we can enhance code readability even further by writing the responses to asynchronous calls right where the calls are made.

```cpp
void safe_service::execute() 
{
  auto context = get_context<int>([this] (int result) {
    std::cout << "received result" << result << "\n";
    this->process_result_of_async_call(result);
  });
  c_long_async_function(context.context, context.callback, *param);
}
```

Having an interface like the one above makes the code much simpler and more readable compared to the original version. Let's continue and write our `get_context` method that will make the above code work. Our new method will have the same mechanics as the previous `get_context` method, but it will only keep the C callback function internally, so that the asynchronous call can provide a function pointer with the desired signature as `context.callback`. To achieve this, it should return a function pointer whose signature it knows, a `context` object, and a `callback_context` object with `operator()` methods for C++ calls.

```cpp
template <typename ...Args>
struct callback_context {
  void *context;
  void (*callback)(Args...);
  void operator()(Args... args) noexcept {
    std::invoke(callback, std::forward<Args>(args)...);
  }
};
```

Making the callback_context object a template allows it to know the types of arguments that the function used in asynchronous calls will need, and ensures that the variables passed to the calls have the correct types. For C calls, we use the `context` and `callback` variables, while for C++ calls, we simply pass the `callback_context` object itself to the asynchronous call.  
Let's start writing the new `get_context` method that returns the `callback_context` object. Inside it, there should be a helper class that inherits the `asyn_call_token` type, just like the previous implementation. However, unlike the previous implementation, this helper class will also hold the [`lambda`][lambda-link] expression or function pointer that will handle the asynchronous call response. When it calls the asynchronous call feedback function, the new helper class we've created will call the function provided at the beginning if the service object is still alive.

```cpp
template <typename Caller>
class async_call_helper
{
public:
  template <typename ...Args, typename Fn>
  callback_context<void*, Args...> 
  get_context(Fn&& cb) noexcept 
  {
    struct trampoline_t final
      : public asyn_call_token
    {
      trampoline_t(std::weak_ptr<auto_ref_holder> ref_, 
                   std::function<void(Args...)> callback_) 
        : ref(ref_) 
        , callback(std::move(callback_)) { }
      ~trampoline_t() = default;

      void* get_caller() override {
        guard.lock();
        auto sref = ref.lock();
        return (sref) ? sref->get_parent() : nullptr;
      }

      static inline void callback_handle(void* context, Args... args) {
        std::unique_ptr<trampoline_t> trampoline_ptr(
          reinterpret_cast<trampoline_t*>(context));
        if (trampoline_ptr->get_caller()) {
          std::invoke(trampoline_ptr->callback, std::forward<Args>(args)...);
        }
      }
    private:
      std::weak_ptr<auto_ref_holder> ref;
      std::function<void(Args...)> callback;
    };
    std::function<void(Args...)> callback = cb;
    return callback_context<void*, Args...>
           {
             new trampoline_t(weak_ref(), std::move(callback)),
             &trampoline_t::callback_handle
           };
  }
  // ...
};
```

In the above code, the statically defined `callback_handle` function will be responsible for handling the response of the asynchronous call and safely executing the desired feedback function. If the service object has been deleted, it will do nothing, preventing the application from crashing.  
The `get_context()` method can create a `callback_context` object with the correct variables by calling it with the types of functions that will be used for the asynchronous call response.

After all these improvements, the callback functions we provide for asynchronous calls will ensure the correct execution of the callback function, checking the validity of the calling object, preventing the application from crashing, and providing a more modern structure.  
If we combine the code we've written, the final version of our code will be as follows:

{: .box-note}
You can access the example code and the final state of the project on the [GitHub page](https://github.com/nixiz/async-call-helper).

```cpp
// async function callback support for single threaded environments.
// can be use only if async calls has sharing same thread with caller instance
struct single_thread_usage {};

// in case if async call executing from another thread,
// the destruction of caller and async callback function execution can run
// at the same time, this policy must be using for thread safety
struct multi_thread_usage {};

// trait struct for the usage of threading policy
template <typename thread_support_t>
struct thread_support_traits {
  // should have type definitions described below
  // typedef mutex_type
  // typedef lock_guard_t
  // typedef unique_lock_t
};

struct asyn_call_token {
  virtual ~asyn_call_token() = default;

  template <typename Cast>
  static inline Cast* from_context(void* context) noexcept {
    std::unique_ptr<asyn_call_token> act_handle(reinterpret_cast<asyn_call_token*>(context));
    if (!act_handle)
      return nullptr;
    auto* cast_ptr = static_cast<Cast*>(act_handle->get_caller());
    return cast_ptr;
  }

 protected:
  virtual void* get_caller() = 0;
};

template <typename Caller, typename thread_support_t = single_thread_usage>
class async_call_helper {
 public:
  using ThisType = async_call_helper<Caller, thread_support_t>;
  ~async_call_helper() = default;

 protected:
  async_call_helper() { lifetime_ref = std::make_shared<auto_ref_holder>(parent()); }

  void* get_context() const noexcept {
    struct special_token final : public asyn_call_token {
      using mutex_t = typename thread_support_traits<thread_support_t>::mutex_type;
      special_token(std::weak_ptr<auto_ref_holder> ref_, mutex_t& guard_)
          : ref(ref_), guard(guard_, std::defer_lock) {}

      ~special_token() {
        if (guard) {
          guard.unlock();
        }
      }

      void* get_caller() override {
        guard.lock();
        auto sref = ref.lock();
        // at this pointer either we have
        // the parent[ThisType] pointer or nullptr so we can safely operate over the pointer
        return (sref) ? sref->get_parent() : nullptr;
      }

     private:
      std::weak_ptr<auto_ref_holder> ref;
      typename thread_support_traits<thread_support_t>::unique_lock_t guard;
    };
    return new special_token(weak_ref(), guard);
  }
  template <typename... Args>
  struct callback_context {
    void* context;
    void (*callback)(void*, Args...);
    void operator()(Args... args) noexcept {
      std::invoke(callback, context, std::forward<Args>(args)...);
    }
  };

  template <typename... Args, typename Fn>
  callback_context<Args...> get_context(Fn&& cb) noexcept {
    struct trampoline_t final : public asyn_call_token {
      using mutex_t = typename thread_support_traits<thread_support_t>::mutex_type;
      trampoline_t(std::weak_ptr<auto_ref_holder> ref_,
                   mutex_t& guard_,
                   std::function<void(Args...)> callback_)
          : ref(ref_), guard(guard_, std::defer_lock), callback(std::move(callback_)) {}

      ~trampoline_t() {
        if (guard) {
          guard.unlock();
        }
      }

      void* get_caller() override {
        guard.lock();
        auto sref = ref.lock();
        // at this pointer either we have
        // the parent[ThisType] pointer or nullptr so we can safely operate over the pointer
        return (sref) ? sref->get_parent() : nullptr;
      }

      static inline void callback_handle(void* context, Args... args) {
        std::unique_ptr<trampoline_t> trampoline_ptr(reinterpret_cast<trampoline_t*>(context));
        if (!trampoline_ptr)
          return;
        if (trampoline_ptr->get_caller()) {
          std::invoke(trampoline_ptr->callback, std::forward<Args>(args)...);
        }
      }

     private:
      std::weak_ptr<auto_ref_holder> ref;
      typename thread_support_traits<thread_support_t>::unique_lock_t guard;
      std::function<void(Args...)> callback;
    };
    std::function<void(Args...)> callback = cb;
    return callback_context<Args...>{new trampoline_t(weak_ref(), guard, std::move(callback)),
                                     &trampoline_t::callback_handle};
  }

 protected:
  Caller* parent() { return static_cast<Caller*>(this); }

  const Caller* parent() const { return static_cast<const Caller*>(this); }

  void set_deleted() noexcept {
    using lock_guard_t = typename thread_support_traits<thread_support_t>::lock_guard_t;
    lock_guard_t lock(guard);
    lifetime_ref.reset();
  }

 private:
  friend struct auto_ref_holder;
  struct auto_ref_holder {
    explicit auto_ref_holder(Caller* caller_) : caller(caller_) {}
    Caller* get_parent() { return caller; }
    const Caller* get_parent() const { return caller; }

   private:
    Caller* caller;
  };

  std::weak_ptr<auto_ref_holder> weak_ref() noexcept { return lifetime_ref; }

  std::weak_ptr<auto_ref_holder> weak_ref() const noexcept { return lifetime_ref; }

  std::shared_ptr<auto_ref_holder> lifetime_ref;
  mutable typename thread_support_traits<thread_support_t>::mutex_type guard;
};

template <>
struct thread_support_traits<single_thread_usage> {
  struct dummy_mutex {};
  using mutex_type = dummy_mutex;
  struct dummy_locker {
    explicit dummy_locker(mutex_type&) {}
    dummy_locker(mutex_type&, std::adopt_lock_t) {}
    dummy_locker(mutex_type&, std::defer_lock_t) {}
    dummy_locker(mutex_type&, std::try_to_lock_t) {}
    explicit operator bool() const noexcept { return false; }
    void lock() {}
    void unlock() {}
  };
  using lock_guard_t = dummy_locker;
  using unique_lock_t = dummy_locker;
};

template <>
struct thread_support_traits<multi_thread_usage> {
  using mutex_type = std::mutex;
  using lock_guard_t = std::lock_guard<mutex_type>;
  using unique_lock_t = std::unique_lock<mutex_type>;
};
```

---

[godbolt-1]: https://gist.github.com/nixiz/0055fefdc2b936b8f9ab6594b57a3abe
[shared-ptr-link]: https://en.cppreference.com/w/cpp/memory/shared_ptr
[weak-ptr-link]: https://en.cppreference.com/w/cpp/memory/weak_ptr
[solid-link-wiki]: https://en.wikipedia.org/wiki/SOLID
[lambda-link]: https://en.cppreference.com/w/cpp/language/lambda
[bind-link]: https://en.cppreference.com/w/cpp/utility/functional/bind
[crtp-link]: https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
[enable-shared-this-link]: https://en.cppreference.com/w/cpp/memory/enable_shared_from_this
