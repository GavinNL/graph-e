#include <iostream>
#include "graph-e/node_graph.h"
#include "graph-e/serial_executor.h"

class A
{
public:
    graphe::out_resource<int> b;
    graphe::out_resource<int> c;

    A( graphe::ResourceRegistry & G)
    {
        b = G.register_output_resource<int>("b");
        c = G.register_output_resource<int>("c");
    }
    void operator()()
    {
        std::this_thread::sleep_for( std::chrono::milliseconds(500));
        b.emplace<int>(3);
        b.make_available();
        std::this_thread::sleep_for( std::chrono::milliseconds(500));
        c.emplace<int>(10);
        c.make_available();
    }
};

class B
{
public:
    graphe::in_resource<int> b;

    B( graphe::ResourceRegistry & G)
    {
        b = G.register_input_resource<int>("b");
    }
    void operator()()
    {
        std::cout << b.get() << std::endl;
        std::this_thread::sleep_for( std::chrono::milliseconds(500));
    }
};

class C
{
public:
    graphe::in_resource<int> c;

    C( graphe::ResourceRegistry & G)
    {
        c = G.register_input_resource<int>("c");
    }
    void operator()()
    {
        std::cout << c.get() << std::endl;
        std::this_thread::sleep_for( std::chrono::milliseconds(500));
    }
};

int main()
{
  graphe::node_graph G;
  G.add_node<A>().set_name("A");
  G.add_node<B>().set_name("B");
  G.add_node<C>().set_name("C");

  G.print();

  graphe::serial_executor Exec(G);


  Exec.execute();

  G.reset();      // reset the resources making them unavailable.
                  // this must be called if you wish to execute the graph again.

  Exec.execute();

  G.print();
  return 0;
}
