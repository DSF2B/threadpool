#ifndef THREADPOOL_H
#define THREADROOL_H

#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <unordered_map>

class Any{
public:
    Any()=default;
    ~Any()=default;
    Any(const Any&)=delete;//拷贝构造函数声明
    Any& operator=(const Any&)=delete;//拷贝赋值操作符声明
    Any(Any&&)=default;//移动构造函数声明，用any=std::move(any)
	Any& operator=(Any&&) = default;//移动赋值操作符的声明

    //用户向Any传入某个类型的值，用模板派生类Derive接受，存到父类的指针对象中
    //模板只能写在头文件中
    template<typename T>
    Any(T data):base_(std::make_unique<Derive<T>>(data))
    {}//unique_ptr不能左值引用

    //将值返回
    template<typename T>
    T cast_(){
        //输出时将父类指针强转为派生类指针，返回模板类型的值
        Derive<T>* pt = dynamic_cast<Derive<T>*>(base_.get());
        if(pt == nullptr){
            throw "type is unmatch!";
        }
        return pt->data_;
    }
/*
void*:
    void*可以指向任意类型指针，即可以用任意指针给void*赋值
    int *a;
    void *b=a;

    void*赋值给其他类型则需要强转
    a=(int*) b;
*/
/*
C强转：Type b = (Type)a;
四种C++强转：
    static_cast:
        static_cast<type>(expression);
        基本内置数据类型之间的转换: float ft = static_cast<float>(double db=1.22);
        指针之间的转换:不同类型指针以void*作为中间参数进行转换，直接转换是不行的，void*也不能直接转换为其他类型指针：
            char* char_p;
            void* void_p=char_p;
            float* float_p=static<float*>(void_p);
        无法消除const和volatile属性、无法直接对两个不同类型的指针或引用进行转换和下行转换无类型安全检查
    const_cast:
        const_cast的作用是去除掉const或volitale属性
        只能用于同类常量的指针或引用
    reinterpret_cast:
        C++中最接近于C风格强制类型转换的一个关键字：reinterpret_cast<type_id>(expression);
        type-id和expression中必须有一个是指针或引用类型
        reinterpret_cast的第一种用途是改变指针或引用的类型
        reinterpret_cast的第二种用途是将指针或引用转换为一个整型，这个整型必须与当前系统指针占的字节数一致,或者将一个整型转换为指针或引用类型
    dynamic_cast:
        安全地向下转型
        不能用于内置基本数据类型的强制转换，并且dynamic_cast只能对指针或引用进行强制转换
        进行上行转换时，与static_cast的效果是完全一样的；进行下行转换时，dynamic_cast具有类型检查的功能，比static_cast更安全；
        并且这种情况下dynamic_cast会要求进行转换的类必须具有多态性（多态要有继承和virtual即具有虚表，直白来说就是有虚函数或虚继承的类）
*/

private:
    //基类类型
    class Base{
    public:
        virtual ~Base()=default;
    };
    //派生类类型
    template<typename T>
    class Derive : public Base{
    public:
        Derive(T data): data_(data){};
        T data_;
    };

    std::unique_ptr<Base> base_;
};
//实现一个信号量类
/*
条件变量：适用于在多个线程之间等待某个条件满足的情况，通常与互斥锁一起使用，用于避免竞态条件（Race Condition）。
信号量：适用于对共享资源的访问进行限制，可以用于实现互斥锁和线程同步。
需要注意的是，条件变量和信号量是不同的同步机制，选择使用哪个取决于具体的需求。条件变量通常用于线程之间等待和通知的情况，而信号量主要用于资源的控制和限制。
*/


class Semaphore
{
private:
    int resLimit_;
    std::mutex mtx_;
    std::condition_variable cond_;
public:
    Semaphore(int resLimit=0);
    ~Semaphore()=default;
    // 获取一个信号量
    void wait();
    void post();
};

class Task;

// 结果类
class Result
{
private:
    Any any_;//存储任务返回值
    Semaphore sem_;//线程任务通信信号量
    std::shared_ptr<Task> task_;//指向对应获取返回值的对象
    std::atomic_bool isVaild_; //任务是否正常返回了res
public:
    Result();
    Result(std::shared_ptr<Task> task,bool isVaild=true);
    ~Result()=default;
    void setVal(Any any);//获取到值唤醒等待这信号量上的getVal()
    Any getVal();//等待在信号量
};

// 任务抽象基类
class Task
{
private:
    Result *result_;//这里只能用指针，且不能用shared_ptr，因为Result里已经有Task的shared_ptr了，Result的生命周期长于Task,
public:
    Task();
    ~Task()=default;
    virtual Any run() = 0;
    void exec();
    void setResult(Result* res);
};
enum class PoolMode
{
    MODE_FIXED,  // 固定数量的线程
    MODE_CACHED, // 线程数量动态增长
};


// 线程类型
class Thread
{
public:
    // 线程函数对象
    //???????????????????
    using ThreadFunc = std::function<void(int)>;
    // 启动线程
    void start();
    // 线程构造
    Thread(ThreadFunc func);
    // 线程析构
    ~Thread();
    int getId() const;
private:
    ThreadFunc func_;
    static int generateId_;
    int threadId_;//保存线程id
};


/*
example:
ThreadPool pool;
pool.start(4);
class Mytask: public Task{
    public:
        void run(){//线程代码...}
};
pool.submitTask(std::make_shared<Mytask>());
*/
// 线程池
class ThreadPool
{
public:
    ThreadPool();
    ~ThreadPool();
    // 设置线程池模式
    void setMode(PoolMode mode);
    // 设置任务队列数量上限
    void setTaskQueMaxThreshHold(int threshhold);
    // 给线程池提交任务
    Result submitTask(std::shared_ptr<Task> sp);
    // 开启线程池
    void start(int initThreadSize = 4);

    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;
    //设置线程数量上限
    void setThreadSizeThreshHold_(int threshhold);
private:
    // 定义线程函数
    void threadFunc(int threadid);
    bool checkRunningState() const;
private:
    // std::vector<std::unique_ptr<Thread>> threads_; // 线程列表
    std::unordered_map<int ,std::unique_ptr<Thread>> threads_;
    int initThreadSize_;                           // 初始线程数量

    std::queue<std::shared_ptr<Task>> taskQue_; // 任务队列
    std::atomic_int taskSize_;                  // 任务数量
    int taskQueMaxThreshHold_;                  // 任务队列数量上限

    std::mutex taskQueMtx_;            // 保证任务队列的线程安全
    std::condition_variable notFull_;  // 表示任务队列不满
    std::condition_variable notEmpty_; // 表示任务队列不空

    PoolMode poolMode_; // 当前线程池工作模式
    //是否已经启动线程池
    std::atomic_bool isPoolRunning_;
    //空闲线程的数量
    std::atomic_int idleThreadSize_;
    int threadSizeThreshHold_;//线程上限
    //当前线程数量
    std::atomic_int curThreadSize_;
};


#endif