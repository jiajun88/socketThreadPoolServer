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

	int maxfd = lfd; // ��ʼ�������ϼ������λ��
	while (1)
	{
		pthread_mutex_lock(&mutex);
		fd_set tmp = readSet; // ���´����ļ���
		pthread_mutex_unlock(&mutex);

		// ֻ�������ϣ������д�������쳣���ϣ����ҽ�timeout��������ΪNULL��ʾ�����ⲻ���������ļ��������ͻ�һֱ������ֱ����⵽�������ļ�������Ϊֹ����ʱselect�Ϳ��Է�����
		int ret = select(maxfd + 1, &tmp, NULL, NULL, NULL);
		// �ж��ǲ��Ǽ������ļ�������
		if (FD_ISSET(lfd, &tmp)) {
			// ���ܿͻ��˵����ӣ��õ�һ������ͨ�ŵ��ļ�������
			pthread_t tid;
			fdinfo* info = (fdinfo*)malloc(sizeof(fdinfo));
			info->fd = lfd;
			info->maxfd = &maxfd;
			info->rdset = &readSet;
			pthread_create(&tid, NULL, acceptNewConnection, info);
			pthread_detach(tid); // �����̷߳��롣ע����ò�Ҫ����Ϊpthread_join(tid);
		}
		// �ж��ǲ���ͨ�ŵ��ļ������������ڼ������ļ�������ֻ��һ����������ͨ�ŵ��ļ������������ж��
		for (int i = 0; i <= maxfd; ++i) {
			// ������ļ��������������ڼ����ģ��������ھ����Ķ������У�˵����������ͨ�ŵ��ļ��������Ҷ�Ӧ�Ķ�������������
			if (i != lfd && FD_ISSET(i, &tmp)) {
				// ��������
				// �������߳�
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

	int cfd = accept(info->fd, NULL, NULL); // ����Ϊ�˷��㣬�Ͳ��ؼ�¼�ͻ��˵�IP����Ϣ�ˣ������������Ӱ��ײ��ͨ�ţ���ֻ���ṩ�������߹��ڿͻ��˵���Ϣ����
	
	// ���ʹ�����Դ��ʱ���Ҫ�ǵü���
	pthread_mutex_lock(&mutex);
	FD_SET(cfd, info->rdset); // ע���ǽ�cfd����ԭʼ�Ĵ����ļ���readSet������tmp��
	*info->maxfd = cfd > *info->maxfd ? cfd : *info->maxfd; // ���������λ��
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
		free(info); // ע���ͷ��ڴ棡
		return NULL;
	}
	else if (len == 0) {
		printf("Client disconnected...\n");

		pthread_mutex_lock(&mutex);
		FD_CLR(info->fd, info->rdset); // ���ڿͻ��˶Ͽ��ˣ����������ͨ�ŵ��ļ�������Ҳ�Ϳ��Ի����ˣ���Ҫ������ԭʼ�Ĵ����ļ�����ɾ��
		pthread_mutex_unlock(&mutex);

		close(info->fd); // �ǵùر�
		return NULL; 
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
			return NULL; ;
		}
	}
	free(info); // ע���ͷ��ڴ棡

	return NULL;
}


