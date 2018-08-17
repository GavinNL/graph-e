# Executable Node Graph

Imagine 3 processes, A, B and C. Process B takes resource b and performs
some additional computation. Process C takes resource c and does some computation. 

A simple graph of the execution might look like this. Time is (in arbitary units) is
shown on the right.

![Alt text](https://g.gravizo.com/svg?
digraph G 
{
    { 
        rank=same 
        A [shape=box];
        0 [shape = "circle" color="white" ];
    }
    { 
        rank=same 
        B [shape=box];
        3 [shape = "circle" color="white" ];
    }

    { 
        rank=same 
        C [shape=box];
        4 [shape = "circle" color="white" ];
    }

    { 
        rank=same 
        b [shape=circle];
        1 [shape = "circle" color="white" ];
    }

    { 
    rank=same 
        c [shape=circle];
        2 [shape = "circle" color="white" ];
    }

    0->1->2->3->4

    A -> b
    A -> c

    b -> B
    c -> C
}
)

But since process A produces b first and then c. We can schedule process B to execute on a separate thread
as soon as b is created. And then when c is created we can schedule C to execute. In this case, the execution
graph my look like this:

![Alt text](https://g.gravizo.com/svg?
digraph G 
{
    { 
     rank=same 
      A [shape=box];
     0  [shape = "circle" color="white" ];
    }
    
    
    B [shape=box];
    
    { 
     rank=same 
     C [shape=box];
     3  [shape = "circle" color="white" ];
    }
    
    
    { 
     rank=same 
      b [shape=circle];
       1 [shape = "circle" color="white" ];
      
    }
    { 
     rank=same 
      c [shape=circle];
      2  [shape = "circle" color="white" ];
    }

    0->1->2->3
    
    A -> b
    A -> c
    b -> B
    c -> C

}
)

This library allows you to construct execution nodes and place them within a graph. It will make the appropriate links and schedule nodes for execution as they become available.


The above example can be seen in code here.

```C++
#include <iostream>
#include "node_graph.h"
#include "serial_executor.h"

class A
{
public:
    out_resource<int> b;
    out_resource<int> c;

    A( ResourceRegistry & G)
    {
        b = G.register_output_resource<int>("b");
        c = G.register_output_resource<int>("c");
    }
    void operator()()
    {
        b.emplace<int>(3);
        b.make_available(); // make b available to any nodes which has registered
        std::this_thread::sleep_for( std::chrono::milliseconds(500);
        c.emplace<int>(10);
        c.make_available();  // make c available to any nodes which has registered
    }
};

class B
{
public:
    in_resource<int> b;

    B( ResourceRegistry & G)
    {
        b = G.register_input_resource<int>("b");
    }
    void operator()()
    {
        std::cout << b.get() << std::endl;
    }
};

class C
{
public:
    in_resource<int> c;

    C( ResourceRegistry & G)
    {
        c = G.register_input_resource<int>("c");
    }
    void operator()()
    {
        std::cout << c.get() << std::endl;
    }
};
```

We can now set up the graph and then execute it serially.  The print() method will pring out the execution
as a Diagraph.

```C++
int main()
{
  node_graph G;
  G.add_node<A>().set_name("A");
  G.add_node<B>().set_name("B");
  G.add_node<C>().set_name("C");

  serial_executor Exec(G);
  Exec.execute();
  G.print();
  return 0;
}

```

## Thread Pool Execution

The above code can be executed using a thread pool. The only thing you have to do is provide a wrapper which allows it to schedule tasks. For example,
using gnl::thread_pool (provided), we simply have to overload the () operator to push tasks on to the queue.

```
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

int main()
{
    node_graph G;
    G.add_node<A>().set_name("A");
    G.add_node<B>().set_name("B");
    G.add_node<C>().set_name("C");

    gnl::thread_pool T(4);   // create the threadpool with 4 workers
    ThreadPoolWrapper TW(T); // create the wrapper.

    threaded_executor<ThreadPoolWrapper> Exec(G); // create the executor
    Exec.set_thread_pool(&TW); // set the threadpool wrapper

    Exec.execute(); // execute

    G.print();
    return 0;
}





    P.execute();      // execute once
    P.wait();         // must wait for all thread to finish.
```
