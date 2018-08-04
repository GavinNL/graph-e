
#pragma once

#ifndef FRAME_GRAPH_3_H
#define FRAME_GRAPH_3_H

#include <memory>
#include <future>

#include <map>
#include <vector>
#include <queue>
#include <any>
#include <iostream>

#include <sstream>
#include <iostream>
#include <iomanip>

#include "semaphore.h"

class FrameGraphBase;
class ExecNode;
class ResourceNode;
using ExecNode_p     = std::shared_ptr<ExecNode>;
using ResourceNode_p = std::shared_ptr<ResourceNode>;
using ExecNode_w      = std::weak_ptr<ExecNode>;
using ResourceNode_w  = std::weak_ptr<ResourceNode>;

/**
 * @brief The ExecNode class
 *
 * An ExecNode is a node that executes some kind of computation.
 */
class ExecNode
{
public:
    std::string  m_name;
    std::any     m_NodeClass;                      // an instance of the Node class
    std::any     m_NodeData;                       // an instance of the node data
    std::mutex   m_mutex;                          // mutex to prevent the node from executing twice
    bool         m_scheduled = false;              // has this node been scheduled to run.
    bool         m_executed = false;               // flag to indicate whether the node has been executed.
    FrameGraphBase * m_Graph; // the parent graph;

    uint32_t     m_resourceCount = 0;

    std::vector<ResourceNode_w> m_requiredResources; // a list of required resources
    std::vector<ResourceNode_w> m_producedResources; // a list of required resources

    std::function<void(void)> execute; // Function object to execute the Node's () operator.


    // nudge the ExecNode to check whether all its resources are available.
    // if they are, then tell the FrameGraphBase to schedule its execution
    void trigger();

    /**
     * @brief can_execute
     * @return
     *
     * Returns true if this node is able to execute. A node is able to execute if
     * all it's required resources have been made available.
     */
    bool can_execute() const;

};

/**
 * @brief The ResourceNode class
 * A resource node is a node which holds a resource and is created or consumed by an ExecNode.
 *
 * An ExecNode is executed when all it's required resources have become available.
 */
class ResourceNode
{
public:
    std::any                m_resource;
    std::string             m_name;
    std::vector<ExecNode_w> m_Nodes; // list of nodes that must be triggered
                                     // when resource becomes availabe
    bool m_is_available = false;

    void make_available()
    {
        m_is_available = true;
    }
    bool is_available() const
    {
        return m_is_available;
    }

    template<typename T>
    T & Get()
    {
        return std::any_cast<T>(m_resource);
    }
};

template<typename T>
class Resource
{
public:
    ResourceNode_w m_node;
    T & get()
    {
        return std::any_cast<T&>(m_node.lock()->m_resource);
    }

    void make_available()
    {
        auto node = m_node.lock();
        if( node )
        {
            if( !node->is_available() )
            {
                node->make_available();

                // loop through all the exec nodes which require this resource
                // and trigger them.
                for(auto & N : node->m_Nodes)
                {
                    if( auto n = N.lock())
                        n->trigger();
                }
            }
        }
    }

    Resource & operator = ( T const & v )
    {
        get() = this;
        return *this;
    }

    operator T&()
    {
        return get();
    }

    void set(T const & x, bool make_avail=true)
    {
        get() = x;
        if( make_avail) make_available();

    }
};

template<typename T>
using Resource_p = std::shared_ptr< Resource<T> >;

class ResourceRegistry
{
    std::map<std::string, ResourceNode_p> & m_resources;
    std::vector<ResourceNode_w> & m_required_resources;
    ExecNode_p & m_Node;

    public:
        ResourceRegistry( ExecNode_p & node,
                          std::map<std::string, ResourceNode_p> & m,
                          std::vector<ResourceNode_w> & required_resources) :
            m_Node(node),
            m_resources(m),
            m_required_resources(required_resources)
        {

        }

        template<typename T>
        Resource<T> create_PromiseResource(const std::string & name)
        {
            if( m_resources.count(name) == 0 )
            {
                ResourceNode_p RN = std::make_shared< ResourceNode >();
                RN->m_resource      = std::make_any<T>();
                RN->m_name          = name;
                m_Node->m_producedResources.push_back(RN);
                m_resources[name] = RN;

                Resource<T> r;
                r.m_node = RN;

                return r;
            }
            else
            {
                Resource<T> r;
                r.m_node = m_resources.at(name);
                m_Node->m_producedResources.push_back( r.m_node);
                return r;
            }
        }

        template<typename T>
        Resource<T> create_FutureResource(const std::string & name)
        {
            if( m_resources.count(name) == 0 )
            {
                ResourceNode_p RN = std::make_shared<ResourceNode>();
                RN->m_resource      = std::make_any<T>();
                RN->m_Nodes.push_back(m_Node);
                RN->m_name = name;
                m_resources[name] = RN;

                Resource<T> r;
                r.m_node = RN;

                m_required_resources.push_back(RN);

                return r;
            }
            else
            {
                ResourceNode_p RN = m_resources[name];
                RN->m_Nodes.push_back(m_Node);

                Resource<T> r;
                r.m_node = RN;

                m_required_resources.push_back(RN);

                return r;
            }
        }
};



class FrameGraphBase
{
public:
    FrameGraphBase() {}
    ~FrameGraphBase()
    {
    }

    FrameGraphBase( FrameGraphBase const & other) = delete;
    FrameGraphBase( FrameGraphBase && other) = delete;
    FrameGraphBase & operator = ( FrameGraphBase const & other) = delete;
    FrameGraphBase & operator = ( FrameGraphBase && other) = delete;


    /**
     * @brief AddNode
     * @param __args
     *
     * Add a node to the Graph. The template class _Tp must contain a struct
     * named Data_t.
     */
    template<typename _Tp, typename... _Args>
    inline void AddNode(_Args&&... __args)
    {
      typedef typename std::remove_const<_Tp>::type Node_t;
      typedef typename Node_t::Data_t               Data_t;

      ExecNode_p N   = std::make_shared<ExecNode>();


      N->m_NodeClass = std::make_any<Node_t>( std::forward<_Args>(__args)...);
      N->m_NodeData  = std::make_any<Data_t>();
      N->m_name      = typeid( _Tp).name();// "Node_" + std::to_string(global_count++);
      ExecNode* rawp = N.get();
      rawp->m_Graph  = this;

      // Create the functor which will execute the
      // Node's operator(data_t &d) method.
      N->execute = [rawp]()
      {
          if( !rawp->m_executed ) // if we haven't executed yet.
          {

              if( rawp->m_mutex.try_lock() ) // try to lock the mutex
              {                              // if we have acquired the lock, execute the node
                  rawp->m_executed = true;
                  std::any_cast< Node_t&>( rawp->m_NodeClass )(   std::any_cast< Data_t&>( rawp->m_NodeData ) );
                  --rawp->m_Graph->m_numToExecute;
                  rawp->m_mutex.unlock();
              }
          }
      };

      ResourceRegistry R(N,  m_resources,  N->m_requiredResources);

      std::any_cast< Node_t&>(N->m_NodeClass).registerResources( std::any_cast< Data_t&>( rawp->m_NodeData ), R);

      m_execNodes.push_back(N);

    }


    void ScheduleNode( ExecNode * p)
    {
        ++m_numToExecute;
        __schedule_node_for_execution(p);
    }

    /**
     * @brief Reset
     * @param destroy_resources - destroys all the resources as well. Default is false.
     */
    void Reset(bool destroy_resources = false)
    {
        for(auto & N : m_execNodes)
        {
            N->m_executed = false;
            N->m_scheduled = false;
        }
        for(auto & N : m_resources)
        {
            N.second->m_is_available = false;
        }
    }


    ResourceNode_p  GetResource(std::string const & name)
    {
        return m_resources.at(name);
    }

    void PrintInfo()
    {
        std::cout << "Num Nodes: " << m_execNodes.size() << std::endl;
        std::cout << "Num Resources: " << m_resources.size() << std::endl;
        std::cout << "Num Running: " << m_numRunning << std::endl;
        std::cout << "Num To Executing: " << m_numToExecute << std::endl;
    }

    void print()
    {
        std::cout << "digraph G {" << std::endl;

#define MAKE_NAME(E) ("_" + E)
        for(auto & E : m_execNodes)
        {
            std::cout <<  MAKE_NAME(E->m_name) << " [shape=square]" << std::endl;
        }
        for(auto & E : m_resources)
        {
            std::cout <<  E.second->m_name << " [shape=circle]" << std::endl;
        }

        for(auto & E : m_execNodes)
        {
            for(auto & r : E->m_requiredResources)
            {

                if(auto R=r.lock() ) std::cout << R->m_name << " -> " <<  MAKE_NAME(E->m_name) << std::endl;
            }
            for(auto & r : E->m_producedResources)
            {
                if(auto R=r.lock() ) std::cout << MAKE_NAME(E->m_name) << " -> " << R->m_name  << std::endl;
            }
        }


        std::cout << "}" << std::endl;

        // for(auto & E : m_execNodes)
        // {
        //     auto name = E->m_name;
        //     auto count = E.use_count();
        //     std::cout <<  MAKE_NAME(name) << " use count: " << count << std::endl;
        // }
        // for(auto & E : m_resources)
        // {
        //     auto name = E.first;
        //     auto count = E.second.use_count();
        //     std::cout <<  name << " use count: " << count << std::endl;
        // }
    }

    std::vector< ExecNode_p >             m_execNodes;
    std::map<std::string, ResourceNode_p> m_resources;

    // queue of all the nodes ready to be launched
    //std::queue<ExecNode*>                 m_ToExecute;

    uint32_t m_numRunning = 0;
    uint32_t m_numToExecute = 0;

protected:

    /**
    * @brief __append_node_to_queue
    * @param node
    *
    * Append a node to the execution queue. so that it can be executed
    * when the next thread worker is available.
    */
   virtual void __schedule_node_for_execution( ExecNode * node) = 0;


   friend class ExecNode;

};

inline void ExecNode::trigger()
{
    ++m_resourceCount;
    if( m_resourceCount >= m_requiredResources.size())
    {
        if(!m_scheduled)
        {
            m_scheduled = true;
            m_Graph->ScheduleNode(this);
        }
    }
}

bool ExecNode::can_execute() const
{
    for(auto & R : m_requiredResources)
    {
        auto r = R.lock();
        if( r )
        {
            if( !r->is_available() )
            {
                return false;
            }
        } else {
            return false;
        }
    }
    return true;
}


class FrameGraphSerial : public FrameGraphBase
{
public:

    void execute()
    {
        for(auto & N : m_execNodes) // place all the nodes with no resource requirements onto the queue.
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

    virtual void __schedule_node_for_execution( ExecNode * node) override
    {
        m_ToExecute.push(node);
    }



    std::queue<ExecNode*>                 m_ToExecute;

};


template<typename ThreadPool_t>
class FrameGraphThreadPool : public FrameGraphBase
{
public:
    void SetThreadPool(ThreadPool_t * T)
    {
        m_thread_pool = T;
    }

    ThreadPool_t * GetThreadPool() const
    {
        return m_thread_pool;
    }
    ~FrameGraphThreadPool()
    {
        std::cout << "FrameGraphThreadPool::Destructor!" << std::endl;
        Wait();
        std::cout << "Waiting for threads to exit!" << std::endl;
    }

    void Wait()
    {
        while( m_numRunning > 0 || m_numToExecute > 0)
        {
            std::this_thread::sleep_for( std::chrono::microseconds(500));
        }
    }

    void Execute()
    {
        for(auto & N : m_execNodes) // place all the nodes with no resource requirements onto the queue.
        {
            if( N->can_execute() )
            {
                ScheduleNode( N.get() );
            }
        }
    }

    virtual void __schedule_node_for_execution( ExecNode * node) override
    {
        m_thread_pool->push(node->execute);
    }


private:
    ThreadPool_t                          *m_thread_pool = nullptr;
};

#endif

