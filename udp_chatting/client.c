#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define BUFFER_SIZE 1024

int sock;
struct sockaddr_in server_addr;
char nickname[20];

void *receive_messages(void *arg) {
    char buffer[BUFFER_SIZE];
    int addr_len = sizeof(server_addr);

    while (1) {
        int recv_len = recvfrom(sock, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&server_addr, &addr_len);
        if (recv_len > 0) {
            buffer[recv_len] = '\0';
            printf("%s\n", buffer);
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "사용법: %s <IP> <포트> <닉네임>\n", argv[0]);
        exit(1);
    }

    char *server_ip = argv[1];
    int server_port = atoi(argv[2]);
    strncpy(nickname, argv[3], sizeof(nickname));

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("소켓 생성 실패");
        exit(1);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    server_addr.sin_port = htons(server_port);

    char connect_message[BUFFER_SIZE];
    snprintf(connect_message, sizeof(connect_message), "[%s] 채팅방에 입장했습니다.", nickname);
    sendto(sock, connect_message, strlen(connect_message), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));

    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, receive_messages, NULL);

    char message[BUFFER_SIZE];
    char full_message[BUFFER_SIZE + 30];

    printf("채팅방에 입장했습니다. 메시지를 입력하세요.\n");

    while (1) {
        fgets(message, BUFFER_SIZE, stdin);
        message[strcspn(message, "\n")] = 0;

        if (strlen(message) == 0) continue;

        snprintf(full_message, sizeof(full_message), "[%s] %s", nickname, message);
        sendto(sock, full_message, strlen(full_message), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    }

    close(sock);
    return 0;
}