// async-call-helper.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <mutex>
#include <vector>
#include <algorithm>
#include <functional>

struct asyn_call_token
{
	virtual ~asyn_call_token() = default;

	template <typename Cast>
	static inline Cast* from_context(void* context) noexcept
	{
		std::unique_ptr<asyn_call_token> act_handle(reinterpret_cast<asyn_call_token*>(context));
		if (!act_handle) return nullptr;
		auto *cast_ptr = static_cast<Cast*>(act_handle->get_caller());
		return cast_ptr;
	}
protected:
	virtual void* get_caller() = 0;
};

template <typename Caller>
class async_call_helper
{
public:
	using ThisType = async_call_helper<Caller>;

	async_call_helper() 
		: IFaces()... 
	{
		lifetime_ref = std::make_shared<auto_ref_holder>(parent());
	}
	~async_call_helper() = default;

	void* get_context() const noexcept
	{
		struct special_token final
			: public asyn_call_token
		{
			special_token(std::weak_ptr<auto_ref_holder> ref_, std::mutex& guard_) 
				: ref(ref_) 
				, guard(guard_, std::defer_lock) {}
			
			~special_token() 
			{
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
			std::unique_lock<std::mutex> guard;
		};
		return new special_token(weak_ref(), guard);
	}
	template <typename ...Args>
	struct callback_context {
		void *context;
		void (*callback)(Args...);
		void operator()(Args... args) noexcept {
			std::invoke(callback, std::forward<Args>(args)...);
		}
	};

	template <typename ...Args, typename Fn>
	callback_context<void*, Args...> 
	get_context(Fn&& cb) noexcept 
	{
		struct trampoline_t final
			: public asyn_call_token
		{
			trampoline_t(std::weak_ptr<auto_ref_holder> ref_, 
						 std::mutex& guard_,
						 std::function<void(Args...)> callback_) 
				: ref(ref_) 
				, guard(guard_, std::defer_lock)
				, callback(std::move(callback_)) {}
			
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
				if (!trampoline_ptr) return;
  			if (trampoline_ptr->get_caller()) {
					std::invoke(trampoline_ptr->callback, std::forward<Args>(args)...);
				}
			}
		private:
			std::weak_ptr<auto_ref_holder> ref;
			std::unique_lock<std::mutex> guard;
			std::function<void(Args...)> callback;
		};
		std::function<void(Args...)> callback = cb;
		return callback_context<void*, Args...> {
			new trampoline_t(weak_ref(), guard, std::move(callback)),
			&trampoline_t::callback_handle
		};
	}

protected:
	Caller* parent() {
		return static_cast<Caller*>(this);
	}

	const Caller* parent() const {
		return static_cast<const Caller*>(this);
	}

	void set_deleted() noexcept {
		std::lock_guard<std::mutex> lock(guard);
		lifetime_ref.reset();
	}

private:
	friend struct auto_ref_holder;
	struct auto_ref_holder	
		: public std::enable_shared_from_this<auto_ref_holder> {
		explicit auto_ref_holder(Caller* caller_): caller(caller_) {}
		Caller* get_parent() { return caller;	}
		const Caller* get_parent() const { return caller;	}
	private:
		Caller* caller;
	};

	std::weak_ptr<auto_ref_holder> weak_ref() noexcept {
		return lifetime_ref;
	}

	std::weak_ptr<auto_ref_holder> weak_ref() const noexcept {
		return lifetime_ref;
	}

	std::shared_ptr<auto_ref_holder> lifetime_ref;
	mutable std::mutex guard;
};

namespace c {
  typedef void(*long_async_function_cb)(void*, int);

  static void long_async_function(void* context, long_async_function_cb cb, int in_param, int sleep_msec = 150) {
    std::thread([=] {
      std::this_thread::sleep_for(std::chrono::milliseconds(sleep_msec));
      auto out_param = in_param * 2;
      cb(context, out_param);
      }).detach();
  }
}

class safe_service 
  : public async_call_helper<safe_service>
{
public:
  explicit safe_service(int in_param);
  ~safe_service();

  void response(int out_param);
  void execute();
private:
  std::unique_ptr<int> param;
};

safe_service::safe_service(int in_param)
  : param(new int(in_param))
{
  std::cout << "ctor  service \n";
}

safe_service::~safe_service() {
  set_deleted();
  std::cout << "~dtor service \n";
}

static inline void safe_response_cb(void* context, int out_param) {
  auto srv_ptr = asyn_call_token::from_context<safe_service>(context);
  if (srv_ptr) {
    srv_ptr->response(out_param);
  } else {
    std::cerr << "no service instance to make callback call\n";
  }
}

void safe_service::execute() {
  c::long_async_function(get_context(), safe_response_cb, *param);
  
  auto context = get_context<int>([this] (int o) {
   std::cout << "received response from lambda " << *param << ": " << o << "\n";
  });
  c::long_async_function(context.context, context.callback, *param);

  using namespace std::placeholders;
  auto context_bind = get_context<int>(std::bind(&safe_service::response, this, _1));
  c::long_async_function(context_bind.context, context_bind.callback, *param * 3);
}

void safe_service::response(int out_param)
{
  std::cout << "received response for " << *param << ": " << out_param << "\n";
}

using namespace std::chrono_literals;

int main()
{
  {
    safe_service s(5);
    s.execute();
  }
	std::this_thread::sleep_for(150ms);

	return 0;
}