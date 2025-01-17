#pragma once

#include <thread>
#include <mutex>
#include <vector>
#include <functional>

#include "semaphore.h"

class Workers final {
  public:
    Workers();
    ~Workers();

    static Workers& inst();

    template<class T,class F>
    static void parallelFor(std::vector<T>& data,size_t maxTh,F func) {
      inst().runParallelFor(data,maxTh,func);
      }

    template<class T,class F>
    void runParallelFor(std::vector<T>& data,size_t maxTh,F func) {
      std::lock_guard<std::mutex> guard(sync);(void)guard;
      workSet     = reinterpret_cast<uint8_t*>(data.data());
      workSize    = data.size();
      workEltSize = sizeof(T);

      workFunc = [&func](void* data){ func(*reinterpret_cast<T*>(data)); };

      if(maxTh>MAX_THREADS)
        workTasks = MAX_THREADS; else
        workTasks = maxTh;

      for(size_t i=0;i<workTasks;++i)
        workInc[i].release(1);
      workDone.acquire(workTasks);
      }

  private:
    enum { MAX_THREADS=16 };

    void threadFunc(size_t id);

    std::thread th     [MAX_THREADS];
    Semaphore   workInc[MAX_THREADS];
    std::mutex  sync;
    bool        running=true;

    uint8_t*                   workSet=nullptr;
    size_t                     workSize=0,workEltSize=0;
    size_t                     workTasks=0;
    std::function<void(void*)> workFunc;

    Semaphore   workDone;
  };
