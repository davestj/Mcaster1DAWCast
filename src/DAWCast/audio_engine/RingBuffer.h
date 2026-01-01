// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <cstddef>
#include <cstring>
#include <memory>
#include <new>
#include <type_traits>

namespace dawcast {

/// Lock-free single-producer single-consumer (SPSC) ring buffer.
/// Cache-line aligned atomics to avoid false sharing.
template <typename T>
class RingBuffer
{
    static_assert(std::is_trivially_copyable_v<T>,
                  "RingBuffer requires a trivially copyable type");

public:
    explicit RingBuffer(size_t capacity)
        : m_capacity(capacity + 1)  // One extra slot to distinguish full vs empty
        , m_buffer(std::make_unique<T[]>(m_capacity))
    {
    }

    ~RingBuffer() = default;

    // Non-copyable, non-movable
    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;
    RingBuffer(RingBuffer&&) = delete;
    RingBuffer& operator=(RingBuffer&&) = delete;

    /// Write up to count items. Returns number actually written.
    size_t write(const T* data, size_t count)
    {
        const size_t avail = availableWrite();
        if (count > avail) count = avail;
        if (count == 0) return 0;

        const size_t writePos = m_writePos.load(std::memory_order_relaxed);

        // Handle wrap-around with two copies
        const size_t firstChunk = std::min(count, m_capacity - writePos);
        std::memcpy(m_buffer.get() + writePos, data, firstChunk * sizeof(T));
        if (count > firstChunk) {
            std::memcpy(m_buffer.get(), data + firstChunk, (count - firstChunk) * sizeof(T));
        }

        m_writePos.store((writePos + count) % m_capacity, std::memory_order_release);
        return count;
    }

    /// Read up to count items. Returns number actually read.
    size_t read(T* data, size_t count)
    {
        const size_t avail = availableRead();
        if (count > avail) count = avail;
        if (count == 0) return 0;

        const size_t readPos = m_readPos.load(std::memory_order_relaxed);

        const size_t firstChunk = std::min(count, m_capacity - readPos);
        std::memcpy(data, m_buffer.get() + readPos, firstChunk * sizeof(T));
        if (count > firstChunk) {
            std::memcpy(data + firstChunk, m_buffer.get(), (count - firstChunk) * sizeof(T));
        }

        m_readPos.store((readPos + count) % m_capacity, std::memory_order_release);
        return count;
    }

    [[nodiscard]] size_t availableRead() const
    {
        const size_t w = m_writePos.load(std::memory_order_acquire);
        const size_t r = m_readPos.load(std::memory_order_relaxed);
        return (w - r + m_capacity) % m_capacity;
    }

    [[nodiscard]] size_t availableWrite() const
    {
        return m_capacity - 1 - availableRead();
    }

    [[nodiscard]] size_t capacity() const { return m_capacity - 1; }

private:
    const size_t m_capacity;
    std::unique_ptr<T[]> m_buffer;

    // Cache-line aligned to prevent false sharing between producer and consumer
    alignas(64) std::atomic<size_t> m_writePos{0};
    alignas(64) std::atomic<size_t> m_readPos{0};
};

} // namespace dawcast
