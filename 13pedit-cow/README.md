# Pedit COW (CVE-2026-46331)

net/sched act_pedit partial COW page cache corruption — Local Privilege Escalation PoC

基于 CVE-2026-46331 的 act_pedit 部分写时复制页缓存损坏变体。

## 影响范围

- **CVE**: CVE-2026-46331
- **内核版本**: Kernel 5.18 ~ 7.1-rc6
- **攻击面**: net/sched act_pedit

## 文件说明

| 文件 | 说明 |
|------|------|
| `CVE-2026-46331.c` | 漏洞利用源码 |
| `poc` | 预编译 x86_64 ELF |

## 用法

```bash
chmod +x poc
./poc
```

## 编译

```bash
gcc -o poc CVE-2026-46331.c -static
```
