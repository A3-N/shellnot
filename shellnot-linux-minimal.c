#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define P "/tmp/kf.sock"
#define B 8192

char buf[B];
int fd, len;

void spawn() {
    int m;
#if defined(__APPLE__)
    extern int forkpty(int*, char*, void*, void*);
#else
    #include <pty.h>
#endif
    if (!forkpty(&m, 0, 0, 0)) {
        char *argv[] = {"/bin/sh", 0};
        char *envp[] = {0};
        execve("/bin/sh", argv, envp);
        _exit(1);
    }
    fcntl(m, F_SETFL, O_NONBLOCK);
    fd = m;
    len = 0;
}

void run() {
    int s = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un a = {.sun_family = AF_UNIX};
    unlink(P);
    strcpy(a.sun_path, P);
    bind(s, (void*)&a, sizeof a);
    listen(s, 1);
    spawn();

    for (;;) {
        int n;
        char t[256];
        n = read(fd, t, sizeof t);
        if (n > 0 && len + n < B) memcpy(buf + len, t, n), len += n;

        fd_set r; FD_ZERO(&r); FD_SET(s, &r);
        if (select(s + 1, &r, 0, 0, &(struct timeval){0,200000}) > 0) {
            int c = accept(s, 0, 0);
            char l[512];
            n = read(c, l, sizeof l - 1);
            if (n > 0) {
                l[n] = 0;
                if (l[0] == 'I' && l[1] == ' ') {
                    char *x = l + 2;
                    write(fd, x, strlen(x));
                    write(fd, "\n", 1);
                } else if (l[0] == 'O' && len) {
                    write(c, buf, len);
                    len = 0;
                }
            }
            close(c);
        }
    }
}

int main(int c, char **v) {
    if (c > 1 && v[1][0] == 'd') {
        run();
        return 0;
    }

    int s = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un a = {.sun_family = AF_UNIX};
    strcpy(a.sun_path, P);
    connect(s, (void*)&a, sizeof a);

    if (v[1][0] == 'i') {
        write(s, "I ", 2);
        write(s, v[2], strlen(v[2]));
    } else {
        write(s, "O\n", 2);
        int n;
        while ((n = read(s, buf, sizeof buf)) > 0) write(1, buf, n);
    }

    return 0;
}

