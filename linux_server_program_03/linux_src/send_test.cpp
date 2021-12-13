//Copyright(c) by yyyyshen 2021

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define REMOTE_ADDR "0.0.0.0"
#define REMOTE_PORT 12345

void
test_send()
{
    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    inet_pton(AF_INET, REMOTE_ADDR, &server_addr.sin_addr);
    server_addr.sin_port = htons(REMOTE_PORT);

    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);

    if(connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
        printf("conn failed\n");
    else
    {
        const char* oob_data = "abc";
        const char* normal_data = "123";
        send(sockfd, normal_data, strlen(normal_data), 0);
        send(sockfd, oob_data, strlen(oob_data), MSG_OOB);
    }

    close(sockfd);
}

int main(int argc, char* argv[])
{
    test_send();

    return 0;
}
