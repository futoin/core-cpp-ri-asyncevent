
### FutoIn EventEmitter for C++

This is reference implementation for unified `futoin::IEventEmitter` API.

See [**FTN15: Native Event API**](https://specs.futoin.org/final/preview/ftn15_native_event.html) for more details.

### Mirrors

This FutoIn sub-project:

- [Codeberg: FutoIn EventEmitter for C++](https://codeberg.org/futoin/core-cpp-ri-asyncevent)
- [GitHub: FutoIn EventEmitter for C++](https://github.com/futoin/core-cpp-ri-asyncevent)
- [GitLab: FutoIn EventEmitter for C++](https://gitlab.com/futoin/core/cpp/ri-asyncevent)

All-in-one FutoIn Core C++:

- [Codeberg: FutoIn Core C++](https://codeberg.org/futoin/core-cpp-ri)
- [GitHub: FutoIn Core C++](https://github.com/futoin/core-cpp-ri)
- [GitLab: FutoIn Core C++](https://gitlab.com/futoin/core/cpp/ri)


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

