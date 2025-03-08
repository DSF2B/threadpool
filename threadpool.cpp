#include "threadpool.h"
#include <pthread.h>
#include <thread>
#include <iostream>
const int TASK_MAX_THRESHHOLD = 1024;
// 线程池构造
ThreadPool::ThreadPool() : initThreadSize_(0),
                           taskSize_(0),
                           taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD),
                           poolMode_(PoolMode::MODE_FIXED)
{
}
ThreadPool::~ThreadPool() {}

// 设置线程池模式
void ThreadPool::setMode(PoolMode mode)
{
    poolMode_ = PoolMode::MODE_FIXED;
}
// 设置任务队列数量上限
void ThreadPool::setTaskQueMaxThreshHold(int threshhold)
{
    taskQueMaxThreshHold_ = threshhold;
}
// 给线程池提交任务 用户调用该接口传入任务，生产任务
void ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
    // 获取锁
    std::unique_lock<std::mutex> lock(taskQueMtx_);
    // 线程通信，等待任务队列有空
    while (taskQue_.size() == taskQueMaxThreshHold_)
    {
        // 超时反馈
        // wait:一直等 wait_for:加上时间参数 wait_until:时间终点
        if (notFull_.wait_for(lock, std::chrono::seconds(1)) == std::cv_status::timeout)
        {
            // 等待1s条件依旧没有满足
            std::cerr << "task queue is full,submit task fail." << std::endl;
            return;
        }
        // notFull_.wait(lock);
    }
    // notFull_.wait(lock,[&]()->bool{return taskQue_.size() < taskQueMaxThreshHold_;});
    // if(!notFull_.wait_for(lock,std::chrono::seconds(1),[&]()->bool{return taskQue_.size()<taskQueMaxThreshHold_;}));
    // 如有空余，把任务放入队列
    taskQue_.emplace(sp);
    taskSize_++;
    // 通知所有等待在notEmpty上的消费者，进行唤醒
    notEmpty_.notify_all();
    // 自动释放锁
}
// 开启线程池
void ThreadPool::start(int initThreadSize)
{
    // 记录初始线程个数
    initThreadSize_ = initThreadSize;
    // 创建线程对象，同时把线程函数bind到thread对象
    //bind
    for (int i = 0; i < initThreadSize_; i++)
    {
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this));
        threads_.emplace_back(std::move(ptr)); // unique_ptr不允许复制，只能move
        /*
        unique_ptr:
            独占所有权：一个对象只能由一个 unique_ptr 拥有，无法复制（copy），但可以通过 move 转移所有权
            轻量高效：没有引用计数的额外开销
            自动释放：当 unique_ptr 离开作用域时，会自动释放其管理的对象
        shared_ptr:
            共享所有权：多个 shared_ptr 可以指向同一个对象，通过引用计数跟踪共享的实例数量
            自动释放：当最后一个 shared_ptr 离开作用域时，对象才会被释放
            支持复制：可以通过赋值或拷贝构造函数共享所有权
         */
    }
    // 启动所有线程
    for (int i = 0; i < initThreadSize_; i++)
    {
        threads_[i]->start();
    }
}
/*
线程同步
线程互斥：mutex atomic（data++简单的操作）
线程通信：条件变量+信号量
*/
// 定义线程函数，从任务队列里消费任务
void ThreadPool::threadFunc()
{ 
    for ( ; ; ) 
    {
        std::shared_ptr<Task> task;
        {
            // 获取锁
            std::cout<<"tid"<<std::this_thread::get_id<<"尝试获取任务"<<std::endl;
            std::unique_lock<std::mutex> lock(taskQueMtx_);
            // 等待notEmpty
            while (taskQue_.size() == 0)
            {
                notEmpty_.wait(lock);
            }
            std::cout<<"tid"<<std::this_thread::get_id<<"获取任务成功"<<std::endl;
            notEmpty_.wait(lock, [&]() -> bool
                           { return taskQue_.size() > 0; });
            // 从任务队列取一个任务
            task = taskQue_.front();
            taskQue_.pop();
            taskSize_--;
            //如果仍有任务剩余，通知剩余等待在NotEmpty上的消费者
            if(taskSize_>0){
                notEmpty_.notify_all();
            }
            // 取出一个任务，通知所有等待在notFull上的生产者
            notFull_.notify_all();
            // 应该释放锁，允许其他线程取任务或用户线程放任务
            //  {}定义作用域，出了作用域，锁自动释放
        }
        std::cout<<"tid"<<std::this_thread::get_id<<"离开冲突区"<<std::endl;
        // 当前线程负责执行这个任务
        if (task != nullptr)
        {
            task->run();
        }
    }
}
// 线程构造
Thread::Thread(ThreadFunc func) : func_(func) {}
// 线程析构
Thread::~Thread() {}
// 启动线程
void Thread::start()
{
    // 创建一个线程执行线程函数
    std::thread t(func_); // C++11 线程对象t和线程函数func_,分离线程对象和线程函数的执行，线程对象析构后不影响线程函数运行
    t.detach();           // 设置分离线程 pthread_detech pthread_t设置为分离线程
}

Semaphore::Semaphore(int resLimit=0):resLimit_(resLimit){}

Semaphore::~Semaphore()
{
}
void Semaphore::wait(){
    std::unique_lock<std::mutex> lock(mtx_);
    //等待信号量有资源，没有资源阻塞
    while(resLimit_==0){
        cond_.wait(lock);
    }
    // cond_.wait(lock,[&]()->bool{resLimit_>0;});
    resLimit_--;

}
//增加一个信号量
void Semaphore::post(){
    std::unique_lock<std::mutex> lock(mtx_);
    resLimit_++;
    cond_.notify_all();
}