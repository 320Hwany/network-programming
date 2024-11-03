#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

#define BUFFER_SIZE 1024
#define DEFAULT_P 0.5

typedef struct {
    uint16_t seq_num;
    char data[BUFFER_SIZE];
} Message;

typedef struct {
    int N1;  // 새로운 메시지 수
    int N2;  // 재전송된 메시지 수
    int N3;  // 전체 메시지 수 (N1 + N2)
} Statistics;

// 가장 최근 메시지 정보를 저장하는 구조체
typedef struct {
    char data[BUFFER_SIZE];
    int msg_num;     // 메시지 번호 (자동생성메시지_N의 N)
    int attempts;    // 현재 메시지의 전송 시도 횟수
} LastMessage;

// 서버 소켓 초기화 함수
int initialize_server(int port) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in server_addr;
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("바인딩 실패");
        exit(1);
    }

    printf("서버가 포트 %d에서 시작되었습니다\n", port);
    return sock;
}

// 현재 메시지 번호 추출 함수
int extract_msg_num(const char* data) {
    if (strncmp(data, "자동생성메시지_", strlen("자동생성메시지_")) == 0) {
        return atoi(data + strlen("자동생성메시지_"));
    }
    return -1;  // QUIT 등 다른 메시지의 경우
}

// 메시지 처리 및 통계 업데이트 함수
void process_message(Message *msg, Statistics *stats, LastMessage *last_msg) {
    int current_msg_num = extract_msg_num(msg->data);
    
    if (current_msg_num == -1) return;  // QUIT 등의 메시지는 통계에서 제외

    if (current_msg_num == last_msg->msg_num) {
        // 같은 메시지의 재전송
        stats->N2++;  // 재전송 카운트 증가
        last_msg->attempts++;
    } else {
        // 새로운 메시지
        if (last_msg->msg_num != 0) {  // 첫 메시지가 아닌 경우
            stats->N1++;  // 새로운 메시지 카운트 증가
        }
        strncpy(last_msg->data, msg->data, BUFFER_SIZE);
        last_msg->msg_num = current_msg_num;
        last_msg->attempts = 1;
    }
    stats->N3 = stats->N1 + stats->N2;
}

// 통계 출력 및 초기화 함수
void print_and_reset_stats(Statistics *stats, float p, const char *client_ip, int client_port) {
    printf("클라이언트 (%s:%d) 종료됨\n", client_ip, client_port);
    printf("통계: p=%.1f, N1=%d, N2=%d, N3=%d, R=%.2f\n",
           p, stats->N1, stats->N2, stats->N3,
           (float)stats->N2 / stats->N3);
    
    // 통계 초기화
    *stats = (Statistics){0, 0, 0};
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("사용법: %s <포트>\n", argv[0]);
        return 1;
    }

    srand(time(NULL));
    Statistics stats = {0, 0, 0};
    LastMessage last_msg = {"", 0, 0};
    float p = DEFAULT_P;

    int sock = initialize_server(atoi(argv[1]));
    struct sockaddr_in client_addr;
    socklen_t addr_size = sizeof(client_addr);
    Message msg;

    while (1) {
        int recv_len = recvfrom(sock, &msg, sizeof(Message), 0,
                              (struct sockaddr*)&client_addr, &addr_size);

        if (recv_len > 0) {
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
            
            printf("클라이언트 (%s:%d) 메시지: [%d][%s]\n",
                   client_ip, ntohs(client_addr.sin_port),
                   msg.seq_num, msg.data);

            // 모든 수신된 메시지에 대해 통계 업데이트
            process_message(&msg, &stats, &last_msg);

            // 메시지 에코 여부 결정
            float random = (float)rand() / RAND_MAX;
            if (random <= p) {
                sendto(sock, &msg, sizeof(Message), 0,
                       (struct sockaddr*)&client_addr, addr_size);
                printf("메시지 에코 완료\n");
            } else {
                printf("메시지 손실 시뮬레이션\n");
            }

            if (strcmp(msg.data, "QUIT") == 0 || strcmp(msg.data, "quit") == 0) {
                print_and_reset_stats(&stats, p, client_ip, ntohs(client_addr.sin_port));
            }
        }
    }

    close(sock);
    return 0;
}