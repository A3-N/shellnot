#if defined(__APPLE__)
#include <util.h>
#elif defined(__linux__)
#include <pty.h>
#else
#error "Unsupported platform"
#endif

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define P "/tmp/koreanfont.sock"
#define M 16
#define B 8192

typedef struct{int active,fd,len;pid_t pid;char buf[B];} session_t;
static session_t S[M];

void spawn(int i){
    int m; pid_t p = forkpty(&m,0,0,0);
    if (!p) execlp("sh", "sh", (char *)0), _exit(1);
    fcntl(m,F_SETFL,O_NONBLOCK);
    S[i].active = 1;
    S[i].fd = m;
    S[i].pid = p;
    S[i].len = 0;
}

void loop_daemon(){
    int sfd = socket(AF_UNIX, SOCK_STREAM, 0), cfd, i;
    struct sockaddr_un a = {.sun_family = AF_UNIX};
    strncpy(a.sun_path, P, sizeof(a.sun_path)-1);
    unlink(a.sun_path);
    bind(sfd, (void*)&a, sizeof a);
    listen(sfd, 5);

    for(;;){
        while (waitpid(-1, 0, WNOHANG) > 0);

        for(i = 0; i < M; i++) if(S[i].active){
            char t[256];
            int n = read(S[i].fd, t, sizeof t);
            if (n > 0 && S[i].len + n < B) {
                memcpy(S[i].buf + S[i].len, t, n);
                S[i].len += n;
            }
            else if (n == 0) {
                close(S[i].fd);
                S[i].active = 0;
            }
        }

        fd_set r; FD_ZERO(&r); FD_SET(sfd, &r);
        if (select(sfd + 1, &r, 0, 0, &(struct timeval){0, 200000}) > 0) {
            if (FD_ISSET(sfd, &r)) {
                cfd = accept(sfd, 0, 0);
                char l[1024];
                int n = read(cfd, l, sizeof l - 1);
                if (n > 0) {
                    l[n] = 0;
                    char *cmd = strtok(l, " ");
                    char *sid_str = strtok(NULL, " ");
                    char *payload = strtok(NULL, "\n");

                    if (cmd && sid_str) {
                        int sid = atoi(sid_str), idx = sid % M;
                        if (cmd[0] == 'I') {
                            if (!S[idx].active) spawn(idx);
                            if (payload) {
                                write(S[idx].fd, payload, strlen(payload));
                                write(S[idx].fd, "\n", 1);
                            }
                        } else if (cmd[0] == 'O' && S[idx].active && S[idx].len) {
                            write(cfd, S[idx].buf, S[idx].len);
                            S[idx].len = 0;
                        }
                    }
                }
                close(cfd);
            }
        }
    }
}

int main(int c, char **v){
    if (c > 1 && !strcmp(v[1], "--daemon")) {
        loop_daemon();
        return 0;
    }

    int sid = -1, mode = 0; char *txt = 0;
    for(int i = 1; i < c; i++) {
        if (!strcmp(v[i], "--session") && i + 1 < c) sid = atoi(v[++i]);
        else if (!strcmp(v[i], "--input") && i + 1 < c) mode = 1, txt = v[++i];
        else if (!strcmp(v[i], "--output")) mode = 2;
    }

    if (sid < 0 || !mode) return 1;

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un a = {.sun_family = AF_UNIX};
    strncpy(a.sun_path, P, sizeof(a.sun_path)-1);
    if (connect(fd, (void*)&a, sizeof a)) return 1;

    char b[1024];
    if (mode == 1) {
        snprintf(b, sizeof b, "IN %d %s\n", sid, txt);
        write(fd, b, strlen(b));
    } else {
        snprintf(b, sizeof b, "OUT %d\n", sid);
        write(fd, b, strlen(b));
        int n;
        while ((n = read(fd, b, sizeof b)) > 0) write(1, b, n);
    }

    close(fd);
    return 0;
}
