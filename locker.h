#ifndef LOCKER_H //头文件名称全部字符的大写
#define LOCKER_H

#include <exception>   //异常
#include <pthread.h>   //线程
#include <semaphore.h> //信号量

// 线程同步机制封装类

// 互斥锁类
class locker
{
public:
    //构造函数
    locker()
    {
        //第二个参数为NULL，互斥锁的属性会设置为默认属性
        if (pthread_mutex_init(&m_mutex, NULL) != 0)
        {
            throw std::exception();
        }
    }
    //析构函数
    ~locker()
    {
        pthread_mutex_destroy(&m_mutex);
    }
    //上锁
    bool lock()
    {
        return pthread_mutex_lock(&m_mutex) == 0;
    }
    //解锁
    bool unlock()
    {
        return pthread_mutex_unlock(&m_mutex) == 0;
    }
    //获取成员
    pthread_mutex_t *get()
    {
        return &m_mutex;
    }

private:
    pthread_mutex_t m_mutex; //互斥锁的对象
    //互斥锁的类型是pthread_mutex_t
};

// 条件变量类
//如果想要实现在一个线程中需要一直等待某种条件被满足的时候，该线程才会进行处理，这个时候可以使用条件变量的方式来实现
//一个线程中进行wait，另一线程中当条件满足时发出通知notify，这样就不需要一直进行while循环进行判断条件了
//条件等待pthread_cond_wait() 和 计时等待pthread_cond_timedwait()，
//无论哪种等待方式，都必须和一个互斥锁配合，以防止多个线程同时请求pthread_cond_wait()（或pthread_cond_timedwait()，下同）的竞争条件（Race Condition）。
class cond
{
public:
    cond()
    {
        if (pthread_cond_init(&m_cond, NULL) != 0)
        {
            throw std::exception();
        }
    }
    ~cond()
    {
        pthread_cond_destroy(&m_cond);
    }

    bool wait(pthread_mutex_t *m_mutex)
    {
        int ret = 0;
        ret = pthread_cond_wait(&m_cond, m_mutex);
        return ret == 0;
    }
    bool timewait(pthread_mutex_t *m_mutex, struct timespec t)
    {
        int ret = 0;
        ret = pthread_cond_timedwait(&m_cond, m_mutex, &t);
        return ret == 0;
    }
    bool signal() //唤醒一个或者多个线程
    {
        return pthread_cond_signal(&m_cond) == 0;
    }
    bool broadcast() //唤醒所有线程
    {
        return pthread_cond_broadcast(&m_cond) == 0;
    }

private:
    pthread_cond_t m_cond; //条件变量
};

// 信号量类
// int sem_init(sem_t *sem, int pshared, unsigned int value); // 创建信号量
// int sem_post(sem_t *sem);                                  // 信号量的值加 1
// int sem_wait(sem_t *sem);                                  // 信号量的值减 1
// int sem_destroy(sem_t *sem);                               // 信号量销毁

class sem
{
public:
    sem()
    {
        if (sem_init(&m_sem, 0, 0) != 0)
        {
            throw std::exception();
        }
    }
    sem(int num)
    {
        if (sem_init(&m_sem, 0, num) != 0)
        {
            throw std::exception();
        }
    }
    ~sem()
    {
        sem_destroy(&m_sem);
    }
    // 等待信号量
    bool wait()
    {
        return sem_wait(&m_sem) == 0;
    }
    // 增加信号量
    bool post()
    {
        return sem_post(&m_sem) == 0;
    }

private:
    sem_t m_sem;
};

#endif