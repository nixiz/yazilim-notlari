<!-- ---
layout: post
title: Secure Asynchronous Callback Handling in C++
subtitle: Securely handle asynchronous callbacks triggering from both C & C++ codes.
thumbnail-img: /assets/img/async_cb_banner.png
share-img: /assets/img/async_cb_banner.png
nav-short: true
comments: true
readtime: true
show-avatar: false
language: tr
tags: [C++, templates, observer, OOP]
--- -->
<!-- buraya gif olsa guzel olur -->
![C++ Road Map](/yazilim-notlari/assets/img/cpp20-timeline.png){: .mx-auto.d-block :}

C ve C++ dillerinin ortak kullanıldığı projelerde, özellikle asenkron çağrıların yapıldığı yerler başta olmak üzere bir
çok noktada nesnelerin ömürlerinin yönetilmesi zor ve sorunlu olabilmektedir. Bunun sebebi, asenkron çağrıların
yapıldığı yerlerde, geribildirimin yapılacağı nesnelerin yaşam sürelerinin otomatik olarak uzatılmıyor olmasıdır.

Örneğin, aşağıdaki koda baktığımız zaman, C kütüphanesindeki bir asenkron çağrıyı yapan `service` objesinin asenkron
çağrı sonuçlanmadan hafızadan silinmesi durumunda, çağrının geribildiriminde kendisinin silinip silinmediğini anlaması
imkansızdır. Burada satır 2'de hafızadan silinen bir nesneye erişim yapılmaya çalışılacağı için uygulama çakılacaktır.

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

### std::enable_shared_from_this<T> Kullanımı

Yukarıdaki örnekte karşılaşılan sorunu çözebilmek için `unsafe_service` sınıfının `std::enable_shared_from_this<unsafe_service>`
yardımcı sınıfından türemesi ve sınıfın [`shared_ptr<unsafe_service>`][shared-ptr-link] türünden yaratılarak, kullanılıyor olması gerekmektedir.
Sadece bunu yapması da yetmeyecektir, bir de asenkron çağrıya direkt olarak kendi referansı yerine, içerisinde `weak_ptr`
referansının tutulduğu başka bir yardımcı nesne göndermesi de gerekmektedir. Bu sayede asenkron çağrının cevabı alındığında,
context argümanı bu yardımcı nesne türüne dönüştürülecek ve içerisindeki `weak_ptr<unsafe_service>` kullanılarak servis
objesinin yaşayıp yaşamadığı kontrol edilebilecektir.

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
**Note:** Yukarıdaki kodun tamamına [buradan][godbolt-1] ulaşabilirsiniz.

Bulmuş olduğumuz çözüm çalışıyor olasa da, öncelikle geliştirilen sınıf üzerinde değişiklikler yapılmasını ve
yöntemi kullanmak isteyen sınıfların yaşam döngülerinin `shared_ptr<T>` türünden yaratılarak yönetilmesini gerektiriyor.
Üzerinde çalıştığımız sınıfları biz geliştirsek bile, kullanım şekillerini değiştirmemiz pek mümkün olmuyor. Örneğin
bir Framework kullanıyorsak, bu framework `service` sınıfını nasıl yaratıyorsa öyle yaratacaktır.

Ayrıca, asenkron çağrılar yapan her sınıf için aynı yöntemi teker teker uyguluyor olmak, hem zor hem de [SOLID][solid-link-wiki]
prensiplerine aykırı olacaktır. Bunun yerine, asenkron çağrıları yapan sınıfların güvenli bir şekilde çağrı cevaplarını
işleyebilecek ve en önemlisi, sistemden silindiği durumlarda uygulamaya zarar vermeyecek, güvenli bir yapının kullanılması
daha doğru olacaktır.

### `async_call_helper` Sınıfının Geliştirilmesi

Yukarıda sorunu tanımladık, çözüm için basit bir geliştirme yaptık. Geliştirdiğimiz örnek üzerinden genel bir
çözüm üretebilmek için gereksinimlerimizi belirleyebiliriz. Geliştireceğimiz asenkron çağrı yardımcısı sınıfımız aşağıdaki özelliklere sahip olmalıdır:

* her şeyden önce kullanılacak sınıfın yaratılma şeklini değiştirmemeli
* aynı şekilde kullanılacak sınıf üzerinde/içerisinde en az değişikliğe sebep olmalı
* kullanılan sınıf sistemden silindiği taktirde, asenkron çağrı cevapları uygulamanın çakılmasına sebep olmamalı
* **bonus:** asenkron çağrı cevapları tercihen [`lambda`][lambda-link] veya [`bind`][bind-link] gibi fonksiyon belirteçleri üzerinden işlenebilmeli

`async_call_helper` sıfımızı, yukarıda belirtilen gereksinimleri karşılayacak şekilde geliştirmeye başlayalım. İlk önce, sınıfın nasıl kullanılacağına karar vermemiz gerekiyor. Kullanılacak sınıflarda daha basit bir arayüz sunması ve servis sınıfları arasındaki ilişkiyi otomatikleştirmesi açısından, `async_call_helper` sınıfımızın kalıtılarak kullanılması, aynı zamanda da servis sınıflar ile kompozisyon ilişkisi içerisinde olması iyi olacaktır. Yani kullanımı aşağı yukarı şu şekilde gözükecektir:

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

Burada dikkat edilmesi gereken yer, `async_call_helper<safe_service>` [CRTP][crtp-link] tekniğinin kullanımı sayesinde, `async_call_helper` sınıfı `safe_service` sınıfının bilgilerine sahip olurken, aynı zamanda `safe_service` sınıfı kalıtım ile asenkron çağrılar için kullanacağı fonksiyonlara sahip olabilmektedir.
Sınıfımızı geliştirmeye devam edelim. Örnek çözümümüzde de kullandığımız [`shared_ptr`][shared-ptr-link] ve [`weak_ptr`][weak-ptr-link] ikilisine burada da ihtiyacımız var. `async_call_helper` sınıfını kullanacak sınıfların nasıl yaratılacağını değiştiremeyeceğimiz için, `async_call_helper` sınıfı içerisinde bir [`shared_ptr`][shared-ptr-link] kullanmalıyız.

```cpp
template <typename Caller>
class async_call_helper
{
public:
  void* get_context() const;
  // ...
private:
  struct auto_ref_holder
    : public std::enable_shared_from_this<auto_ref_holder> {
    // ...
  };
  std::shared_ptr<auto_ref_holder> lifetime_ref;
};
```

Yukarıda yazdığımız örnek koda tekrar bakacak olursak, asenkron çağrıya servis sınıfının kendisi yerine
`async_callback_token` ismini verdiğimiz ara bir nesnenin gönderildiğini göreceğiz. Bu ara sınıf sayesinde, servis objesinin güvenli bir şekilde yaşayıp yaşamadığını kontrol edebiliyor, asenkron çağrının cevabının işlendiği fonksiyonda uygulamayı çakılmalardan koruyabiliyoruz. Aynı sınıfı yeni yazdığımız `async_call_helper` sınıfı içerisinde de kullanıyor olacağız. Bunun için `get_context()` fonksiyonu içerisinde bu sınıfını dönecek, asenkron çağrı cevaplarında güvenli bir şekilde servis sınıfımıza erişebileceğiz. Erişemediğimiz durumlar için istediğimizi yapma imkanına sahip olacağız.

```cpp
template <typename Caller>
class async_call_helper
{
public:
  void* get_context() const;
  // ...
private:
  struct auto_ref_holder
    : public std::enable_shared_from_this<auto_ref_holder> {
    // ...
  };
  std::shared_ptr<auto_ref_holder> lifetime_ref;
};
```

Asenkron çağrılara verilecek `context` objesi, içerisinde `auto_ref_holder` sınıfının bir weak referansını tutacak, çağrı cevabı işleneceği zaman da bu referans üzerinden ilk önce `async_call_helper` sınıfı, onun üzerinden de servis sınıfına erişim sağlanabilecek.

```cpp
struct asyn_call_token
{
  virtual ~asyn_call_token() = default;
  template <typename Cast> static Cast* from_context(void* context) noexcept;
protected:
  virtual void* get_caller() = 0;
};

template <typename Caller>
class async_call_helper
{
public:
  void* get_context() const {
    // ... 
    return new asyn_call_token(/*...*/);
  }
  // ...
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

Asenkron çağrı tarafından `response_cb` fonksiyonu çağrıldığında, artık `safe_service` nesnesine `context` üzerinden güvenli bir şekilde erişebiliyoruz. Bunu yaparken de `safe_service` sınıfımız içerisnde minimum değişiklik yapmamız yetiyor. Tabi şu ana kadar sadece iskelet kodu gerçekleştirdik, fonksiyonların içerisini doldurmak için yazının devamını okuyabilir; veya bana bu kadarı yetti, ben konuyu anladım sadece koda erişsem de olur diyorsanız [buradan](https://github.com/nixiz/async-call-helper) projenin son haline direk erişebilirsiniz.

Gelelim asıl alengirli yerlere. Eminim ki, `asyn_call_token` sınıfının soyut sınıf olarak kullanıldığını çoktan farkettiniz. Bunun sebebi, ileride asenkron çağrıların cevaplarını işlerken birden fazla farklı özellikte çağrı kontekstlerinin olabilme ihtimali ve `async_call_helper` sınıfının lambda ve fonksiyon pointer'ları için farklı bir `get_context()` fonksiyonuna sahip olmasıdır. Yazımda bahsettiğim örnek ile devam edelim ve C stilinde asenkron çağrılarda kullanılmak üzere çağrılacak `get_context()` fonksiyonunu yazalım:

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

`get_context()` fonksiyonu, `asyn_call_token` türünden olan kendine özgü bir sınıf kullanarak, C asenkron çağrılarında kullanılmak üzere bir kontekst objesi yaratarak geri döndürmektedir. Asenkron çağrı sonuçlandığında çalıştırılacak geri bildirim fonksiyonu, `asyn_call_token` sınıfının `get_caller()` fonksiyonu üzerinden asıl servis objesine erişebilecektir. Servis objesi silindikten sonra `get_caller()` metodu çağrıldığında ise, `special_token` metod `nullptr` dönerek sistemin güvenli bir şekilde referans kontrolü yapmasına imkan sağlayacaktır.









[godbolt-1]: https://godbolt.org/z/jfx5E73f4
[shared-ptr-link]: https://en.cppreference.com/w/cpp/memory/shared_ptr
[weak-ptr-link]: https://en.cppreference.com/w/cpp/memory/weak_ptr
[solid-link-wiki]: https://en.wikipedia.org/wiki/SOLID
[lambda-link]: https://en.cppreference.com/w/cpp/language/lambda
[bind-link]: https://en.cppreference.com/w/cpp/utility/functional/bind
[generalization-link]: https://www.uml-diagrams.org/generalization.html
[composition-link]: https://www.uml-diagrams.org/composition.html
[crtp-link]: https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern

gereksinimlerimi belirleyerek, 

Yukarıda, sorun olarak tanımladığımız ve 
