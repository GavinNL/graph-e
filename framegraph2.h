#pragma once
#ifndef FRAME_GRAPH_2_H
#define FRAME_GRAPH_2_H

#include <memory>
#include <future>

#include <map>
#include <vector>

#include <any>

#include <iostream>

#if 0
class ResourceBus
{
public:
    std::map<std::string, std::shared_ptr<ResourceBase> >            & m_resources;

    // list of resources that must be available
    // before we trigger the Node's exec function
    std::vector< std::shared_ptr<ResourceBase> > m_list;
    std::function<void(void)> m_exec;

    ResourceBus(std::map<std::string, std::shared_ptr<ResourceBase> > & resources ) : m_resources(resources)
    {

    }

    ~ResourceBus()
    {
        std::cout << "Destroying Resource Bus" << std::endl;
    }

    void clear()
    {
        m_list.clear();
        m_exec = std::function<void(void)>();
    }

    template<typename T>
    Resource_p<T> create_PromiseResource(const std::string & name)
    {
        std::cout << "Creating Promise: " << name <<std::endl;
        if( m_resources.count(name) == 0 )
        {
            std::cout << "  " << name << " not created. Creating now" << std::endl;
            Resource_p<T> X = std::make_shared< Resource<T> >();
            m_resources[name] = X;
            return X;
        }
        else
        {
            std::cout << "  " << name << " already created." << std::endl;
            Resource_p<T> X = std::dynamic_pointer_cast< Resource<T> >(m_resources[name]);
            m_list.push_back(X);

            return X;
        }

        auto X = std::make_shared< Resource<T> >();

        m_resources[name] = X;

        return X;
    }

    template<typename T>
    Resource_p<T> create_FutureResource(const std::string & name)
    {
        std::cout << "Creating Future: " << name <<std::endl;
        if( m_resources.count(name) == 0 )
        {
            std::cout << "  " << name << " not created. Creating now" << std::endl;
            Resource_p<T> X = std::make_shared< Resource<T> >();
            m_resources[name] = X;
            m_list.push_back(X);
        }
        else
        {
            std::cout << "  " << name << " already created." << std::endl;
            Resource_p<T> X = std::dynamic_pointer_cast< Resource<T> >(m_resources[name]);
            m_list.push_back(X);

            return X;
        }
    }

    bool check()
    {
        std::cout << "Checking resources: " << m_list.size() << std::endl;
        if( m_list.size() == 0)
        {
            m_exec();
        }
        for(auto & r : m_list)
        {
            if( r->is_available )
            {
                m_exec();
            }
        }
    }
};

#endif



class ExecNode;
class ResourceNode;
using ExecNode_p = std::shared_ptr<ExecNode>;
using ResourceNode_p = std::shared_ptr<ResourceNode>;


class ExecNode
{
public:
    std::any m_NodeClass;
    std::any m_NodeData;

    // a list of required resources
    std::vector<ResourceNode_p> m_requiredResources;

    std::function<void(void)> execute; // excutes' the node's execute() method


  //  std::function<void(void)> trigger;
    uint32_t m_resourceCount = 0;

    void trigger()
    {
        ++m_resourceCount;
        //std::cout << ""
        if( m_resourceCount >= m_requiredResources.size())
        {
            std::cout << "Triggered" << std::endl;
            execute();
        }
    }
};


class ResourceNode
{
public:
    std::any                resource;
    std::string             name;
    std::vector<ExecNode_p> m_Nodes; // list of nodes that must be triggered
                                     // when resource becomes availabe

    bool is_available = false;
};

template<typename T>
class Resource
{
public:
    ResourceNode_p m_node;
    T & get()
    {
        return std::any_cast<T&>(m_node->resource);
    }

    void make_available()
    {
        m_node->is_available = true;
        for(auto & n : m_node->m_Nodes)
        {
            n->trigger();
        }
    }

    operator T&()
    {
        return std::any_cast<T&>(m_node->resource);
    }

    void set(T const & x)
    {
        std::any_cast<T&>(m_node->resource) = x;
    }
};

template<typename T>
using Resource_p = std::shared_ptr< Resource<T> >;

class ResourceRegistry
{
    std::map<std::string, ResourceNode_p> & m_resources;
    std::vector<ResourceNode_p> & m_required_resources;
    ExecNode_p & m_Node;

    public:
        ResourceRegistry( ExecNode_p & node,
                          std::map<std::string, ResourceNode_p> & m,
                          std::vector<ResourceNode_p> & required_resources) :
            m_Node(node),
            m_resources(m),
            m_required_resources(required_resources)
        {

        }

        template<typename T>
        Resource<T> create_PromiseResource(const std::string & name)
        {
            //std::cout << "Creating Promise: " << name <<std::endl;
            if( m_resources.count(name) == 0 )
            {
                //std::cout << "  " << name << " not created. Creating now" << std::endl;

                ResourceNode_p RN = std::make_shared< ResourceNode >();
                RN->resource      = std::make_any<T>();
                RN->name          = name;
                m_resources[name] = RN;

                Resource<T> r;
                r.m_node = RN;

                return r;
            }
            else
            {
                //std::cout << "  " << name << " already created." << std::endl;

                Resource<T> r;
                r.m_node = m_resources.at(name);
                return r;
            }
        }

        template<typename T>
        Resource<T> create_FutureResource(const std::string & name)
        {
            //std::cout << "Creating Future: " << name <<std::endl;
            if( m_resources.count(name) == 0 )
            {
                //std::cout << "  " << name << " not created. Creating now" << std::endl;
                Resource_p<T> X = std::make_shared< Resource<T> >();

                ResourceNode_p RN = std::make_shared<ResourceNode>();
                RN->resource      = X;
                RN->m_Nodes.push_back(m_Node);
                RN->name = name;
                m_resources[name] = RN;

                Resource<T> r;
                r.m_node = RN;

                m_required_resources.push_back(RN);

                return r;
            }
            else
            {
                //std::cout << "  " << name << " already created." << std::endl;

                ResourceNode_p RN = m_resources[name];
                RN->m_Nodes.push_back(m_Node);

                Resource<T> r;
                r.m_node = RN;

                m_required_resources.push_back(RN);

                return r;
            }
        }
};


class FrameGraph
{
public:
    FrameGraph() {}
    ~FrameGraph()
    {
        //std::cout << "Destroying Framegraph " << std::endl;


    }

    FrameGraph( FrameGraph const & other)
    {
        if( this != & other)
        {

        }
    }

    FrameGraph( FrameGraph && other)
    {
    }

    FrameGraph & operator = ( FrameGraph const & other)
    {
        if( this != & other)
        {

        }
        return *this;
    }

    FrameGraph & operator = ( FrameGraph && other)
    {
        if( this != & other)
        {

        }
        return *this;
    }

    template<typename _Tp, typename... _Args>
      inline void
      AddNode(_Args&&... __args)
      {
          typedef typename std::remove_const<_Tp>::type Node_t;
          typedef typename Node_t::Data_t               Data_t;

          ExecNode_p N   = std::make_shared<ExecNode>();

          N->m_NodeClass = std::make_any<Node_t>( std::forward<_Args>(__args)...);
          N->m_NodeData  = std::make_any<Data_t>();

          ExecNode* rawp = N.get();
          N->execute = [rawp]()
          {
              std::any_cast< Node_t&>( rawp->m_NodeClass )(   std::any_cast< Data_t&>( rawp->m_NodeData ) );
          };

          ResourceRegistry R(N,m_resources,
                             N->m_requiredResources);

          std::any_cast< Node_t&>(N->m_NodeClass).registerResources( std::any_cast< Data_t&>( rawp->m_NodeData ), R);

          m_execNodes.push_back(N);
      }


    void execute()
    {
        std::vector<ExecNode_p> zero_resources;
        for(auto & N : m_execNodes)
        {
            //std::cout << "                 Available Resources: " << N->m_resourceCount << std::endl;
            if( N->m_requiredResources.size() == 0)
            {
                zero_resources.push_back(N);
                //std::cout << "====== Executing: Required Resources: " << N->m_requiredResources.size() << std::endl;
            }
        }
        for(auto & N : zero_resources)
        {
            N->execute();
        }
    }

    std::vector< ExecNode_p > m_execNodes;
    std::map<std::string, ResourceNode_p> m_resources;

};

#endif
