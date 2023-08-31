//#pragma once
#define _THREADPOOL_H
#ifdef _THREADPOOL_H

#include <pthread.h>

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

//-----------------------------------------------------------
// �̳߳صĻ����ṹ�Ѿ�����ɣ�����Ҫ���һЩ��������API����
// �������̳߳�����ṹ�壨���ǽṹ�����ڱ�ĵط�������Ϊֻ�ǵ�����ʹ��������ͣ����������������Ԫ�أ��������һ�¾�����
//typedef struct ThreadPool ThreadPool; // ע�������C++������C�������û��ʹ��typedef����ô������͵�д��struct ThreadPool* threadPoolCreate();

// �����̳߳ز���ʼ��
// ���˼�룺������Щ��������Ҫ�õ��̳߳�ʵ��������ܽ��в�������˴�����ʱ�����õ�һ��ʵ���������÷���ֵ�õ����ʵ��
// ������Ҫ�û����������������̳߳������̵߳������������ٸ������Լ�������е�����
ThreadPool* threadPoolCreate(int min, int max, int queueSize);

// �����̳߳أ��ͷ�ȫ�����̳߳���Դ��
int threadPoolDestory(ThreadPool* pool);

// ���̳߳��������
void threadPoolAdd(ThreadPool* pool, void(*func)(void*), void* arg);

// ��ȡ�̳߳��й������̵߳ĸ���
int threadPoolBusyNum(ThreadPool* pool);

// ��ȡ�̳߳��л��ŵ��̵߳ĸ���
int threadPoolAliveNum(ThreadPool* pool);

///////////////
void* worker(void* arg);
void* manager(void* arg);
void threadExit(ThreadPool* pool);

#endif // _THREADPOOL_H







