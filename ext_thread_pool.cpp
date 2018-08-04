#include "thread_pool_execute_graph.h"
#include "serial_execute_graph.h"

#include <memory>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#include <sstream>
#include <iostream>
#include <iomanip>

auto global_start = std::chrono::system_clock::now();
uint32_t global_thread_count=0;

static std::chrono::microseconds time()
{
    return std::chrono::duration_cast< std::chrono::microseconds>(std::chrono::system_clock::now() - global_start);
}

thread_local uint32_t thread_number=global_thread_count++;

thread_local std::ofstream tout("thread_" + std::to_string(thread_number++));

//#define FOUT tout << std::setw(8) << time().count() << ": " << std::string( 40*thread_number, ' ')
#define FOUT std::cout << std::setw(8) << time().count() << ": " << std::string( 40*thread_number, ' ')
#define WAIT(ms) std::this_thread::sleep_for(std::chrono::milliseconds(ms))

class Node0
{
    public:

    struct Data_t
    {
        Resource<int> w;
        Resource<int> x;
    };

    void registerResources(Data_t & data, ResourceRegistry & G)
    {
        data.w = G.create_PromiseResource<int>("w");
        data.x = G.create_PromiseResource<int>("x");
    }

    void operator()(Data_t & G)
    {
        FOUT << "Node0 Start" << std::endl;

        WAIT(1000);
        G.x.set(1); G.x.make_available();

        WAIT(1000);
        G.w.set(3);
        G.w.make_available();

        FOUT << "Node0 Ended" << std::endl;
    }

};


// Node1 produces a resource
class Node1
{
    public:

    struct Data_t
    {
        Resource<int> y;
    };

    void registerResources(Data_t & data, ResourceRegistry & G)
    {
        data.y = G.create_PromiseResource<int>("y");
    }

    void operator()(Data_t & G)
    {
        FOUT << "Node1 Start" << std::endl;

        WAIT(750);
        G.y.set(2);
        G.y.make_available();

        FOUT << "Node1 End" << std::endl;
    }

};


class Node2
{
    public:
    struct Data_t
    {
        Resource<int> x;
        Resource<int> y;
        Resource<int> z;
    };

    void registerResources(Data_t & data, ResourceRegistry & G)
    {
        data.x = G.create_FutureResource<int>("x");
        data.y = G.create_FutureResource<int>("y");

        data.z = G.create_PromiseResource<int>("z");
    }

    void operator()(Data_t & G)
    {
        FOUT << "Node2 Start" << std::endl;

        WAIT(250);
        //FOUT << "  z available: " << 5 << std::endl;
        G.z.set(5);
        G.z.make_available();

        FOUT << "Node2 End" << std::endl;
    }

};


class Node3
{
    public:
    struct Data_t
    {
        Resource<int> x;
        Resource<int> w;
        Resource<int> z;
    };

    void registerResources(Data_t & data, ResourceRegistry & G)
    {
        data.x = G.create_FutureResource<int>("x");
        data.w = G.create_FutureResource<int>("w");
        data.z = G.create_FutureResource<int>("z");
    }

    void operator()(Data_t & G)
    {
        FOUT << "Node3 Start" << std::endl;
        WAIT(500);
        FOUT << "Node3 end" << std::endl;
    }

};


class Node4
{
    public:
    struct Data_t
    {
        Resource<int> w;
    };

    void registerResources(Data_t & data, ResourceRegistry & G)
    {
        data.w = G.create_FutureResource<int>("w");
    }

    void operator()(Data_t & G)
    {
        FOUT << "Node4 started" << std::endl;
        WAIT(250);
        FOUT << "Node4 ended" << std::endl;
    }

};

#include <gnl/gnl_threadpool.h>

int main(int argc, char **argv)
{
//#define USE_THREAD_POOL

#if defined USE_THREAD_POOL

    struct ThreadPoolWrapper
    {
        ThreadPoolWrapper( gnl::thread_pool & T) : m_threadpool(&T)
        {
        }
        void operator()( std::function<void(void)> & exec)
        {
            m_threadpool->push(exec);
        }
        gnl::thread_pool *m_threadpool;
    };

    gnl::thread_pool T;
    T.create_workers(3);

    ThreadPoolWrapper TW(T);

    thread_pool_execution_graph<ThreadPoolWrapper> G;

    G.set_thread_pool(&TW);
#else
    serial_execution_graph G;
#endif

    G.add_node<Node2>(); // node added and constructed
    G.add_node<Node0>(); // node added and constructed
    G.add_node<Node1>(); // node added and constructed
    G.add_node<Node4>(); // node added and constructed
    G.add_node<Node3>(); // node added and constructed

    G.print_info();

    G.execute();

    G.print_info();

#if defined USE_THREAD_POOL
    G.wait();
#endif

    G.print();
    return 0;

}
