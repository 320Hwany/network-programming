#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <time.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

typedef struct {
    int socket;
    struct sockaddr_in addr;
    char nickname[20];
} Client;

Client clients[MAX_CLIENTS];
int client_count = 0;
int message_count = 0;
time_t start_time;

int create_socket(int port) {
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("소켓 생성 실패");
        exit(1);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("바인딩 실패");
        exit(1);
    }

    if (listen(server_sock, 5) < 0) {
        perror("리스닝 실패");
        exit(1);
    }

    return server_sock;
}

void relay_message(char *message, int sender_socket) {
    for (int i = 0; i < client_count; i++) {
        if (clients[i].socket != sender_socket) {
            send(clients[i].socket, message, strlen(message), 0);
        }
    }
    message_count++;
}

int find_client(int socket) {
    for (int i = 0; i < client_count; i++) {
        if (clients[i].socket == socket) {
            return i;
        }
    }
    return -1;
}

void add_client(int client_sock, struct sockaddr_in *addr, const char *nickname) {
    if (client_count < MAX_CLIENTS) {
        clients[client_count].socket = client_sock;
        clients[client_count].addr = *addr;
        strncpy(clients[client_count].nickname, nickname, sizeof(clients[client_count].nickname) - 1);
        clients[client_count].nickname[sizeof(clients[client_count].nickname) - 1] = '\0';
        client_count++;
        printf("새 클라이언트 접속: [%s] %s:%d\n", 
               clients[client_count-1].nickname, 
               inet_ntoa(addr->sin_addr), 
               ntohs(addr->sin_port));
    }
}

void handle_new_connection(int server_sock) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_len);
    
    if (client_sock < 0) {
        perror("클라이언트 연결 수락 실패");
        return;
    }

    char buffer[BUFFER_SIZE];
    int recv_len = recv(client_sock, buffer, BUFFER_SIZE, 0);
    if (recv_len > 0) {
        buffer[recv_len] = '\0';
        char nickname[20];
        if (sscanf(buffer, "[%19[^]]]", nickname) == 1) {
            add_client(client_sock, &client_addr, nickname);
        }
    }
}

void handle_client_message(int client_sock) {
    char buffer[BUFFER_SIZE];
    int recv_len = recv(client_sock, buffer, BUFFER_SIZE, 0);
    
    if (recv_len <= 0) {
        int client_index = find_client(client_sock);
        if (client_index != -1) {
            printf("클라이언트 연결 종료: [%s]\n", clients[client_index].nickname);
            close(client_sock);
            for (int i = client_index; i < client_count - 1; i++) {
                clients[i] = clients[i + 1];
            }
            client_count--;
        }
    } else {
        buffer[recv_len] = '\0';
        relay_message(buffer, client_sock);
    }
}

void show_client_info() {
    printf("\n접속 클라이언트 수: %d\n", client_count);
    for (int i = 0; i < client_count; i++) {
        printf("[%s] %s:%d\n", clients[i].nickname,
               inet_ntoa(clients[i].addr.sin_addr),
               ntohs(clients[i].addr.sin_port));
    }
}

void show_chat_stats() {
    time_t current_time = time(NULL);
    double running_time_minutes = difftime(current_time, start_time) / 60.0;
    double avg_messages_per_minute = message_count / running_time_minutes;
    
    printf("\n총 메시지 수: %d\n", message_count);
    printf("서버 러닝 타임: %.2f분\n", running_time_minutes);
    printf("분당 평균 메시지 수: %.2f\n", avg_messages_per_minute);
}

void print_menu() {
    printf("\n1. 클라이언트 정보\n2. 채팅 통계\n3. 채팅 종료\n\n");
}

void print_prompt() {
    fflush(stdout);
}

void handle_user_input(int server_sock) {
    char choice[10];
    fgets(choice, sizeof(choice), stdin);

    switch (choice[0]) {
        case '1':
            show_client_info();
            break;
        case '2':
            show_chat_stats();
            break;
        case '3':
            printf("서버를 종료합니다.\n");
            close(server_sock);
            exit(0);
        default:
            printf("잘못된 선택입니다.\n");
    }
    print_prompt();
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "사용법: %s <포트>\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]);
    int server_sock = create_socket(port);

    printf("TCP 채팅 서버가 포트 %d에서 시작되었습니다.\n", port);
    start_time = time(NULL);

    fd_set read_fds;
    int max_fd = server_sock;

    print_menu();
    print_prompt();

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(server_sock, &read_fds);
        
        for (int i = 0; i < client_count; i++) {
            FD_SET(clients[i].socket, &read_fds);
            if (clients[i].socket > max_fd) {
                max_fd = clients[i].socket;
            }
        }

        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            perror("select 오류");
            exit(1);
        }

        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            handle_user_input(server_sock);
        }

        if (FD_ISSET(server_sock, &read_fds)) {
            handle_new_connection(server_sock);
        }

        for (int i = 0; i < client_count; i++) {
            if (FD_ISSET(clients[i].socket, &read_fds)) {
                handle_client_message(clients[i].socket);
            }
        }
    }

    return 0;
}