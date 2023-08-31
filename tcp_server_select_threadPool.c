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

// 允许服务端最多与maxClients个客户端【同时】进行通信，也是线程池的线程最大数量（一个线程处理一个与客户端通信的任务函数）
#define maxClients 8 

// 互斥锁，防止多个线程操作fdinfo时出现竞争
pthread_mutex_t mutex;

typedef struct fdinfo {
	int fd; // 文件描述符
	int* maxfd; // 读集合最大检测位置
	fd_set* rdset; // 读集合
}fdinfo;

// 客户端信息结构体
typedef struct sockInfo
{
	struct sockaddr_in addr; // 包含客户端的IP和端口信息
	int fd; // 通信用的文件描述符
}sockInfo;

// 关于线程池和监听的文件描述符以及关于select所需的信息结构体
typedef struct poolInfo
{
	ThreadPool* pool; // 线程池
	int lfd; // 监听的文件描述符
	fdinfo* fdInfo; // 读集合相关
}poolInfo;

void acceptNewConnection(void* arg);
void acceptNewConnection2(void*);
void communication(void* arg);

int main(int argc, const char* argv[])
{
	// 0. 初始化
	pthread_mutex_init(&mutex, NULL);

	// 1. 创建监听的fd
	int lfd = socket(AF_INET, SOCK_STREAM, 0);
	if (lfd == -1) {
		perror("socket error");
		exit(1);
	}

	// 2. 绑定
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

	// 3. 设置监听
	ret = listen(lfd, 128);
	if (ret == -1) {
		perror("listen error");
		exit(1);
	}
	fd_set readSet; // 读集合，实际上是一块1024bit的空间，也就是1024/8 = 128字节大小，每bit对应一个文件描述符
	FD_ZERO(&readSet); // 初始化读集合
	FD_SET(lfd, &readSet); // 将读集合中监听文件描述符/套接字lfd对应的位置置1

	// 4、创建线程池实例对象
	int minThreads = 3; // 线程池最少线程数
	int maxThreads = maxClients; // 线程池最大线程数（一个客户端连接对应一个线程）
	int queueSize = 12 * maxClients; // 线程池任务队列的容量
	ThreadPool* pool = threadPoolCreate(minThreads, maxThreads, queueSize); // 创建线程池实例对象

	// 5、将任务按照一定顺序添加到线程池任务队列中去
	poolInfo* info = (poolInfo*)malloc(sizeof(poolInfo));
	info->fdInfo = (fdinfo*)malloc(sizeof(fdinfo));
	info->pool = pool;
	info->lfd = lfd;
	info->fdInfo->maxfd = &lfd;
	info->fdInfo->rdset = &readSet;
	threadPoolAdd(pool, acceptNewConnection2, info); // 将处理新客户端连接的任务添加到线程池的任务队列中去

	// 6、主线程退出即可（任务都交给线程池了）
	pthread_exit(NULL);

	return 0;
}

// 负责处理客户端连接的任务函数
void acceptNewConnection2(void* arg) {
	// 将任务函数所需的参数从通用类型强制转换成poolInfo类型
	poolInfo* info = (poolInfo*)arg; // 包含监听用的套接字实例对象和线程池实例对象

	int addrLen = sizeof(struct sockaddr_in);
	while (1) {
		pthread_mutex_lock(&mutex);
		fd_set tmp = *info->fdInfo->rdset; // 更新待检测的集合
		pthread_mutex_unlock(&mutex);

		// 循环检测有哪些文件描述符处于就绪状态
		int ret = select(*info->fdInfo->maxfd + 1, &tmp, NULL, NULL, NULL);
		// 判断就绪状态的文件描述符的类别
		if (FD_ISSET(info->lfd, &tmp)) { // 如果是监听的文件描述符处于就绪状态
			printf("22222222222222222222222222222222222\n");
			sockInfo* pinfo = (sockInfo*)malloc(sizeof(sockInfo));
			int cfd = accept(info->lfd, (struct sockaddr*)&pinfo->addr, &addrLen); // 就处理新连接
			pinfo->fd = cfd;
			printf("333333333333333333333333333333333333\n");
			if (cfd == -1) {
				perror("accept");
				break;
			}
			// 将得到的新连接的通信用的文件描述符归纳到下一轮待检测的文件描述符中
			// 访问共享资源的时候就要记得加锁
			pthread_mutex_lock(&mutex);
			FD_SET(cfd, info->fdInfo->rdset); // 注意是将cfd放入原始的待检测的集合readSet而不是tmp中
			printf("4444444444444444444444444444444444\n");
			*info->fdInfo->maxfd = cfd > *info->fdInfo->maxfd ? cfd : *info->fdInfo->maxfd; // 更新最大检测位置
			pthread_mutex_unlock(&mutex);
			printf("5555555555555555555555555555555555555\n");
		}
		// 如果是通信的文件描述符处于就绪状态
		for (int i = 0; i <= *info->fdInfo->maxfd; ++i) {
			if (i != info->lfd && FD_ISSET(i, &tmp)) {
				// 就将通信的任务交给线程池去做
				printf("%d %d %d %d %d %d %d %d %d %d \n",i,i,i,i,i,i,i,i,i,i);
				fdinfo* fdInfo = (fdinfo*)malloc(sizeof(fdinfo));
				fdInfo->fd = i;
				fdInfo->rdset = info->fdInfo->rdset;
				threadPoolAdd(info->pool, communication, fdInfo);
			}
		}
		sleep(1);
	}
	close(info->lfd); // 释放监听的套接字
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
			FD_CLR(info->fd, info->rdset); // 由于客户端断开了，那这个用于通信的文件描述符也就可以回收了，需要将它从原始的待检测的集合中删掉
			pthread_mutex_unlock(&mutex);

			close(info->fd); // 记得关闭
			return;
		}
		else {
			printf("read buf = %s\n", buf);

			// 小写转大写，然后回复给客户端
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
		FD_CLR(info->fd, info->rdset); // 由于客户端断开了，那这个用于通信的文件描述符也就可以回收了，需要将它从原始的待检测的集合中删掉
		pthread_mutex_unlock(&mutex);

		close(info->fd); // 记得关闭
		return;
	}
	else {
		printf("read buf = %s\n", buf);

		// 小写转大写，然后回复给客户端
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


