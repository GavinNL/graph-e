#pragma once

#ifndef SERIAL_EXECUTE_GRAPH_3_H
#define SERIAL_EXECUTE_GRAPH_3_H

#include "execute_graph.h"

class serial_execution_graph : public execution_graph_base
{
public:

    void execute()
    {
        for(auto & N : m_exec_nodes) // place all the nodes with no resource requirements onto the queue.
        {
            if( N->can_execute() )
            {
                __schedule_node_for_execution(N.get());
            }
        }
        // execute the all nodes in the queue.
        // New nodes will be added
        while( m_ToExecute.size() )
        {
            m_ToExecute.front()->execute();
            m_ToExecute.pop();
        }
    }

protected:
    virtual void __schedule_node_for_execution( exec_node * node) override
    {
        m_ToExecute.push(node);
    }



    std::queue<exec_node*>                 m_ToExecute;

};


#endif

