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

pthread_mutex_t mutex;

typedef struct fdinfo {
	int fd;
	int* maxfd;
	fd_set* rdset;
}fdinfo;

void* acceptNewConnection(void* arg);
void* communication(void* arg);

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

	int maxfd = lfd; // 初始化读集合检测的最大位置
	while (1)
	{
		pthread_mutex_lock(&mutex);
		fd_set tmp = readSet; // 更新待检测的集合
		pthread_mutex_unlock(&mutex);

		// 只检测读集合，不检测写集合与异常集合，并且将timeout参数设置为NULL表示如果检测不到就绪的文件描述符就会一直阻塞，直到检测到就绪的文件描述符为止，此时select就可以返回了
		int ret = select(maxfd + 1, &tmp, NULL, NULL, NULL);
		// 判断是不是监听的文件描述符
		if (FD_ISSET(lfd, &tmp)) {
			// 接受客户端的连接，得到一个用于通信的文件描述符
			pthread_t tid;
			fdinfo* info = (fdinfo*)malloc(sizeof(fdinfo));
			info->fd = lfd;
			info->maxfd = &maxfd;
			info->rdset = &readSet;
			pthread_create(&tid, NULL, acceptNewConnection, info);
			pthread_detach(tid); // 设置线程分离。注意最好不要设置为pthread_join(tid);
		}
		// 判断是不是通信的文件描述符。由于监听的文件描述符只有一个，而用于通信的文件描述符可能有多个
		for (int i = 0; i <= maxfd; ++i) {
			// 如果该文件描述符不是用于监听的，并且它在就绪的读集合中，说明它是用于通信的文件描述符且对应的读缓冲区有数据
			if (i != lfd && FD_ISSET(i, &tmp)) {
				// 接收数据
				// 创建子线程
				pthread_t tid;
				fdinfo* info = (fdinfo*)malloc(sizeof(fdinfo));
				info->fd = i;
				//info->maxfd = &maxfd;
				info->rdset = &readSet;
				pthread_create(&tid, NULL, communication, info);		
			}
		}
	}
	close(lfd);
	
	pthread_mutex_destroy(&mutex);

	return 0;
}

void* acceptNewConnection(void* arg) {
	fdinfo* info = (fdinfo*)arg;
	printf("Sub thread ID: %ld \n", pthread_self());

	int cfd = accept(info->fd, NULL, NULL); // 这里为了方便，就不必记录客户端的IP等信息了，这个参数并不影响底层的通信，它只是提供给开发者关于客户端的信息而已
	
	// 访问共享资源的时候就要记得加锁
	pthread_mutex_lock(&mutex);
	FD_SET(cfd, info->rdset); // 注意是将cfd放入原始的待检测的集合readSet而不是tmp中
	*info->maxfd = cfd > *info->maxfd ? cfd : *info->maxfd; // 更新最大检测位置
	pthread_mutex_unlock(&mutex);

	free(info);

	return NULL;
}

void * communication(void * arg)
{
	fdinfo* info = (fdinfo*)arg;

	char buf[1024];
	int len = read(info->fd, buf, sizeof(buf));
	if (len == -1) {
		perror("read error");
		free(info); // 注意释放内存！
		return NULL;
	}
	else if (len == 0) {
		printf("Client disconnected...\n");

		pthread_mutex_lock(&mutex);
		FD_CLR(info->fd, info->rdset); // 由于客户端断开了，那这个用于通信的文件描述符也就可以回收了，需要将它从原始的待检测的集合中删掉
		pthread_mutex_unlock(&mutex);

		close(info->fd); // 记得关闭
		return NULL; 
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
			return NULL; ;
		}
	}
	free(info); // 注意释放内存！

	return NULL;
}


