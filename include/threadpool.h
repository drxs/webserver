/**
 * @ Author: WangYusong
 * @ E-Mail: admin@wangyusong.cn
 * @ Create Time  : 2021-08-10 19:54:18
 * @ Modified Time: 2021-08-11 09:41:08
 * @ Description  : 线程池
 */

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "locker.h"

using std::list;

template< typename T >
class threadpool{
private:
    int m_thread_number;            /* 线程池中的线程数 */
    unsigned int m_max_requests;             /* 请求队列中允许的最大线程数 */
    pthread_t* m_threads;           /* 描述线程池的数组，大小为m_thread_number */
    list< T* > m_workqueue;         /* 请求队列 */
    locker m_queuelocker;           /* 保护请求队列的互斥锁 */
    sem m_queuestat;                /* 是否有任务需要处理 */
    bool m_stop;                    /* 是否结束线程 */
public:
    threadpool( int thread_number = 8, unsigned int max_requests = 10000 );
    ~threadpool();
    bool append( T* request );      /* 向请求队列中添加任务 */

private:
    /* 工作线程运行的函数，它不断从队列中取任务并执行 */
    static void* worker( void* arg );
    void run();
};

template< typename T >
threadpool< T >::threadpool( int thread_number, unsigned int max_requests ) : 
        m_thread_number( thread_number ), m_max_requests( max_requests ),  m_threads( NULL ), m_stop( false )
{
    if( ( thread_number <= 0 ) || ( max_requests <= 0 ) ) {
        throw std::exception();
    }

    m_threads = new pthread_t[ m_thread_number ];
    
    if( ! m_threads ) {
        throw std::exception();
    }

    /* 创建thread_number个线程，并设置线程分离 */
    printf("begin --- creating threads……\n");
    for ( int i = 0; i < thread_number; ++i ) {
        if( pthread_create( m_threads + i, nullptr, worker, this ) != 0 ) {
            delete [] m_threads;        /* 出错，释放资源 */
            throw std::exception();
        }
        if( pthread_detach( m_threads[i] ) ) {
            delete [] m_threads;        /* 出错，释放资源 */
            throw std::exception();
        }
    }
    printf("ok --- %d threads created……\n",thread_number);
}

template< typename T >
threadpool< T >::~threadpool(){
    delete [] m_threads;
    m_stop = true;
}

template< typename T >
bool threadpool< T >::append( T* request ){
    /* 工作队列被所有线程共享，操作时需要加锁 */
    m_queuelocker.lock();
    if ( m_workqueue.size() > m_max_requests ) {
        /* 队列内任务数已达到上限，解锁并返回false */
        m_queuelocker.unlock();
        return false;
    }
    /* 将request添加至任务队列 */
    m_workqueue.push_back( request );
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template< typename T >
void* threadpool< T >::worker( void* arg ){
    threadpool* pool = ( threadpool* )arg;
    pool->run();
    return pool;
}

template< typename T >
void threadpool< T >::run(){
    while ( ! m_stop )
    {
        m_queuestat.wait();
        m_queuelocker.lock();
        if ( m_workqueue.empty() ) {
            /* 任务队列为空，解锁并继续循环 */
            m_queuelocker.unlock();
            continue;
        }
        /* 取出任务队列中的第一个任务 */
        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if ( ! request ) {
            continue;
        }
        request->process();
    }
}

#endif
