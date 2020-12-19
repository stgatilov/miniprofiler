#include "profiler.h"
#include <thread>
#include <atomic>
#include <vector>
#include <map>
#include <set>
#include <algorithm>

#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbgHelp.lib")

//.h --- for compiler
//.lib/.a --- for linker


struct MiniProfiler::Impl {
    struct StkTrace {
        DWORD64 cnt;
        DWORD64 arr[63];
    };

    //main thread
    std::unique_ptr<std::thread> profThread;
    //shared
    std::atomic_bool shouldExit = false;
    //profiling thread
    std::vector<StkTrace> traces;

    void ProfileFunc(DWORD mainThreadId) {
        HANDLE mainThread = OpenThread(THREAD_ALL_ACCESS, FALSE, mainThreadId);

        while (!shouldExit.load()) {
            //wait 1 ms
            Sleep(1);

            StkTrace trace = {0};
            SuspendThread(mainThread);
            {
                //CaptureStackBackTrace -- this thread  (see e.g. Heapy)

                CONTEXT context;
                context.ContextFlags = CONTEXT_ALL;
                GetThreadContext(mainThread, &context);

                STACKFRAME64 stackFrame = {0};
                stackFrame.AddrPC.Mode = AddrModeFlat;
                stackFrame.AddrStack.Mode = AddrModeFlat;
                stackFrame.AddrFrame.Mode = AddrModeFlat;
                stackFrame.AddrPC.Offset = context.Rip;
                stackFrame.AddrStack.Offset = context.Rsp;
                stackFrame.AddrFrame.Offset = context.Rbp;

                for (int frame = 0; frame < 63; frame++) {
                    BOOL ok = StackWalk64(
                        IMAGE_FILE_MACHINE_AMD64,
                        GetCurrentProcess(),
                        mainThread,
                        &stackFrame,
                        &context,
                        NULL,
                        SymFunctionTableAccess64,
                        SymGetModuleBase64,
                        NULL
                    );
                    if (!ok)
                        break;

                    trace.arr[trace.cnt++] = stackFrame.AddrPC.Offset;
                }
            }
            ResumeThread(mainThread);

            traces.push_back(trace);
        }
    }

    Impl() {
        SymInitialize(GetCurrentProcess(), NULL, TRUE);

        DWORD threadId = GetCurrentThreadId();
        profThread.reset(new std::thread([this,threadId]() {
            ProfileFunc(threadId);
        }));
        //*profThread = std::thread(...);
    }

    ~Impl() {
        shouldExit.store(true);
        profThread->join();
        profThread.reset();

        using namespace std;

        vector<map<string, int>> trie(1);
        vector<int> samples(1);

        for (auto trace : traces) {
            vector<string> path;
            for (int i = trace.cnt - 1; i >= 0; i--) {
                union {
                    SYMBOL_INFO symbol;
                    char trash[sizeof(SYMBOL_INFO) + 1024];
                };
                symbol.SizeOfStruct = sizeof(SYMBOL_INFO);
                symbol.MaxNameLen = 1024;
                DWORD64 displ;
                BOOL ok = SymFromAddr(GetCurrentProcess(), trace.arr[i], &displ, &symbol);
                path.push_back(symbol.Name);
            }
            int v = 0;
            samples[v]++;
            for (string s : path) {
                if (trie[v].count(s) == 0) {
                    trie[v][s] = trie.size();
                    trie.push_back({});
                    samples.push_back(0);
                }
                v = trie[v][s];
                samples[v]++;
            }
        }

        function<void(int, int)> Traverse = [&](int u, int indent) {
            vector<pair<string, int>> sons(trie[u].begin(), trie[u].end());
            sort(sons.begin(), sons.end(), [&](auto a, auto b) {
                return samples[a.second] > samples[b.second];
            });
            for (auto pKV : sons) {
                int v = pKV.second;
                double frac = (samples[v] + 0.0) / traces.size();
                if (frac < 0.03)
                    continue;

                for (int i = 0; i < indent; i++)
                    printf("  ");
                printf("%4.1lf%%  : %s\n", frac * 100.0, pKV.first.c_str());

                Traverse(v, indent + 1);
            }
        };
        Traverse(0, 0);
    }
};

MiniProfiler::MiniProfiler() {
    impl.reset(new Impl());
}
MiniProfiler::~MiniProfiler() {
    impl.reset();
}
