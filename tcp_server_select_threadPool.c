//// server
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <pthread.h>
#include "threadpool.h"

// �������������maxClients���ͻ��ˡ�ͬʱ������ͨ�ţ�Ҳ���̳߳ص��߳����������һ���̴߳���һ����ͻ���ͨ�ŵ���������
#define maxClients 8 

// ����������ֹ����̲߳���fdinfoʱ���־���
pthread_mutex_t mutex;

typedef struct fdinfo {
	int fd; // �ļ�������
	int* maxfd; // �����������λ��
	fd_set* rdset; // ������
}fdinfo;

// �ͻ�����Ϣ�ṹ��
typedef struct sockInfo
{
	struct sockaddr_in addr; // �����ͻ��˵�IP�Ͷ˿���Ϣ
	int fd; // ͨ���õ��ļ�������
}sockInfo;

// �����̳߳غͼ������ļ��������Լ�����select�������Ϣ�ṹ��
typedef struct poolInfo
{
	ThreadPool* pool; // �̳߳�
	int lfd; // �������ļ�������
	fdinfo* fdInfo; // ���������
}poolInfo;

void acceptNewConnection(void* arg);
void acceptNewConnection2(void*);
void communication(void* arg);

int main(int argc, const char* argv[])
{
	// 0. ��ʼ��
	pthread_mutex_init(&mutex, NULL);

	// 1. ����������fd
	int lfd = socket(AF_INET, SOCK_STREAM, 0);
	if (lfd == -1) {
		perror("socket error");
		exit(1);
	}

	// 2. ��
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(9999);
	addr.sin_addr.s_addr = INADDR_ANY;
	int ret = bind(lfd, (struct sockaddr*)&addr, sizeof(addr));
	if (ret == -1) {
		perror("bind error");
		exit(1);
	}

	// 3. ���ü���
	ret = listen(lfd, 128);
	if (ret == -1) {
		perror("listen error");
		exit(1);
	}
	fd_set readSet; // �����ϣ�ʵ������һ��1024bit�Ŀռ䣬Ҳ����1024/8 = 128�ֽڴ�С��ÿbit��Ӧһ���ļ�������
	FD_ZERO(&readSet); // ��ʼ��������
	FD_SET(lfd, &readSet); // ���������м����ļ�������/�׽���lfd��Ӧ��λ����1

	// 4�������̳߳�ʵ������
	int minThreads = 3; // �̳߳������߳���
	int maxThreads = maxClients; // �̳߳�����߳�����һ���ͻ������Ӷ�Ӧһ���̣߳�
	int queueSize = 12 * maxClients; // �̳߳�������е�����
	ThreadPool* pool = threadPoolCreate(minThreads, maxThreads, queueSize); // �����̳߳�ʵ������

	// 5����������һ��˳����ӵ��̳߳����������ȥ
	poolInfo* info = (poolInfo*)malloc(sizeof(poolInfo));
	info->fdInfo = (fdinfo*)malloc(sizeof(fdinfo));
	info->pool = pool;
	info->lfd = lfd;
	info->fdInfo->maxfd = &lfd;
	info->fdInfo->rdset = &readSet;
	threadPoolAdd(pool, acceptNewConnection2, info); // �������¿ͻ������ӵ�������ӵ��̳߳ص����������ȥ

	// 6�����߳��˳����ɣ����񶼽����̳߳��ˣ�
	pthread_exit(NULL);

	return 0;
}

// ������ͻ������ӵ�������
void acceptNewConnection2(void* arg) {
	// ������������Ĳ�����ͨ������ǿ��ת����poolInfo����
	poolInfo* info = (poolInfo*)arg; // ���������õ��׽���ʵ��������̳߳�ʵ������

	int addrLen = sizeof(struct sockaddr_in);
	while (1) {
		pthread_mutex_lock(&mutex);
		fd_set tmp = *info->fdInfo->rdset; // ���´����ļ���
		pthread_mutex_unlock(&mutex);

		// ѭ���������Щ�ļ����������ھ���״̬
		int ret = select(*info->fdInfo->maxfd + 1, &tmp, NULL, NULL, NULL);
		// �жϾ���״̬���ļ������������
		if (FD_ISSET(info->lfd, &tmp)) { // ����Ǽ������ļ����������ھ���״̬
			printf("22222222222222222222222222222222222\n");
			sockInfo* pinfo = (sockInfo*)malloc(sizeof(sockInfo));
			int cfd = accept(info->lfd, (struct sockaddr*)&pinfo->addr, &addrLen); // �ʹ���������
			pinfo->fd = cfd;
			printf("333333333333333333333333333333333333\n");
			if (cfd == -1) {
				perror("accept");
				break;
			}
			// ���õ��������ӵ�ͨ���õ��ļ����������ɵ���һ�ִ������ļ���������
			// ���ʹ�����Դ��ʱ���Ҫ�ǵü���
			pthread_mutex_lock(&mutex);
			FD_SET(cfd, info->fdInfo->rdset); // ע���ǽ�cfd����ԭʼ�Ĵ����ļ���readSet������tmp��
			printf("4444444444444444444444444444444444\n");
			*info->fdInfo->maxfd = cfd > *info->fdInfo->maxfd ? cfd : *info->fdInfo->maxfd; // ���������λ��
			pthread_mutex_unlock(&mutex);
			printf("5555555555555555555555555555555555555\n");
		}
		// �����ͨ�ŵ��ļ����������ھ���״̬
		for (int i = 0; i <= *info->fdInfo->maxfd; ++i) {
			if (i != info->lfd && FD_ISSET(i, &tmp)) {
				// �ͽ�ͨ�ŵ����񽻸��̳߳�ȥ��
				printf("%d %d %d %d %d %d %d %d %d %d \n",i,i,i,i,i,i,i,i,i,i);
				fdinfo* fdInfo = (fdinfo*)malloc(sizeof(fdinfo));
				fdInfo->fd = i;
				fdInfo->rdset = info->fdInfo->rdset;
				threadPoolAdd(info->pool, communication, fdInfo);
			}
		}
		sleep(1);
	}
	close(info->lfd); // �ͷż������׽���
	pthread_mutex_destroy(&mutex);
}


void communication(void * arg)
{
	fdinfo* info = (fdinfo*)arg;

	char buf[1024];

	while (1) {
		int len = read(info->fd, buf, sizeof(buf));
		if (len == -1) {
			perror("read error");
			return;
		}
		else if (len == 0) {
			printf("Client disconnected...\n");

			pthread_mutex_lock(&mutex);
			FD_CLR(info->fd, info->rdset); // ���ڿͻ��˶Ͽ��ˣ����������ͨ�ŵ��ļ�������Ҳ�Ϳ��Ի����ˣ���Ҫ������ԭʼ�Ĵ����ļ�����ɾ��
			pthread_mutex_unlock(&mutex);

			close(info->fd); // �ǵùر�
			return;
		}
		else {
			printf("read buf = %s\n", buf);

			// Сдת��д��Ȼ��ظ����ͻ���
			for (int i = 0; i < len; ++i) {
				buf[i] = toupper(buf[i]);
			}
			printf("after buf = %s\n", buf);

			int ret = write(info->fd, buf, sizeof(buf));
			if (ret == -1) {
				perror("write error");
				return;
			}
		}
	}
}

void communication2(void * arg)
{
	fdinfo* info = (fdinfo*)arg;

	char buf[1024];
	int len = read(info->fd, buf, sizeof(buf));
	if (len == -1) {
		perror("read error");
		return;
	}
	else if (len == 0) {
		printf("Client disconnected...\n");

		pthread_mutex_lock(&mutex);
		FD_CLR(info->fd, info->rdset); // ���ڿͻ��˶Ͽ��ˣ����������ͨ�ŵ��ļ�������Ҳ�Ϳ��Ի����ˣ���Ҫ������ԭʼ�Ĵ����ļ�����ɾ��
		pthread_mutex_unlock(&mutex);

		close(info->fd); // �ǵùر�
		return;
	}
	else {
		printf("read buf = %s\n", buf);

		// Сдת��д��Ȼ��ظ����ͻ���
		for (int i = 0; i < len; ++i) {
			buf[i] = toupper(buf[i]);
		}
		printf("after buf = %s\n", buf);

		int ret = write(info->fd, buf, sizeof(buf));
		if (ret == -1) {
			perror("write error");
			return;
		}
	}
}


