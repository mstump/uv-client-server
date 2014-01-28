## Description

[The Code](src/main.cpp) demonstratates several ideas.

 1. Communication between caller code and an event loop through a lock free queue. This eliminates the need for intrusive fine-grain locking.
 1. The implementation of futures/promises through by way of a condition variable.
 1. Exposing a library implemented in C++ through a clean C interface. This eliminates most ABI issues, and makes the library easy to embed in other languages.
 1. Cool nerd points for lock free implementations of MPSC and SPSC queues.

## Dependencies

* libuv
* C++11

## Building

```
cmake -DCMAKE_BUILD_TYPE=Debug && make && ./build/bin/libuv-test
```
