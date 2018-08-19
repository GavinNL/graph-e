
#pragma once

#ifndef FRAME_GRAPH_3_H
#define FRAME_GRAPH_3_H

#include <memory>
#include <mutex>
#include <functional>
#include <thread>
#include <algorithm>
#include <map>
#include <vector>
#include <queue>
#include <any>
#include <iostream>

namespace graphe
{

using time_point = std::chrono::system_clock::time_point;

class node_graph;
class exec_node;
class resource_node;
using exec_node_p     = std::shared_ptr<exec_node>;
using resource_node_p = std::shared_ptr<resource_node>;
using exec_node_w      = std::weak_ptr<exec_node>;
using resource_node_w  = std::weak_ptr<resource_node>;

enum class node_flags
{
    execute_once,     // node will only execute once. Even after reset() is called, this node will not execute
                      // nodes of this type must produce One_Shot resources.
    execute_multiple

};

enum class resource_flags
{
    resetable,   // resource can be reset
    permanent,   // resource is created once and never reset, even after reset() is called.
    moveable,    // resource is moved from one ExecNode to another. Only one ExecNode can use it as an input.
};

/**
 * @brief The exec_node class
 *
 * An exec_node is a node that executes some kind of computation.
 */
class exec_node
{
protected:
    friend class node_graph;
    friend class ResourceRegistry;

    std::string  m_name;
    std::any     m_NodeClass;                      // an instance of the Node class
    std::any     m_NodeData;                       // an instance of the node data
    std::mutex   m_mutex;                          // mutex to prevent the node from executing twice
    bool         m_scheduled = false;              // has this node been scheduled to run.
    bool         m_executed = false;               // flag to indicate whether the node has been executed.
    node_graph * m_Graph; // the parent graph;

    time_point     m_exec_start_time_us;            // the time at which this node was executed

    std::thread::id m_thread_id;                  // the id of the thread that executed this.

    node_flags   m_flags;
    std::vector<resource_node_w> m_requiredResources; // a list of required resources
    std::vector<resource_node_w> m_producedResources; // a list of required resources


public:
    std::function<void(void)> execute; // Function object to execute the Node's () operator.


    ~exec_node()
    {
    //    std::cout << "Node Destroyed: " << m_name << std::endl;
    }

    time_point get_time()
    {
        return m_exec_start_time_us;
    }

    std::chrono::microseconds get_time(time_point start)
    {
        return std::chrono::duration_cast<std::chrono::microseconds>(m_exec_start_time_us-start);
    }
    /**
     * @brief trigger
     *
     * Nudge the exec_node to check whether all its resource are available. If it is available
     * execute the call
     */
    void trigger();

    /**
     * @brief can_execute
     * @return
     *
     * Returns true if this node is able to execute. A node is able to execute if
     * all it's required resources have been made available.
     */
    bool can_execute() const;

    const std::string & get_name() const
    {
        return m_name;
    }

    void set_name(const std::string & name) {
        m_name = name;
    }

    node_flags get_flags() const
    {
        return m_flags;
    }

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
    bool                     m_is_available = false;
    resource_flags           m_flags;

    exec_node_w              m_parent;
public:
    time_point m_time_available;

    ~resource_node()
    {

    }

    /**
     * @brief make_available
     * @param av
     *
     * Makes the resource available to other nodes. If this resource is needed by another node, that node
     * will be scheduled for execution so long as all other required resources are satisfied
     */
    void make_available(bool av = true)
    {
        m_is_available = av;
        m_time_available = std::chrono::system_clock::now();
    }

    resource_flags get_flags() const
    {
        return m_flags;
    }

    /**
     * @brief is_available
     * @return
     *
     * Returns true if this resource is available
     */
    bool is_available() const
    {
        return m_is_available;
    }

    time_point get_time()
    {
        return m_time_available;
    }
    std::chrono::microseconds get_time(time_point start)
    {
        return std::chrono::duration_cast<std::chrono::microseconds>(m_time_available-start);
    }

    /**
     * @brief get_resource
     * @return
     *
     * Gets a reference to the resource.
     */
    std::any & get_resource()
    {
        return m_resource;
    }

    bool has_parent() const
    {
        return m_parent.lock()!=nullptr;
    }
    /**
     * @brief Get
     * @return
     *
     * Gets a reference to the resource cast to the particular type. Throws an
     * exception of the resource has not been created
     */
    template<typename T>
    T & Get()
    {
        return std::any_cast<T&>(m_resource);
    }

    std::string const & get_name() const
    {
        return m_name;
    }

    /**
     * @brief notify_dependents
     *
     * Notify all nodes waiting on this resource that this resource is available.
     */
    void notify_dependents()
    {
        for(auto & N : m_Nodes)
        {
            if( auto n = N.lock())
                n->trigger();
        }
    }
};


/**
 * @brief The in_resource class
 *
 * Class used to register an input resource.
 */
template<typename T>
class in_resource
{
protected:
    friend class ResourceRegistry;
    resource_node_w m_node;
public:

    /**
     * @brief get
     * @return
     *
     * Gets a reference to the resource.
     */
    T & get()
    {
        return m_node.lock()->Get<T>();
    }
};

/**
 * @brief The out_resource class
 *
 * Class for outgoing resources.
 */
template<typename T>
class out_resource
{
protected:
    friend class ResourceRegistry;
    resource_node_w m_node;
public:

    /**
     * @brief get
     * @return
     *
     * Returns a reference to the node. This will throw an exception
     * if the resource has not either been emplaced() or set()
     */
    T & get()
    {
        return m_node.lock()->Get<T>();//std::any_cast<T&>( m_node.lock()->m_resource );
    }

    /**
     * @brief make_available
     * Makes this resource available. Once it is available, any ExecNodes
     * which require this resource will be allowed to execute.
     */
    void make_available()
    {
        auto node = m_node.lock();
        if( node )
        {
            if( !node->is_available() )
            {
                //std::cout << node->get_name() << " is available" << std::endl;
                node->make_available();
                node->notify_dependents();
            }
        }
    }

    /**
     * @brief emplace
     * @param __args
     *
     * Construct the resource in place
     */
    template<typename... _Args>
    void emplace(_Args&&... __args)
    {
      typedef typename std::remove_const<T>::type Node_t;

      m_node.lock()->get_resource().emplace<T>( std::forward<_Args>(__args)...);
    }

    /**
     * @brief set
     * @param x
     * @param make_avail
     *
     * Sets the resource
     */
    void set(T const & x, bool make_avail=true)
    {
        m_node.lock()->get_resource() = x;
        if( make_avail) make_available();
    }

    void set(T && x, bool make_avail=true)
    {
        m_node.lock()->get_resource() = std::move(x);
        if( make_avail) make_available();
    }
};




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

        template<typename T, resource_flags F=resource_flags::resetable>
        out_resource<T> register_output_resource(const std::string & name)
        {
            if( m_resources.count(name) == 0 )
            {
                resource_node_p RN = std::make_shared< resource_node >();

                RN->m_name     = name;
                RN->m_flags    = F;

                RN->m_parent = m_Node;

                m_Node->m_producedResources.push_back(RN);
                m_resources[name] = RN;

                out_resource<T> r;
                r.m_node = RN;

                return r;
            }
            else
            {
                out_resource<T> r;


                r.m_node = m_resources.at(name);
                if(r.m_node.lock()->m_flags != F)
                {
                    throw std::runtime_error(std::string("Resource ") + name + std::string(" previously registered as different type") );
                }
                m_Node->m_producedResources.push_back( r.m_node);

                return r;
            }
        }

        template<typename T, resource_flags F=resource_flags::resetable>
        in_resource<T> register_input_resource(const std::string & name)
        {
            if( m_resources.count(name) == 0 )
            {
                resource_node_p RN = std::make_shared<resource_node>();

                RN->m_Nodes.push_back(m_Node);
                RN->m_name = name;
                RN->m_flags = F;
                m_resources[name] = RN;

                in_resource<T> r;
                r.m_node = RN;

                m_required_resources.push_back(RN);

                return r;
            }
            else
            {
                resource_node_p RN = m_resources[name];
                RN->m_Nodes.push_back(m_Node);

                in_resource<T> r;
                r.m_node = RN;
                if(r.m_node.lock()->m_flags != F)
                {
                    throw std::runtime_error(std::string("Resource ") + name + std::string(" previously registered as different type") );
                }

                m_required_resources.push_back(RN);

                return r;
            }
        }
};


class node_graph
{
public:
    node_graph() {}
    ~node_graph()
    {

        std::sort( m_exec_nodes.begin(), m_exec_nodes.end(),
                   [](exec_node_p & l, exec_node_p & r)
        {
            return l->m_exec_start_time_us < r->m_exec_start_time_us;
        });

        for(auto & e : m_exec_nodes)
        {
            //std::cout << e->get_name() << " use count: " << e.use_count() << "  time: " << e->m_exec_start_time_us.count() << std::endl;
        }

        while(m_exec_nodes.size())
        {
            m_exec_nodes.pop_back();
        }
    }

    node_graph( node_graph const & other) = delete;
    node_graph( node_graph && other) = delete;
    node_graph & operator = ( node_graph const & other) = delete;
    node_graph & operator = ( node_graph && other) = delete;


    template<typename _Tp, typename... _Args>
    /**
     * @brief add_oneshot_node
     * @param __args
     * @return
     *
     * Add a one-shot node to the graph. A one-shot node only executes once.
     *
     * A one-shot nodemay only register "permanent" resources.
     */
    inline exec_node & add_oneshot_node(_Args&&... __args)
    {
        return add_node_flags<node_flags::execute_once,_Tp>( std::forward<_Args>(__args)... );
    }

    /**
     * @brief add_node
     * @param __args
     * @return
     *
     * Add a regular exec node to the graph
     */
    template<typename _Tp, typename... _Args>
    inline exec_node & add_node(_Args&&... __args)
    {
        return add_node_flags<node_flags::execute_multiple,_Tp>( std::forward<_Args>(__args)... );
    }
    /**
     * @brief AddNode
     * @param __args
     *
     * Add a node to the Graph. The template class _Tp must contain a struct
     * named Data_t.
     */
    template<node_flags F, typename _Tp, typename... _Args>
    inline exec_node & add_node_flags(_Args&&... __args)
    {
      typedef typename std::remove_const<_Tp>::type Node_t;

      exec_node_p N   = std::make_shared<exec_node>();

      N->m_flags = F;
      ResourceRegistry R(N,  m_resources,  N->m_requiredResources);

      N->m_NodeClass.emplace<Node_t>( R, std::forward<_Args>(__args)...);

      N->m_name      = typeid( _Tp).name();// "Node_" + std::to_string(global_count++);
      exec_node* rawp = N.get();
      rawp->m_Graph  = this;

      // Create the functor which will execute the
      // Node's operator(data_t &d) method.
      N->execute = [rawp,N]()
      {
          if( !rawp->m_executed ) // if we haven't executed yet.
          {

              if( rawp->m_mutex.try_lock() ) // try to lock the mutex
              {                              // if we have acquired the lock, execute the node
                  rawp->m_executed = true;
                  rawp->m_exec_start_time_us = std::chrono::system_clock::now();

                  //std::cout << "Executing: " << rawp->m_name << std::endl;
                  //for(auto & r : rawp->m_requiredResources)
                  //{
                  //    auto R = r.lock();
                  //    std::cout <<  "  " << R->get_name() << "  available: " << R->is_available() << std::endl;
                  //}

                  rawp->m_thread_id = std::this_thread::get_id();
                  //======== Exectue ========================
                  std::any_cast< Node_t&>( rawp->m_NodeClass )();
                  //==========================================

                  --rawp->m_Graph->m_numToExecute;
                  rawp->m_mutex.unlock();

                  for(auto & r : rawp->m_producedResources)
                  {
                    auto R = r.lock();

                    if( !R->is_available() )
                    {
                        throw std::runtime_error( std::string("Node ") + rawp->get_name() + std::string(" failed to create resource: ") + R->get_name());
                    }
                  }
              }
          }
      };

      if( F == node_flags::execute_once)
      {
          for( auto & r : N->m_producedResources)
          {
              auto R = r.lock();
              if( R->get_flags() != resource_flags::permanent )
              {

                throw std::runtime_error( std::string("Node, set as ExecuteOnce, but produces resettable resource, ") + R->get_name() + std::string(". Nodes set as ExecuteOnce may only produce permenant resources.") );
              }
          }
      }

      //std::any_cast< Node_t&>(N->m_NodeClass).registerResources( std::any_cast< Data_t&>( rawp->m_NodeData ), R);

      m_exec_nodes.push_back(N);

      return *N;
    }

    /**
     * @brief schedule_node
     * @param p
     *
     * Schedules this node for execution.
     */
    void schedule_node( exec_node * p)
    {
        ++m_numToExecute;
        if(onSchedule)
            onSchedule(p);
    }

    /**
     * @brief Reset
     * @param destroy_resources - destroys all the resources as well. Default is false.
     */
    void reset(bool destroy_resources = false)
    {

        m_exec_nodes.erase(std::remove_if(m_exec_nodes.begin(),
                                  m_exec_nodes.end(),
                                  [](exec_node_p & x)
                                  {
                                      x->m_executed  = false;
                                      x->m_scheduled = false;
                                      //x->m_resourceCount = 0;
                                      //std::cout << "Clearing " << x->m_name << std::endl;

                                      if(x->get_flags() == node_flags::execute_once)
                                      {
                                          x.reset();
                                          return true;
                                      }
                                      return false;
                                  }),
                   m_exec_nodes.end());


        for(auto & N : m_resources)
        {
            if( N.second->get_flags() != resource_flags::permanent)
            {
                N.second->make_available(false);
            }
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


    void print_node_resource_order(exec_node_p & e)
    {
        for(auto & r : e->m_requiredResources)
        {
            std::cout << r.lock()->get_name() << " -> " << e->get_name() << std::endl;
        }
        for(auto & r : e->m_producedResources)
        {
            std::cout << e->get_name() << " -> " << r.lock()->get_name() << std::endl;
        }
        if( e->m_producedResources.size()==0) return;

        //std::cout << e->m_name <<( e->m_producedResources.size()==0?"\n":" -> ");

    }

    void print_node(resource_node_p & N, time_point start)
    {
        std::cout << "{\n";
        std::cout << "    rank=same \n";
        std::cout << "    " << N->get_name() << "[shape=circle style=filled  color=\"0.650 0.700 0.700\"]\n";
        std::cout << "    " << N->get_time(start).count() << "\n";
        std::cout << "}" << std::endl;;
    }

    void print_node(exec_node_p & N, time_point start)
    {
        std::cout << "{\n";
        std::cout << "    rank=same \n";
        std::cout << "    " << N->get_name() << "[shape=square]\n";
        std::cout << "    " << N->get_time(start).count() << "\n";
        std::cout << "}" << std::endl;;
    }

    /**
     * @brief print
     *
     * Prints the graph in dot format.
     */
    void print()
    {

        std::map<std::thread::id, int> threadProps;
        std::map<time_point, int> nodes;

        auto min = m_exec_nodes[0]->get_time();
        for(auto & E : m_exec_nodes)
        {
            auto t = E->get_time();
            min = std::min(min, t);
            nodes[t]=0;
        }
        for(auto & E : m_resources)
        {

            auto t = E.second->get_time();
            if(E.second->get_flags() != resource_flags::permanent)
            {
                min    = std::min(min, t);
            }
            nodes[t]=0;


        }

        std::cout << "digraph G {" << std::endl;

        auto s = nodes.size();
        for(auto & E : nodes)
        {
            auto t = std::chrono::duration_cast<std::chrono::microseconds>(E.first - min).count();
            std::cout << t << (s==1? "\n":" -> ") ;
            s--;
        };


        for(auto & E : m_exec_nodes)
        {
            print_node(E,min);
        }
        for(auto & E : m_resources)
        {
            print_node(E.second,min);
        }

        for(auto & E : m_exec_nodes)
        {
            print_node_resource_order(E);
        }


        std::cout << "}" << std::endl;
    }

    std::vector< exec_node_p > & get_exec_nodes()
    {
        return m_exec_nodes;
    }


    uint32_t get_num_running() const
    {
        return m_numRunning;
    }

    uint32_t get_left_to_execute() const
    {
        return m_numToExecute;
    }
protected:

    std::vector< exec_node_p >             m_exec_nodes;
    std::map<std::string, resource_node_p> m_resources;

    uint32_t m_numRunning   = 0;
    uint32_t m_numToExecute = 0;

   friend class exec_node;
public:
   std::function<void(exec_node*)>  onSchedule;
};

inline void exec_node::trigger()
{
    if( can_execute() )
    {
        if(!m_scheduled)
        {
            m_scheduled = true;
            m_Graph->schedule_node(this);
        }
    }
}

inline bool exec_node::can_execute() const
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

}

#endif

