# miniprofiler

This is supplementary code from 2020.12.19 lecture about hand-made profiler.

The code contains Windows-only minimalistic embedded sampling profiler.
It only supports profiling one main thread (toy project, you know), and lives in the same process as the profiled thread (i.e. "embedded").
In order to use it on a program, create object of `MiniProfiler` class at the very start of `main` function.
It should look like this:

```cpp
#include "profiler.h"
int main(int argc, char **argv) {
    MiniProfiler profiler;      //MiniProfiler: starts profiler thread

    ... //profiled code

    return 0;                   //~MiniProfiler: ends profiler thread
}
```

Since it heavily relies on WinAPI, it works only under Windows.
Moreover, due to `StackWalk64` clumsiness currently it works only for 64-bit programs.
It is quite likely that Visual C++ is required too: not sure if it will build e.g. under MinGW.


The main idea is that profiler thread does `Sleep(1)` (wait for ~1 ms) in infinite loop,
temporarily suspending the main thread and getting its stack trace using `StackWalk64`.
The stack traces are stored in one big array, which is later processed in `~MiniProfiler`, when main thread is going to finish,
To decode function names from addresses, `SymFromAddr` is used.
The decoded call stacks are inserted into trie data structure, and the heavy subset of this trie is printed to console as report.


Typical report looks like this (maybe I should have added dots instead of spaces):

    100.0%  : RtlUserThreadStart
      100.0%  : BaseThreadInitThunk
        100.0%  : __scrt_common_main_seh
          100.0%  : main
            99.9%  : PackTemplateLine<9>::operator()
              99.9%  : PackFile<8>
                94.9%  : Pack<8>
                  81.1%  : JPEGCoder<8>::SetHeader
                    72.7%  : DCT<double,8>
                      14.3%  : cos
                         3.6%  : powf
                     3.1%  : floor
                   4.9%  : Downscale
                   3.2%  : BitmapToCoder<8>
                4.7%  : LoadBMP
