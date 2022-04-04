---
layout: post
title: C++ 20 ile Gelen Yenilikler - Part III
subtitle: C++ 20 Coroutines
thumbnail-img: /assets/img/cpp20-timeline.png
share-img: /assets/img/cpp20-timeline.png
nav-short: true
comments: true
readtime: true
show-avatar: false
language: tr
tags: [C++, C++20, yazılım, coroutines]
---
![C++ Road Map](/yazilim-notlari/assets/img/cpp20-timeline.png){: .mx-auto.d-block :}

C++20 standardıyla birlikte gelen dört büyük yenilikten bir tanesi de Coroutines özelliklerinin gelmesidir. Modern C++
ile gelen multithreading desteğiyle birlikte asenkron hesaplamalar farklı thread'ler üzerinden yapılabilirken, asenkron
çağrının sonucunun alınması için iki farklı yöntem uygulanmaktadır.

* Bu yöntemlerden en çok kullanılanı ve bilineni: Asenkron çağrının sonucunu almak için bir geri dönüş fonksiyonu
 *Callback Function* tanımlanarak asenkron işlemin sonucu bu callback fonksiyon üzerinden kullanılır.
 Bu yöntemde dikkat  edilmesi gereken nokta, asenkron işlemin çağırıldığı yerde beklenmeden devam edileceği için:
 Asenkron çağrı tamamlanmadan ana rutinden çıkılır ve çağrıyı işleten nesneler silinmiş olursa, callback fonksiyonu
 işletildiğinde tanımsız bir davranış oluşacaktır ve program hata vererek sonlanacaktır.

  ```cpp  
  struct connection_t {};

  void when_connected(connection_t conn) 
  { 
    cout << "connection established : " << conn << "\n"; 
  }

  struct socket_t
  {
    // ...

    template <typename CB>
    void async_connect(CB&& cb_function)
    {
      std::this_thread::sleep_for(500ms);
      std::invoke(cb_function, connection_t{ params });
    }
  };

  int main()
  {
    // ...
    io_context io_service;
    socket_t socket{io_service};
    socket.async_connect(when_connected);

    // starts io service and waits until all jobs are done.
    io_service.run();
  }
  ```
  
  > Yukarıdaki örnek [Boost Asio][boost-asio-async-echo-example] servisini baz alarak verilmiştir.
  <!-- Bu yaklaşımda ise `main` fonksiyonunda yapılan asenkron çağrının sonucu ayrı bir fonksiyon üzerinden işletileceği
  için, çağrı sonlanmadan programın sonlanmaması için bir aracı servisin kullanılması gerekmektedir. Aksi takdirde
  program sonlandıktan sonra asenkron çağrının geri dönüş fonksiyonu işletilmeye çalışılacaktır ve en iyi ihtimalle
  programın hata vermesiyle sonuçlanacaktır. -->

* Bir diğer yöntem ise, asenkron çağrıyı yapan fonksiyon, sonucun beklenmesi için `future` nesnesi geriye dönürür ve
  çağrıyı yapan taraf sonucu kullanmak istediği yerde bekleyecek şekilde rutinini devam ettirebilir.

  ```cpp  
  struct connection_t {};

  template <typename Fn, typename ...Args>
  auto call_async(Fn&& func, Args&& ...args)
    -> future<invoke_result_t<Fn, Args...>>
  {
    using return_t = invoke_result_t<Fn, Args...>;
    promise<return_t> promise;
    auto ret = promise.get_future();

    thread([promise = move(promise), 
           func     = forward<Fn>(func), 
           ...args  = forward<Args>(args)]() mutable
    {
      auto res = invoke(func, forward<Args>(args)...);
      promise.set_value(res);
    }).detach();
    return ret;
  }

  std::future<connection_t> async_connect()
  {
    // ... initialize connection params.
    return call_async([] (connection_params_t params) {
     std::this_thread::sleep_for(500ms);
     // create connection
     return connection_t{ params }; 
    }, params);
  }

  int main()
  {
    // asenkron çağrı yapılır
    auto conn_future = async_connect();

    //sonuç beklenmeden rutin devam ettirilir
    for (int i= 0; i < 100; i++) {
      cout << "do other jobs\n";
    }

    // asenkron çağrının tamamlanması beklenir.
    conn_future.wait();
    // çağrının sonucu alınır ve conn kurulmuş mu kontrol edilir
    connection_t conn = conn_future.get();
    // ... connection objesi kullanılır 
  }
  ```

  Yukarıdaki koda baktığımızda `connect` fonksiyonu içerisinde asenkron bir çağrı yapıyor ve işlemin sonucunu beklemeden
  fonksiyondan çıkarak döngüsünü sonlandırıyor. Bu noktada `connect` fonksiyonunu çağıran üst fonksiyon asenkron
  çağrının sonucunu beklemeli ve sonuç alındıktan sonra kaldığı yerden devam etmelidir.

****

Modern C++ ile gelen multithreading desteğiyle birlikte asenkron hesaplama yeteneği, threadler aracılığıyla eşzamanlı
çalışarak, çalıştığı sistemin sahip olduğu işlem gücünü tümüyle kullanmamıza imkan sağlmaktadır. Ancak bir thread
içerisinde eşzamanlı olarak işlem yapabilme imkanı C++20 standardıyla gelen coroutines güncellemesine kadar mümkün
değildi.

![routine subroutie](/yazilim-notlari/assets/img/func-routine.png)
<div style="text-align:center; font-size: smaller; margin: -13px 0 10px 0;">Image Description</div>

C++ gibi prosedürel *([imperative][wiki-imperative])* diller tarafından yazılan kodların çalıştırılmasındaki girdi
sonuç ilişkisi, fonksiyonların (prosedürlerin) yazıldıkları düzen ile çalıştırılmasıyla sağlanıyor. Yazılan bir
fonksiyon işletilirken, onu çağıran üst fonksiyon(lar) ilgili çağrının bitmesini bekler ve sonucunu aldıktan sonra
kendi rutinlerine devam ederler.

---

<table>
<tr>
  <th>Kod</th>
  <th>Assembly Çıktısı</th>
</tr>
<tr>
<td>
  <pre lang="cpp">
  int get_zero() {
    return 0;                      // (2)
  }
  int main(int argc, char *argv[]) {
    return get_zero();             // (1)
  }
  </pre>
</td>
<td>
  <pre land="asm">
  int get_zero(void) PROC
    xor  eax, eax
    ret  0                          ; (2)
  int get_zero(void) ENDP
  </pre>
  <pre land="asm">
  main  PROC  
    mov   QWORD PTR [rsp + 16], rdx
    mov   DWORD PTR [rsp + 8], ecx
    sub   rsp, 40
    call  int get_zero(void)        ; (1)
    add   rsp, 40
    ret   0
  main  ENDP  
  </pre>
</td>
</tr>
</table>

Yukarıdaki örnekte main içerisinde çağrılan `get_zero()` fonksiyonunun işleyişine baktığımızda, main fonksiyonunun
çağırdığı alt rutin **(1)** bitene kadar durduğunu görmekteyiz; `get_zero()` fonksiyonu sonlandıktan **(2)** sonra
main fonksiyonu kaldığı yerden devam etmekte ve kendi döngüsünü bitirerek programı sonlandırmaktadır. Aynı thread
içerisinde çalıştırılan döngülerin senkron olarak ilerlemesi ve bir döngünün bağlı olduğu diğer döngüyü beklemek
zorunda olmasından dolayı, tek thread içerisinde IO işlemleri gibi döngüleri kilitleyen işlemlerin farklı threadler
üzerinden yapılarak ana döngülerin donmadan işlemlerine devam ettirilmesi sağlanmaktadır.

## C++ Coroutines

[**Co-operative Multitasking**][wiki-co-multitasking] anlamına gelen Coroutine tanımı temelinde bir fonksiyonun
normalde sahip olduğu giriş ve çıkış state'lerini genişleterek, fonksiyona duraklatma ve duraklatıldığı yerden devam
etme imkanı kazandırılmasıdır. C++20 standardıyla birlikte gelen coroutine operatörleri sayesinde fonksiyonların bu
özelliğe erişmesi sağlanmaktadır.

| A coroutine is a generalisation of a function that allows the function to be suspended and then later resumed. [^1]

![routine subroutie](/yazilim-notlari/assets/img/func-coroutine.png)
<div style="text-align:center; font-size: smaller; margin: -13px 0 10px 0;">Image Description</div>

C++ içerisinde tanımlanan `co_yield`, `co_await` ve `co_return` operatörleri kullanılarak bir fonksiyonun coroutine
olarak yapılandırılması sağlanmaktadır. Coroutine operatörleri vasıtasıyla fonksiyon içinde bulunduğu döngüyü bozmadan
durma ve devam etme yeteneği kazanarak, asenkron olarak yapılan işlemlerin sonuçlarının alınması gibi işlemleri aynı
yerde, ancak normal döngülere nazaran donmadan yapabilmektedir. C++ coroutine operatörleri:

* `co_await` : yapılan bir asenkron çağrının sonlanana kadar fonksiyonun duraklatılmasını sağlayan operatördür.
`co_await` operatörünün kullanıldığı yerde fonksiyon duraklatılarak durdurulur; hazır olunduğunda coroutine üzerinden
fonksiyon kaldığı noktadan devam ettirilir.

  ```cpp
  1. task<> tcp_echo_socket(socket_t& socket)
  2. {
  3.   char packet[1024];
  4.   for (;;)
  5.   {
  6.     auto read_bytes = co_await socket.async_read_some(buffer(packet));
  7.     co_await async_write(socket, buffer(packet, read_bytes));
  8.   }
  9. }
  ```

  Coroutine olarak kullanılan fonksiyonların dönüş tipi, fonksiyonun durdurma ve devam ettirilme operasyonlarını
  gerçekleştirebilmesi için coroutine kurallarını sağlayan **awaitable** bir sınıf olması gerekmektedir. Fonksiyonun
  gerçekte döneceği tip bu sınıf içerisinde belirtilerek girdi çıktı ilişkisi korunmuş olur.

* `co_yield` : `co_await` operatörü coroutine fonksiyonların asenkron çağrımı için kullanılırken, `co_yield` fonksiyonun
kendisini duraklatması için kullanılır.

  ```cpp
  generator<int> iota(int n = 0)
  {
    while (true)
      co_yield n++;
  }
  ```

  Aynı şekilde `co_yield` kullanılan fonksiyonların da dönüş tiplerinin **awaitable** bir sınıf olması gerekmektedir.
  Örnekte gösterilen `iota` fonksiyonu sonsuz bir döngüye sahip olmasına rağmen, coroutine bir yapıda olduğu için
  bulunduğu döngüyü kilitlemez.

  ```cpp
  int main()
  {
    int times = -1;
    cout << "please enter how many integers do you want. ";
    cin >> times;

    for (auto i : iota()) {
      cout << "new number: " << i << "\n";
      if (i == times) break;
    }
  }
  ```

  Yukarıdaki örnekte gördüğünüz gibi `main` fonksiyonu içerisinde sonsuz döngüye sahip olan `iota` fonksiyonunu çağırmış
  olmasına rağmen konsoldan girilen sayı kadar çalıştırılarak program sorunsuz bir şekilde sonlandırılır.

* `co_return` : coroutine bir fonksiyonun sonlandırılması için kullanılır.

  ```cpp
  generator<int> iota(int times)
  {
    while (times-- > 0)
      co_yield n++;
    co_return;
  }
  ```

Coroutine operatörleri ile fonksiyonların duraklatılması ve kaldığı yerden devam ettirilmesi işlemleri **awaitable**
olarak adlandırılan sınıflar üzerinden gerçekleşmektedir. Derleyiciler bu sınıfları kullanarak fonksiyonları bir noktada
durdurma veya devam ettirme işlemlerini gerçekleştirirler; bunu yapabilmek için coroutine fonksiyonların içerisine bazı
kodların eklenmesi gerekmektedir.

<table>
<tr>
  <th>Kod</th>
  <th>Derleyicinin Gördüğü Kod</th>
</tr>
<tr>
<td>
  <pre lang="cpp">
  using namespace std::experimental;
  using awaitable = suspend_always;

  auto coroutine_func(awaitable& aw) -> return_ignore
  {
    co_await aw;
  }
  .
  .
  .
  .
  .
  .
  .
  .
  </pre>
</td>
<td>
  <pre land="cpp">
  using namespace std::experimental;
  using awaitable = suspend_always;
  
  auto coroutine_func(awaitable& aw) -> return_ignore
  {
    using promise_type = return_ignore::promise_type;
    promise_type *p;
    if (aw.await_ready() == false) {
      auto coro_handle = coroutine_handle<promise_type>::from_promise(*p);
      aw.await_suspend(coro_handle);
      // ... return ...
    }
  __suspend_point_n:
    aw.await_resume();
  }
  </pre>
</td>
</tr>
</table>



Bir fonksiyonun coroutine olarak kullanılması, fonksiyonda kullanılan coroutine operatörlerinin derleyiciler tarafından
standartta tanımlanan kod parçalarının fonksiyona eklenmesiyle sağlanmaktadır. Duraklatma ve devam ettirme işlemleri
için oluşturulan sınıflar ve özellikleri aşağıdaki kuralları sağlamak zorundadır:

Bir fonksiyon coroutine olarak kullanılmak istendiğinde, fonksiyonun duraklatma / devam ettirme özelliklerine sahip
olması için derleyiciler tarafından fonksiyonun kontrol edilmesi gerekmektedir. Derleyiciler, standart tarafından
belirlenen kurallar çerçevesinde fonksiyon içerisine coroutine operatörlerinin kullanıldığı yerlere belirli kodlar
eklerler. Bu sayede fonksiyon belirlenen noktalarda kendi state'ini kaybetmeden durdurulur ve devam ettirilebilir.  

Coroutine fonksiyonların durdurulduklarında, sonradan devam ettirilene kadar döngülerini sonlandırmaları gerekmektedir;
döngünün sonlandırıldığı noktada geriye döndürülen sınıf, fonksiyonun en son nerede kaldığını hatırlayan ve sonrasında
kaldığı yerden devam etmesini sağlayan bir yapıda olması için bu sınıfların sahip olması gereken özellikler:

* fonksiyonun duraklatılması ve devam ettirilmesi için kullanılan **awaitable** olarak adlandırılan, derleyiciler
tarafından kullanılan sınıfları

* `promise_type` tipinde  barından bir dönüş tutucusuna sahip olmalıdır. Coroutine'ler bu
  obje üzerinden dönüş değerini veya oluşan hataları bildirirler

* 




Execution
Each coroutine is associated with

* the promise object, manipulated from inside the coroutine. The coroutine submits its result or exceptionthrough this object.

* the coroutine handle, manipulated from outside the coroutine. This is a non-owning handle used to resumeexecution of the coroutine or to destroy the coroutine frame.

* the coroutine state, which is an internal, heap-allocated (unless the allocation is optimized out), object thatcontains
  * the promise object
  * the parameters (all copied by value)
  * some representation of the current suspension point, so that resume knows where to continue anddestroy knows what local variables were in scope
  * local variables and temporaries whose lifetime spans the current suspension point



[wiki-imperative]: wikipedia.com/prg_lng
[wiki-co-multitasking]: wikipedia.com/prg_lng
[boost-asio-async-echo-example]: https://www.boost.org/doc/libs/1_75_0/doc/html/boost_asio/example/cpp11/echo/async_tcp_echo_server.cpp

[^1]: Definition of Coroutines: Lewis Baker - https://lewissbaker.github.io/

---

### Kaynaklar

1. Definition of Coroutines: Lewis Baker - https://lewissbaker.github.io/
2. Exploring MSVC Coroutine - luncliff.github.io
3. Exploring the C++ Coroutine - Approach, Compiler, and Issues
4. Coroutines (C++20) - en.cppreference.com/asdasd


