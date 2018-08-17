#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include <iostream>

#ifndef GNL_NAMESPACE
    #define GNL_NAMESPACE gnl
#endif
namespace GNL_NAMESPACE
{

class thread_pool
{

    public:
        thread_pool(size_t num_threads);
        thread_pool();


        template<class F, class... Args>
        std::future<typename std::result_of<F(Args...)>::type> push( F && f, Args &&... args);

        /**
         * @brief create_workers
         * @param num
         * Creates a new worker to work the threadpool
         */
        void create_workers(std::size_t num);
        /**
         * @brief remove_thread
         * Remove a worker from the thread pool
         */
        void remove_worker();
        /**
         * @brief clear_tasks
         *
         * Clears all the current tasks in the queue
         */
        void clear_tasks();

        /**
         * @brief num_tasks
         * @return
         *
         * Returns the number of tasks still in the queue
         */
        std::size_t num_tasks() { return m_tasks.size(); }

        /**
         * @brief num_workers
         * @return
         *
         * Returns the number of workers in this thread pool
         */
        std::size_t num_workers() { return m_worker_count; }




        ~thread_pool();

    protected:
        /**
         * @brief add_thread
         * Add a new worker to the thread pool
         */
        void add_thread();


        // need to keep track of threads so we can join them
        std::vector< std::thread > workers;

        // the task queue
        std::queue< std::function<void()> > m_tasks;

        // synchronization
        std::mutex              m_mutex;
        std::condition_variable m_cv;
        uint32_t                m_worker_count=0; // number of currently active workers
        uint32_t                m_thread_count=0;
       // bool stop;

};


inline void thread_pool::remove_worker()
{
    --m_thread_count;
}

inline void thread_pool::create_workers(std::size_t num)
{
    for(size_t i=0;i<num;++i)
    {
        add_thread();
    }
    if( m_tasks.size())
        m_cv.notify_all();
}

inline void thread_pool::add_thread()
{
    ++m_thread_count;
    ++m_worker_count;

    workers.emplace_back(
        [this]
        {
            for(;;)
            {
                std::function<void()> task;

                {
                    std::unique_lock<std::mutex> lock(this->m_mutex);

                    // Wait for someone to trigger the condition variable.
                    // But do not wait if the task list is empty
                    //this->m_cv.wait(lock, [this]{ return this->stop || !this->m_tasks.empty(); });
                    this->m_cv.wait(lock, [this]{ return (m_thread_count < m_worker_count) || !this->m_tasks.empty(); });

                    //  ========== Start Safe Zone =========================
                    //if( (this->stop && this->m_tasks.empty()) || (m_thread_count < m_worker_count) )
                    if( m_thread_count < m_worker_count )
                    {
                        // We do not need this thread anymore, so we can exit.
                        --m_worker_count;
                       // std::cout << std::this_thread::get_id() << " shutting down" << std::endl;
                        return;
                    }

                    task = std::move(this->m_tasks.front());
                    this->m_tasks.pop();
                    //std::cout << std::this_thread::get_id() << " Starting Task! " << m_tasks.size() << " tasks left" << std::endl;
                    //  ========== End Safe Zone =========================
                }
                task();
            }
        }
    );
}

// The constructor just launches some amount of workers
inline thread_pool::thread_pool(size_t threads)
    : m_worker_count(0), m_thread_count(0)
//    :   stop(false)
{
    for(size_t i = 0;i<threads;++i)
    {
        add_thread();
    }
}

inline thread_pool::thread_pool()  : m_worker_count(0), m_thread_count(0)
{

}

inline void thread_pool::clear_tasks()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    while(m_tasks.size()) m_tasks.pop();
}

// add new work item to the pool
template<class F, class... Args>
#define RETURN_TYPE typename std::result_of<F(Args...)>::type
std::future< RETURN_TYPE > thread_pool::push(F&& f, Args&&... args)
{
#undef RETURN_TYPE
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared< std::packaged_task<return_type()> >
                (
                    std::bind(std::forward<F>(f), std::forward<Args>(args)...)
                );

    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(m_mutex);

        // don't allow enqueueing after stopping the pool
        //if(stop)
        //    throw std::runtime_error("enqueue on stopped ThreadPool");

        m_tasks.emplace([task](){ (*task)(); });
    }
    m_cv.notify_one();
    return res;
}

// the destructor joins all threads
inline thread_pool::~thread_pool()
{
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_thread_count = 0;
    }

    m_cv.notify_all();

    for(std::thread &worker: workers)
    {
        if( worker.joinable())
            worker.join();
    }
}

}
#endif

