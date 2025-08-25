# Botan 示例调试指南

本指南说明如何在 Cursor 或 VSCode 中调试运行 Botan 的示例程序。

## 🚀 快速开始

### 前提条件

1. **确保 Botan 已构建**：
   ```bash
   python3 configure.py --debug-mode --build-targets=shared,static,cli
   make -j$(sysctl -n hw.logicalcpu)
   ```

2. **安装推荐的 VSCode 扩展**：
   - C/C++ (ms-vscode.cpptools)
   - C/C++ Extension Pack
   - CodeLLDB (vadimcn.vscode-lldb)

### 调试步骤

#### 方法一：使用调试配置

1. **打开示例文件**：在 VSCode/Cursor 中打开任意示例文件，如 `src/examples/aes.cpp`

2. **设置断点**：在代码中点击行号左侧设置断点

3. **启动调试**：
   - 按 `F5` 或 `Cmd+Shift+D` 打开调试面板
   - 选择对应的调试配置：
     - `Debug AES Example` - 调试 AES 示例
     - `Debug Hash Example` - 调试 Hash 示例  
     - `Debug Current Example` - 调试当前打开的示例文件

4. **开始调试**：点击绿色播放按钮开始调试

#### 方法二：使用构建脚本

1. **编译示例**：
   ```bash
   ./build_example.sh aes -c  # 仅编译
   ```

2. **手动启动调试**：在调试面板中选择对应的配置运行

## 🔧 配置说明

### 调试配置 (launch.json)

已预配置以下调试选项：

- **Debug AES Example** - 调试 AES 加密示例
- **Debug Hash Example** - 调试哈希函数示例
- **Debug AES CBC Example** - 调试 AES CBC 模式示例
- **Debug Current Example** - 调试当前打开的示例文件
- **Debug Botan Tests** - 调试 Botan 单元测试

所有配置都：
- 使用 `lldb` 调试器（macOS 最佳选择）
- 自动设置动态库路径 `DYLD_LIBRARY_PATH`
- 启用调试符号和美化打印

### 编译任务 (tasks.json)

预定义的编译任务：

- **Build AES Example (Debug)** - 编译 AES 示例（调试版本）
- **Build Hash Example (Debug)** - 编译 Hash 示例（调试版本）
- **Build Current Example (Debug)** - 编译当前示例文件（调试版本）
- **Build Example with Script (Debug)** - 使用脚本编译（推荐）
- **Build and Run Current Example** - 编译并运行当前示例

编译参数说明：
```bash
clang++ -std=c++20 -g -O0 -DDEBUG \
        -I./build/include/public \
        -L. -lbotan-3 \
        src/examples/example.cpp \
        -o example_debug
```

### IntelliSense 配置 (c_cpp_properties.json)

配置了以下路径和设置：
- **包含路径**：Botan 头文件路径、系统头文件路径
- **编译器**：clang++
- **C++ 标准**：C++20
- **预定义宏**：Botan 相关宏定义

## 🐛 调试技巧

### 常用调试操作

1. **设置断点**：点击行号左侧
2. **条件断点**：右键断点设置条件
3. **查看变量**：
   - 鼠标悬停查看变量值
   - 在 "变量" 面板中查看所有局部变量
   - 在 "监视" 面板中添加表达式
4. **单步调试**：
   - `F10` - 单步跳过（Step Over）
   - `F11` - 单步进入（Step Into）  
   - `Shift+F11` - 单步跳出（Step Out）
   - `F5` - 继续执行（Continue）

### 调试 Botan 特定对象

#### 查看加密数据
```cpp
// 在调试时，这些对象可以在变量面板中查看
auto cipher = Botan::BlockCipher::create("AES-256");
auto key = Botan::hex_decode("...");
auto data = Botan::hex_decode("...");

// 设置断点后，可以查看：
// - cipher 对象的内部状态
// - key 和 data 的十六进制表示
// - 加密前后的数据对比
```

#### 查看异常信息
Botan 的异常通常包含详细的错误信息，在调试器中可以：
1. 设置异常断点（Debug -> Break on Exceptions）
2. 查看异常对象的 `what()` 方法返回值
3. 检查调用栈定位问题源头

## 📝 示例调试场景

### 调试 AES 加密过程

1. 打开 `src/examples/aes.cpp`
2. 在第 10 行 `cipher->set_key(key);` 设置断点
3. 启动 "Debug AES Example" 配置
4. 逐步查看：
   - `key` 变量的内容
   - `cipher` 对象的状态
   - 加密前后 `block` 的变化

### 调试自定义示例

1. 创建新的 .cpp 文件，如 `my_example.cpp`
2. 在文件中编写 Botan 代码
3. 打开该文件，使用 "Debug Current Example" 配置
4. 系统会自动编译并启动调试

## ⚠️ 常见问题

### 库文件找不到
**错误**：`dyld: Library not loaded: libbotan-3.dylib`

**解决方案**：
- 确保环境变量已设置：`DYLD_LIBRARY_PATH=.`
- 或者在终端中运行：
  ```bash
  export DYLD_LIBRARY_PATH=/path/to/botan:$DYLD_LIBRARY_PATH
  ```

### 调试器无法启动
**错误**：调试器启动失败

**解决方案**：
1. 检查 Xcode Command Line Tools 是否安装：
   ```bash
   xcode-select --install
   ```
2. 确认 `lldb` 可用：
   ```bash
   lldb --version
   ```

### 无法设置断点
**问题**：断点显示为灰色或无效

**解决方案**：
1. 确保编译时包含调试信息（`-g` 参数）
2. 检查文件路径是否正确
3. 重新编译并清理缓存

## 🔗 有用的资源

- [Botan 官方文档](https://botan.randombit.net/handbook/)
- [VSCode C++ 调试指南](https://code.visualstudio.com/docs/cpp/cpp-debug)
- [LLDB 调试器文档](https://lldb.llvm.org/)

## 🎯 下一步

尝试调试其他示例程序：
- `hash.cpp` - 哈希函数
- `hmac.cpp` - HMAC 认证码
- `rsa_encrypt.cpp` - RSA 加密
- `ecdsa.cpp` - ECDSA 数字签名

享受你的 Botan 调试之旅！🎉
