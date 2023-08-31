//#pragma once
#define _THREADPOOL_H
#ifdef _THREADPOOL_H

#include <pthread.h>

// 任务结构体
typedef struct Task {
	void(*function)(void* arg); // 使用void* 的传入参数类型表示接受任意类型的指针
	void* arg;
}Task;

// 线程池结构体
typedef struct ThreadPool {
	// 任务队列相关
	Task* taskQ;
	int queueCapacity; // 任务队列的最大容量
	int queueSize; // 当前任务个数
	int queueFront; // 队头位置（对应取任务操作）
	int queueRear; // 队尾位置（对应放任务操作）

	// 工作线程和管理者线程相关
	pthread_t managerID; // 管理者线程ID
	pthread_t* threadIDs; // 线程ID数组（有多个，因此是个数组）

	// 线程池维护了若干个线程，因此线程池被初始化成功之后，线程池里面就有若干个线程，个数可多可少，但也要有个范围
	int minNum; // 线程的最小个数
	int maxNum; // 线程的最大个数
	int busyNum; // 忙线程（工作线程）的个数（它少了就增加一些，它多了就削减一些）
	int liveNum; // 存活的线程个数（它等于 忙线程个数 + 空闲线程的个数）
	int exitNum; // 要杀死的线程个数（记录它是为了方便后续操作）

	pthread_mutex_t mutexPool; // 互斥锁，锁整个的线程池
	pthread_mutex_t mutexBusy; // 互斥锁，锁busyNum变量

	pthread_cond_t notFull;  // 任务队列是不是满了
	pthread_cond_t notEmpty; // 任务队列是不是空了

	// 再添加一个辅助性的成员，帮助判断当前线程池是否在工作
	int shutdown; // 是否需要销毁线程池，销毁为1，不销毁为0

}ThreadPool;

//-----------------------------------------------------------
// 线程池的基本结构已经算完成，现在要添加一些操作它的API函数
// 先声明线程池这个结构体（但是结构定义在别的地方），因为只是单纯的使用这个类型，而不是用它里面的元素，因此声明一下就行了
//typedef struct ThreadPool ThreadPool; // 注意这里和C++的区别，C语言如果没有使用typedef，那么像下面就得写成struct ThreadPool* threadPoolCreate();

// 创建线程池并初始化
// 设计思想：后续那些函数都需要拿到线程池实例对象才能进行操作，因此创建的时候必须得到一个实例，这里用返回值得到这个实力
// 这里需要用户传入三个参数：线程池里面线程的最大个数和最少个数，以及任务队列的容量
ThreadPool* threadPoolCreate(int min, int max, int queueSize);

// 销毁线程池（释放全部的线程池资源）
int threadPoolDestory(ThreadPool* pool);

// 给线程池添加任务
void threadPoolAdd(ThreadPool* pool, void(*func)(void*), void* arg);

// 获取线程池中工作的线程的个数
int threadPoolBusyNum(ThreadPool* pool);

// 获取线程池中活着的线程的个数
int threadPoolAliveNum(ThreadPool* pool);

///////////////
void* worker(void* arg);
void* manager(void* arg);
void threadExit(ThreadPool* pool);

#endif // _THREADPOOL_H







