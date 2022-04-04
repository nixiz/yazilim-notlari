<!-- ---
layout: post
title: Multiple Observers With Single Observer
subtitle: How to handle multiple observers in publishers with single observer variable
thumbnail-img: /assets/img/matrix_multip_benchmark.png
share-img: /assets/img/matrix_multip_benchmark.png
nav-short: true
comments: true
readtime: true
show-avatar: false
language: en
tags: [C++, templates, observer, OOP]
--- -->

![Observer Patter](https://upload.wikimedia.org/wikipedia/commons/a/a8/Observer_w_update.svg){: .mx-auto.d-block :}

By definition on [Wikipedia][wiki-observer]: The Observer pattern is a software design pattern in which an object, named
the subject, maintains a list of its dependents, called observers, and notifies them automatically of any state changes,
usually by calling one of their methods.  
In this pattern design, it is left to the publisher class to notify the obverser classes. Anyone who uses this pattern
should manage observers in their own way. The most widely used implementation in the design: It is done by keeping the
observers in a list and calling them sequentially for notification.

<!-- Bu pattern tasarımında obverser sınıflara notification yapılması, publisher sınıfına bırakılmıştır. Bu pattern'ı
kullanan herkes kendi yöntemi ile observer'ları yönetmelidir. Tasarım içerisinde en yaygın kullanılan implementasyon: 
observer'ları genellikle bir listede tutarak, notification için sırayla çağırılması şeklinde yapılmaktadır.  -->

```cpp
class Publisher {
  std::list<IObserver*> observers;
public:
  void RegisterObserver(IObserver* obs) {
    observers.push_back(obs);
  }

  void UnRegisterObserver(IObserver* obs) {
    observers.remove(obs);
  }

  void PublishMessage(std::string msg) {
    for (auto* obs : observers) {
      obs->OnMessage(msg);
    }
  }
};
```

In most observer pattern implementations has subscriptions where they have done in runtime. But this has to be done
where it is not clear which class will subscribe and when. However, in most of time observer classes are known during
the development process. Therefore having a runtime dependency for observer registration and having lists are not
necessary for most of observer pattern implementations.  

Ass we all know C++ is 

The solution is using compile time 



















---

[wiki-observer]: https://en.wikipedia.org/wiki/Observer_pattern
