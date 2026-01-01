// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <functional>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace dawcast::platform {

class WorkerPool {
public:
    explicit WorkerPool(unsigned int threadCount = std::thread::hardware_concurrency());
    ~WorkerPool();

    WorkerPool(const WorkerPool&) = delete;
    WorkerPool& operator=(const WorkerPool&) = delete;

    void submit(std::function<void()> task);
    void shutdown();
    unsigned int threadCount() const;

private:
    void workerLoop();

    std::vector<std::thread>            m_threads;
    std::queue<std::function<void()>>   m_tasks;
    std::mutex                          m_mutex;
    std::condition_variable             m_cv;
    std::atomic<bool>                   m_stopped{false};
};

} // namespace dawcast::platform
