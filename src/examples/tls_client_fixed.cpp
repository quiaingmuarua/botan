#include <botan/auto_rng.h>
#include <botan/certstor.h>
#include <botan/certstor_system.h>
#include <botan/tls.h>
#include <botan/tls_version.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

// ---- POSIX sockets ----
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

// =============== 工具函数：TCP 连接 ===============
int connect_tcp(const std::string& host, const std::string& port) {
    std::cout << "[DEBUG] 正在连接到 " << host << ":" << port << std::endl;
    
    addrinfo hints{};
    hints.ai_family   = AF_UNSPEC;  // IPv4/IPv6
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* res = nullptr;
    int rc = getaddrinfo(host.c_str(), port.c_str(), &hints, &res);
    if (rc != 0) {
        std::cerr << "getaddrinfo failed: " << gai_strerror(rc) << "\n";
        return -1;
    }

    int sockfd = -1;
    for (addrinfo* p = res; p != nullptr; p = p->ai_next) {
        int s = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (s == -1) continue;

        if (::connect(s, p->ai_addr, p->ai_addrlen) == 0) {
            sockfd = s;
            break;
        }

        ::close(s);
    }
    freeaddrinfo(res);

    if (sockfd == -1) {
        std::cerr << "connect() failed to " << host << ":" << port << "\n";
        return -1;
    }
    
    std::cout << "[DEBUG] TCP 连接成功" << std::endl;
    return sockfd;
}

// =============== 自定义 TLS 策略（更宽松） ===============
class Compatible_TLS_Policy : public Botan::TLS::Policy {
public:
    std::vector<std::string> allowed_ciphers() const override {
        // 返回更广泛的密码套件支持
        return {
            "ChaCha20Poly1305",
            "AES-256/GCM",
            "AES-128/GCM",
            "AES-256/CCM",
            "AES-128/CCM",
            "AES-256/OCB(12)",
            "AES-128/OCB(12)",
            "Camellia-256/GCM",
            "Camellia-128/GCM",
            "ARIA-256/GCM",
            "ARIA-128/GCM",
            "AES-256",
            "AES-128",
            "Camellia-256",
            "Camellia-128",
            "SEED",
            "3DES"
        };
    }

    std::vector<std::string> allowed_macs() const override {
        return {
            "AEAD",
            "SHA-384",
            "SHA-256", 
            "SHA-1"
        };
    }

    std::vector<std::string> allowed_key_exchange_methods() const override {
        return {
            "SRP_SHA",
            "ECDHE_PSK",
            "DHE_PSK",
            "PSK",
            "CECPQ1",
            "ECDH",
            "DH",
            "RSA",
        };
    }

    std::vector<std::string> allowed_signature_methods() const override {
        return {
            "ECDSA",
            "RSA",
            "DSA",
            "IMPLICIT",
        };
    }

    bool allow_tls10() const { return true; }
    bool allow_tls11() const { return true; }
    bool allow_tls12() const { return true; }
    bool allow_tls13() const { return true; }
    
    // 允许更多的椭圆曲线
    std::vector<Botan::TLS::Group_Params> key_exchange_groups() const override {
        return {
            Botan::TLS::Group_Params::SECP256R1,
            Botan::TLS::Group_Params::SECP384R1,
            Botan::TLS::Group_Params::SECP521R1,
            Botan::TLS::Group_Params::X25519,
            Botan::TLS::Group_Params::FFDHE_2048,
            Botan::TLS::Group_Params::FFDHE_3072,
            Botan::TLS::Group_Params::FFDHE_4096,
        };
    }
};

// =============== TLS 回调实现（增强调试信息） ===============
class Callbacks : public Botan::TLS::Callbacks {
   public:
      explicit Callbacks(int socket_fd) : m_fd(socket_fd) {}

      void tls_emit_data(std::span<const uint8_t> data) override {
         std::cout << "[DEBUG] 发送 TLS 数据: " << data.size() << " bytes" << std::endl;
         const uint8_t* p = data.data();
         size_t remaining = data.size();
         while (remaining > 0) {
            ssize_t sent = ::send(m_fd, p, remaining, 0);
            if (sent < 0) {
               if (errno == EINTR) continue;
               std::perror("send");
               break;
            }
            p += sent;
            remaining -= static_cast<size_t>(sent);
         }
      }

      void tls_record_received(uint64_t, std::span<const uint8_t> data) override {
         std::cout << "[DEBUG] 接收到应用数据: " << data.size() << " bytes" << std::endl;
         std::cout.write(reinterpret_cast<const char*>(data.data()),
                         static_cast<std::streamsize>(data.size()));
         std::cout.flush();
      }

      void tls_alert(Botan::TLS::Alert alert) override {
         std::cerr << "[TLS alert] " << alert.type_string()
                   << (alert.is_fatal() ? " (fatal)" : " (warning)")
                   << " - 代码: " << static_cast<int>(alert.type()) << std::endl;
      }

      // ---- 握手完成回调：同时兼容 Botan 2 与 Botan 3 ----
      #if BOTAN_VERSION_MAJOR >= 3
      void tls_session_activated(const Botan::TLS::Session_Summary& summary) {
         std::cout << "[TLS] 握手成功完成！" << std::endl;
         std::cout << "[TLS] 协议版本: " << summary.version().to_string() << std::endl;
         std::cout << "[TLS] 密码套件: " << summary.ciphersuite().to_string() << std::endl;
         if (!summary.server_info().hostname().empty()) {
            std::cout << "[TLS] 服务器: " << summary.server_info().hostname() << std::endl;
         }
      }
      #else
      void tls_session_activated() override {
         std::cout << "[TLS] 握手成功完成！" << std::endl;
      }
      #endif

      void tls_verify_cert_chain(
         const std::vector<Botan::X509_Certificate>& cert_chain,
         const std::vector<std::optional<Botan::OCSP::Response>>& ocsp_responses,
         const std::vector<Botan::Certificate_Store*>& trusted_roots,
         Botan::Usage_Type usage,
         std::string_view hostname,
         const Botan::TLS::Policy& policy) override {
         
         std::cout << "[DEBUG] 证书链验证 - 主机名: " << hostname << std::endl;
         std::cout << "[DEBUG] 证书链长度: " << cert_chain.size() << std::endl;
         
         try {
            // 调用默认的证书验证
            Botan::TLS::Callbacks::tls_verify_cert_chain(
               cert_chain, ocsp_responses, trusted_roots, usage, hostname, policy);
            std::cout << "[DEBUG] 证书验证通过" << std::endl;
         } catch (const std::exception& e) {
            std::cerr << "[ERROR] 证书验证失败: " << e.what() << std::endl;
            throw;
         }
      }

   private:
      int m_fd;
};

// =============== 证书/凭据管理 ===============
class Client_Credentials : public Botan::Credentials_Manager {
public:
    std::vector<Botan::Certificate_Store*> trusted_certificate_authorities(
        const std::string& type, const std::string& context) override {
        std::cout << "[DEBUG] 请求信任根证书 - 类型: " << type << ", 上下文: " << context << std::endl;
        return { &m_cert_store }; // 使用系统信任根
    }

    std::vector<Botan::X509_Certificate> cert_chain(
        const std::vector<std::string>& cert_key_types,
        const std::vector<Botan::AlgorithmIdentifier>& cert_signature_schemes,
        const std::string& type,
        const std::string& context) override {
        std::cout << "[DEBUG] 请求客户端证书链 - 类型: " << type << std::endl;
        return {}; // 无客户端证书
    }

    std::shared_ptr<Botan::Private_Key> private_key_for(
        const Botan::X509_Certificate& cert,
        const std::string& type,
        const std::string& context) override {
        return nullptr;
    }

private:
    Botan::System_Certificate_Store m_cert_store;
};

// =============== 主程序 ===============
int main(int argc, char** argv) {
    // 使用更简单的测试目标
    const std::string host = (argc > 1) ? argv[1] : "httpbin.org";
    const std::string port = (argc > 2) ? argv[2] : "443";

    std::cout << "=== Botan TLS 客户端示例 (修复版) ===" << std::endl;
    std::cout << "目标: " << host << ":" << port << std::endl;

    int sock = connect_tcp(host, port);
    if (sock < 0) return 1;

    try {
        auto rng        = std::make_shared<Botan::AutoSeeded_RNG>();
        auto callbacks  = std::make_shared<Callbacks>(sock);
        auto sessionMgr = std::make_shared<Botan::TLS::Session_Manager_In_Memory>(rng);
        auto creds      = std::make_shared<Client_Credentials>();

        // 使用兼容性更好的策略
        auto policy     = std::make_shared<Compatible_TLS_Policy>();

        // 目标服务器信息（用于 SNI/证书主机名校验）
        Botan::TLS::Server_Information server_info(host, static_cast<uint16_t>(std::stoi(port)));

        // 可选：声明我们接受的 ALPN
        std::vector<std::string> alpn = {"http/1.1"};  // 简化 ALPN，不使用 h2

        // 优先使用 TLS 1.2（更兼容）
        const auto version = Botan::TLS::Protocol_Version::TLS_V12;
        
        std::cout << "[DEBUG] 开始 TLS 握手..." << std::endl;

        // 构建 TLS 客户端会立即产生 ClientHello，通过 tls_emit_data 发到网络
        Botan::TLS::Client client(
            callbacks,
            sessionMgr,
            creds,
            policy,
            rng,
            server_info,
            version,
            alpn
        );

        // 等握手完成后再发送 HTTP 请求
        bool request_sent = false;

        // 简单的事件循环：读 socket -> 投喂给 Botan；握手后发送请求；根据状态关闭
        std::array<uint8_t, 16 * 1024> netbuf{};
        bool peer_closed = false;
        int timeout_count = 0;
        const int max_timeouts = 10;  // 最多等待 50 秒

        while (!client.is_closed() && timeout_count < max_timeouts) {
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(sock, &readfds);
            timeval tv{};
            tv.tv_sec = 5;  // 超时避免永久阻塞，便于检查状态

            int n = ::select(sock + 1, &readfds, nullptr, nullptr, &tv);
            if (n < 0) {
                if (errno == EINTR) continue;
                std::perror("select");
                break;
            }

            if (n == 0) {
                // 超时：如果已握手且未发送请求，试着发送；否则继续等
                timeout_count++;
                std::cout << "[DEBUG] 超时等待 (" << timeout_count << "/" << max_timeouts << ")" << std::endl;
                
                if (client.is_active() && !request_sent) {
                    // 握手已完成但还没发送请求，现在发送
                    std::cout << "[DEBUG] 握手已完成，发送 HTTP 请求" << std::endl;
                    std::string req =
                        "GET /get HTTP/1.1\r\n"
                        "Host: " + host + "\r\n"
                        "User-Agent: botan-tls-example/1.0\r\n"
                        "Accept: */*\r\n"
                        "Connection: close\r\n"
                        "\r\n";
                    client.send(reinterpret_cast<const uint8_t*>(req.data()), req.size());
                    request_sent = true;
                }
                continue;
            } else if (FD_ISSET(sock, &readfds)) {
                ssize_t got = ::recv(sock, netbuf.data(), netbuf.size(), 0);
                if (got > 0) {
                    std::cout << "[DEBUG] 接收到网络数据: " << got << " bytes" << std::endl;
                    client.received_data(std::span<const uint8_t>(netbuf.data(), static_cast<size_t>(got)));
                    timeout_count = 0;  // 重置超时计数
                } else if (got == 0) {
                    // TCP 对端关闭。通知 Botan 我们收不到更多数据了。
                    std::cout << "[DEBUG] 对端关闭连接" << std::endl;
                    peer_closed = true;
                    client.close(); // 触发 close_notify（如果可能）
                    break;
                } else {
                    if (errno == EINTR) continue;
                    std::perror("recv");
                    break;
                }
            }

            // 握手完成且还没发请求 -> 发送一个简单的 HTTP/1.1 GET，并声明 Connection: close
            if (!request_sent && client.is_active()) {
                std::cout << "[DEBUG] 发送 HTTP 请求" << std::endl;
                std::string req =
                    "GET /get HTTP/1.1\r\n"
                    "Host: " + host + "\r\n"
                    "User-Agent: botan-tls-example/1.0\r\n"
                    "Accept: */*\r\n"
                    "Connection: close\r\n"
                    "\r\n";
                client.send(reinterpret_cast<const uint8_t*>(req.data()), req.size());
                request_sent = true;
            }
        }

        if (timeout_count >= max_timeouts) {
            std::cout << "[WARNING] 达到最大超时次数，结束连接" << std::endl;
        }

        if (!peer_closed) {
            // 主动优雅关闭（如果还没收对端 FIN）
            std::cout << "[DEBUG] 主动关闭 TLS 连接" << std::endl;
            try {
                client.close();
            } catch (...) {
                // 忽略关闭过程中的异常
            }
        }

        ::close(sock);
        std::cout << "\n=== 连接完成 ===" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        ::close(sock);
        return 2;
    }
}
