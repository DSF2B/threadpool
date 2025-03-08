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
        std::cout<<"tid:"<<std::this_thread::get_id<<"begin"<<std::endl;
        // std::this_thread::sleep_for(std::chrono::seconds(5));
        uLong sum=0;
        for(uLong i=begin_;i<=end_;i++){
            sum+=i;
        }
        std::cout<<"tid:"<<std::this_thread::get_id<<"end"<<std::endl;
        return sum;
    }
    Mytask(uLong begin,uLong end):begin_(begin),end_(end){}
private:
    int begin_;
    int end_;
};
int main()
{
/*
shared_ptr:
    
*/
    ThreadPool pool;
    pool.setMode(PoolMode::MODE_CACHED);
    pool.start(4);
    //用户设置模式

    //如何设计这里的Result机制呢,要等到线程执行完后才能主线程才能取走
    Result res1=pool.submitTask(std::make_shared<Mytask>(1,100000000));
    Result res2=pool.submitTask(std::make_shared<Mytask>(100000001,200000000));
    Result res3=pool.submitTask(std::make_shared<Mytask>(200000001,300000000));
    //随着task执行完毕，task对象析构，不能用tast->getResult();返回值
    uLong sum1=res1.getVal().cast_<uLong>();
    uLong sum2=res2.getVal().cast_<uLong>();
    uLong sum3=res3.getVal().cast_<uLong>();

    std::cout<<(sum1+sum2+sum3)<<std::endl;
    getchar();
}