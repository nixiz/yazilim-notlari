#include <functional>
#include <iostream>
#include <string>
#include <tuple>
#include <type_traits>

struct IObserver {
  virtual ~IObserver() = default;
  virtual void OnMessage(const std::string&) = 0;
};

class Publisher {
 private:
  IObserver* observer{nullptr};

 public:
  void RegisterObserver(IObserver* observer_) { observer = observer_; }

  void PublishMessage(std::string msg) {
    if (observer) {
      observer->OnMessage(msg);
    }
  }
};

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

template <typename I, typename... Observers>
class MultipleObserverAdapter : public I {
 private:
  static_assert(std::conjunction_v<std::is_base_of<I, Observers>...>,
                "all observer classes should derived from I type");
  std::tuple<std::add_pointer_t<Observers>...> observers;

 public:
  explicit MultipleObserverAdapter(std::add_pointer_t<Observers>... obs) {
    observers = std::make_tuple(obs...);
  }
  ~MultipleObserverAdapter() = default;

  template <typename Func, typename... Args>
  void notify(Func&& func, Args&&... args) noexcept(true) {
    std::apply(
        [&](auto... obs_pointers) {
          ((obs_pointers->*func)(std::forward<Args>(args)...), ...);
        },
        observers);
  }
};

template <typename... Observers>
class MultiObserverBridge
    : public MultipleObserverAdapter<IObserver, Observers...> {
  using Adapter = MultipleObserverAdapter<IObserver, Observers...>;

 public:
  MultiObserverBridge(std::add_pointer_t<Observers>... observers)
      : Adapter(observers...) {}

  ~MultiObserverBridge() {}
  void OnMessage(const std::string& msg) override {
    Adapter::notify(&IObserver::OnMessage, std::cref(msg));
  }
};

using ObserversXYZ = MultiObserverBridge<ObserverX, ObserverY, ObserverZ>;
int main(int argc, char* argv[]) {
  Publisher pub;
  pub.RegisterObserver(
      new ObserversXYZ(new ObserverX, new ObserverY, new ObserverZ));

  pub.PublishMessage("it works!");
  return 0;
}