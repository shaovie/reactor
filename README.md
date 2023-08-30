# Reactor Event-driven network framework in c++
reactor is a lightweight, concise i/o event demultiplexer implementation in c++11

[新手看这里](https://zhuanlan.zhihu.com/p/653148135)

## Features

* I/O event-driven architecture
* Lightweight and minimalist implementation of the Reactor pattern, allowing flexible combinations of multiple reactors
* Object-oriented implementation for easier encapsulation of business logic.
* Supporting asynchronous sending allows higher-level applications to perform synchronous I/O operations while asynchronously handling business processing
* Lock-free operations in a polling stack, enabling zero-copy data transfer for synchronous I/O.
* Perfect support for REUSEPORT multi-poller mode
* Built-in four-heap timer implementation, enabling lock-free/synchronous handling of I/O and timer events.
* Fully native implementation of acceptor/connector, providing maximum customizability.
* Multiple poller mode, one poller per thread/CPU
* Support interaction between the application layer and the poller, e.g. creating a cache within the poller coroutine, enabling lock-free usage. (like runtime.mcache)
* Few APIs and low learning costs

## Benchmarks


### 新手阅读顺序
1. ev_handler
2. reactor
3. poller -> poller_desc
4. acceptor
5. connector
6. timer_qheap
