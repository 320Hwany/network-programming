#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFLEN 512

int totalBytesReceived = 0;
int totalMessagesReceived = 0;

int create_socket() {
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    return sockfd;
}

void bind_socket(int sockfd, struct sockaddr_in *server_addr, int port) {
    memset((char *) server_addr, 0, sizeof(*server_addr));
    server_addr->sin_family = AF_INET;
    server_addr->sin_port = htons(port);
    server_addr->sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sockfd, (struct sockaddr*) server_addr, sizeof(*server_addr)) == -1) {
        perror("bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
}

int receive_message(int sockfd, char *buffer, struct sockaddr_in *client_addr, socklen_t *slen) {
    int recv_len = recvfrom(sockfd, buffer, BUFLEN - 1, 0, (struct sockaddr*) client_addr, slen);
    if (recv_len == -1) {
        perror("recvfrom");
        return -1;
    }
    buffer[recv_len] = '\0';
    return recv_len;
}

int handle_exit(char *buffer) {
    if (buffer[0] == 0x04) { 
        printf("서버가 종료되었습니다.\n");
        return 1;
    }
    return 0;
}

void send_response_to_client(int sockfd, struct sockaddr_in *client_addr, socklen_t addr_len, char *response, int response_len) {
    if (sendto(sockfd, response, response_len, 0, (struct sockaddr*) client_addr, addr_len) == -1) {
        perror("sendto");
    }
}

void handle_client_message(int sockfd, char *buffer, int recv_len, struct sockaddr_in *client_addr, socklen_t addr_len) {
    totalBytesReceived += recv_len;
    totalMessagesReceived++;

    printf("\n%s:%d 으로부터 메세지를 받습니다.\n", inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));

    char response[BUFLEN];
    int response_len = 0;

    switch(buffer[0]) {
        case 0x01: {  
            printf("Echo 메세지 : %s\n", buffer + 1);
            strcpy(response, buffer + 1);
            response_len = strlen(buffer + 1) + 1;
            send_response_to_client(sockfd, client_addr, addr_len, response, response_len);
            break;
        }
        case 0x02: {  
            printf("Chat 메세지 : %s\n", buffer + 1);
            snprintf(response, BUFLEN, "메세지를 받았습니다. ( %s )", buffer + 1);
            response_len = strlen(response) + 1;
            send_response_to_client(sockfd, client_addr, addr_len, response, response_len);
            break;
        }
        case 0x03: {
            printf("Stat 메세지 : %s\n", buffer + 1);
            if (strcmp(buffer + 1, "bytes") == 0) {
                printf("서버에서 수신한 총 바이트 수 요청 : %d\n", totalBytesReceived);
                snprintf(response, BUFLEN, "서버가 수신한 메세지 바이트 수의 총합 : %d", totalBytesReceived);
            } else if (strcmp(buffer + 1, "number") == 0) {
                printf("서버에서 수신한 총 메세지 개수 요청 : %d\n", totalMessagesReceived);
                snprintf(response, BUFLEN, "클라이언트가 보낸 메세지의 총 개수 : %d", totalMessagesReceived);
            } else {
                printf("서버가 수신한 메세지 바이트 수의 총합 : %d, 클라이언트가 보낸 메세지의 총 개수 : %d\n", totalBytesReceived, totalMessagesReceived);
                snprintf(response, BUFLEN, "서버가 수신한 메세지 바이트 수의 총합 : %d, 클라이언트가 보낸 메세지의 총 개수 : %d", totalBytesReceived, totalMessagesReceived);
            }
            response_len = strlen(response) + 1;
            send_response_to_client(sockfd, client_addr, addr_len, response, response_len);
            break;
        }
        case 0x04: {
            printf("클라이언트가 종료를 요청했습니다.\n");
            strcpy(response, "서버가 종료되었습니다.");
            response_len = strlen(response) + 1;
            send_response_to_client(sockfd, client_addr, addr_len, response, response_len);
            break;
        }
        default:
            printf("잘못된 형식의 메세지입니다 : %x\n", buffer[0]);
            strcpy(response, "잘못된 형식의 메세지입니다.");
            response_len = strlen(response) + 1;
            send_response_to_client(sockfd, client_addr, addr_len, response, response_len);
            break;
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "잘못된 명령어 형식입니다. 다음 형식으로 다시 시도해주세요: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    char buffer[BUFLEN];
    socklen_t slen = sizeof(client_addr);

    sockfd = create_socket();
    bind_socket(sockfd, &server_addr, port);

    printf("%d 포트에서 서버가 시작되었습니다. 클라이언트의 요청을 기다리는중...\n", port);

    while (1) {
        int recv_len = receive_message(sockfd, buffer, &client_addr, &slen);
        if (recv_len == -1) {
            continue;
        }

        handle_client_message(sockfd, buffer, recv_len, &client_addr, slen);

        if (handle_exit(buffer)) {
            break;
        }
    }

    close(sockfd);
    return 0;
}
