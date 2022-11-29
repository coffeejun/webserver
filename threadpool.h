#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "locker.h"

// 线程池类，将它定义为模板类是为了代码复用，模板参数T是任务类
template <typename T>
class threadpool
{
public:
    /*thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的、等待处理的请求的数量*/
    threadpool(int thread_number = 8, int max_requests = 10000);
    ~threadpool();
    bool append(T *request);

private:
    /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
    static void *worker(void *arg);
    void run();

private:
    // 线程的数量
    int m_thread_number;

    // 描述线程池的数组，大小为m_thread_number
    pthread_t *m_threads;

    // 请求队列中最多允许的、等待处理的请求的数量
    int m_max_requests;

    // 请求队列
    std::list<T *> m_workqueue;

    // 保护请求队列的互斥锁
    locker m_queuelocker;

    // 是否有任务需要处理
    sem m_queuestat;

    // 是否结束线程
    bool m_stop;
};

template <typename T>
threadpool<T>::threadpool(int thread_number, int max_requests) : m_thread_number(thread_number), m_max_requests(max_requests),
                                                                 m_stop(false), m_threads(NULL)
{
    //线程池的数量和请求队列中的数量
    if ((thread_number <= 0) || (max_requests <= 0))
    {
        throw std::exception();
    }
    //指向线程池数组的指针
    m_threads = new pthread_t[m_thread_number];
    //没有创建成功就报错
    if (!m_threads)
    {
        throw std::exception();
    }

    //创建thread_number 个线程，并将他们设置为脱离线程。
    //线程分离状态：指定该状态，线程主动与主控线程断开关系。
    //使用pthread_exit或者线程自动结束后，其退出状态不由其他线程获取，而直接自己自动释放。网络、多线程服务器常用。
    //不会产生僵尸进程
    for (int i = 0; i < thread_number; ++i)
    {
        printf("create the %dth thread\n", i);
        //第一个参数为指向线程标识符的指针。
        //第二个参数用来设置线程属性。
        //第三个参数是线程运行函数的起始地址。
        //第四个参数是运行函数的参数。

        //为什么第三个参数一定是静态函数----->
        // C++类中的成员函数其实默认在参数中包含有一个this指针，这样成员函数才知道应该对哪个实例作用。
        //而线程函数必须接受一个void指针作为参数，所以导致了矛盾。
        //为了解决矛盾，我们可以使用static函数，它独立于实例，参数中将不会有this指针，所以可以用于打开线程。
        //将类的对象作为参数传递给该静态函数，然后在静态函数中引用这个对象，并调用其动态方法。

        if (pthread_create(m_threads + i, NULL, worker, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }

        if (pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template <typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
    m_stop = true;
}

template <typename T>
bool threadpool<T>::append(T *request) //队列中添加任务
{
    // 操作工作队列时一定要加锁，因为它被所有线程共享。
    m_queuelocker.lock();
    if (m_workqueue.size() > m_max_requests)
    {
        m_queuelocker.unlock();
        //超过我设定的最大的量了
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post(); //信号量增加
    return true;
}

template <typename T>
void *threadpool<T>::worker(void *arg)
{
    //静态函数没办法引用非静态成员
    threadpool *pool = (threadpool *)arg;
    pool->run();
    return pool;
}

template <typename T>
void threadpool<T>::run()
{

    while (!m_stop)
    {
        //信号量有值就不阻塞了，没有值就阻塞了
        m_queuestat.wait();
        //操作队列要上锁
        m_queuelocker.lock();
        if (m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }
        //获取第一个任务
        T *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if (!request)
        {
            continue;
        }
        request->process();
    }
}

#endif
