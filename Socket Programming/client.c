#include<dirent.h>
#include<fcntl.h>
#include<netinet/in.h>
#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>
#include<string.h>
#include<time.h>
#include<sys/socket.h>
#include<unistd.h>
#include<arpa/inet.h>

#define processesToGet 10

struct Process {
    int pid;
    char comm[255];
    int CPU_time;
};

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
struct Process getTopProcess() {
    DIR *dir;
    struct dirent *entry;
    struct Process topProcess = {0, "", 0};
    if ((dir = opendir("/proc")) == NULL) {
        perror("opendir()");
    } else {
        while ((entry = readdir(dir)) != NULL) {
            if (isNumber(entry->d_name)) {
                char filePath[] = "/proc/";
                strcat(filePath, entry->d_name);
                strcat(filePath, "/stat");
                
                int fd = open(filePath, O_RDONLY);
                if (fd == -1) {
                    perror(filePath);
                    exit(1);
                }
                char buffer[1000];
                int readBuf = read(fd, buffer, 1000);
                struct Process p = getProcess(buffer);

                if (p.CPU_time > topProcess.CPU_time) {
                    topProcess = p;
                }
                close(fd);
            }
        }
        closedir(dir);
    }
    return topProcess;
}

void sendTopProcess(int socket) {
    struct Process p = getTopProcess();

    char *topProcess = itoa(p.pid);
    strcat(topProcess, " ");
    strcat(topProcess, p.comm);
    strcat(topProcess, " ");
    strcat(topProcess, itoa(p.CPU_time));
    if (send(socket , topProcess, strlen(topProcess) , 0 ) == -1) {
        perror("send");
    }
}

char * getFilename(int sockfd) {
    static char filename[] = "recv";
    strcat(filename, itoa(rand()));
    strcat(filename, ".txt");
    printf("FILENAME %s\n", filename);
    return filename;
}

void saveFile(int sockfd) {
    char serverReply[2000];
    if (recv(sockfd, serverReply, 2000, 0) < 0) {
        perror("recv");
    }
    printf("File received\n");

    int fileDesc = open(getFilename(sockfd), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fileDesc == -1) {
        perror("open");
        return;
    }
    if (write(fileDesc, serverReply, strlen(serverReply)) == -1) {
        perror("write");
    }
    close(fileDesc);
}

//------------------------------------------------------------------------------------------

int main(int argc, char const *argv[]) {
    srand(time(0));
    int socketFileDesc = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFileDesc == -1) {
        perror("Socket creation failed");
        exit(1);
    }
    printf("Socket created\n");

    struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_port = htons(8080);
    if (connect(socketFileDesc, (struct sockaddr *)&server, sizeof(server)) == -1) {
        perror("Connection failed");
        exit(1);
    }
    printf("Connection established\n");

    char *N = itoa(processesToGet);
    if (send(socketFileDesc, N, strlen(N), 0) == -1) {
        perror("Sending failed");
        exit(1);
    }
    printf("Send done\n");
    
    saveFile(socketFileDesc);
    sendTopProcess(socketFileDesc);
    close(socketFileDesc);

    return 0;
}
