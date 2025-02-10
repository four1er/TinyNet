# TinyNet
a tiny net library bases on C++20 coroutine, learning From Coroio.

# Build
## Cmake
```shell
cmake -B _build -DCMAKE_EXPORT_COMPILE_COMMANDS=1
cmake --build _build --target all
```

## Bazel config

### use Bazelisk

On macOS: brew install bazelisk.

On Windows: choco install bazelisk.

On Linux: You can download Bazelisk binary on our Releases page and add it to your PATH manually, which also works on macOS and Windows.

Bazelisk is also published to npm. Frontend developers may want to install it with npm install -g @bazel/bazelisk.
