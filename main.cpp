
#include <iostream>
#include "framegraph.h"

#include <cmath>

int main(int argc, char ** argv)
{
    FrameGraph2 Graph;

#define TIMENODE
#define COS_NODE
#define SINE_NODE
#define ADD_NODE

#if defined TIMENODE
    struct TimeNodeData
    {
        resource2<double> time; // produces
    };


    // N0
    Graph.Add_Node<TimeNodeData>
    (
            "Time",
            // The initalize Function. This is called when the
            // Node is initalized. We use this to pull resources from the BlackBoard, B, and
            // Store them in the data structure, data
            [](TimeNodeData & data, Blackboard2 & B)
            {
                data.time = B.produces<double>("time");
            },

            // The Execute Function. This function is called when all resources in
            // data are available.
            [](TimeNodeData & data)
            {
                std::cout << "Executing Time = 0.5 " << std::endl;
                data.time.set(0.5);
                data.time.make_available();

            }
    );
#endif

#if defined COS_NODE
    struct CosNodeData
    {
        resource2<double> time; // required

        resource2<double> cos; // produces
    };


    // N0
    Graph.Add_Node<CosNodeData>
    (
            "Cos",
            // The initalize Function. This is called when the
            // Node is initalized. We use this to pull resources from the BlackBoard, B, and
            // Store them in the data structure, data
            [](CosNodeData & data, Blackboard2 & B)
            {
                data.time = B.requires<double>("time");
                data.cos  = B.produces<double>("cos");
            },

            // The Execute Function. This function is called when all resources in
            // data are available.
            [](CosNodeData & data)
            {
                data.cos.set( std::cos( data.time.get() ) );
                std::cout << "Executing Cos(" << data.time.get() << ") = " << data.cos.get() << std::endl;
                data.cos.make_available();
            }
    );
#endif

#if defined ADD_NODE
    struct AddData
    {
        resource2<double> r0; // requires
        resource2<double> r1; // requires
        resource2<double> r2; // requires
    };

    // N2
    Graph.Add_Node<AddData>
    (
            "Add",
            // The initalize Function. This is called when the
            // Node is initalized. We use this to pull resources from the BlackBoard, B, and
            // Store them in the data structure, data
            [](AddData & data, Blackboard2 & B)
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
            }
    );
#endif

#if defined SINE_NODE
    struct SineNodeData
    {
        resource2<double> time; // required

        resource2<double> sine; // produces
    };


    // N0
    Graph.Add_Node<SineNodeData>
    (
            "Sine",
            // The initalize Function. This is called when the
            // Node is initalized. We use this to pull resources from the BlackBoard, B, and
            // Store them in the data structure, data
            [](SineNodeData & data, Blackboard2 & B)
            {
                data.time   = B.requires<double>("time");
                data.sine   = B.produces<double>("sine");
            },

            // The Execute Function. This function is called when all resources in
            // data are available.
            [](SineNodeData & data)
            {
                data.sine.set( std::sin( data.time.get() ) );
                std::cout << "Executing Sin(" << data.time.get() << ") = " << data.sine.get() << std::endl;
                data.sine.make_available();
            }
    );
#endif

    Graph.compile();

    Graph.print();

    Graph.execute();
}
