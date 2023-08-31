// server
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <arpa/inet.h>

int main(int argc, const char* argv[])
{
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

	// 4、阻塞并等待连接请求
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	int cfd = accept(lfd, (struct sockaddr*)&client_addr, &client_len);
	if (cfd == -1) {
		perror("accept error");
		exit(1);
	}

	char ipbuf[128];
	printf("Client IP: %s, Port: %d\n", inet_ntop(AF_INET, &client_addr.sin_addr.s_addr, ipbuf, sizeof(ipbuf)), ntohs(client_addr.sin_port));

	char buf[1024] = { 0 };
	while (1) {
		int len = read(cfd, buf, sizeof(buf));
		if (len == -1) {
			perror("read error");
			exit(1);
		}
		else if (len == 0) {
			printf("Client %d disconnected...\n", ntohs(client_addr.sin_port));
			break;
		}
		else {
			printf("read buf = %s\n", buf);

			// 小写转大写，然后回复给客户端
			for (int i = 0; i < len; ++i) {
				buf[i] = toupper(buf[i]);
			}
			printf("after buf = %s\n", buf);

			int ret = write(cfd, buf, sizeof(buf));
			if (ret == -1) {
				perror("write error");
				exit(1);
			}
		}
	}
	close(lfd);
	close(cfd);

	return 0;
}

