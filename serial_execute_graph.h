#pragma once

#ifndef SERIAL_EXECUTE_GRAPH_3_H
#define SERIAL_EXECUTE_GRAPH_3_H

#include "execute_graph.h"


class serial_execute
{
public:
    serial_execute(node_graph & graph) : m_graph(graph)
    {
        m_graph.onSchedule = [this](exec_node *N)
        {
            m_ToExecute.push(N);
        };
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
        // execute the all nodes in the queue.
        // New nodes will be added
        while( m_ToExecute.size() )
        {
            m_ToExecute.front()->execute();
            m_ToExecute.pop();
        }
    }

protected:
    void __schedule_node_for_execution( exec_node * node)
    {
        m_ToExecute.push(node);
    }

    node_graph                 & m_graph;
    std::queue<exec_node*>                 m_ToExecute;

};

#endif

