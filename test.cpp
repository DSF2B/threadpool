#include <iostream>

#include <chrono>
#include <pthread.h>
#include <thread>

#include "threadpool.h"
class Mytask:public Task
{
public:
    //  如何设计返回值，可以表示任意类型
    Any run(){
        std::cout<<"tid:"<<std::this_thread::get_id<<"begin"<<std::endl;
        // std::this_thread::sleep_for(std::chrono::seconds(5));
        int sum=0;
        for(int i=begin_;i<end_;i++){
            sum+=i;
        }
        std::cout<<"tid:"<<std::this_thread::get_id<<"end"<<std::endl;
        return sum;
    }
    Mytask(int begin,int end):begin_(begin),end_(end){}
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
    pool.start(4);
    //如何设计这里的Result机制呢,要等到线程执行完后才能主线程才能取走
    Result res=pool.submitTask(std::make_shared<Mytask>());


    //随着task执行完毕，task对象析构，不能用tast->getResult();返回值
    int sum=res.get().cast_<long>();
    pool.submitTask(std::make_shared<Mytask>());
    pool.submitTask(std::make_shared<Mytask>());
    getchar();
}