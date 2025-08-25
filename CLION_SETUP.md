# CLion 配置指南 - Botan 示例

本指南详细说明如何在 CLion 中配置编译和调试 Botan 示例程序。

## 🚀 CLion 运行/调试配置

### 方法一：单个文件配置（推荐用于学习）

#### 1. 基本配置
按照你的截图，在 "运行/调试配置" 中填写：

**名称(N)：** `aes_example`

**工具链：** `使用默认值 Default`

**源文件：** `open_source/botan/src/examples/aes.cpp`

#### 2. 编译器选项 - 关键配置 ⭐
在 **编译器选项** 字段中输入：
```bash
-std=c++20 -g -O0 -DDEBUG -I./build/include/public -L. -lbotan-3 -Wl,-rpath,.
```

详细说明：
- `-std=c++20` - 使用 C++20 标准
- `-g` - 包含调试信息
- `-O0` - 关闭优化（方便调试）
- `-DDEBUG` - 定义 DEBUG 宏
- `-I./build/include/public` - 包含 Botan 头文件路径
- `-L.` - 链接库搜索路径（当前目录）
- `-lbotan-3` - 链接 Botan 库
- `-Wl,-rpath,.` - 设置运行时库路径

#### 3. 工作目录 - 重要 ⭐
**工作目录(W)：** `/Users/kyler/Documents/code/geekrun/learnc/open_source/botan`

或者使用相对路径：`$PROJECT_DIR$/open_source/botan`

#### 4. 环境变量
点击 **环境变量** 按钮，添加：
```
名称: DYLD_LIBRARY_PATH
值: /Users/kyler/Documents/code/geekrun/learnc/open_source/botan
```

或者简化版：
```
名称: DYLD_LIBRARY_PATH  
值: .
```

### 方法二：CMake 配置（推荐用于项目开发）

#### 1. 创建 CMakeLists.txt
在 `botan` 目录下创建：

```cmake
cmake_minimum_required(VERSION 3.20)
project(BotanExamples)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置 Botan 库路径
set(BOTAN_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(BOTAN_INCLUDE_DIR ${BOTAN_ROOT}/build/include/public)
set(BOTAN_LIBRARY_DIR ${BOTAN_ROOT})

# 查找 Botan 库
find_library(BOTAN_LIBRARY 
    NAMES botan-3 
    PATHS ${BOTAN_LIBRARY_DIR}
    NO_DEFAULT_PATH
)

if(NOT BOTAN_LIBRARY)
    message(FATAL_ERROR "Botan library not found in ${BOTAN_LIBRARY_DIR}")
endif()

# 包含目录
include_directories(${BOTAN_INCLUDE_DIR})

# 链接目录
link_directories(${BOTAN_LIBRARY_DIR})

# 编译选项
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

# 示例程序列表
set(EXAMPLES
    aes
    aes_cbc
    hash
    hmac
    rsa_encrypt
    ecdsa
)

# 为每个示例创建可执行文件
foreach(EXAMPLE ${EXAMPLES})
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/examples/${EXAMPLE}.cpp)
        add_executable(${EXAMPLE}_example src/examples/${EXAMPLE}.cpp)
        target_link_libraries(${EXAMPLE}_example ${BOTAN_LIBRARY})
        
        # 设置运行时库路径
        set_target_properties(${EXAMPLE}_example PROPERTIES
            INSTALL_RPATH ${BOTAN_LIBRARY_DIR}
            BUILD_WITH_INSTALL_RPATH TRUE
        )
    endif()
endforeach()

# 自定义目标：运行 AES 示例
add_custom_target(run_aes
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/aes_example
    DEPENDS aes_example
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
```

#### 2. CLion CMake 配置
1. **File** → **Open** → 选择包含 CMakeLists.txt 的目录
2. CLion 会自动识别 CMake 项目
3. 在右上角选择要运行的目标（如 `aes_example`）
4. 点击绿色运行按钮

## 🔧 详细配置步骤

### 步骤 1：打开配置
1. **Run** → **Edit Configurations...**
2. 点击 **+** → **C/C++ Single File**

### 步骤 2：填写配置（按照你的截图）

**基本信息：**
- 名称：`aes_example`
- 源文件：选择 `src/examples/aes.cpp`

**编译设置：**
```bash
编译器选项: -std=c++20 -g -O0 -I./build/include/public -L. -lbotan-3 -Wl,-rpath,.
工作目录: /Users/kyler/Documents/code/geekrun/learnc/open_source/botan
```

**环境变量：**
```
DYLD_LIBRARY_PATH = .
```

### 步骤 3：高级选项
展开 **其他选项**，确保：
- ☑️ **激活工具窗口**
- ☐ **使工具窗口获得焦点**（可选）

## 🎯 快速配置模板

### AES 示例配置
```
名称: aes_example
源文件: open_source/botan/src/examples/aes.cpp
编译器选项: -std=c++20 -g -I./build/include/public -L. -lbotan-3 -Wl,-rpath,.
工作目录: /Users/kyler/Documents/code/geekrun/learnc/open_source/botan
环境变量: DYLD_LIBRARY_PATH=.
```

### Hash 示例配置
```
名称: hash_example  
源文件: open_source/botan/src/examples/hash.cpp
编译器选项: -std=c++20 -g -I./build/include/public -L. -lbotan-3 -Wl,-rpath,.
工作目录: /Users/kyler/Documents/code/geekrun/learnc/open_source/botan
环境变量: DYLD_LIBRARY_PATH=.
```

## 🐛 调试配置

### 启用调试
1. 确保编译器选项包含 `-g -O0`
2. 在代码中设置断点
3. 点击 **Debug** 按钮（绿色虫子图标）

### 调试技巧
- **F8**: 单步跳过
- **F7**: 单步进入
- **F9**: 继续执行
- **Shift+F8**: 单步跳出

## ⚠️ 常见问题及解决

### 问题 1：找不到 Botan 库
**错误信息：** `ld: library not found for -lbotan-3`

**解决方案：**
1. 确保 Botan 已正确编译
2. 检查工作目录是否正确
3. 验证库文件存在：
   ```bash
   ls -la /Users/kyler/Documents/code/geekrun/learnc/open_source/botan/libbotan-3.dylib
   ```

### 问题 2：运行时找不到动态库
**错误信息：** `dyld: Library not loaded: libbotan-3.dylib`

**解决方案：**
1. 添加环境变量 `DYLD_LIBRARY_PATH=.`
2. 或在编译选项中添加 `-Wl,-rpath,.`

### 问题 3：头文件找不到
**错误信息：** `fatal error: 'botan/block_cipher.h' file not found`

**解决方案：**
1. 检查包含路径：`-I./build/include/public`
2. 确保 Botan 已配置和构建：
   ```bash
   python3 configure.py && make
   ```

## 📋 完整配置检查清单

- [ ] Botan 库已编译完成
- [ ] 源文件路径正确
- [ ] 编译器选项包含所有必要参数
- [ ] 工作目录设置为 Botan 根目录
- [ ] 环境变量 DYLD_LIBRARY_PATH 已设置
- [ ] 可以成功编译
- [ ] 可以成功运行
- [ ] 调试断点正常工作

## 🎉 验证配置

配置完成后，尝试：
1. **编译**：点击锤子图标
2. **运行**：点击绿色播放按钮  
3. **调试**：设置断点后点击绿色虫子图标

成功的话应该看到类似输出：
```
AES-256 single block encrypt: 8EA2B7CA516745BFEAFC49904B496089
AES-256 single block encrypt: 6BD443EC423D5CD317672847EEB62E81
```

祝你在 CLion 中调试 Botan 愉快！🚀
