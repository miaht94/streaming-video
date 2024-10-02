#include "threadsafequeue.hpp"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <stdexcept>



template <typename T>
void ThreadSafeQueue<T>::push(const T& item) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push(item);
    if (queue_.size() > max_size && this->on_queue_full != nullptr) {
        T item = queue_.front();
        queue_.pop();
        this->on_queue_full(item); // Call the callback function
    }
    if (queue_.size() == 0) 
        return;
    condition_.notify_one(); // Notify one waiting thread
}

template <typename T>
T ThreadSafeQueue<T>::pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    condition_.wait(lock, [this]() { return !queue_.empty(); });
    
    T item = queue_.front();
    queue_.pop();
    return item;
}

template <typename T>
bool ThreadSafeQueue<T>::try_pop(T& item) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) {
        return false;
    }
    item = queue_.front();
    queue_.pop();
    return true;
}

template <typename T>
bool ThreadSafeQueue<T>::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
}

template <typename T>
size_t ThreadSafeQueue<T>::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}
