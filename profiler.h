#pragma once

#include <memory>

class MiniProfiler {
    struct Impl;
    std::unique_ptr<Impl> impl;
public:
    MiniProfiler();
    ~MiniProfiler();
};
