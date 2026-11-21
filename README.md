# TinyNet

一个基于 **C++20 协程** 的小型网络库，学习项目，设计上参考了 [Coroio](https://github.com/jet47/coroio)。核心思路是把 epoll 事件循环和协程结合起来：socket 的读写、接受连接、定时器都可以直接 `co_await`，用同步式的写法写异步网络代码。

## 核心设计

事件循环是单线程的：`TLoop` 每轮调用 poller 等待事件，再唤醒就绪事件对应的协程句柄；协程在 `co_await` 一个未就绪的操作时挂起，把句柄登记给 poller，事件到达后被 resume。

主要模块（均在 `NNet` 命名空间下）：

- **事件循环**：`TLoop<TPoller>`（`loop.h`）——驱动 `Poll()` + `WakeupReadyHandles()` 的主循环。
- **Poller 基类**：`TPollerBase`（`poller.h`）——维护待注册的事件变更列表、用优先队列管理定时器，并提供可 `co_await` 的 `Sleep()` / `Yield()`。
- **epoll 后端**：`TEpoll`（`epoll.h/.cpp`）——目前唯一的后端，封装 Linux epoll 的事件注册与分发。
- **Socket**：`TSocket`（`socket.h/.cpp`）——非阻塞 socket 封装，`ReadSome` / `WriteSome` / `Accept` / `Connect`（支持超时）/ `Monitor`（对端关闭监测）都返回 awaitable 对象；先在 `await_ready` 里尝试一次非阻塞调用，返回 EAGAIN 时才挂起等事件。
- **地址**：`TAddress`（`address.h/.cpp`）——用 `std::variant<sockaddr_in, sockaddr_in6>` 统一表示 IPv4 / IPv6 地址。
- **协程任务类型**：
  - `TVoidTask`（`promises.h`）——即发即弃（fire-and-forget）的协程，启动后不阻塞调用方，echotest 里的 server / client / handler 都用它；
  - `TValueTask<T>`（`corochain.h`，别名 `TFuture<T>`）——带返回值的协程，子协程完成时通过 final awaiter 对称转移回调用方，并提供 `Apply` / `Accept` / `Ignore` 链式组合以及 `All` / `Any` 组合子。
- **读写工具**（`sockutils.h/.cpp`）：`TByteReader` / `TByteWriter`（读满 / 写满指定字节数）、`TStructReader`（按结构体读）、`TLineReader` + `TZeroCopyLineSplitter`（零拷贝按行读取）。

## 目录结构

```
TinyNet/
├── CMakeLists.txt        # 顶层构建（C++20）
├── src/                  # 库源码（静态库 tinynet）
│   ├── base.h            # 时间、定时器、事件等基础类型
│   ├── promises.h        # TVoidTask 等基础 promise 类型
│   ├── corochain.h       # TValueTask / TFuture 与 All / Any 组合子
│   ├── poller.h          # TPollerBase：事件登记与定时器管理
│   ├── epoll.h/.cpp      # epoll 后端
│   ├── loop.h            # TLoop 事件循环
│   ├── socket.h/.cpp     # TSocket 协程化封装
│   ├── address.h/.cpp    # TAddress 地址封装
│   ├── sockutils.h/.cpp  # 定长读写、按行读取等工具
│   └── all.h             # 一次性包含主要头文件
└── examples/
    └── echotest.cpp      # echo 服务器 + 客户端示例
```

## 构建

依赖：CMake ≥ 3.15、C++20 编译器、Linux（epoll）。已在 g++ 13.3 + CMake 3.28 上验证。

```shell
cmake -B _build -DCMAKE_EXPORT_COMPILE_COMMANDS=1
cmake --build _build
```

构建产物：`_build/src/libtinynet.a`、`_build/examples/echotest`。

## 运行示例

echotest 在**同一个进程**里同时跑一个 echo 服务器和若干个客户端：服务器监听 `127.0.0.1:8888`，每个客户端连上后每隔 1 秒发送一条 `Hello 序号/客户端编号` 消息，服务器原样回显。

```shell
./_build/examples/echotest 2   # 参数是客户端数量，缺省为 1
```

程序会一直运行（客户端循环发送），用 Ctrl+C 退出。已实测服务器能正确回显多个客户端的消息。

## 局限与 TODO

- [ ] **仅支持 Linux**：`TLoop` 模板上预留了可替换的 poller，但目前只有 epoll 一个后端，没有 select / poll / kqueue 实现。
- [ ] **Bazel 构建不可用**：仓库里只有 `.bazelrc`，没有 BUILD / WORKSPACE 文件，旧版 README 中的 Bazel 说明无法实际构建；目前请使用 CMake。
- [ ] **调试输出较多**：poller / socket 内部有不少 `std::cout` 调试打印，未做日志开关，不适合直接用于生产环境。
- [ ] **异常会被吞掉**：`TVoidPromise::unhandled_exception()` 是空实现，`TVoidTask` 协程内未捕获的异常不会向外传播，业务代码需要自己在协程里 try/catch（echotest 就是这样做的）。
- [ ] **没有单元测试**；`corochain.h` 中的 `Any()` 组合子是初步实现，未经充分验证。
