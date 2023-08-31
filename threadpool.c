#include "threadpool.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/*
	线程池的组成主要分为3个部分，这三部分配合工作就可以得到一个完整的线程池：

	1、任务队列，存储需要处理的任务，由工作的线程来处理这些任务
		通过线程池提供的API函数，将一个待处理的任务添加到任务队列，或者从任务队列中删除
		已处理的任务会被从任务队列中删除
		线程池的使用者，也就是调用线程池函数往任务队列中添加任务的线程就是生产者线程

	2、工作的线程（任务队列任务的消费者） ，N个
		线程池中维护了一定数量的工作线程, 他们的作用是是不停的读任务队列, 从里边取出任务并处理
		工作的线程相当于是任务队列的消费者角色，
		如果任务队列为空, 工作的线程将会被阻塞 (使用条件变量/信号量阻塞)
		如果阻塞之后有了新的任务, 由生产者将阻塞解除, 工作线程开始工作

	3、管理者线程（不处理任务队列中的任务），1个
		它的任务是周期性的对任务队列中的任务数量以及处于忙状态的工作线程个数进行检测
		当任务过多的时候, 可以适当的创建一些新的工作线程
		当任务过少的时候, 可以适当的销毁一些工作的线程
*/

const int NUMBER = 2; // 管理者线程一次性/批量添加的线程个数

/*
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
*/

ThreadPool * threadPoolCreate(int min, int max, int queueSize)
{
	// 创建线程池实例对象
	ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));
	int X[4];
	do {
		if (pool == NULL) {
			printf("Thread pool creation failed...\n");
			break;
		}

		// 创建线程ID数组
		pool->threadIDs = (pthread_t*)malloc(sizeof(pthread_t) * max); // 按最大线程数分配空间,应对所有线程都在工作的情况
		if (pool->threadIDs == NULL) {
			printf("Worker threadIDs array creation failed...\n");
			return NULL;
		}
		// 初始化threadIDs数组,0就表示该线程ID未被使用(该线程ID可用,也就是说还可以有线程被创造,没到线程池可容纳的线程的上限).注意它与工作线程和活着的线程的区别
		// 函数原型: void *memset(void *ptr, int value, size_t num); ptr：指向要填充的内存起始地址的指针。value：要设置的值，通常是一个整数。num：要设置的字节数，即要填充的内存大小。
		memset(pool->threadIDs, 0, sizeof(pthread_t) * max); 

		pool->minNum = min; // 线程池里面线程的最小个数(只要线程池还在,它里面的线程个数就不能少于这个数,即使刚创建的时候也应该是这样)
		pool->maxNum = max; // 线程池里面线程的最大个数
		pool->busyNum = 0; // 正在工作的线程为0
		pool->liveNum = min; // 注意它应该和minNum相等,表示当前活着的线程数,也就是马上可以投入工作的线程数
		pool->exitNum = 0; // 要杀死的线程个数

		// 初始化互斥锁 和 初始化条件变量
		X[0] = pthread_mutex_init(&pool->mutexPool, NULL);
		X[1] = pthread_mutex_init(&pool->mutexBusy, NULL);
		X[2] = pthread_cond_init(&pool->notFull, NULL);
		X[3] = pthread_cond_init(&pool->notEmpty, NULL);
		for (int i = 0; i < 4; ++i) {
			if (X[i] != 0) {
				printf("Failed to initialize lock or condition variable...\n");
				break;
			}
		}

		// 初始化线程池销毁标记
		pool->shutdown = 0; 

		// 创建任务队列
		pool->taskQ = (Task*)malloc(sizeof(Task) * queueSize);
		if (pool->taskQ == NULL) {
			printf("Failed to create task queue...\n");
			break;
		}
		pool->queueCapacity = queueSize; // 初始化任务队列的最大容量
		pool->queueSize = 0; // 初始化任务队列当前的任务个数
		pool->queueFront = 0; // 初始化队头位置
		pool->queueRear = 0; // 初始化队尾位置

		// 创建管理者的线程
		if (pthread_create(&pool->managerID, NULL, manager, pool) != 0) { 
			printf("Failed to create manager thread...\n");
			break;
		}

		// 创建最少的工作的线程
		int flag = 0;
		for (int i = 0; i < min; ++i) {
			// 这里将pool作为任务函数的参数，是因为worker工作函数是从任务队列taskQ中取任务的，而taskQ属于pool这个线程池实例
			if (pthread_create(&pool->threadIDs[i], NULL, worker, pool) != 0) { 
				printf("Failed to create worker thread for thread pool...\n");
				flag = 1;
				break;
			}
		}
		if (flag == 1) {
			break;
		}

		return pool; // 返回创建成功的线程池实例对象
	} while (0);

	// 如果能执行到这儿,就说明出现了异常,此时就需要释放资源
	// 注意释放资源的顺序,不能先把pool给释放掉了,每一个"嵌套的"对象都应该从内到外进行"析构"
	if (pool && pool->taskQ) {
		free(pool->taskQ);
		pool->taskQ = NULL;
	}

	// 释放已经创建的线程
	for (int i = 0; i < min; ++i) {
		if (pool && pool->threadIDs[i] != 0) {
			pthread_join(pool->threadIDs[i], NULL);
		}
	}

	// 销毁已创建成功的锁
	if (pool) {
		if (X[0] == 0) pthread_mutex_destroy(&pool->mutexPool);
		if (X[1] == 0) pthread_mutex_destroy(&pool->mutexBusy);
		if (X[2] == 0) pthread_cond_destroy(&pool->notEmpty);
		if (X[3] == 0) pthread_cond_destroy(&pool->notFull);
	}

	// pool->threadIDs并不是指针，更是可以直接释放了
	if (pool && pool->threadIDs) {
		free(pool->threadIDs);
		pool->threadIDs = NULL;
	}

	// 销毁线程池结构体
	if (pool) {
		free(pool);
		pool = NULL;
	}

	return NULL;
}

int threadPoolDestory(ThreadPool * pool)
{
	if (pool == NULL) {
		return -1;
	}
	// 释放资源前，先“关闭”线程池
	pool->shutdown = 1;

	// 使用pthread_join等待回收管理者线程（它会等待管理者线程结束再回收）
	// 由于管理者线程的实现逻辑是，只要线程池没关闭就一直定时工作，即while(!pool->shutdown)
	pthread_join(pool->managerID, NULL);

	// 唤醒阻塞的消费者线程（为了释放它们）
	// 注意下面这个for循环并不能简单用pthread_cond_broadcast替代，因为它只会唤醒一次，虽然这一次都会被唤醒，但是接着又会被阻塞（因为这里的锁都是同一把）
	for (int i = 0; i < pool->liveNum; ++i) { // 不知道有多少个阻塞的消费者线程，那就用i < pool->liveNum确保所有阻塞的消费者线程都被唤醒了
		// 实际上，如果任务队列为空的时候，并且没有新的任务加进来，那么过一会儿所有消费者线程都会变成空闲线程，然后阻塞在条件变量pool->notEmpty上，此时就能将消费者线程全部退出了
		pthread_cond_signal(&pool->notEmpty); // 唤醒之后，依此拿到pool->mutexPool锁，然后发现pool->shutdown = 1，于是就释放锁，再退出（自杀），见worker函数
	}

	// 释放堆内存
	if (pool->taskQ) {
		free(pool->taskQ);
		//pool->taskQ = NULL;
	}
	if (pool->threadIDs) {
		free(pool->threadIDs);
		//pool->threadIDs = NULL;
	}

	// 释放互斥锁和条件变量
	pthread_mutex_destroy(&pool->mutexPool);
	pthread_mutex_destroy(&pool->mutexBusy);
	pthread_cond_destroy(&pool->notEmpty);
	pthread_cond_destroy(&pool->notFull);

	free(pool);
	pool = NULL;

	return 0;
}

// 为线程池（的任务队列）添加任务
void threadPoolAdd(ThreadPool * pool, void(*func)(void *), void * arg)
{
	// 由于线程池的任务队列是公共资源，因此需要添加互斥锁
	pthread_mutex_lock(&pool->mutexPool);

	// 判断当前任务队列是否满了且线程池有没有被关闭 
	while (!pool->shutdown && pool->queueSize == pool->queueCapacity) {
		// 阻塞生产者线程（它的唤醒需要依靠消费者线程，也就是工作线程，看worker函数）
		pthread_cond_wait(&pool->notFull, &pool->mutexPool);
	}
	if (pool->shutdown) {
		pthread_mutex_unlock(&pool->mutexPool);
		return;
	}

	// 为任务队列添加任务
	pool->taskQ[pool->queueRear].function = func; // 注意是在队尾添加
	pool->taskQ[pool->queueRear].arg = arg;
	pool->queueRear = (pool->queueRear + 1) % pool->queueCapacity;
	pool->queueSize++;

	// 还有一件事要做，生产者需要唤醒阻塞在条件变量上的那些工作的线程
	// 当生产者生产产品后就需要告诉（唤醒）消费者
	pthread_cond_signal(&pool->notEmpty); // 注意这里的条件变量是pool->notEmpty

	pthread_mutex_unlock(&pool->mutexPool);
}

int threadPoolBusyNum(ThreadPool * pool)
{
	// 记得加锁(当然也可以加mutexPool这把锁，不过这样的话效率太低！)
	pthread_mutex_lock(&pool->mutexBusy);
	int busyNum = pool->busyNum;
	pthread_mutex_unlock(&pool->mutexBusy);

	return busyNum;
}

int threadPoolAliveNum(ThreadPool * pool)
{
	// 记得加锁(也可以给pool->liveNum单独配一把锁，而本线程池没配，因为没有太大的必要)
	pthread_mutex_lock(&pool->mutexPool);
	int liveNum = pool->liveNum;
	pthread_mutex_unlock(&pool->mutexPool);

	return liveNum;
}

void* worker(void* arg)
{
	// 先将传入进来的参数进行类型转换（因为为了普适性传入的是void* 类型，而实际传入的是线程池指针类型）
	ThreadPool* pool = (ThreadPool* )arg;

	// 每个线程的工作函数（消费者）一直尝试读任务队列taskQ。什么情况跳出，需要根据实际情况。
	// 由于每个线程都需要对任务队列taskQ进行操作（不只是读），而taskQ属于同一个线程池实例对象pool，并且对taskQ操作不仅会改变taskQ本身，也会改变线程池里面的内容，因此会出现竞争的情况，所以得对线程池pool加锁
	while (1) {
		pthread_mutex_lock(&pool->mutexPool); // 给线程池加互斥锁，一次只能有一个线程对线程池进行操作
		// 判断当前任务队列是否为空且线程池有没有被关闭
		while (!pool->shutdown && pool->queueSize == 0) { // 防止伪唤醒
			// 阻塞工作线程
			// 注意，当它被唤醒的时候，它会尝试获得锁。也就是说，可能会唤醒多个线程，但只有一个线程能拿到锁，并继续执行。其他没拿到锁的但被唤醒的线程将继续等待。
			pthread_cond_wait(&pool->notEmpty, &pool->mutexPool);  //  pthread_cond_wait 函数在等待条件变量变量的时候，并在等待期间释放互斥锁。
			
			// 判断是不是要销毁线程（看后面manage函数关于销毁线程的部分的注释）
			// 虽然队列不为空也会唤醒阻塞的线程，但是队列不为空唤醒线程时不会对pool->exitNum做操作，pool->exitNum不为零是管理者线程想让其自杀时才做的操作
			//if (pool->exitNum > 0) { 
			//	pool->exitNum--;
			//	if (pool->liveNum > pool->minNum) {
			//		pool->liveNum--;
			//		pthread_mutex_unlock(&pool->mutexPool); // 注意在让当前线程退出（自杀）之前，需要让它接除（交出）它获得的锁，否则就造成死锁了，没线程可解
			//		pthread_exit(NULL); // 让【管理者线程唤醒】的阻塞在上面pool->notEmpty的线程自杀
			//	}	
			//}
			// 上面的代码有个bug在于让【管理者线程唤醒】的线程直接自杀了，而它对应threadIDs里面的位置却没有重置为0，导致该位置不可复用
			if (pool->exitNum > 0) {
				pool->exitNum--; // 注意它不能放到下面的if代码块去
				if (pool->liveNum > pool->minNum) { // 只有满足这个条件才真正销毁线程
					pool->liveNum--;
					pthread_mutex_unlock(&pool->mutexPool); // 注意在让当前线程退出（自杀）之前，需要让它接除（交出）它获得的锁，否则就造成死锁了，没线程可解
					threadExit(pool); // 让【管理者线程唤醒】的阻塞在上面pool->notEmpty的线程自杀，并且将其对应threadIDs里面的位置重置为0
				}
			}
		}

		// 判断线程池是否被关闭了
		if (pool->shutdown) {
			pthread_mutex_unlock(&pool->mutexPool); // 解锁，这里解锁的原因是为了避免死锁。因为如果线程池关闭了，而当前线程已经执行了加锁操作了，但是后面的操作由于线程池关闭已经不能正常进行了，也就不能正常解锁，因此要在这里把锁解开
			threadExit(pool);
			//pthread_exit(NULL); // 退出当前线程，当线程调用 pthread_exit 后，它会立即终止当前线程的执行，而不会影响其他线程的运行。会在线程退出之前清理线程相关的资源。
			// 这里也可以直接调用pthread_exit(NULL);因为线程池都被关闭了，threadIDs里面的位置是否重置为0已经不重要了
		}

		// 开始消费（从任务队列中取出一个任务）
		Task task; // 将要执行的任务
		task.function = pool->taskQ[pool->queueFront].function; // 从线程池任务队列的队头节点取任务
		task.arg = pool->taskQ[pool->queueFront].arg; // 从线程池任务队列的队头节点取任务的参数

		// 移动队头节点（循环移动）
		pool->queueFront = (pool->queueFront + 1) % pool->queueCapacity;
		// 修改线程池关于维护任务队列的元素
		pool->queueSize--;

		pthread_cond_signal(&pool->notFull); // 当消费者消费产品后就需要告诉（唤醒）生产者

		// 以上对线程池关于任务队列的操作就算完毕了
		pthread_mutex_unlock(&pool->mutexPool); // 解锁

		// ----------------但是别忘了线程池的其他元素也需要时刻考虑到---------------------
		// 因为虽然每次只允许一个线程从任务队列中取任务，在本设计中也就是每次只允许一个线程操作线程池
		// 但是如果任务队列中有多个任务待做，那么将会有多个线程同时执行到这里（上一个拿到锁后取完，下一个拿到锁继续取，取完就立马都到了这里准备执行）
		// 因此，需要考虑多个线程的pool->busyNum的竞争问题
		pthread_mutex_lock(&pool->mutexBusy);
		printf("Child thread %ld starts working...\n", pthread_self());
		pool->busyNum++;
		pthread_mutex_unlock(&pool->mutexBusy);

		// 在当前线程中执行任务
		task.function(task.arg); 
		//(*task.function)(task.arg); // 用函数指针调用函数的时候，*号加多少个都无所谓，因为(*f)()和f()的效果一样，所以(**f)()的效果等于(*f)()的效果等于f()的效果，依此类推
		// 本线程池设计的时候，任务函数的传入参数需要是一块堆内存，以保证它不会在某处被意外释放
		// 因此执行完任务后，需要释放掉传入参数的这块堆内存，避免内存泄露
		free(task.arg);
		task.arg = NULL;

		pthread_mutex_lock(&pool->mutexBusy);
		printf("Child thread %ld ends work...\n", pthread_self());
		pool->busyNum--;
		pthread_mutex_unlock(&pool->mutexBusy);
	}

	return NULL;
}

void * manager(void * arg)
{
	// 先将传入进来的参数进行类型转换（因为为了普适性传入的是void* 类型，而实际传入的是线程池指针类型）
	ThreadPool* pool = (ThreadPool*)arg;

	while (!pool->shutdown) {
		// 每隔3秒钟检测一次
		sleep(3);

		// 取出线程池中任务的数量和当前线程的数量
		pthread_mutex_lock(&pool->mutexPool); // 加锁（pool->mutexPool，锁线程池的那把锁）
		int queueSize = pool->queueSize;
		int liveNum = pool->liveNum;
		pthread_mutex_unlock(&pool->mutexPool); // 解锁

		// 需要取出工作线程（忙线程）的数量
		pthread_mutex_lock(&pool->mutexBusy); // 加锁（pool->mutexBusy，锁忙线程数量的那把锁）
		int busyNum = pool->busyNum;
		pthread_mutex_unlock(&pool->mutexBusy);

		// 添加线程（根据业务逻辑修改添加线程的策略，并没有一个统一的标准）
		if (queueSize > liveNum - busyNum && liveNum < pool->maxNum) { // 注意这里不能是pool->queueSize > pool->liveNum，否则逻辑就有问题了 
			pthread_mutex_lock(&pool->mutexPool); // 在这里【加锁】的原因是下面for循环中也操作了线程池的变量pool->liveNum
			int counter = 0; // 这个放上面也没问题，因为它不是共享资源
			for (int i = 0; i < pool->maxNum && counter < NUMBER && pool->liveNum < pool->maxNum; ++i) { // 注意实际添加过程中线程的个数可能超出最大线程数，因此也要判断liveNum < pool->maxNum
				if (pool->threadIDs[i] == 0) { // 这也是为什么循环从0到pool->maxNum的原因，为的是找出空闲的线程ID
					pthread_create(&pool->threadIDs[i], NULL, worker, pool);
					counter++;
					pool->liveNum++; // 注意每创建一个新的线程，活着的线程数就要+1
				}
			}
			pthread_mutex_unlock(&pool->mutexPool); // 解锁
		}

		// 销毁线程（根据业务逻辑修改销毁线程的策略，并没有一个统一的标准）
		if (busyNum * 2 < liveNum && liveNum > pool->minNum) {
			pthread_mutex_lock(&pool->mutexPool); // 注意加锁
			pool->exitNum = NUMBER;
			pthread_mutex_unlock(&pool->mutexPool); // 解锁
			// 让空闲的线程自杀
			// 引导它自杀（释放一个信号）
			for (int i = 0; i < NUMBER; ++i) {
				pthread_cond_signal(&pool->notEmpty); // 一次唤醒一个阻塞在指定条件变量上的线程，而pthread_cond_broadcast是一次唤醒所有阻塞在指定条件变量上的线程
			}
		}
	}

	return NULL;
}

// 此函数的作用是当线程退出时，将对应的threadIDs数组对应的位置重置为0，以便复用
void threadExit(ThreadPool* pool)
{
	pthread_t tid = pthread_self(); // 获取当前线程ID
	for (int i = 0; i < pool->maxNum; ++i) {
		if (pool->threadIDs[i] == tid) {
			pool->threadIDs[i] = 0;
			printf("threadEixt() called, %ld exiting...\n", tid);
			break;
		}
	}
	pthread_exit(NULL); // 让当前线程退出
}
