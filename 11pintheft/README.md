# PinTheft — CVE-2026-43494 RDS zerocopy double-free 本地提权

Linux 内核 RDS 协议 `rds_message_zcopy_from_user()` 在 zerocopy 发送路径存在双重释放漏洞：错误路径中对已 pin 的页面执行了 `put_page()` 与 `__free_page()` 双重减引用。结合 `io_uring` 固定缓冲区机制，可将 stolen page 回收为 SUID 二进制页缓存后写入 payload，最终 execve 获得 root shell。

## 文件说明

| 文件 | 说明 |
|------|------|
| `poc.c` | exploit 源码（x86_64 payload 内嵌） |
| `poc` | 静态编译 x86_64 ELF，可直接上传运行 |

## 编译

```bash
cd 11pintheft
gcc -o exp poc.c
```

静态编译版本可直接使用：

```bash
chmod +x poc
./poc
```

## Requirements

- `CONFIG_RDS` + `CONFIG_RDS_TCP`（通过 `SO_RDS_TRANSPORT=2` 可自动加载 `rds_tcp` 模块）
- `CONFIG_IO_URING` 且 `io_uring_disabled=0`
- 存在可读的 SUID-root 二进制
- x86_64 架构（payload 为 x86_64 ELF，利用技术本身与架构无关）

常见发行版中仅 Arch Linux 默认内建 RDS 内核模块。其余发行版需手动加载或编译内核。

## Run

```bash
# 源码编译执行
cd 11pintheft && gcc -o exp poc.c && ./exp

# 或使用静态编译二进制
chmod +x poc && ./poc
```

## Cleanup

PoC 在执行前会自动备份目标 SUID 二进制到 `/tmp/.backup_<name>_<pid>`，并在篡改前打印恢复命令：

```bash
sudo cp /tmp/.backup_<name>_<pid> <target> && sudo chmod u+s <target>
```

测试环境直接重启或 `echo 3 > /proc/sys/vm/drop_caches` 亦可清除页缓存覆写。

## 缓解

如不需要 RDS 模块，建议禁用：

```bash
rmmod rds_tcp rds
printf 'install rds /bin/false\ninstall rds_tcp /bin/false\n' > /etc/modprobe.d/pintheft.conf
```

## 参考

- [v12-security/pocs — PinTheft](https://github.com/v12-security/pocs/tree/main/pintheft)
- [Lore patch](https://lore.kernel.org/netdev/20260505234336.2132721-1-achender@kernel.org/)
- Discovered with [V12](https://v12.sh) by Aaron Esau ([@v12sec](https://x.com/v12sec))
