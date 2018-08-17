# Executable Node Graph

Contruct an executable graph which with resource

# Simple Example

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
