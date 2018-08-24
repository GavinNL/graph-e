#pragma once

#ifndef THREAD_POOL_EXECUTE_GRAPH_3_H
#define THREAD_POOL_EXECUTE_GRAPH_3_H

#include "node_graph.h"
#include <condition_variable>
#include <mutex>

namespace graphe
{


template<typename ThreadPool_t>
class threaded_executor
{
public:
    threaded_executor(node_graph & graph) : m_graph(graph)
    {
        graph.onSchedule = [this](exec_node *N)
        {
            m_thread_pool->operator()(N->execute);
        };

        graph.onFinished = [this]()
        {
           std::cout << "Notifying" << std::endl;
           m_cv.notify_all();
        };
    }

    void set_thread_pool(ThreadPool_t * T)
    {
        m_thread_pool = T;
    }

    ThreadPool_t * get_thread_pool() const
    {
        return m_thread_pool;
    }
    ~threaded_executor()
    {
        wait();
    }

    void wait()
    {
        std::unique_lock<std::mutex> lk(m_wait_lock);
        m_cv.wait(lk, [this] { return !m_graph.busy(); } );
    }

    void execute()
    {
        for(auto & N : m_graph.get_exec_nodes()) // place all the nodes with no resource requirements onto the queue.
        {
            if( N->can_execute() )
            {
                m_graph.schedule_node(N.get());
            }
        }
    }


private:
    node_graph                 & m_graph;
    ThreadPool_t               *m_thread_pool = nullptr;
    std::mutex                  m_wait_lock;
    std::condition_variable     m_cv;
};

}

#endif

