#ifndef THREADSAFEQUEUE_H
#define THREADSAFEQUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <stdexcept>

template <typename T>
class ThreadSafeQueue {
public:
    ThreadSafeQueue() = default;

    ThreadSafeQueue(uint64_t max_size, void (*callback_on_full)(T)) : max_size(max_size), on_queue_full(callback_on_full) {}

    void (*on_queue_full)(T) = nullptr;

    // Add an element to the queue
    void push(const T& item);

    // Remove and return an element from the queue
    T pop();

    // Try to pop an item, but return false if the queue is empty
    bool try_pop(T& item);

    // Check if the queue is empty
    bool empty() const;

    // Get the size of the queue
    size_t size() const;


private:
    mutable std::mutex mutex_;
    std::queue<T> queue_;
    std::condition_variable condition_;
    uint64_t max_size = UINT64_MAX;
};

#include "threadsafequeue.cpp"
#endif // THREADSAFEQUEUE_H
