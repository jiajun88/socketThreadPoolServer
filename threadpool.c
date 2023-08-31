#include "threadpool.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/*
	�̳߳ص������Ҫ��Ϊ3�����֣�����������Ϲ����Ϳ��Եõ�һ���������̳߳أ�

	1��������У��洢��Ҫ����������ɹ������߳���������Щ����
		ͨ���̳߳��ṩ��API��������һ���������������ӵ�������У����ߴ����������ɾ��
		�Ѵ��������ᱻ�����������ɾ��
		�̳߳ص�ʹ���ߣ�Ҳ���ǵ����̳߳غ�����������������������߳̾����������߳�

	2���������̣߳������������������ߣ� ��N��
		�̳߳���ά����һ�������Ĺ����߳�, ���ǵ��������ǲ�ͣ�Ķ��������, �����ȡ�����񲢴���
		�������߳��൱����������е������߽�ɫ��
		����������Ϊ��, �������߳̽��ᱻ���� (ʹ����������/�ź�������)
		�������֮�������µ�����, �������߽��������, �����߳̿�ʼ����

	3���������̣߳���������������е����񣩣�1��
		���������������ԵĶ���������е����������Լ�����æ״̬�Ĺ����̸߳������м��
		����������ʱ��, �����ʵ��Ĵ���һЩ�µĹ����߳�
		��������ٵ�ʱ��, �����ʵ�������һЩ�������߳�
*/

const int NUMBER = 2; // �������߳�һ����/������ӵ��̸߳���

/*
// ����ṹ��
typedef struct Task {
	void(*function)(void* arg); // ʹ��void* �Ĵ���������ͱ�ʾ�����������͵�ָ��
	void* arg;
}Task;

// �̳߳ؽṹ��
typedef struct ThreadPool {
	// ����������
	Task* taskQ;
	int queueCapacity; // ������е��������
	int queueSize; // ��ǰ�������
	int queueFront; // ��ͷλ�ã���Ӧȡ���������
	int queueRear; // ��βλ�ã���Ӧ�����������

	// �����̺߳͹������߳����
	pthread_t managerID; // �������߳�ID
	pthread_t* threadIDs; // �߳�ID���飨�ж��������Ǹ����飩

	// �̳߳�ά�������ɸ��̣߳�����̳߳ر���ʼ���ɹ�֮���̳߳�����������ɸ��̣߳������ɶ���٣���ҲҪ�и���Χ
	int minNum; // �̵߳���С����
	int maxNum; // �̵߳�������
	int busyNum; // æ�̣߳������̣߳��ĸ����������˾�����һЩ�������˾�����һЩ��
	int liveNum; // �����̸߳����������� æ�̸߳��� + �����̵߳ĸ�����
	int exitNum; // Ҫɱ�����̸߳�������¼����Ϊ�˷������������

	pthread_mutex_t mutexPool; // �����������������̳߳�
	pthread_mutex_t mutexBusy; // ����������busyNum����

	pthread_cond_t notFull;  // ��������ǲ�������
	pthread_cond_t notEmpty; // ��������ǲ��ǿ���

	// �����һ�������Եĳ�Ա�������жϵ�ǰ�̳߳��Ƿ��ڹ���
	int shutdown; // �Ƿ���Ҫ�����̳߳أ�����Ϊ1��������Ϊ0

}ThreadPool;
*/

ThreadPool * threadPoolCreate(int min, int max, int queueSize)
{
	// �����̳߳�ʵ������
	ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));
	int X[4];
	do {
		if (pool == NULL) {
			printf("Thread pool creation failed...\n");
			break;
		}

		// �����߳�ID����
		pool->threadIDs = (pthread_t*)malloc(sizeof(pthread_t) * max); // ������߳�������ռ�,Ӧ�������̶߳��ڹ��������
		if (pool->threadIDs == NULL) {
			printf("Worker threadIDs array creation failed...\n");
			return NULL;
		}
		// ��ʼ��threadIDs����,0�ͱ�ʾ���߳�IDδ��ʹ��(���߳�ID����,Ҳ����˵���������̱߳�����,û���̳߳ؿ����ɵ��̵߳�����).ע�����빤���̺߳ͻ��ŵ��̵߳�����
		// ����ԭ��: void *memset(void *ptr, int value, size_t num); ptr��ָ��Ҫ�����ڴ���ʼ��ַ��ָ�롣value��Ҫ���õ�ֵ��ͨ����һ��������num��Ҫ���õ��ֽ�������Ҫ�����ڴ��С��
		memset(pool->threadIDs, 0, sizeof(pthread_t) * max); 

		pool->minNum = min; // �̳߳������̵߳���С����(ֻҪ�̳߳ػ���,��������̸߳����Ͳ������������,��ʹ�մ�����ʱ��ҲӦ��������)
		pool->maxNum = max; // �̳߳������̵߳�������
		pool->busyNum = 0; // ���ڹ������߳�Ϊ0
		pool->liveNum = min; // ע����Ӧ�ú�minNum���,��ʾ��ǰ���ŵ��߳���,Ҳ�������Ͽ���Ͷ�빤�����߳���
		pool->exitNum = 0; // Ҫɱ�����̸߳���

		// ��ʼ�������� �� ��ʼ����������
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

		// ��ʼ���̳߳����ٱ��
		pool->shutdown = 0; 

		// �����������
		pool->taskQ = (Task*)malloc(sizeof(Task) * queueSize);
		if (pool->taskQ == NULL) {
			printf("Failed to create task queue...\n");
			break;
		}
		pool->queueCapacity = queueSize; // ��ʼ��������е��������
		pool->queueSize = 0; // ��ʼ��������е�ǰ���������
		pool->queueFront = 0; // ��ʼ����ͷλ��
		pool->queueRear = 0; // ��ʼ����βλ��

		// ���������ߵ��߳�
		if (pthread_create(&pool->managerID, NULL, manager, pool) != 0) { 
			printf("Failed to create manager thread...\n");
			break;
		}

		// �������ٵĹ������߳�
		int flag = 0;
		for (int i = 0; i < min; ++i) {
			// ���ｫpool��Ϊ�������Ĳ���������Ϊworker���������Ǵ��������taskQ��ȡ����ģ���taskQ����pool����̳߳�ʵ��
			if (pthread_create(&pool->threadIDs[i], NULL, worker, pool) != 0) { 
				printf("Failed to create worker thread for thread pool...\n");
				flag = 1;
				break;
			}
		}
		if (flag == 1) {
			break;
		}

		return pool; // ���ش����ɹ����̳߳�ʵ������
	} while (0);

	// �����ִ�е����,��˵���������쳣,��ʱ����Ҫ�ͷ���Դ
	// ע���ͷ���Դ��˳��,�����Ȱ�pool���ͷŵ���,ÿһ��"Ƕ�׵�"����Ӧ�ô��ڵ������"����"
	if (pool && pool->taskQ) {
		free(pool->taskQ);
		pool->taskQ = NULL;
	}

	// �ͷ��Ѿ��������߳�
	for (int i = 0; i < min; ++i) {
		if (pool && pool->threadIDs[i] != 0) {
			pthread_join(pool->threadIDs[i], NULL);
		}
	}

	// �����Ѵ����ɹ�����
	if (pool) {
		if (X[0] == 0) pthread_mutex_destroy(&pool->mutexPool);
		if (X[1] == 0) pthread_mutex_destroy(&pool->mutexBusy);
		if (X[2] == 0) pthread_cond_destroy(&pool->notEmpty);
		if (X[3] == 0) pthread_cond_destroy(&pool->notFull);
	}

	// pool->threadIDs������ָ�룬���ǿ���ֱ���ͷ���
	if (pool && pool->threadIDs) {
		free(pool->threadIDs);
		pool->threadIDs = NULL;
	}

	// �����̳߳ؽṹ��
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
	// �ͷ���Դǰ���ȡ��رա��̳߳�
	pool->shutdown = 1;

	// ʹ��pthread_join�ȴ����չ������̣߳�����ȴ��������߳̽����ٻ��գ�
	// ���ڹ������̵߳�ʵ���߼��ǣ�ֻҪ�̳߳�û�رվ�һֱ��ʱ��������while(!pool->shutdown)
	pthread_join(pool->managerID, NULL);

	// �����������������̣߳�Ϊ���ͷ����ǣ�
	// ע���������forѭ�������ܼ���pthread_cond_broadcast�������Ϊ��ֻ�ỽ��һ�Σ���Ȼ��һ�ζ��ᱻ���ѣ����ǽ����ֻᱻ��������Ϊ�����������ͬһ�ѣ�
	for (int i = 0; i < pool->liveNum; ++i) { // ��֪���ж��ٸ��������������̣߳��Ǿ���i < pool->liveNumȷ�������������������̶߳���������
		// ʵ���ϣ�����������Ϊ�յ�ʱ�򣬲���û���µ�����ӽ�������ô��һ��������������̶߳����ɿ����̣߳�Ȼ����������������pool->notEmpty�ϣ���ʱ���ܽ��������߳�ȫ���˳���
		pthread_cond_signal(&pool->notEmpty); // ����֮�������õ�pool->mutexPool����Ȼ����pool->shutdown = 1�����Ǿ��ͷ��������˳�����ɱ������worker����
	}

	// �ͷŶ��ڴ�
	if (pool->taskQ) {
		free(pool->taskQ);
		//pool->taskQ = NULL;
	}
	if (pool->threadIDs) {
		free(pool->threadIDs);
		//pool->threadIDs = NULL;
	}

	// �ͷŻ���������������
	pthread_mutex_destroy(&pool->mutexPool);
	pthread_mutex_destroy(&pool->mutexBusy);
	pthread_cond_destroy(&pool->notEmpty);
	pthread_cond_destroy(&pool->notFull);

	free(pool);
	pool = NULL;

	return 0;
}

// Ϊ�̳߳أ���������У��������
void threadPoolAdd(ThreadPool * pool, void(*func)(void *), void * arg)
{
	// �����̳߳ص���������ǹ�����Դ�������Ҫ��ӻ�����
	pthread_mutex_lock(&pool->mutexPool);

	// �жϵ�ǰ��������Ƿ��������̳߳���û�б��ر� 
	while (!pool->shutdown && pool->queueSize == pool->queueCapacity) {
		// �����������̣߳����Ļ�����Ҫ�����������̣߳�Ҳ���ǹ����̣߳���worker������
		pthread_cond_wait(&pool->notFull, &pool->mutexPool);
	}
	if (pool->shutdown) {
		pthread_mutex_unlock(&pool->mutexPool);
		return;
	}

	// Ϊ��������������
	pool->taskQ[pool->queueRear].function = func; // ע�����ڶ�β���
	pool->taskQ[pool->queueRear].arg = arg;
	pool->queueRear = (pool->queueRear + 1) % pool->queueCapacity;
	pool->queueSize++;

	// ����һ����Ҫ������������Ҫ�������������������ϵ���Щ�������߳�
	// ��������������Ʒ�����Ҫ���ߣ����ѣ�������
	pthread_cond_signal(&pool->notEmpty); // ע�����������������pool->notEmpty

	pthread_mutex_unlock(&pool->mutexPool);
}

int threadPoolBusyNum(ThreadPool * pool)
{
	// �ǵü���(��ȻҲ���Լ�mutexPool����������������Ļ�Ч��̫�ͣ�)
	pthread_mutex_lock(&pool->mutexBusy);
	int busyNum = pool->busyNum;
	pthread_mutex_unlock(&pool->mutexBusy);

	return busyNum;
}

int threadPoolAliveNum(ThreadPool * pool)
{
	// �ǵü���(Ҳ���Ը�pool->liveNum������һ�����������̳߳�û�䣬��Ϊû��̫��ı�Ҫ)
	pthread_mutex_lock(&pool->mutexPool);
	int liveNum = pool->liveNum;
	pthread_mutex_unlock(&pool->mutexPool);

	return liveNum;
}

void* worker(void* arg)
{
	// �Ƚ���������Ĳ�����������ת������ΪΪ�������Դ������void* ���ͣ���ʵ�ʴ�������̳߳�ָ�����ͣ�
	ThreadPool* pool = (ThreadPool* )arg;

	// ÿ���̵߳Ĺ��������������ߣ�һֱ���Զ��������taskQ��ʲô�����������Ҫ����ʵ�������
	// ����ÿ���̶߳���Ҫ���������taskQ���в�������ֻ�Ƕ�������taskQ����ͬһ���̳߳�ʵ������pool�����Ҷ�taskQ����������ı�taskQ����Ҳ��ı��̳߳���������ݣ���˻���־�������������Եö��̳߳�pool����
	while (1) {
		pthread_mutex_lock(&pool->mutexPool); // ���̳߳ؼӻ�������һ��ֻ����һ���̶߳��̳߳ؽ��в���
		// �жϵ�ǰ��������Ƿ�Ϊ�����̳߳���û�б��ر�
		while (!pool->shutdown && pool->queueSize == 0) { // ��ֹα����
			// ���������߳�
			// ע�⣬���������ѵ�ʱ�����᳢�Ի������Ҳ����˵�����ܻỽ�Ѷ���̣߳���ֻ��һ���߳����õ�����������ִ�С�����û�õ����ĵ������ѵ��߳̽������ȴ���
			pthread_cond_wait(&pool->notEmpty, &pool->mutexPool);  //  pthread_cond_wait �����ڵȴ���������������ʱ�򣬲��ڵȴ��ڼ��ͷŻ�������
			
			// �ж��ǲ���Ҫ�����̣߳�������manage�������������̵߳Ĳ��ֵ�ע�ͣ�
			// ��Ȼ���в�Ϊ��Ҳ�ỽ���������̣߳����Ƕ��в�Ϊ�ջ����߳�ʱ�����pool->exitNum��������pool->exitNum��Ϊ���ǹ������߳���������ɱʱ�����Ĳ���
			//if (pool->exitNum > 0) { 
			//	pool->exitNum--;
			//	if (pool->liveNum > pool->minNum) {
			//		pool->liveNum--;
			//		pthread_mutex_unlock(&pool->mutexPool); // ע�����õ�ǰ�߳��˳�����ɱ��֮ǰ����Ҫ�����ӳ�������������õ������������������ˣ�û�߳̿ɽ�
			//		pthread_exit(NULL); // �á��������̻߳��ѡ�������������pool->notEmpty���߳���ɱ
			//	}	
			//}
			// ����Ĵ����и�bug�����á��������̻߳��ѡ����߳�ֱ����ɱ�ˣ�������ӦthreadIDs�����λ��ȴû������Ϊ0�����¸�λ�ò��ɸ���
			if (pool->exitNum > 0) {
				pool->exitNum--; // ע�������ܷŵ������if�����ȥ
				if (pool->liveNum > pool->minNum) { // ֻ������������������������߳�
					pool->liveNum--;
					pthread_mutex_unlock(&pool->mutexPool); // ע�����õ�ǰ�߳��˳�����ɱ��֮ǰ����Ҫ�����ӳ�������������õ������������������ˣ�û�߳̿ɽ�
					threadExit(pool); // �á��������̻߳��ѡ�������������pool->notEmpty���߳���ɱ�����ҽ����ӦthreadIDs�����λ������Ϊ0
				}
			}
		}

		// �ж��̳߳��Ƿ񱻹ر���
		if (pool->shutdown) {
			pthread_mutex_unlock(&pool->mutexPool); // ���������������ԭ����Ϊ�˱�����������Ϊ����̳߳عر��ˣ�����ǰ�߳��Ѿ�ִ���˼��������ˣ����Ǻ���Ĳ��������̳߳عر��Ѿ��������������ˣ�Ҳ�Ͳ����������������Ҫ����������⿪
			threadExit(pool);
			//pthread_exit(NULL); // �˳���ǰ�̣߳����̵߳��� pthread_exit ������������ֹ��ǰ�̵߳�ִ�У�������Ӱ�������̵߳����С������߳��˳�֮ǰ�����߳���ص���Դ��
			// ����Ҳ����ֱ�ӵ���pthread_exit(NULL);��Ϊ�̳߳ض����ر��ˣ�threadIDs�����λ���Ƿ�����Ϊ0�Ѿ�����Ҫ��
		}

		// ��ʼ���ѣ������������ȡ��һ������
		Task task; // ��Ҫִ�е�����
		task.function = pool->taskQ[pool->queueFront].function; // ���̳߳�������еĶ�ͷ�ڵ�ȡ����
		task.arg = pool->taskQ[pool->queueFront].arg; // ���̳߳�������еĶ�ͷ�ڵ�ȡ����Ĳ���

		// �ƶ���ͷ�ڵ㣨ѭ���ƶ���
		pool->queueFront = (pool->queueFront + 1) % pool->queueCapacity;
		// �޸��̳߳ع���ά��������е�Ԫ��
		pool->queueSize--;

		pthread_cond_signal(&pool->notFull); // �����������Ѳ�Ʒ�����Ҫ���ߣ����ѣ�������

		// ���϶��̳߳ع���������еĲ������������
		pthread_mutex_unlock(&pool->mutexPool); // ����

		// ----------------���Ǳ������̳߳ص�����Ԫ��Ҳ��Ҫʱ�̿��ǵ�---------------------
		// ��Ϊ��Ȼÿ��ֻ����һ���̴߳����������ȡ�����ڱ������Ҳ����ÿ��ֻ����һ���̲߳����̳߳�
		// �����������������ж�������������ô�����ж���߳�ͬʱִ�е������һ���õ�����ȡ�꣬��һ���õ�������ȡ��ȡ���������������׼��ִ�У�
		// ��ˣ���Ҫ���Ƕ���̵߳�pool->busyNum�ľ�������
		pthread_mutex_lock(&pool->mutexBusy);
		printf("Child thread %ld starts working...\n", pthread_self());
		pool->busyNum++;
		pthread_mutex_unlock(&pool->mutexBusy);

		// �ڵ�ǰ�߳���ִ������
		task.function(task.arg); 
		//(*task.function)(task.arg); // �ú���ָ����ú�����ʱ��*�żӶ��ٸ�������ν����Ϊ(*f)()��f()��Ч��һ��������(**f)()��Ч������(*f)()��Ч������f()��Ч������������
		// ���̳߳���Ƶ�ʱ���������Ĵ��������Ҫ��һ����ڴ棬�Ա�֤��������ĳ���������ͷ�
		// ���ִ�����������Ҫ�ͷŵ���������������ڴ棬�����ڴ�й¶
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
	// �Ƚ���������Ĳ�����������ת������ΪΪ�������Դ������void* ���ͣ���ʵ�ʴ�������̳߳�ָ�����ͣ�
	ThreadPool* pool = (ThreadPool*)arg;

	while (!pool->shutdown) {
		// ÿ��3���Ӽ��һ��
		sleep(3);

		// ȡ���̳߳�������������͵�ǰ�̵߳�����
		pthread_mutex_lock(&pool->mutexPool); // ������pool->mutexPool�����̳߳ص��ǰ�����
		int queueSize = pool->queueSize;
		int liveNum = pool->liveNum;
		pthread_mutex_unlock(&pool->mutexPool); // ����

		// ��Ҫȡ�������̣߳�æ�̣߳�������
		pthread_mutex_lock(&pool->mutexBusy); // ������pool->mutexBusy����æ�߳��������ǰ�����
		int busyNum = pool->busyNum;
		pthread_mutex_unlock(&pool->mutexBusy);

		// ����̣߳�����ҵ���߼��޸�����̵߳Ĳ��ԣ���û��һ��ͳһ�ı�׼��
		if (queueSize > liveNum - busyNum && liveNum < pool->maxNum) { // ע�����ﲻ����pool->queueSize > pool->liveNum�������߼����������� 
			pthread_mutex_lock(&pool->mutexPool); // �������������ԭ��������forѭ����Ҳ�������̳߳صı���pool->liveNum
			int counter = 0; // ���������Ҳû���⣬��Ϊ�����ǹ�����Դ
			for (int i = 0; i < pool->maxNum && counter < NUMBER && pool->liveNum < pool->maxNum; ++i) { // ע��ʵ����ӹ������̵߳ĸ������ܳ�������߳��������ҲҪ�ж�liveNum < pool->maxNum
				if (pool->threadIDs[i] == 0) { // ��Ҳ��Ϊʲôѭ����0��pool->maxNum��ԭ��Ϊ�����ҳ����е��߳�ID
					pthread_create(&pool->threadIDs[i], NULL, worker, pool);
					counter++;
					pool->liveNum++; // ע��ÿ����һ���µ��̣߳����ŵ��߳�����Ҫ+1
				}
			}
			pthread_mutex_unlock(&pool->mutexPool); // ����
		}

		// �����̣߳�����ҵ���߼��޸������̵߳Ĳ��ԣ���û��һ��ͳһ�ı�׼��
		if (busyNum * 2 < liveNum && liveNum > pool->minNum) {
			pthread_mutex_lock(&pool->mutexPool); // ע�����
			pool->exitNum = NUMBER;
			pthread_mutex_unlock(&pool->mutexPool); // ����
			// �ÿ��е��߳���ɱ
			// ��������ɱ���ͷ�һ���źţ�
			for (int i = 0; i < NUMBER; ++i) {
				pthread_cond_signal(&pool->notEmpty); // һ�λ���һ��������ָ�����������ϵ��̣߳���pthread_cond_broadcast��һ�λ�������������ָ�����������ϵ��߳�
			}
		}
	}

	return NULL;
}

// �˺����������ǵ��߳��˳�ʱ������Ӧ��threadIDs�����Ӧ��λ������Ϊ0���Ա㸴��
void threadExit(ThreadPool* pool)
{
	pthread_t tid = pthread_self(); // ��ȡ��ǰ�߳�ID
	for (int i = 0; i < pool->maxNum; ++i) {
		if (pool->threadIDs[i] == tid) {
			pool->threadIDs[i] = 0;
			printf("threadEixt() called, %ld exiting...\n", tid);
			break;
		}
	}
	pthread_exit(NULL); // �õ�ǰ�߳��˳�
}
