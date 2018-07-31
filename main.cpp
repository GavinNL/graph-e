#if 1

#include "framegraph2.h"

#include <memory>
#include <iostream>


// Node1 produces a resource
class Node1
{
    public:

    struct Data_t
    {
        Resource<int> x;
    };

    Node1(int x)
    {
        std::cout << "Node1 Constructed: " << x << std::endl;
    }

    ~Node1()
    {
        std::cout << "Node1 Destroyed" << std::endl;
    }

    void registerResources(Data_t & data, ResourceRegistry & G)
    {
        data.x = G.create_PromiseResource<int>("x");
    }

    void operator()(Data_t & G)
    {
        std::cout << "Node1 called" << std::endl;
        //G.x->make_available();
    }

};


class Node2
{
    public:
    struct Data_t
    {
        Resource<int> x;
    };

    Node2(int x)
    {
        std::cout << "Node2 Constructed: " << x << std::endl;
    }

    ~Node2()
    {
        std::cout << "Node2 Destroyed" << std::endl;
    }

    void registerResources(Data_t & data, ResourceRegistry & G)
    {
        data.x = G.create_FutureResource<int>("x");
    }

    void operator()(Data_t & G)
    {
        std::cout << "Node2 called" << std::endl;
    }

};



int main(int argc, char **argv)
{
    FrameGraph G;

    G.AddNode<Node1>(3); // node added and constructed
    G.AddNode<Node2>(3); // node added and constructed
   // G.AddNode<Node2>(3); // node added and constructed

    G.execute();

    return 0;
}
#else

#include <iostream>
#include "framegraph.h"

#include <cmath>

#include <future>

int future_main()
{
    std::promise<int> ready_promise;

    std::shared_future<int> ready_future(ready_promise.get_future());

    std::chrono::time_point<std::chrono::high_resolution_clock> start;

    auto fun1 = [&, ready_future]() -> std::chrono::duration<double, std::milli>
    {
        ready_future.wait(); // waits for the signal from main()
        auto d = std::chrono::high_resolution_clock::now() - start;

        std::cout << "Fun 1 received value: " << ready_future.get() << std::endl;
    };


    auto fun2 = [&, ready_future]() -> std::chrono::duration<double, std::milli>
    {
        ready_future.wait(); // waits for the signal from main()
        auto d = std::chrono::high_resolution_clock::now() - start;
        std::cout << "Fun 2 received value: " << ready_future.get() << std::endl;
    };

    auto result1 = std::async(std::launch::async, fun1);
    auto result2 = std::async(std::launch::async, fun2);


    // the threads are ready, start the clock
    start = std::chrono::high_resolution_clock::now();

    std::this_thread::sleep_for( std::chrono::seconds(2));
    // signal the threads to go
    ready_promise.set_value( 5 );
    std::this_thread::sleep_for( std::chrono::seconds(2));


    ready_promise = std::promise<int>();
    auto result3 = std::async(std::launch::async, fun2);
    ready_promise.set_value( 3 );
  // std::cout << "Thread 1 received the signal "
  //           << result1.get().count() << " ms after start\n"
  //           << "Thread 2 received the signal "
  //           << result2.get().count() << " ms after start\n";
}

int main(int argc, char ** argv)
{
//    future_main();
//    return 0;
    FrameGraph Graph;

#define TIMENODE
#define COS_NODE
#define SINE_NODE
#define ADD_NODE

#if defined TIMENODE
    struct TimeNodeData
    {
        resource<double> time; // produces
    };


    // N0
    Graph.Add_Node<TimeNodeData>
    (
            "Time",
            // The initalize Function. This is called when the
            // Node is initalized. We use this to pull resources from the BlackBoard, B, and
            // Store them in the data structure, data
            [](TimeNodeData & data, BlackBoard & B)
            {
                data.time = B.produces<double>("time");
            },

            // The Execute Function. This function is called when all resources in
            // data are available.
            [](TimeNodeData & data)
            {
                static auto T0 =std::chrono::system_clock::now();

                double t = std::chrono::duration<double>(std::chrono::system_clock::now() - T0).count();
               // std::cout << "Executing Time = " << t << std::endl;
                data.time.set(t);
                data.time.make_available();
                return true;
            }
    );
#endif

#if defined COS_NODE
    struct CosNodeData
    {
        resource<double> time; // required

        resource<double> cos; // produces
    };


    // N0
    Graph.Add_Node<CosNodeData>
    (
            "Cos",
            // The initalize Function. This is called when the
            // Node is initalized. We use this to pull resources from the BlackBoard, B, and
            // Store them in the data structure, data
            [](CosNodeData & data, BlackBoard & B)
            {
                data.time = B.requires<double>("time");
                data.cos  = B.produces<double>("cos");
            },

            // The Execute Function. This function is called when all resources in
            // data are available.
            [](CosNodeData & data)
            {
                data.cos.set( std::cos( data.time.get() ) );
               // std::cout << "Executing Cos(" << data.time.get() << ") = " << data.cos.get() << std::endl;
                data.cos.make_available();
                return true;
            }
    );
#endif

#if defined ADD_NODE
    struct AddData
    {
        resource<double> r0; // requires
        resource<double> r1; // requires
        resource<double> r2; // requires
    };

    // N2
    Graph.Add_Node<AddData>
    (
            "Add",
            // The initalize Function. This is called when the
            // Node is initalized. We use this to pull resources from the BlackBoard, B, and
            // Store them in the data structure, data
            [](AddData & data, BlackBoard & B)
            {
                data.r0 = B.requires<double>("sine");
                data.r1 = B.requires<double>("cos");
                data.r2 = B.requires<double>("time");
            },

            // The Execute Function. This function is called when all resources in
            // data are available.
            [](AddData & data)
            {
                auto v = data.r0.get()  + data.r1.get() + data.r2.get();
                std::cout << "Executing ( " <<
                             data.r0.get()  << " + " << data.r1.get() << " ) = " << v << std::endl;
                return true;
            }
    );
#endif

#if defined SINE_NODE
    struct SineNodeData
    {
        resource<double> time; // required

        resource<double> sine; // produces
    };


    // N0
    Graph.Add_Node<SineNodeData>
    (
            "Sine",
            // The initalize Function. This is called when the
            // Node is initalized. We use this to pull resources from the BlackBoard, B, and
            // Store them in the data structure, data
            [](SineNodeData & data, BlackBoard & B)
            {
                data.time   = B.requires<double>("time");
                data.sine   = B.produces<double>("sine");
            },

            // The Execute Function. This function is called when all resources in
            // data are available.
            [](SineNodeData & data)
            {
                data.sine.set( std::sin( data.time.get() ) );
                //std::cout << "Executing Sin(" << data.time.get() << ") = " << data.sine.get() << std::endl;
                data.sine.make_available();
                return true;
            }
    );
#endif

    Graph.compile();

    Graph.print();

    for(int i=0; i < 10000; i++)
    {
        Graph.execute();
        Graph.reset_resources();
    }

}
#endif
