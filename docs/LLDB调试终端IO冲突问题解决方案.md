# LLDB调试终端IO冲突问题解决方案

# 选型建议

1. 要调试**真实键盘 raw 输入、终端渲染**：方案 2（lldb\-server/debugserver 端口远程调试）最优；
2. 临时快速调试、不想开端口：方案 1（双终端 attach 等待）；
3. 只调试内部逻辑、不需要键盘交互：方案 3（宏关闭 raw 模式）；
4. 不想改一行代码、全程单 lldb 窗口控制：方案 4（stdio 重定向 tty）。

# 核心问题根源

你的编辑器开启 `raw` 终端模式、占用标准输入输出，直接在同一个终端 `lldb ./editor` 会出现：

1. lldb 控制台和程序键盘输入冲突；

2. raw 模式扰乱 lldb 交互光标、回显、换行；

3. 断点停下后终端状态错乱，输入乱码。

## 四种可行调试方案（从简单到专业，推荐方案 2/3）

## 方案 1：双终端分离（最简单，不用改代码，优先试）

思路：**程序运行在终端 A（负责键盘 / UI），lldb 运行在终端 B（只做调试命令）**，彻底隔离 IO。

### 步骤

1. 终端 A：启动程序并阻塞等待 attach（加一段等待代码）

```c
// 放在 main 最开头
#include <unistd.h>
#include <stdio.h>
void wait_for_debug() {
    pid_t pid = getpid();
    printf("Editor PID: %d, wait lldb attach...\n", pid);
    while(1) sleep(1);
}
int main() {
    wait_for_debug();
    // 下面初始化 raw mode、编辑器逻辑
}
```

编译运行：

```bash
gcc -g editor.c -o editor
./editor
# 输出 PID 并卡住，此时终端A只用来后续接收编辑器按键
```

2. 终端 B，用 lldb attach PID：

```bash
lldb -p 12345
(lldb) c
```

- 调试命令（断点、next、print）全部在终端 B 输入；

- 编辑器的键盘、屏幕渲染全部在终端 A；

- 两者 IO 完全隔离，raw 模式不会干扰 lldb。

### 缺点

程序启动会先停住，需要手动放行；想调试**初始化 raw mode 之前**的代码断点，需要搭配方案 3。

## 方案 2：lldb\-server /debugserver 端口分离（完美隔离 IO，推荐）

利用远程调试协议，调试器与程序分属两个终端，互不抢占 stdio。

### macOS（debugserver）

终端 A（UI / 键盘终端）：

```bash
# 监听本地端口，程序在此终端运行、接收按键
debugserver 127.0.0.1:8888 ./editor
```

终端 B（纯 lldb 调试控制台）：

```bash
lldb
(lldb) target create ./editor
(lldb) process connect connect://127.0.0.1:8888
# 直接断点、n/s/print，不污染编辑器终端
```

### Linux（lldb\-server）

终端 A：

```bash
lldb-server g 127.0.0.1:8888 ./editor
```

终端 B：

```bash
lldb
(lldb) gdb-remote 8888
```

### 优势

1. 程序从第一行代码就被调试器接管，能抓 `tcgetattr/tcsetattr` raw 初始化逻辑；

2. 编辑器终端完整保留终端交互，lldb 终端只处理调试指令；

3. 断点停下不会乱终端设置。

## 方案 3：代码分支 —— 调试时禁用 raw 模式（开发调试专用）

增加编译宏，调试构建下跳过 raw 终端配置，用普通行缓冲终端调试，适合快速逻辑断点。

```c
#include <termios.h>
#include <stdio.h>

struct termios orig_tty;

void enable_raw() {
#ifdef NO_RAW_DEBUG
    // 调试模式：不开启raw，普通终端
    return;
#endif
    // 正常发布的raw逻辑
    tcgetattr(STDIN_FILENO, &orig_tty);
    struct termios raw = orig_tty;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void restore_terminal() {
#ifdef NO_RAW_DEBUG
    return;
#endif
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_tty);
}
```

编译时关闭 raw：

```bash
gcc -g -DNO_RAW_DEBUG editor.c -o editor_debug
lldb ./editor_debug
```

### 适用场景

只调试编辑器内部逻辑、文本处理、缓冲区，**不需要测试真实键盘 raw 输入**；
如果要调试键盘输入、终端渲染，必须用方案 1/2。

## 方案 4：lldb 重定向程序 stdio 到另一个终端（高级无侵入）

不修改代码，启动 lldb 时把程序的 stdin/stdout 绑定到独立终端窗口。

1. 新开一个终端，获取它的设备文件（终端 A，给编辑器用）：

```bash
tty
# 输出类似 /dev/ttys003
```

2. 另一个终端启动 lldb，重定向 IO 到上面的 tty：

```bash
lldb ./editor
(lldb) set target.stdio-pass-through false
(lldb) process launch --stdio /dev/ttys003
```

- lldb 自身控制台负责调试命令；

- 编辑器的所有输入输出、raw 终端全部跑在 `/dev/ttys003`；

- 不用改代码，直接从程序入口调试。

# 关键踩坑修复：断点后终端乱掉

不管哪种方案，一旦断点暂停，程序不再接管终端，raw 模式没恢复会导致终端输入异常，加信号处理自动恢复终端：

```c
#include <signal.h>
struct termios orig_tty;

void sigint_handler(int sig) {
    // 恢复原始终端属性
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_tty);
    exit(0);
}

int main() {
    tcgetattr(STDIN_FILENO, &orig_tty);
    signal(SIGINT, sigint_handler);
    signal(SIGSEGV, sigint_handler);
    // ...剩余代码
}
```

lldb 中断、程序崩溃时自动还原终端，不会出现输入看不见、回车无效。

> （注：部分内容可能由 AI 生成）
