#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>

#define BUFFER_SIZE 1024
#define TIMEOUT_SEC 1
#define MESSAGE_COUNT 10  // 전송할 메시지 수
#define SLEEP_SEC 1      // 메시지 전송 간격 (초)

typedef struct {
    uint16_t seq_num;
    char data[BUFFER_SIZE];
} Message;

// 클라이언트 소켓 초기화 함수
int initialize_client(const char* ip, int port, struct sockaddr_in* server_addr) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    
    memset(server_addr, 0, sizeof(*server_addr));
    server_addr->sin_family = AF_INET;
    server_addr->sin_addr.s_addr = inet_addr(ip);
    server_addr->sin_port = htons(port);

    // 소켓 타임아웃 설정
    struct timeval tv;
    tv.tv_sec = TIMEOUT_SEC;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    return sock;
}

// 임의의 메시지 생성 함수
void generate_message(char* buffer, int msg_num) {
    snprintf(buffer, BUFFER_SIZE, "자동생성메시지_%d", msg_num);
}

// 메시지 송수신 처리 함수
int process_message(int sock, Message* msg, struct sockaddr_in* server_addr, socklen_t addr_size) {
    int retry_count = 0;
    while (retry_count < 3) {  // 최대 3번 재시도
        // 메시지 전송
        sendto(sock, msg, sizeof(Message), 0, 
               (struct sockaddr*)server_addr, addr_size);
        
        printf("메시지 전송: [%d][%s]\n", msg->seq_num, msg->data);
        
        // 에코 대기
        Message echo_msg;
        int recv_len = recvfrom(sock, &echo_msg, sizeof(Message), 0,
                              (struct sockaddr*)server_addr, &addr_size);

        if (recv_len > 0) {
            if (echo_msg.seq_num == msg->seq_num) {
                printf("에코 수신: [%d][%s]\n", 
                       echo_msg.seq_num, echo_msg.data);
                return 1;  // 성공
            }
        } else {
            printf("타임아웃 발생. 재전송 중... (시도 %d/3)\n", retry_count + 1);
            retry_count++;
        }
    }
    return 0;  // 실패
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("사용법: %s <IP> <포트>\n", argv[0]);
        return 1;
    }

    struct sockaddr_in server_addr;
    socklen_t addr_size = sizeof(server_addr);
    
    // 클라이언트 초기화
    int sock = initialize_client(argv[1], atoi(argv[2]), &server_addr);
    
    Message msg;
    uint16_t current_seq = 0;
    int successful_sends = 0;
    int failed_sends = 0;

    // 자동 메시지 전송
    for (int i = 1; i <= MESSAGE_COUNT; i++) {
        generate_message(msg.data, i);
        msg.seq_num = current_seq;

        printf("\n전송 시도 %d/%d\n", i, MESSAGE_COUNT);
        
        // 메시지 처리
        if (process_message(sock, &msg, &server_addr, addr_size)) {
            current_seq += strlen(msg.data);
            successful_sends++;
        } else {
            failed_sends++;
        }

        sleep(SLEEP_SEC);  // 메시지 전송 간격 대기
    }

    // 종료 메시지 전송
    strcpy(msg.data, "QUIT");
    msg.seq_num = current_seq;
    process_message(sock, &msg, &server_addr, addr_size);

    close(sock);
    return 0;
}