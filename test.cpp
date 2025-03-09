#include <iostream>
#include <chrono>
#include <pthread.h>
#include <thread>

#include "threadpool.h"

using uLong = unsigned long long;

class Mytask:public Task
{
public:
    //  如何设计返回值，可以表示任意类型
    Any run(){
        std::cout<<"tid:"<<std::this_thread::get_id()<<"begin"<<std::endl;
        // std::this_thread::sleep_for(std::chrono::seconds(3));
        uLong sum=0;
        for(uLong i=begin_;i<=end_;i++){
            sum+=i;
        }
        std::cout<<"tid:"<<std::this_thread::get_id()<<"end.ans:"<<sum<<std::endl;
        return sum;
    }
    Mytask(uLong begin,uLong end):begin_(begin),end_(end){}
private:
    uLong begin_;
    uLong end_;
};
int main()
{
    std::cout<<"start main"<<std::endl;
    ThreadPool pool;
    //用户设置模式
    pool.setMode(PoolMode::MODE_CACHED);
    pool.start(4);
    //如何设计这里的Result机制呢,要等到线程执行完后才能主线程才能取走
    Result res1=pool.submitTask(std::make_shared<Mytask>(1,100000000));
    Result res2=pool.submitTask(std::make_shared<Mytask>(100000001,200000000));
    Result res3=pool.submitTask(std::make_shared<Mytask>(200000001,300000000));
    Result res4=pool.submitTask(std::make_shared<Mytask>(200000001,300000000));
    Result res5=pool.submitTask(std::make_shared<Mytask>(200000001,300000000));
    Result res6=pool.submitTask(std::make_shared<Mytask>(200000001,300000000));
    Result res7=pool.submitTask(std::make_shared<Mytask>(200000001,300000000));
    Result res8=pool.submitTask(std::make_shared<Mytask>(200000001,300000000));
    Result res9=pool.submitTask(std::make_shared<Mytask>(200000001,300000000));

    //随着task执行完毕，task对象析构，不能用tast->getResult();返回值
    uLong sum1=res1.getVal().cast_<uLong>();
    uLong sum2=res2.getVal().cast_<uLong>();
    uLong sum3=res3.getVal().cast_<uLong>();

    std::cout<<(sum1+sum2+sum3)<<std::endl;

    //ThreadPool析构，资源回收
    std::this_thread::sleep_for(std::chrono::seconds(5));

    std::cout << "main over!" << std::endl;
    getchar();
}