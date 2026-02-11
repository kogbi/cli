# CarLink CLI Library

一个功能强大的 C++ 命令行接口（CLI）库，提供交互式命令行界面和命令执行框架。

## 特性

- 📝 **交互式命令行** - 基于 readline 库实现，支持历史记录和行编辑
- 🎯 **命令注册系统** - 简洁的 API 用于注册自定义命令
- 🌲 **树形参数补全** - 支持任意深度的参数依赖关系和智能补全
- ✅ **参数验证** - 内置参数验证器，支持数值范围和候选值检查
- 🎨 **彩色输出** - 支持终端颜色输出，提升用户体验
- 🔧 **单命令模式** - 支持命令行参数直接执行命令
- 📦 **易于集成** - 作为静态库或动态库集成到你的项目中

## 构建要求

- C++11 或更高版本
- CMake 3.10+
- readline 库

### 安装依赖

**Ubuntu/Debian:**
```bash
sudo apt-get install libreadline-dev
```

**Fedora/RHEL:**
```bash
sudo dnf install readline-devel
```

**macOS:**
```bash
brew install readline
```

## 构建和安装

### 构建库

```bash
mkdir build
cd build
cmake ..
make
```

### 安装（可选）

```bash
sudo make install
```

### 构建选项

- `BUILD_SHARED_LIBS=ON` - 构建动态库（默认为静态库）
- `CMAKE_BUILD_TYPE=Release` - 构建发行版（默认）
- `CMAKE_BUILD_TYPE=Debug` - 构建调试版

示例：
```bash
cmake -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Debug ..
```

## 使用方法

### 基本示例

```cpp
#include "CLI.h"
#include <iostream>

using namespace carlink::cli;

int main(int argc, char** argv)
{
    CLI cli;
    
    // 注册自定义命令
    cli.registerCommand("hello", "Say hello",
        [](const std::vector<std::string>& args) {
            std::cout << "Hello, World!" << std::endl;
        });
    
    // 运行 CLI
    return cli.run(argc, argv);
}
```

### 树形参数补全示例

```cpp
#include "CLI.h"
#include <iostream>

using namespace carlink::cli;

int main(int argc, char** argv)
{
    CLI cli;
    
    // 创建树形参数结构
    ParamNode root;
    TreeBuilder(root)
        .root({"device1", "device2", "timeout"})
        .node({"device1"}, {"light", "sound"})
        .node({"device1", "light"}, {"0", "1", "2"})
        .node({"device1", "sound"}, {"low", "medium", "high"})
        .node({"device2"}, {"enable", "disable"})
        .numeric({"timeout"}, 1, 600);
    
    // 创建补全器和验证器
    auto [completer, validator] = makeTreeParamMap(root);
    
    // 注册命令
    cli.registerCommand("set", "Set configuration",
        [](const std::vector<std::string>& args) {
            std::cout << "Setting: ";
            for (size_t i = 1; i < args.size(); ++i) {
                std::cout << args[i] << " ";
            }
            std::cout << std::endl;
        },
        completer,
        validator);
    
    return cli.run(argc, argv);
}
```

### 编译你的程序

**使用静态库:**
```bash
g++ -o myapp main.cpp -lcarlink_cli -lreadline
```

**使用动态库:**
```bash
g++ -o myapp main.cpp -L/path/to/lib -lcarlink_cli -lreadline
```

**使用 CMake:**
```cmake
find_library(CARLINK_CLI carlink_cli)
target_link_libraries(myapp ${CARLINK_CLI} readline)
```

## API 文档

### 核心类

#### `CLI`

主要的 CLI 类。

**方法:**
- `int run(int argc, char** argv)` - 运行 CLI（交互模式或单命令模式）
- `void registerCommand(...)` - 注册自定义命令

#### `ParamNode`

树形参数节点，用于构建参数依赖关系。

#### `TreeBuilder`

用于构建树形参数结构的辅助类。

**方法:**
- `root(candidates)` - 设置根节点候选值
- `node(path, candidates)` - 设置指定路径的候选值
- `numeric(path, min, max)` - 设置数值参数的范围

### 类型定义

- `CommandHandler` - 命令处理函数类型
- `TreeCompleter` - 树形补全函数类型
- `ParamValidator` - 参数验证器类型

## 内置命令

- `help` - 显示所有可用命令
- `exit` / `quit` - 退出 CLI
- `clear` - 清屏

## 许可证

GPL-3.0 - 详见 LICENSE 文件

## 作者

**Kogbi**  
Email: kogbi0209@outlook.com

## 贡献

欢迎提交 Issues 和 Pull Requests！

## 更新日志

### v1.0 (2024)
- 初始版本发布
- 基本命令注册和执行
- 树形参数补全和验证
- readline 集成
- 彩色终端输出
