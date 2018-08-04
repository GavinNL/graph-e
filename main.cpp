#if 1

#include "framegraph2.h"

#include <memory>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

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
#define WAIT(ms)// std::this_thread::sleep_for(std::chrono::milliseconds(ms))

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





int main(int argc, char **argv)
{

    FrameGraph G;

    G.AddNode<Node2>(); // node added and constructed
    G.AddNode<Node0>(); // node added and constructed
    G.AddNode<Node1>(); // node added and constructed
    G.AddNode<Node4>(); // node added and constructed
    G.AddNode<Node3>(); // node added and constructed

    // Serial Execution: Nodes are executed in a single thread
    // nodes with 0 resource requirements are executed first
    {
        auto s = std::chrono::system_clock::now();
        G.ExecuteSerial();
        auto e = std::chrono::system_clock::now();

        std::cout << "Serial run: " << std::chrono::duration<double>(e-s).count() << std::endl;
    }


    // Threaded Execution: Each node is executed as part of a threadpool
    // The threadpool.  This function will block until
    // all nodes have been executed.
    G.Reset();
    {
        auto s = std::chrono::system_clock::now();
        {
            G.ExecuteThreaded(5);
            G.Wait();
        }
        auto e = std::chrono::system_clock::now();
        std::cout << "Threaded run: " << std::chrono::duration<double>(e-s).count() << std::endl;
    }
    G.print();

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

        FOUT << "Fun 1 received value: " << ready_future.get() << std::endl;
    };


    auto fun2 = [&, ready_future]() -> std::chrono::duration<double, std::milli>
    {
        ready_future.wait(); // waits for the signal from main()
        auto d = std::chrono::high_resolution_clock::now() - start;
        FOUT << "Fun 2 received value: " << ready_future.get() << std::endl;
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
  // FOUT << "Thread 1 received the signal "
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
               // FOUT << "Executing Time = " << t << std::endl;
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
               // FOUT << "Executing Cos(" << data.time.get() << ") = " << data.cos.get() << std::endl;
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
                FOUT << "Executing ( " <<
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
                //FOUT << "Executing Sin(" << data.time.get() << ") = " << data.sine.get() << std::endl;
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
