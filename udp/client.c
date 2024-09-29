#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFLEN 512

void send_message_to_server(int sockfd, struct sockaddr_in *server_addr, socklen_t addr_len, char type, char *message) {
    char buffer[BUFLEN];
    buffer[0] = type;
    if (message != NULL && strlen(message) > 0) {
        strcpy(buffer + 1, message);
    }
    int message_len = (message != NULL) ? strlen(message) + 1 : 1;

    if (sendto(sockfd, buffer, message_len, 0, (struct sockaddr*) server_addr, addr_len) == -1) {
        perror("sendto");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "잘못된 명령어 형식입니다. 다음 형식으로 다시 시도해주세요 : %s <server_ip> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *server_ip = argv[1];
    int port = atoi(argv[2]);
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[BUFLEN];
    socklen_t slen = sizeof(server_addr);

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset((char *) &server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_aton(server_ip, &server_addr.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("서버와 연결되었습니다. 다음 서버 주소로 메세지를 보냅니다. %s:%d\n", server_ip, port);

    while (1) {
        printf("\n명령어를 입력해주세요. (echo/chat/stat/quit): ");
        if (fgets(buffer, BUFLEN, stdin) == NULL) {
            printf("Error reading input.\n");
            continue;
        }
        buffer[strcspn(buffer, "\n")] = '\0'; 

        char command[BUFLEN];
        strcpy(command, buffer);

        if (strcmp(buffer, "echo") == 0) {
            printf("Echo 메세지를 입력해주세요 : ");
            if (fgets(buffer, BUFLEN, stdin) == NULL) {
                printf("메세지 읽기 중 오류가 발생하였습니다.\n");
                continue;
            }
            buffer[strcspn(buffer, "\n")] = '\0';
            send_message_to_server(sockfd, &server_addr, slen, 0x01, buffer);
        }
        else if (strcmp(buffer, "chat") == 0) {
            printf("채팅 메세지를 입력해주세요 : ");
            if (fgets(buffer, BUFLEN, stdin) == NULL) {
                printf("메세지 읽기 중 오류가 발생하였습니다.\n");
                continue;
            }
            buffer[strcspn(buffer, "\n")] = '\0';
            send_message_to_server(sockfd, &server_addr, slen, 0x02, buffer);
        }
        else if (strcmp(buffer, "stat") == 0) {
            printf("다음 명령어중 하나를 입력해주세요. (bytes/number/both): ");
            if (fgets(buffer, BUFLEN, stdin) == NULL) {
                printf("메세지 읽기 중 오류가 발생하였습니다.\n");
                continue;
            }
            buffer[strcspn(buffer, "\n")] = '\0';
            send_message_to_server(sockfd, &server_addr, slen, 0x03, buffer);
        }
        else if (strcmp(buffer, "quit") == 0) {
            send_message_to_server(sockfd, &server_addr, slen, 0x04, NULL);
            int recv_len = recvfrom(sockfd, buffer, BUFLEN - 1, 0, (struct sockaddr*) &server_addr, &slen);
            if (recv_len > 0) {
                buffer[recv_len] = '\0';
                printf("서버 응답 메세지 : %s\n", buffer);
            }
            break;
        }
        else {
            printf("잘못된 형식의 명령어입니다. 다시 입력해주세요. (echo/chat/stat/quit): \n");
            continue;
        }

        int recv_len = recvfrom(sockfd, buffer, BUFLEN - 1, 0, (struct sockaddr*) &server_addr, &slen);
        if (recv_len == -1) {
            perror("recvfrom");
            continue;
        }
        buffer[recv_len] = '\0';
        printf("서버 응답 메세지 : %s\n", buffer);
    }

    close(sockfd);
    return 0;
}