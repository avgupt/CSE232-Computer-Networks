#include<fcntl.h>
#include<dirent.h>
#include<pthread.h>
#include<stdbool.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<sys/socket.h>

struct Process {
    int pid;
    char comm[255];
    int CPU_time;
};

int comparator(const void* p0, const void* p1) {
    struct Process* process0 = (struct Process*)p0;
    struct Process* process1 = (struct Process*)p1;
    return process1->CPU_time > process0->CPU_time;
}


bool isNumber(char *s) {
    char *ptr;
    long num = strtol(s, &ptr, 10);
    return *ptr == 0;
}

char* itoa(int toConvert) {
	static char converted[32] = {0};
	int i = 30;
	for(; toConvert && i; --i, toConvert /= 10)
		converted[i] = "0123456789abcdef"[toConvert % 10];
	return &converted[i+1];
}

struct Process getProcess(char *buffer) {
    char *token = strtok(buffer, " ");
    char *pid = token;
    token = strtok(NULL, " ");
    char *comm = token;
    for (int i = 0; i < 11; i++){
        token = strtok(NULL, " ");
    }
    token = strtok(NULL, " ");
    char *utime = token;
    token = strtok(NULL, " ");
    char *stime = token;

    struct Process p;
    p.pid = atoi(pid);
    strcpy(p.comm, comm);
    p.CPU_time = atoi(utime) + atoi(stime);
    return p;
}

void sendTopProcesses(struct Process *processes, int N, int socket) {
    FILE *file;
    char filename[] = "topN";
    strcat(filename, itoa(socket)); // new file for every socket
    strcat(filename, ".txt");
    file = fopen(filename, "w+");
    if (file == NULL) {
        perror("fopen");
        exit(1);
    }
    for (int i = 0; i < 10; i++) {
        fprintf(file, "%d %s %d\n", processes[i].pid, processes[i].comm, processes[i].CPU_time);
    }
    fclose(file);
    
    printf("FILENAME: %s\n", filename);
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("open");
        return;
    }
    char buffer[1000];
    if (read(fd, buffer, 1000) == -1) {
        perror("read");
    }
    close(fd);

    if (write(socket, buffer, strlen(buffer)) == -1) {
        perror("write");
    }
}

void readTopProcess(int socket) {
    char buffer[1024] = {0};
    int valread = read(socket , buffer, 1024);
	printf("TOP PROCESS FROM SOCKET %d...\n%s\n",socket, buffer);
}

void *connectionHandler(void *socket) {
    sleep(30);
    char N[1024] = {0};
    read(*((int*)socket) , N, 1024);

    struct dirent *de;
    DIR *dr = opendir("/proc");
    if (dr == NULL) {
        perror("opendir");
        exit(1);
    }

    struct Process processes[10000];
    int processNum = 0;

    while ((de = readdir(dr)) != NULL) {
        if (isNumber(de->d_name)) {
            char filePath[] = "/proc/";
            strcat(filePath, de->d_name);
            strcat(filePath, "/stat");

            int fd = open(filePath, O_RDONLY);
            if (fd == -1) {
                perror(filePath);
            }
            char buffer[1000];
            int readBuf = read(fd, buffer, 1000);
            processes[processNum] = getProcess(buffer);
            processNum++;
            close(fd);
        }
    }
    closedir(dr); 

    qsort(processes, processNum, sizeof(struct Process), comparator);
    sendTopProcesses(processes, atoi(N), *((int*)socket));
    readTopProcess(*((int*)socket));
    free(socket);
}

// -------------------------------------------------------------------------------------------------

int main(int argc, char const *argv[]) {
    int socketFileDesc = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFileDesc == -1) {
        perror("Socket creation failed");
        exit(1);
    }
    printf("Socket created\n");

    struct sockaddr_in server;
    server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(8080);

    if (bind(socketFileDesc, (struct sockaddr *)&server, sizeof(server)) == -1) {
        perror("Bind failed");
        exit(1);
    }
    printf("Bind done\n");

    if (listen(socketFileDesc, 10) == -1) {
        perror("Listen failed");
        exit(1);
    }
    printf("Listen done\n");

    struct sockaddr_in clientAddr;
    int newSocketFileDesc, addrlen = sizeof(struct sockaddr_in);
    while( (newSocketFileDesc = accept(socketFileDesc, (struct sockaddr *)&clientAddr, (socklen_t*)&addrlen)) ) {
		puts("Accept done\n");
		
		pthread_t thread;
		int *funcArg = malloc(1);
		*funcArg = newSocketFileDesc;
		if (pthread_create(&thread , NULL , connectionHandler , (void *)(intptr_t)funcArg) != 0) {
			perror("pthread_create");
			exit(1);
		}
	}
    return 0;
}
