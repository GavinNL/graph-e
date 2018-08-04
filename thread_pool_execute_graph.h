#pragma once

#ifndef THREAD_POOL_EXECUTE_GRAPH_3_H
#define THREAD_POOL_EXECUTE_GRAPH_3_H

#include "execute_graph.h"

template<typename ThreadPool_t>
class thread_pool_execution_graph : public execution_graph_base
{
public:
    void set_thread_pool(ThreadPool_t * T)
    {
        m_thread_pool = T;
    }

    ThreadPool_t * get_thread_pool() const
    {
        return m_thread_pool;
    }
    ~thread_pool_execution_graph()
    {
        std::cout << "execution_graph_base_thread_pool::Destructor!" << std::endl;
        wait();
        std::cout << "Waiting for threads to exit!" << std::endl;
    }

    void wait()
    {
        while( m_numRunning > 0 || m_numToExecute > 0)
        {
            std::this_thread::sleep_for( std::chrono::microseconds(500));
        }
    }

    void execute()
    {
        for(auto & N : m_exec_nodes) // place all the nodes with no resource requirements onto the queue.
        {
            if( N->can_execute() )
            {
                schedule_node( N.get() );
            }
        }
    }
protected:
    virtual void __schedule_node_for_execution( exec_node * node) override
    {
        m_thread_pool->operator()(node->execute);
    }


private:
    ThreadPool_t                          *m_thread_pool = nullptr;
};

#endif

