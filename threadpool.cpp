#include "threadpool.h"
#include <pthread.h>
#include <thread>
#include <iostream>
const int TASK_MAX_THRESHHOLD = INT32_MAX;
const int THREAD_MAX_THRESHHOLD = 100;
const int THREAD_MAX_IDLE_TIME = 10;
//******ThreadPool******//
// 线程池构造
ThreadPool::ThreadPool() : initThreadSize_(0),
                           taskSize_(0),
                           taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD),
                           poolMode_(PoolMode::MODE_FIXED),
                           isPoolRunning_(false),
                           idleThreadSize_(0),
                           threadSizeThreshHold_(THREAD_MAX_THRESHHOLD),
                           curThreadSize_(0)
{}
//析构，资源回收
ThreadPool::~ThreadPool() 
{   
    isPoolRunning_=false;
    //指针可以自动析构
    //等待线程池里所有线程返回   被阻塞的线程，正在执行的线程，线程通信
    std::unique_lock<std::mutex> lock(taskQueMtx_);

    exitCond_.wait(lock);
}

// 设置线程池模式
void ThreadPool::setMode(PoolMode mode)
{
    if(checkRunningState())return ;
    poolMode_ = mode;
}
// 设置任务队列数量上限
void ThreadPool::setTaskQueMaxThreshHold(int threshhold)
{
    if(checkRunningState())return ;
    taskQueMaxThreshHold_ = threshhold;
}
void ThreadPool::setThreadSizeThreshHold_(int threshhold)
{
    if(checkRunningState())return ;
    if(poolMode_ == PoolMode::MODE_CACHED){
        threadSizeThreshHold_ = threshhold;
    }
}

// 开启线程池
void ThreadPool::start(int initThreadSize)
{
    isPoolRunning_=true;
    // 记录初始线程个数
    initThreadSize_ = initThreadSize;
    curThreadSize_ = initThreadSize;
    // 创建线程对象，同时把线程函数bind到thread对象
    //bind
    for (int i = 0; i < initThreadSize_; i++)
    {
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
        //
        int threadId=ptr->getId();
        threads_.emplace(threadId,std::move(ptr)); // unique_ptr不允许复制，只能move
        //push_back 适用于已经有对象的情况，涉及到拷贝或移动。
        //emplace_back 和 emplace 都通过原地构造避免了不必要的拷贝和移动，前者只用于末尾，后者可用于任意位置。
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
        idleThreadSize_++;
    }
}
// 给线程池提交任务 用户调用该接口传入任务，生产任务
Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
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
            return Result(sp,false);//Task result;
            //return tast->getResult();wrong可能考虑Task里设置Result存放返回值，然后用getResult（）返回一个Result,但是此时
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
    //根据任务数量和空闲线程数量，自动添加新的线程,cached模式，任务处理比较紧急，场景：小而快的任务
    if(poolMode_ == PoolMode::MODE_CACHED
    && taskSize_ > idleThreadSize_
    && curThreadSize_ < threadSizeThreshHold_){
        std::cout<<"create new thread:"<<std::endl;
        auto ptr=std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc,this,std::placeholders::_1));
        int threadId=ptr->getId();
        threads_.emplace(threadId,std::move(ptr));
        curThreadSize_++;
        idleThreadSize_++;
    }
    // 自动释放锁
    return Result(sp);
}
/*
线程同步
线程互斥：mutex atomic（data++简单的操作）
线程通信：条件变量+信号量
*/
// 定义线程函数，从任务队列里消费任务
void ThreadPool::threadFunc(int threadid)// 线程函数执行完毕，线程也就结束了
{ 
    auto lastTime=std::chrono::high_resolution_clock().now();
    for(;;)
    {
        std::shared_ptr<Task> task;
        {
            // 获取锁
            
            std::unique_lock<std::mutex> lock(taskQueMtx_);
            std::cout<<"tid:"<<std::this_thread::get_id()<<"尝试获取任务"<<std::endl;
            //cached模式下，有线程空闲时间过长超过60s，就回收线程，线程数不少于初始线程数量
            //当前时间 - 上一次线程执行时间 大于 60s
            if(poolMode_ == PoolMode::MODE_CACHED){
                // 等待notEmpty
                //每秒返回一次 区分 超市返回和等待生产任务
                while (taskQue_.size() == 0)
                {
                    if(notEmpty_.wait_for(lock,std::chrono::seconds(1)) == std::cv_status::timeout){
                        //1s轮询
                        auto now = std::chrono::high_resolution_clock().now();
                        auto dur = std::chrono::duration_cast<std::chrono::seconds>(now-lastTime);
                        //超时60s
                        if(dur.count() >= THREAD_MAX_IDLE_TIME && curThreadSize_>initThreadSize_){
                            //回收线程
                            //把线程对象从线程容器中删除  
                            //threadid -> thread对象删除
                            curThreadSize_--;
                            threads_.erase(threadid);
                            idleThreadSize_--;
                            std::cout<<"threadid:"<<std::this_thread::get_id()<<"exit"<<std::endl;
                            return ;
                        }
                    }
                }
            }
            else{
                notEmpty_.wait(lock, [&]() -> bool{ return taskQue_.size() > 0; });
            }

            idleThreadSize_--;
            std::cout<<"tid:"<<std::this_thread::get_id()<<"获取任务成功"<<std::endl;

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
        std::cout<<"tid:"<<std::this_thread::get_id()<<"离开冲突区"<<std::endl;
        // 当前线程负责执行这个任务
        if (task != nullptr)
        {
            //task->run();//1.执行任务2.将返回值交给Result
            task->exec();
        }
        idleThreadSize_++;
        lastTime=std::chrono::high_resolution_clock().now();//更新任务执行完任务的时间
    }
    
}
bool ThreadPool::checkRunningState() const{
    return isPoolRunning_;
}



//******Thread******//
int Thread::generateId_=0;


// 线程构造
Thread::Thread(ThreadFunc func) : func_(func),threadId_(generateId_++) {
    // threadId_=generateId_;
    // generateId_++;//所有Thread对象共用
}
// 线程析构
Thread::~Thread() {}
// 启动线程
void Thread::start()
{
    // 创建一个线程执行线程函数
    std::thread t(func_,threadId_); // C++11 线程对象t和线程函数func_,分离线程对象和线程函数的执行，线程对象析构后不影响线程函数运行
    t.detach();           // 设置分离线程 pthread_detech pthread_t设置为分离线程
}
int Thread::getId() const{
    return threadId_;
}


//******Semaphore******//
Semaphore::Semaphore(int resLimit):resLimit_(resLimit){}
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


//*********Task**********//
Task::Task():result_(nullptr){}
void Task::exec(){
    if(result_!=nullptr){
        result_->setVal(run());
    }
}
void Task::setResult(Result* res){
    result_=res;
}


//******Result******//
Result::Result(std::shared_ptr<Task> task,bool isVaild):task_(task),isVaild_(isVaild)
{
    task_->setResult(this);
}
void Result::setVal(Any any){//获取到值唤醒等待这信号量上的getVal()
    this->any_=std::move(any);
    sem_.post();
}
Any Result::getVal(){//用户调用
    if(!isVaild_)return "";
    sem_.wait();//如果任务没有执行完毕，用户线程阻塞在这里，等待在信号量
    return std::move(any_);
}
