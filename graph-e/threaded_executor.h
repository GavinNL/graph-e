#pragma once

#ifndef THREAD_POOL_EXECUTE_GRAPH_3_H
#define THREAD_POOL_EXECUTE_GRAPH_3_H

#include "node_graph.h"

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
        //std::cout << "execution_graph_base_thread_pool::Destructor!" << std::endl;
        wait();
        //std::cout << "Waiting for threads to exit!" << std::endl;
    }

    void wait()
    {
        while( m_graph.get_num_running() > 0 || m_graph.get_left_to_execute() > 0)
        {
            std::this_thread::sleep_for( std::chrono::microseconds(500));
        }
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
    ThreadPool_t                          *m_thread_pool = nullptr;
};

}

#endif

