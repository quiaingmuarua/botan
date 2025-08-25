# TLS 握手失败问题分析与解决方案

## 🔍 问题诊断

你遇到的 `[TLS alert] handshake_failure (fatal)` 错误的可能原因：

### 1. TLS 版本不兼容
- **原始代码使用 TLS 1.3**：`Protocol_Version::TLS_V13`
- **建议改为 TLS 1.2**：`Protocol_Version::TLS_V12`

### 2. 策略过于严格
- **原始代码使用**：`Strict_Policy`
- **建议改为**：`Policy()` （默认策略）

### 3. 目标服务器问题
- **Cloudflare.com** 可能有特殊的 TLS 配置要求
- **建议测试目标**：`example.com`, `google.com`, `httpbin.org`

## 🔧 快速修复方案

### 修复 1：更改 TLS 版本
在原始 `tls_client.cpp` 的第 160 行：

```cpp
// 修改前
const auto version = Botan::TLS::Protocol_Version::TLS_V13;

// 修改后  
const auto version = Botan::TLS::Protocol_Version::TLS_V12;
```

### 修复 2：使用默认策略
在第 151 行：

```cpp
// 修改前
auto policy = std::make_shared<Botan::TLS::Strict_Policy>();

// 修改后
auto policy = std::make_shared<Botan::TLS::Policy>();
```

### 修复 3：简化 ALPN
在第 157 行：

```cpp
// 修改前
std::vector<std::string> alpn = {"h2", "http/1.1"};

// 修改后
std::vector<std::string> alpn = {"http/1.1"};
```

### 修复 4：更改测试目标
在第 138-139 行：

```cpp
// 修改前
const std::string host = (argc > 1) ? argv[1] : "cloudflare.com";

// 修改后
const std::string host = (argc > 1) ? argv[1] : "example.com";
```

## 🚀 测试方法

### 1. 应用修复后测试
```bash
# 编译
clang++ -std=c++20 -g -I./build/include/public -L. -lbotan-3 src/examples/tls_client.cpp -o tls_client_test

# 运行（设置动态库路径）
DYLD_LIBRARY_PATH=. ./tls_client_test
```

### 2. 测试不同服务器
```bash
# 测试基本的 Web 服务器
DYLD_LIBRARY_PATH=. ./tls_client_test example.com 443
DYLD_LIBRARY_PATH=. ./tls_client_test google.com 443
DYLD_LIBRARY_PATH=. ./tls_client_test httpbin.org 443
```

### 3. 调试模式
如果仍然失败，可以添加调试输出：

```cpp
// 在构建 TLS 客户端前添加
std::cout << "[DEBUG] 使用 TLS 版本: " << version.to_string() << std::endl;
std::cout << "[DEBUG] 连接目标: " << host << ":" << port << std::endl;
```

## 📋 常见 TLS 握手失败原因

| 错误类型 | 可能原因 | 解决方案 |
|---------|---------|---------|
| `handshake_failure` | TLS版本不匹配 | 使用 TLS 1.2 |
| `handshake_failure` | 密码套件不兼容 | 使用默认策略 |
| `protocol_version` | 服务器不支持客户端版本 | 降级到 TLS 1.2 |
| `certificate_unknown` | 证书验证失败 | 检查系统根证书 |
| `internal_error` | Botan 内部错误 | 检查库版本兼容性 |

## 🛠️ CLion 调试配置

在 CLion 中调试 TLS 问题：

1. **编译器选项**：
```bash
-std=c++20 -g -O0 -DDEBUG -I./build/include/public -L. -lbotan-3 -Wl,-rpath,.
```

2. **工作目录**：
```
/Users/kyler/Documents/code/geekrun/learnc/open_source/botan
```

3. **环境变量**：
```
DYLD_LIBRARY_PATH=/Users/kyler/Documents/code/geekrun/learnc/open_source/botan
```

4. **断点位置**：
- 第 163 行：`Botan::TLS::Client client(...)` - TLS 客户端创建
- 第 200 行：`client.received_data(...)` - 接收服务器数据
- 第 88 行：`tls_alert()` 回调 - 捕获 TLS 警告

## 🎯 预期结果

成功连接后，你应该看到类似输出：
```
[TLS] handshake complete
[DEBUG] 发送 HTTP 请求
HTTP/1.1 200 OK
Server: nginx/1.18.0
...
```

## 📞 如果仍然失败

如果修复后仍然出现问题：

1. **检查 Botan 版本**：
```bash
./botan version
```

2. **测试基本连接**：
```bash
curl -v https://example.com
```

3. **检查系统 TLS 工具**：
```bash
openssl s_client -connect example.com:443 -tls1_2
```

4. **查看详细日志**：在代码中添加更多调试输出，特别是在 `tls_alert()` 回调中

记住：TLS 握手失败通常是客户端和服务器之间协商参数不匹配导致的，使用更兼容的配置通常能解决问题。
