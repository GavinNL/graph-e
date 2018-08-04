#pragma once
#ifndef SEMAPHORE_BARRIER_TEST_H_
#define SEMAPHORE_BARRIER_TEST_H_

#include <condition_variable>
#include <mutex>
#include <thread>

class semaphore
{
private:
    std::mutex              m_mutex;
    std::condition_variable m_condition;
    uint32_t                m_count = 0; // Initialized as locked.

public:
    void notify() {
        std::unique_lock<decltype(m_mutex)> lock(m_mutex);
        ++m_count;
        m_condition.notify_one();
    }

    void notify_all(){

    }
    void wait() {
        std::unique_lock<decltype(m_mutex)> lock(m_mutex);
        while(!m_count) // Handle spurious wake-ups.
            m_condition.wait(lock);
        --m_count;
    }

    bool try_wait() {
        std::unique_lock<decltype(m_mutex)> lock(m_mutex);
        if(m_count) {
            --m_count;
            return true;
        }
        return false;
    }
};

#endif
