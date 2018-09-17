
### FutoIn EventEmitter for C++

This is reference implementation for unified `futoin::IEventEmitter` API.

See [**FTN15: Native Event API**](https://specs.futoin.org/final/preview/ftn15_native_event.html) for more details.

#### Usage

Please check [FutoIn C++ API][https://github.com/futoin/core-cpp-api]
for example of usage.

```cpp
#include <futoin/iasynctool.hpp>
#include <futoin/ri/eventemitter.hpp>

class MyExample : public futoin::ri::EventEmitter {
private:
    MyExample(futoin::IAsyncTool& async_tool) :
        EventEmitter(async_tool)
    {}
};
```

