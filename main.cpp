

#include <memory>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#include <sstream>
#include <iostream>
#include <iomanip>


//==============================================================================
// Uncomment this line to use in threaded mode
//==============================================================================
// #define USE_THREAD_POOL
//==============================================================================

// the main header to include
#include "node_graph.h"

// Serial executor to execute the graph on a single thread.
#include "serial_executor.h"

// Threaded executor to execute the graph in threaded mode.
// A threadpool class should be used. This example uses
// gnl_threadpool
#if defined USE_THREAD_POOL
 #include "gnl/gnl_threadpool.h"
 #include "threaded_executor.h"
#endif


#define WAIT(ms) std::this_thread::sleep_for(std::chrono::milliseconds(ms))

#define FOUT std::cout

struct SpecialResource
{
    ~SpecialResource()
    {
       std::cout << "Special Resource::Destroyed"    << std::endl;
    }
};

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

    out_resource<int> w;
    out_resource<int> x;

    out_resource<SpecialResource> z;
    /**
     * @brief Node0
     * @param G
     *
     * This constructor is called when the node is added to the graph.
     * We will use this to create promises and future resources.
     *
     * The first parameter must be ResourceRegistry&
     *
     * A promise resource indicates that this Node will produce
     * the resource by the time it finishes executing.
     *
     * A Future resource is a required resource. This resoruce will
     * become available when another node has created it.
     */
    Node0(ResourceRegistry & G)
    {
        w = G.register_output_resource<int>("w");
        x = G.register_output_resource<int>("x");

    }


    /**
     * @brief operator ()
     * When a node is ready to execute, it will call the () operator
     *
     * At this point all required resources should be met.
     */
    void operator()()
    {
        FOUT << "Node0 Start" << std::endl;

        WAIT(1000);
        x.set(1);
        x.make_available(); // make x available
                              // at this point, if running in threaded mode, will
                              // schedule any nodes that only depend on x to run.
        FOUT <<  "x available" << std::endl;

        WAIT(1000);
        w.set(3);
        w.make_available(); // make w available.
        FOUT <<  "w available" << std::endl;

        FOUT << "Node0 Ended" << std::endl;
    }

};


// Node1 produces a resource
class Node1
{
    public:

    out_resource<int> y;

    Node1(ResourceRegistry & G)
    {
        // Node1 will promise to create resource y
        y = G.register_output_resource<int, resource_flags::permanent>("y");
    }

    void operator()()
    {
        FOUT << "Node1 Start" << std::endl;

        WAIT(750);
        y.emplace(2);
        y.make_available();
        FOUT <<  "y available" << std::endl;

        FOUT << "Node1 End" << std::endl;
    }

};


class Node2
{
    public:

    in_resource<int> x;
    in_resource<int> y;
    out_resource<int> z;

    Node2(ResourceRegistry & G)
    {
        // Node 2 requires x and y to execute.
        // This means, Node 2 will only be scheduled once
        // node 0 has created x and node 1 has created y
        x = G.register_input_resource<int>("x"); // created by node 0
        y = G.register_input_resource<int, resource_flags::permanent>("y"); // created by node 1

        // given x and y, Node2 will produce z
        z = G.register_output_resource<int>("z");
    }

    void operator()()
    {
        FOUT << "Node2 Start" << std::endl;

        WAIT(250);
        //FOUT << "  z available: " << 5 << std::endl;
        z.set(3);
        z.make_available();
        FOUT <<  "z available" << std::endl;

        FOUT << "Node2 End" << std::endl;
    }

};


class Node3
{
    public:

    in_resource<int> x;
    in_resource<int> w;
    in_resource<int> z;


    Node3( ResourceRegistry & G)
    {
        // Node 3 requires 3 resources. It does not produce any
        x = G.register_input_resource<int>("x");
        w = G.register_input_resource<int>("w");
        z = G.register_input_resource<int>("z");
    }

    void operator()()
    {
        FOUT << "Node3 Start" << std::endl;
        WAIT(500);
        FOUT << "Node3 end" << std::endl;
    }

};


class Node4
{
    public:

    in_resource<int> w;


    Node4(ResourceRegistry & G)
    {
        // node 4 needs 1 resource, w.
        w = G.register_input_resource<int>("w");
    }

    void operator()()
    {
        FOUT << "Node4 started" << std::endl;
        WAIT(250);
        FOUT << "Node4 ended" << std::endl;
    }

};



int main(int argc, char **argv)
{


    node_graph G;

    // Ade nodes to the graph. Nodes can be added in any order
    // as long as they clearly define their resource requirements.

    G.add_node<Node2>().set_name("Node_2"); // node added and constructed
    G.add_node<Node0>().set_name("Node_0"); // node added and constructed
    G.add_oneshot_node<Node1>().set_name("Node_1"); // node added and constructed
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
    gnl::thread_pool T(4);
    ThreadPoolWrapper TW(T);

    threaded_executor<ThreadPoolWrapper> P(G);
    P.set_thread_pool(&TW);


    P.execute();      // execute once
    P.wait();         // must wait for all thread to finish.
    G.reset();        // Reset. All resources are reset, except for permenant resources


    P.execute();      // execute again
    P.wait();         // must wait for all thread to finish.
#else

    serial_executor P(G);

    P.execute();    // execute once
    G.reset();      // Reset. All resources are reset, except for permenant resources
    P.execute();    // execute again

#endif

    // Pring the graph is dot format.
    // Copy and paste the output in a dot renderer: http://www.webgraphviz.com/
    G.print();
    return 0;

}
