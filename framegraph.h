#pragma once
#ifndef MGO_FRAMEGRAPH_TEST
#define MGO_FRAMEGRAPH_TEST

#include <iostream>
#include <condition_variable>
#include <mutex>
#include <functional>
#include <vector>
#include <iostream>
#include <any>
#include <map>
#include <set>
#include <thread>

#include <queue>

class barrier
{
private:
    std::mutex mutex_;
    std::condition_variable condition_;
    unsigned long count_ = 0; // Initialized as locked.
    int id=0;
public:
    barrier()
    {
        static int i=1;
        id = i++;
    }

    void notify_one()
    {
        std::cout << "Notifying " << id << std::endl;
        std::unique_lock<decltype(mutex_)> lock(mutex_);
        ++count_;
        condition_.notify_all();
    }

    void notify_all()
    {
        ++count_;
        condition_.notify_all();
    }

    void wait()
    {
        {
            std::cout << std::this_thread::get_id() << "  Waiting " << id << std::endl;
            std::unique_lock<decltype(mutex_)> lock(mutex_);

            while( count_ == 0) // Handle spurious wake-ups.
            {
                condition_.wait( lock );
            }

            std::cout << std::this_thread::get_id() << "  Woken up " << id << std::endl;
            --count_;
        }
        notify_all();
    }

    bool try_wait()
    {
        std::unique_lock<decltype(mutex_)> lock(mutex_);
        if(count_) {
            --count_;
            return true;
        }
        return false;
    }
};





template<typename T>
class resource
{
public:
    using shared_type = std::pair<bool, T>;

    resource()
    {
        m_value = std::make_shared<shared_type>();
        m_value->first = false;
    }

    ~resource()
    {
    }

    operator bool() const
    {
        return m_value->first;
    }

    T & get()
    {
        return m_value->second;
    }

    void set(T value)
    {
        m_value->second = value;
    }

    void make_available()
    {
        m_value->first = true;
    }

    void make_unavailable()
    {
        m_value->first = false;
    }
    bool is_available()
    {
        return m_value->first;
    }

    std::shared_ptr<shared_type> m_value;

};




class FrameNode;
class ResourceNode;

using FrameNode_p    = std::shared_ptr<FrameNode>;
using ResourceNode_p = std::shared_ptr<ResourceNode>;

class ResourceNode
{
public:
    std::any                     m_resource;
    //std::shared_ptr<resource_base> m_Resource;

    std::string                m_name;
    std::vector<FrameNode_p>   m_next; // a list of FrameNodes that must be triggered when this
                                       // resource becomes available.
    std::function<bool()> isAvailable;
    std::function<void()> MakeUnavailable;

    void clear()
    {
        m_resource.reset();
        m_next.clear();
        isAvailable = std::function<bool()>();
        MakeUnavailable = std::function<void()>();
    }

    ~ResourceNode()
    {
        std::cout << "Deleting Resource: " << m_name << std::endl;;
    }

};

class FrameNode
{
public:
    std::function<void(void)> init_f; // the excutable function that gets called.
    std::function<void(std::any&)> exec_f; // the excutable function that gets called.

    std::any    m_NodeData;
    std::string m_name;

    std::set< std::shared_ptr<ResourceNode> >   m_required; // a list of required Resource Nodes that this FrameNode consumes/uses

    std::set< std::shared_ptr<ResourceNode> >   m_produces; // a list of Resource Nodes that this
                                                            // FrameNode produces.

    ~FrameNode()
    {
        std::cout << m_name << " deleted" << std::endl;
    }
    void clear()
    {
        m_NodeData.reset();
        m_required.clear();
        m_produces.clear();
    }
};


class BlackBoard
{
    public:

    BlackBoard(
            FrameNode_p         Node,
            std::map<std::string, FrameNode_p >    & FrameNodes,
            std::map<std::string, ResourceNode_p > & ResourceNodes
    ) : m_Node(Node), m_FrameNodes(FrameNodes), m_ResourceNodes(ResourceNodes)
    {

    }


    template<typename T>
    ResourceNode_p create_resource(const std::string & name)
    {
        std::shared_ptr<ResourceNode> R;

        if( m_ResourceNodes.count(name) ) // node already exists, return this node
        {
            std::cout << "Resource : " << name << " exists " << std::endl;
            return m_ResourceNodes.at(name);
        }
        else
        {
            std::cout << "Creating resource: " << name << std::endl;
            R = std::make_shared<ResourceNode>();
            R->m_name = name;
            //R->m_Resource = std::make_shared< resource2<T> > ();
            R->m_resource = resource<T>();
            R->isAvailable = [R](){ return std::any_cast< resource<T>&>(R->m_resource).is_available(); };
            R->MakeUnavailable = [R](){ return std::any_cast< resource<T>&>(R->m_resource).make_available(); };
            m_ResourceNodes[name] = R;
            return R;
        }
    }

    /**
     * @brief requires
     * @param name
     * @return
     *
     * Indicates that the ndoe requires a resource named, name.
     */
    template<typename T>
    resource<T> requires(const std::string & name)
    {
        std::cout << "Node " << m_Node->m_name << " Requires: " << name << std::endl;

        std::shared_ptr<ResourceNode> R = create_resource<T>(name);

        m_Node->m_required.insert( R );

        R->m_next.push_back( m_Node );

        return std::any_cast< resource<T> >(R->m_resource);
    }

    template<typename T>

    /**
     * @brief produces
     * @param name
     * @return
     *
     * Registers a resource as a producer. The node will produce
     * a resource with name, name
     */
    resource<T> produces(const std::string & name)
    {
        std::cout << "Node Produces: " << name << std::endl;

        std::shared_ptr<ResourceNode> R = create_resource<T>(name);

        //R->m_resource = resource2<T>();
        R->m_name = name;
        m_Node->m_produces.insert(R);
        m_ResourceNodes[name] = R;

        return std::any_cast<resource<T>>(R->m_resource);
    }

    FrameNode_p m_Node;
    std::map<std::string, FrameNode_p >    & m_FrameNodes;
    std::map<std::string, ResourceNode_p > & m_ResourceNodes;

    //std::map<std::string, std::any> m_data;
};






class FrameGraph
{
  public:

    ~FrameGraph()
    {
        std::cout << "Destroying FrameGraph: " << std::endl;
        for(auto & a : m_FrameNodes)
        {
            std::cout << a.second->m_name << " use count = " << a.second.use_count() << std::endl;
            a.second->clear();
        }
        for(auto & a : m_ResourceNodes)
        {
            std::cout << a.second->m_name << " use count = " << a.second.use_count() << std::endl;
            a.second->clear();
        }
        m_FrameNodes.clear();
        m_ResourceNodes.clear();
    }
    template<typename T>
    void Add_Node(
            std::string const & name,
            std::function<void(T &, BlackBoard&)> init_f,
            std::function<void(T&)>               exec_f
            )
    {
        uint32_t NodeIndex = temp_counter++;
        //std::cout << "----------------------------------------------" << std::endl;
        //std::cout << "Adding node: " << name << std::endl;
        //std::cout << "----------------------------------------------" << std::endl;

        std::shared_ptr<FrameNode> N = std::make_shared<FrameNode>();
        N->m_name = name;
        N->m_NodeData = T();

        BlackBoard B(N, m_FrameNodes, m_ResourceNodes );

        init_f( std::any_cast<T&>(N->m_NodeData) , B);


        // At this point, New resources should have been
        // registered in the blackboard;
        // we need to figure out which new resources were created

        N->exec_f = [=](std::any & FrameData)
        {
            exec_f(  std::any_cast<T&>(FrameData) );
        };

        m_FrameNodes[name] = N;
       // std::cout << "----------------------------------------------" << std::endl;
    }


    void print(FrameNode_p F)
    {
        for(auto & p : F->m_produces)
        {
            std::cout << "\"" << F->m_name << "\" -> " << "\"" << p->m_name << "\"" << std::endl;;
        }
        for(auto & p : F->m_required)
        {
            std::cout << "\"" << p->m_name << "\" -> " << "\"" << F->m_name << "\"" << std::endl;;
        }
    }

    void print(ResourceNode_p F)
    {
        for(auto & p : F->m_next)
        {
            std::cout << "\"" << F->m_name << "\" -> " << "\"" << p->m_name << "\"" << std::endl;;
        }
    }

    void print()
    {
        std::cout << "digraph G {" << std::endl;
        std::cout << "   rankdir=LR"  << std::endl ;
        for(auto & F: m_FrameNodes)
        {
            print(F.second);
        }
        for(auto & F: m_FrameNodes)
        {
            std::cout << F.first << "[shape=Mdiamond]" << std::endl;;
        }
        std::cout << " subgraph cluster_3  {"
                     "     node [style=filled]; \n"
                     "     label = \"Execution Order\" \n";
                     "     color = blue\n";

        for(auto & N : m_ExecutionOrder)
        {
            std::cout << N->m_name << " -> ";
        }
        std::cout << "end\n";
        std::cout << "}" << std::endl;
    }

    void compile()
    {
        m_ExecutionOrder.clear();

        auto cmp = [](FrameNode_p & left, FrameNode_p & right)
        {
            return left->m_required.size() > right->m_required.size();
        };

        std::priority_queue<FrameNode_p, std::vector<FrameNode_p>, decltype(cmp)> q3(cmp);
        for(auto & F : m_FrameNodes)
        {
            q3.push( F.second );
        }

        while( q3.size() )
        {
            m_ExecutionOrder.push_back( q3.top() );
            q3.pop();
        }

        std::cout << "Execution Order: \n";
        for(auto & N : m_ExecutionOrder)
        {
            std::cout << N->m_name << std::endl;
        }
    }

    void execute()
    {
        /**
          When executing, the frame graph checks which node in the
          frame graph does not require any resources and then executes that.

          Once it finishes executing, it will look at the resources which that node
          produced, find which nodes require those resources and execute them
        **/
        for(auto & N : m_ExecutionOrder)
        {
           // std::cout << "Executing: " << N->m_name << std::endl;
            bool can_execute = true;
            for(auto & a : N->m_required)
            {
                if( !a->isAvailable() )
                {
                    can_execute = false;
                    //std::cout << a->m_name << " is not available" << std::endl;
                    break;
                }
               // std::cout << a->m_name << " is available" << std::endl;
            }
            if( can_execute )
            {
                N->exec_f( N->m_NodeData );
            } else {
                throw std::runtime_error("Cannot execute");
            }
        }

    }

    void reset_resources()
    {
        for(auto & R : m_ResourceNodes)
        {
            R.second->MakeUnavailable();
        }
    }


    //---------------------------------------------------

    //---------------------------------------------------

    std::map<std::string, FrameNode_p >    m_FrameNodes;
    std::map<std::string, ResourceNode_p > m_ResourceNodes;



    std::vector<FrameNode_p> m_ExecutionOrder;
    uint32_t temp_counter=0;

};

#endif
