#include <iostream>
#include "graph-e/node_graph.h"
#include "graph-e/threaded_executor.h"

#include "gnl/gnl_threadpool.h"

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
    in_resource<int> b;

    B( ResourceRegistry & G)
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
    in_resource<int> c;

    C( ResourceRegistry & G)
    {
        c = G.register_input_resource<int>("c");
    }
    void operator()()
    {
        std::cout << c.get() << std::endl;
        std::this_thread::sleep_for( std::chrono::milliseconds(500));
    }
};

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

int main()
{
  node_graph G;

  G.add_node<A>().set_name("A");
  G.add_node<B>().set_name("B");
  G.add_node<C>().set_name("C");

  G.print();

  gnl::thread_pool T(4);   // create the threadpool with 4 workers
  ThreadPoolWrapper TW(T); // create the wrapper.

  threaded_executor<ThreadPoolWrapper> Exec(G); // create the executor
  Exec.set_thread_pool(&TW); // set the threadpool wrapper

  Exec.execute(); // execute
  Exec.wait();  // If using a threadpool, we must call wait() to wait until all the threads have executed.

  G.reset();      // reset the resources making them unavailable.
                  // this must be called if you wish to execute the graph again.

  Exec.execute(); // execute
  Exec.wait();  // If using a threadpool, we must call wait() to wait until all the threads have executed.

  G.print();

  return 0;
}
