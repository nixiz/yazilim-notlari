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

Yukarıdaki örnekte karşılaşılan sorunu çözebilmek için `unsafe_service` sınıfının `std::enable_shared_from_this<unsafe_service>` yardımcı sınıfından türemesi ve sınıfın `shared_ptr<unsafe_service>` türünden yaratılarak, kullanılıyor olması gerekmektedir. Sadece bunu yapması da yetmeyecektir, bir de asenkron çağrıya direkt olarak kendi referansı yerine, içerisinde `weak_ptr` referansının tutulduğu başka bir yardımcı nesne göndermesi de gerekmektedir. Bu sayede asenkron çağrının cevabı alındığında, context argümanı bu yardımcı nesne türüne dönüştürülecek ve içerisindeki `weak_ptr<unsafe_service>` kullanılarak servis objesinin yaşayıp yaşamadığı kontrol edilebilecektir.

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

Peki üzerinde çalıştığımız servis sınıfının nasıl yaratılacağını değiştiremediğimiz durumlarda ne yapmalıyız. Ayrıca sadece bir sınıf için değil,
asenkron çağrı yapan diğer bütün sınıflarımız için de bu değişiklikleri yapmamız gerekiyor. Bunun yerine 


`enable_shared_from_this<unsafe_service>` sınıfı, `shared_ptr<unsafe_service>` olarak yaratılan servis objesinin bir [*zayıf kopyasını*][weak-ptr-link] tutar ve 

`enable_shared_from_this<T>` yardımcı sınıfı, çok basitçe kendi içerisinde `T` sınıfından bir `weak_ptr` nesnesi tutar ve 


[godbolt-1]: https://godbolt.org/z/jfx5E73f4
[weak-ptr-link]: https://en.cppreference.com/w/cpp/memory/weak_ptr