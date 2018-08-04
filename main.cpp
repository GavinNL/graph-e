#include "threaded_executor.h"
#include "serial_executor.h"

#include <memory>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

// Thread pool class from
#include <gnl/gnl_threadpool.h>

#include <sstream>
#include <iostream>
#include <iomanip>


uint32_t global_thread_count=0;

static uint64_t time()
{
    static auto global_start = std::chrono::system_clock::now();
    return std::chrono::duration_cast< std::chrono::microseconds>(std::chrono::system_clock::now() - global_start).count();
}

//#define FOUT tout << std::setw(8) << time().count() << ": " << std::string( 40*thread_number, ' ')

#define WAIT(ms) std::this_thread::sleep_for(std::chrono::milliseconds(ms))


/**
  If USE_FILE_OUTPUT is defined. Each thread will print to it's own file. To visualize the flow of the
  node calls. Use the following command from bash

  cat thread_* | sort -n

  This will concatentate all the logs into a file to visualize the thread execution. The time
  stamps are in microseconds

  //------
       2:                                                Node1 Start
       4:        Node0 Start
  750106:                                                y available
  750129:                                                Node1 End
 1000104:        x available
 1000184:                                                                                        Node2 Start
 1250266:                                                                                        z available
 1250288:                                                                                        Node2 End
 2000226:        w available
 2000228:                                                Node4 started
 2000247:                                                                                        Node3 Start
 2000250:        Node0 Ended
 2250342:                                                Node4 ended
 2500336:                                                                                        Node3 end

  //------
  **/
#define USE_FILE_OUTPUT

#if defined USE_FILE_OUTPUT
thread_local uint32_t thread_number = global_thread_count++;
thread_local std::ofstream tout("thread_" + std::to_string(thread_number++));
#define FOUT tout << std::setw(8) << time() << ": " << std::string( 40*thread_number, ' ')
#else
#define FOUT std::cout << std::setw(8) << time() << ": " << std::string( 40*thread_number, ' ')
#endif


/**
 * @brief The Node0 class
 *
 * Node 0 is an execution node which will perofrm some kind of computation
 *
 * Each node must define a sub struction called Data_t, which will
 * be used to hold all the resources required for the node
 */
class Node0
{
    public:

    struct Data_t
    {
        // Node 0 will produce a resource named w and a resource named x.
        Resource<int> w;
        Resource<int> x;
    };

    /**
     * @brief registerResources
     * @param data
     * @param G
     * This function is called when the node is added to the graph.
     * We will use this to create promises and future resources.
     *
     * A promise resource indicates that this Node will produce
     * the resource by the time it finishes executing.
     *
     * A Future resource is a required resource. This resoruce will
     * become available when another node has created it.
     */
    void registerResources(Data_t & data, ResourceRegistry & G)
    {
        // Node 0 will promise to create 2 int resources named w and x.
        data.w = G.create_PromiseResource<int>("w");
        data.x = G.create_PromiseResource<int>("x");
    }

    void operator()(Data_t & G)
    {
        FOUT << "Node0 Start" << std::endl;

        WAIT(1000);
        G.x.set(1);
        G.x.make_available(); // make x available
                              // at this point, if running in threaded mode, will
                              // schedule any nodes that only depend on x to run.
        FOUT <<  "x available" << std::endl;

        WAIT(1000);
        G.w.set(3);
        G.w.make_available(); // make w available.
        FOUT <<  "w available" << std::endl;

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
        // Node1 will promise to create resource y
        data.y = G.create_PromiseResource<int>("y");
    }

    void operator()(Data_t & G)
    {
        FOUT << "Node1 Start" << std::endl;

        WAIT(750);
        G.y.set(2);
        G.y.make_available();
        FOUT <<  "y available" << std::endl;

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
        // Node 2 requires x and y to execute.
        // This means, Node 2 will only be scheduled once
        // node 0 has created x and node 1 has created y
        data.x = G.create_FutureResource<int>("x"); // created by node 0
        data.y = G.create_FutureResource<int>("y"); // created by node 1

        // given x and y, Node2 will produce z
        data.z = G.create_PromiseResource<int>("z");
    }

    void operator()(Data_t & G)
    {
        FOUT << "Node2 Start" << std::endl;

        WAIT(250);
        //FOUT << "  z available: " << 5 << std::endl;
        G.z.set(3);
        G.z.make_available();
        FOUT <<  "z available" << std::endl;

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
        // Node 3 requires 3 resources. It does not produce any
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
        // node 4 needs 1 resource, w.
        data.w = G.create_FutureResource<int>("w");
    }

    void operator()(Data_t & G)
    {
        FOUT << "Node4 started" << std::endl;
        WAIT(250);
        FOUT << "Node4 ended" << std::endl;
    }

};



int main(int argc, char **argv)
{
#define USE_THREAD_POOL

    node_graph G;

    // Ade nodes to the graph. Nodes can be added in any order
    // as long as they clearly define their resource requirements.

    G.add_node<Node2>().set_name("Node_2"); // node added and constructed
    G.add_node<Node0>().set_name("Node_0"); // node added and constructed
    G.add_node<Node1>().set_name("Node_1"); // node added and constructed
    G.add_node<Node4>().set_name("Node_4"); // node added and constructed
    G.add_node<Node3>().set_name("Node_3"); // node added and constructed


#if defined USE_THREAD_POOL

    /**
     * @brief The ThreadPoolWrapper struct
     * We need to use a wrapper for the thread pool to
     * take a function<void(void)> object and
     * push it onto the threadpool to allow it to
     * execute
     */
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

    // Create an instance of the threadpool we want to use
    // and wrap our ThreadPoolWrapper around it.
    //
    // It is the developers responsibility to make sure
    // the threadpool does not get destroyed before the executor
    gnl::thread_pool T(3);
    ThreadPoolWrapper TW(T);

    threaded_executor<ThreadPoolWrapper> P(G);
    P.set_thread_pool(&TW);

#else
    serial_execute P(G);
#endif

    // Execute the graph. If using the serial execute this will block
    // if using the threadpool
    P.execute();


    #if defined USE_THREAD_POOL
    P.wait();
    #endif

    //==================================

    // Pring the graph is dot format.
    // Copy and paste the output in a dot renderer: http://www.webgraphviz.com/
    G.print();
    return 0;

}
