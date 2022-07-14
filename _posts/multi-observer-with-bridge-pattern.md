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
In this pattern design, it is left to the publisher class to notify the observer classes. Anyone who uses this pattern
should manage observers in their own way. The most widely implementation used in the design: It is done by keeping the
observers in a *list or vector* and calling observers sequentially for notification.

An average Subject *(Publisher)* implementation will have at least these three methods described below in order to handle
observer registration and notification.

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

<!-- Typically, observer pattern implementations have subscriptions where they are done at runtime. But this should be done
when it is not clear which class will subscribe and when. However, most of the time observer classes are certain in the
development process. Therefore having a runtime dependency for observer registration and having lists isn't necessary
for most observer model implementations. -->

Genelde observer patern kullanılan uygulamalarda, bu modelin implementasyonu çalışma zamanında gerçekleşecek şekilde
yapılmaktadır. `Publisher` sınıfına baktığımız zaman, `IObserver` türünden objelerin kendisine kayıt olması 

<!-- Tipik olarak, gözlemci model uygulamaları, çalışma zamanında yapıldıkları aboneliklere sahiptir. Ancak bu, hangi sınıfın ne zaman abone olacağı belli olmadığında yapılmalıdır. Ancak çoğu zaman gözlemci sınıfları geliştirme sürecinde bilinmektedir. Bu nedenle, gözlemci kaydı için bir çalışma zamanı bağımlılığına sahip olmak ve listelere sahip olmak, çoğu gözlemci modeli uygulaması için gerekli değildir. -->

```cpp
class ObserverX : public IObserver {
 public:
  void OnMessage(const std::string& msg) override {
    std::cout << "ObserverX::OnMessage: " << msg << "\n";
  }
};

class ObserverY : public IObserver {
 public:
  void OnMessage(const std::string& msg) override {
    std::cout << "ObserverY::OnMessage: " << msg << "\n";
  }
};

class ObserverZ : public IObserver {
 public:
  void OnMessage(const std::string& msg) override {
    std::cout << "ObserverZ::OnMessage: " << msg << "\n";
  }
};
```

`ObserverX`, `ObserverY` ve `ObserverZ` sınıflarının 
















---

[wiki-observer]: https://en.wikipedia.org/wiki/Observer_pattern
