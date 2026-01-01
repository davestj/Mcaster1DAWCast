// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "worker_pool.h"

namespace dawcast::platform {

WorkerPool::WorkerPool(unsigned int threadCount)
{
    unsigned int count = (threadCount > 0) ? threadCount : 1;
    m_threads.reserve(count);
    for (unsigned int i = 0; i < count; ++i) {
        m_threads.emplace_back(&WorkerPool::workerLoop, this);
    }
}

WorkerPool::~WorkerPool()
{
    shutdown();
}

void WorkerPool::submit(std::function<void()> task)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stopped) return;
        m_tasks.push(std::move(task));
    }
    m_cv.notify_one();
}

void WorkerPool::shutdown()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stopped) return;
        m_stopped = true;
    }
    m_cv.notify_all();

    for (auto& t : m_threads) {
        if (t.joinable()) {
            t.join();
        }
    }
}

unsigned int WorkerPool::threadCount() const
{
    return static_cast<unsigned int>(m_threads.size());
}

void WorkerPool::workerLoop()
{
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this]() { return m_stopped || !m_tasks.empty(); });

            if (m_stopped && m_tasks.empty()) return;

            task = std::move(m_tasks.front());
            m_tasks.pop();
        }
        task();
    }
}

} // namespace dawcast::platform
