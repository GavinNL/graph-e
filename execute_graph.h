
#pragma once

#ifndef FRAME_GRAPH_3_H
#define FRAME_GRAPH_3_H

#include <memory>
#include <mutex>
#include <functional>
#include <thread>

#include <map>
#include <vector>
#include <queue>
#include <any>
#include <iostream>



class execution_graph_base;
class exec_node;
class resource_node;
using exec_node_p     = std::shared_ptr<exec_node>;
using resource_node_p = std::shared_ptr<resource_node>;
using exec_node_w      = std::weak_ptr<exec_node>;
using resource_node_w  = std::weak_ptr<resource_node>;

/**
 * @brief The exec_node class
 *
 * An exec_node is a node that executes some kind of computation.
 */
class exec_node
{
protected:
    friend class execution_graph_base;
    friend class ResourceRegistry;

    std::string  m_name;
    std::any     m_NodeClass;                      // an instance of the Node class
    std::any     m_NodeData;                       // an instance of the node data
    std::mutex   m_mutex;                          // mutex to prevent the node from executing twice
    bool         m_scheduled = false;              // has this node been scheduled to run.
    bool         m_executed = false;               // flag to indicate whether the node has been executed.
    execution_graph_base * m_Graph; // the parent graph;

    uint32_t     m_resourceCount = 0;

    std::vector<resource_node_w> m_requiredResources; // a list of required resources
    std::vector<resource_node_w> m_producedResources; // a list of required resources


public:
    std::function<void(void)> execute; // Function object to execute the Node's () operator.

    // nudge the exec_node to check whether all its resources are available.
    // if they are, then tell the execution_graph_base to schedule its execution
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
 * @brief The resource_node class
 * A resource node is a node which holds a resource and is created or consumed by an exec_node.
 *
 * An exec_node is executed when all it's required resources have become available.
 */
class resource_node
{
protected:
    friend class ResourceRegistry;

    std::any                 m_resource;
    std::string              m_name;
    std::vector<exec_node_w> m_Nodes; // list of nodes that must be triggered
                                     // when resource becomes availabe
    bool m_is_available = false;

public:
    void make_available(bool av = true)
    {
        m_is_available = av;
    }

    bool is_available() const
    {
        return m_is_available;
    }

    template<typename T>
    T & Get()
    {
        return std::any_cast<T&>(m_resource);
    }

    std::string const & get_name() const
    {
        return m_name;
    }

    void notify_dependents()
    {
        for(auto & N : m_Nodes)
        {
            if( auto n = N.lock())
                n->trigger();
        }
    }
};

template<typename T>
class Resource
{
protected:
    friend class ResourceRegistry;
    resource_node_w m_node;
public:
    T & get()
    {
        return m_node.lock()->Get<T>();//std::any_cast<T&>( m_node.lock()->m_resource );
    }

    void make_available()
    {
        auto node = m_node.lock();
        if( node )
        {
            if( !node->is_available() )
            {
                node->make_available();
                node->notify_dependents();
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
    std::map<std::string, resource_node_p> & m_resources;
    std::vector<resource_node_w> & m_required_resources;
    exec_node_p & m_Node;

    public:
        ResourceRegistry( exec_node_p & node,
                          std::map<std::string, resource_node_p> & m,
                          std::vector<resource_node_w> & required_resources) :
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
                resource_node_p RN = std::make_shared< resource_node >();
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
                resource_node_p RN = std::make_shared<resource_node>();
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
                resource_node_p RN = m_resources[name];
                RN->m_Nodes.push_back(m_Node);

                Resource<T> r;
                r.m_node = RN;

                m_required_resources.push_back(RN);

                return r;
            }
        }
};



class execution_graph_base
{
public:
    execution_graph_base() {}
    ~execution_graph_base()
    {
    }

    execution_graph_base( execution_graph_base const & other) = delete;
    execution_graph_base( execution_graph_base && other) = delete;
    execution_graph_base & operator = ( execution_graph_base const & other) = delete;
    execution_graph_base & operator = ( execution_graph_base && other) = delete;


    /**
     * @brief AddNode
     * @param __args
     *
     * Add a node to the Graph. The template class _Tp must contain a struct
     * named Data_t.
     */
    template<typename _Tp, typename... _Args>
    inline void add_node(_Args&&... __args)
    {
      typedef typename std::remove_const<_Tp>::type Node_t;
      typedef typename Node_t::Data_t               Data_t;

      exec_node_p N   = std::make_shared<exec_node>();


      N->m_NodeClass = std::make_any<Node_t>( std::forward<_Args>(__args)...);
      N->m_NodeData  = std::make_any<Data_t>();
      N->m_name      = typeid( _Tp).name();// "Node_" + std::to_string(global_count++);
      exec_node* rawp = N.get();
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

      m_exec_nodes.push_back(N);

    }


    void schedule_node( exec_node * p)
    {
        ++m_numToExecute;
        __schedule_node_for_execution(p);
    }

    /**
     * @brief Reset
     * @param destroy_resources - destroys all the resources as well. Default is false.
     */
    void reset(bool destroy_resources = false)
    {
        for(auto & N : m_exec_nodes)
        {
            N->m_executed = false;
            N->m_scheduled = false;
        }
        for(auto & N : m_resources)
        {
            N.second->make_available(false);
        }
    }


    resource_node_p  get_resources(std::string const & name)
    {
        return m_resources.at(name);
    }

    void print_info()
    {
        std::cout << "Num Nodes: " << m_exec_nodes.size() << std::endl;
        std::cout << "Num Resources: " << m_resources.size() << std::endl;
        std::cout << "Num Running: " << m_numRunning << std::endl;
        std::cout << "Num To Executing: " << m_numToExecute << std::endl;
    }

    void print()
    {
        std::cout << "digraph G {" << std::endl;

#define MAKE_NAME(E) ("_" + E)
        for(auto & E : m_exec_nodes)
        {
            std::cout <<  MAKE_NAME(E->m_name) << " [shape=square]" << std::endl;
        }
        for(auto & E : m_resources)
        {
            std::cout <<  E.second->get_name() << " [shape=circle]" << std::endl;
        }

        for(auto & E : m_exec_nodes)
        {
            for(auto & r : E->m_requiredResources)
            {

                if(auto R=r.lock() ) std::cout << R->get_name() << " -> " <<  MAKE_NAME(E->m_name) << std::endl;
            }
            for(auto & r : E->m_producedResources)
            {
                if(auto R=r.lock() ) std::cout << MAKE_NAME(E->m_name) << " -> " << R->get_name()  << std::endl;
            }
        }


        std::cout << "}" << std::endl;

    }


protected:

    std::vector< exec_node_p >             m_exec_nodes;
    std::map<std::string, resource_node_p> m_resources;

    uint32_t m_numRunning   = 0;
    uint32_t m_numToExecute = 0;

    /**
    * @brief __append_node_to_queue
    * @param node
    *
    * Append a node to the execution queue. so that it can be executed
    * when the next thread worker is available.
    */
   virtual void __schedule_node_for_execution( exec_node * node) = 0;


   friend class exec_node;

};

inline void exec_node::trigger()
{
    ++m_resourceCount;
    if( m_resourceCount >= m_requiredResources.size())
    {
        if(!m_scheduled)
        {
            m_scheduled = true;
            m_Graph->schedule_node(this);
        }
    }
}

bool exec_node::can_execute() const
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


#endif

