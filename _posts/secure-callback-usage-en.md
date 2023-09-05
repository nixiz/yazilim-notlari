---
layout: post
title: C++ Güvenli Asenkron Çağrı Kullanımı
subtitle: C ve C++ asenkron çağrıların güvenli olarak kullanılması
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

In projects where both C and C++ are used, especially in places where asynchronous calls are made, managing the lifetimes of objects can be challenging and error-prone. This is because, in places where asynchronous calls are made, the lifetimes of objects that will receive the feedback are not automatically extended.

![C++ Road Map](/yazilim-notlari/assets/img/async_cb_banner.png){: .mx-auto.d-block :}

For example, when we look at the code below, if the `service` object, which makes an asynchronous call in the C library, is removed from memory before the asynchronous call is completed, it is impossible for it to know whether it has been deleted when receiving the call's feedback. Since the `unsafe_service` object accessed in line 3 has already been deleted, the application will crash.

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

### Usage of `std::enable_shared_from_this<T>`

To solve the problem encountered in the example above, the `unsafe_service` class should derive from the [`std::enable_shared_from_this<unsafe_service>`][enable-shared-this-link] helper class and should be created as a [`shared_ptr<unsafe_service>`][shared-ptr-link] type object. However, this is not enough; another auxiliary object that holds a [`weak_ptr`][weak-ptr-link] reference should also be sent instead of its own reference to the asynchronous call. This way, when the response to the asynchronous call is received, the context argument will be converted to this auxiliary object type, and the [`weak_ptr<unsafe_service>`][weak-ptr-link] inside it will be used to check whether the service object is alive or not.

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

**Note:** You can access the full code for the above example [here][godbolt-1].

The solution we found works, but it requires changes to the class being developed and requires the management of the lifetimes of the classes that want to use this method using [`shared_ptr<T>`][shared-ptr-link], even if we are developing the classes ourselves. If we are using a framework that creates the `service` class as the framework wants, we have to use it in the same way. Moreover, applying the same method repeatedly for classes that make asynchronous calls is not in line with the [SOLID][solid-link-wiki] principles and can be both difficult and impractical.

Instead, it would be a better approach to have classes that make asynchronous calls automatically handle asynchronous call responses and, most importantly, use a safe structure that does not crash the application when calling objects are removed from the system.

### Improving the `async_call_helper` Class

In the previous section, we identified the problem and made a simple improvement to the `unsafe_service` class. To generate a general solution from the example we developed, we can determine our requirements. Our asynchronous call helper class should have at least the following features:

* It should not change how the class to be used is created or its lifetime is managed.
* It should introduce minimal or no changes to the class to be used.
* It should not cause the application to crash when calling objects are deleted.
* **Bonus:** Asynchronous call responses should preferably be processed using function pointers such as [`lambda`][lambda-link] or [`bind`][bind-link].

To achieve this, our `async_call_helper` class should be inherited with the help of the [CRTP][crtp-link] technique, so that classes using this interface have natural access to the properties we defined. At the same time, the `async_call_helper` class should also use the [CRTP][crtp-link] technique to access the type and reference of the service class so that it can be accessed when needed.

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

The use of [CRTP][crtp-link] allows the `safe_service` class to use the `get_context()` method as if it were its own, while the `async_call_helper` class can access the reference of the `safe_service` class when needed. We continue to improve our class. In our example, we also used [`shared_ptr`][shared-ptr-link] and [`weak_ptr`][weak-ptr-link] pairs that we need here. Since we cannot change how the classes that use the `async_call_helper` class are created, the `async_call_helper` class itself needs a [`shared_ptr`][shared-ptr-link] object.

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

**Note:** The `auto_ref_holder` class does not need to inherit from `std::enable_shared_from_this<auto_ref_holder>` since it is created and used by the `async_call_helper` class.

If we look again at the example code we wrote above, we will see that an auxiliary object named `async_callback_token` is sent instead of the service object itself to receive the response of the asynchronous call. With this auxiliary class, we can safely check whether the service object is alive when the response of the call is received and prevent the application from crashing. We should also use the same class inside the `async_call_helper` class we just wrote. The `get_context()` method should return this auxiliary class, and the `response_cb` function should be able to use it.

```cpp
template <typename Caller>
class async_call_helper
{
public:
  async_callback_token* get_context() const;
  // ...
};
```

When the asynchronous call invokes the `response_cb(void *context, ...)` function, the `context` object is actually an `async_call_token` object. First, we will perform the conversion to this type of object. Then, when calling the function that processes the response of the asynchronous call, we will use the `weak_ptr` reference inside it to check whether the service object

 is still alive.

```cpp
template <typename Caller>
class async_call_helper
{
public:
  async_callback_token* get_context() const {
    return lifetime_ref.get();
  }
  // ...
};
```

**Note:** You can access the full code for the above example [here][godbolt-2].

This approach allows us to design a general-purpose class, the `async_call_helper`, which can be used with any class that needs to make asynchronous calls, without requiring significant changes to the classes themselves. The `async_call_helper` takes care of managing the lifetimes of the objects and ensuring that the application doesn't crash when receiving asynchronous call responses. It also allows for flexible customization of the response handling process using function pointers like lambdas or bind.

In summary, the `async_call_helper` class, when used with classes that need to make asynchronous calls, simplifies the management of object lifetimes and enhances the safety and reliability of handling asynchronous call responses, all without introducing significant changes to the classes using it.

[godbolt-1]: https://godbolt.org/z/9x7vno16s
[godbolt-2]: https://godbolt.org/z/7j47TcTdc
[enable-shared-this-link]: https://en.cppreference.com/w/cpp/memory/enable_shared_from_this
[shared-ptr-link]: https://en.cppreference.com/w/cpp/memory/shared_ptr
[weak-ptr-link]: https://en.cppreference.com/w/cpp/memory/weak_ptr
[solid-link-wiki]: https://en.wikipedia.org/wiki/SOLID
[lambda-link]: https://en.cppreference.com/w/cpp/language/lambda
[bind-link]: https://en.cppreference.com/w/cpp/utility/functional/bind
[crtp-link]: https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern