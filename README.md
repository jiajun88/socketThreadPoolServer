# socketThreadPoolServer
此项目基于Linux的轻量级多线程Socket服务器。底层实现了一个线程池，能够对线程进行较为高效的管理，并且使用了Select多路复用机制以进一步提高并发效率。

其主要功能包括：
1. 使用多线程的方式来增加并行的服务数量；
2. 利用Select多路复用技术进一步提高系统性能；
3. 底层实现线程池，避免频繁创建和销毁线程，提高系统性能的同时简化了主线程的操作

线程池实现的主要功能：
1. 可以在一定范围内指定线程池内线程的最小数量和最大数量；
2. 实现了任务队列机制，每当任务队列有任务时，消费者线程就会将之取出并执行任务；
3. 实现了管理者线程和消费者线程的同步，两者互相配合使得线程池高效工作；
4. 实现了管理者线程对线程池的高效管理，可以在恰当的时机创建和销毁线程；
5. 稍微修改线程池源码就可以定制化具有特定功能的线程池。

待改进的地方：
1. 由于使用了数组来存储线程ID，导致线程池的容量有一定限制，后续可以升级为链表来管理线程ID；
2. 对线程的创建和销毁策略还需进一步改进，比如引入定时机制，将长时间闲置的线程销毁掉，而不是等闲置线程数量达到一定的数量再销毁；
3. 任务队列没有优先级，并且也是以数组的形式进行管理。后续可为任务队列增加优先级属性，并且以链表的方式进行管理；
4. 线程池的拒绝策略单一，本线程池默认丢弃任务并抛出异常就直接下一个任务。后续可以尝试重新执行任务。

文件夹概述：
1. threadpool.h 和 threadpool.c分别为线程池头文件和源文件，都具有较为详细的注释，也是本人在写代码时的思路；
2. tcp_server_select_threadPool.c是使用了select多路复用技术的服务器端源代码，tcp_client_select.c是对应的客户端源代码。你应该先开启服务端，再开启多个客户端。注意，客户端代码需要你设置服务端的IP和端口，并且你可以在服务端设置线程池的最大容量；
3. 其他文件，如thread_server_select_multiThreads.c是未使用线程池的服务端源代码，而只是普通的多线程，而tcp_server.c是使用了select多路复用的单线程服务端源代码，也能实现一定数量的并发。

其他说明：
1. 本项目是在Linux环境下使用C语言进行开发；
2. 在编译服务端代码时，注意将threadpool.c一起进行编译；
3. 为方便测试，客户端和服务端都可设为在本地运行。

作者：jiajunXiong
转载请注明出处。

This project is based on a lightweight multi-threaded Socket server on Linux. It implements a thread pool at the lower level for efficient thread management and utilizes the Select multiplexing mechanism to further enhance concurrency efficiency.

Its main functionalities include:

1.Increasing the parallel service capacity using a multi-threaded approach.

2.Enhancing system performance through the utilization of the Select multiplexing technique.

3.Implementing a thread pool at the underlying level to avoid frequent thread creation and destruction, thus improving system performance and simplifying main thread operations.

The key features of the thread pool implementation are as follows:

Flexibly specifying the minimum and maximum thread counts within the thread pool.

1.Implementing a task queue mechanism where consumer threads retrieve and execute tasks whenever there are tasks in the queue.

2.Synchronization between manager threads and consumer threads, ensuring efficient operation of the thread pool.

3.Efficient management of the thread pool by the manager threads, dynamically creating and destroying threads as needed.

4.The thread pool's functionality can be customized with minor modifications to its source code.

Areas for improvement include:

1.Using an array to store thread IDs limits the thread pool's capacity. Consider upgrading to using a linked list for thread ID management.

2.Further refining thread creation and destruction strategies, such as introducing a timing mechanism to promptly discard long-idle threads rather than waiting until a certain number of idle threads are reached.

3.The task queue lacks priority and is managed as an array. Consider adding a priority attribute to the task queue and managing it using a linked list for improved efficiency.

4.The thread pool's rejection strategy is currently limited. It discards tasks and throws exceptions. Consider attempting to re-execute rejected tasks.

Summary of the provided files:

1.threadpool.h and threadpool.c are the header and source files for the thread pool, respectively. They both include detailed comments reflecting the thought process during coding.

2.tcp_server_select_threadPool.c is the source code for the server using the Select multiplexing technique. tcp_client_select.c is the corresponding client source code. Clients should be started after the server. Note that the client code requires setting the server's IP, port, and the maximum capacity of the thread pool.

3.Other files, such as thread_server_select_multiThreads.c, contain source code for the server without using the thread pool. tcp_server.c contains the source code for a single-threaded server using the Select multiplexing technique, capable of handling a limited level of concurrency.

Additional notes:

1.This project is developed using the C language on the Linux environment.

2.When compiling the server code, ensure that threadpool.c is compiled together.

3.For ease of testing, both client and server can be run locally.


Author: jiajunXiong. Please provide proper attribution if reposting.
