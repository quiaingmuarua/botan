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
    return sockfd;
}

// =============== TLS 回调实现 ===============
class Callbacks : public Botan::TLS::Callbacks {
   public:
      explicit Callbacks(int socket_fd) : m_fd(socket_fd) {}

      void tls_emit_data(std::span<const uint8_t> data) override {
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
         std::cout.write(reinterpret_cast<const char*>(data.data()),
                         static_cast<std::streamsize>(data.size()));
         std::cout.flush();
      }

      void tls_alert(Botan::TLS::Alert alert) override {
         std::cerr << "[TLS alert] " << alert.type_string()
                   << (alert.is_fatal() ? " (fatal)" : "") << "\n";
      }

      // ---- 握手完成回调：同时兼容 Botan 2 与 Botan 3 ----
      #if BOTAN_VERSION_MAJOR >= 3
      void tls_session_activated(const Botan::TLS::Session_Summary& /*summary*/) {
         std::cerr << "[TLS] handshake complete\n";
      }
      #else
      void tls_session_activated() override {
         std::cerr << "[TLS] handshake complete\n";
      }
      #endif

   private:
      int m_fd;
};


// =============== 证书/凭据管理 ===============
class Client_Credentials : public Botan::Credentials_Manager {
public:
    std::vector<Botan::Certificate_Store*> trusted_certificate_authorities(
        const std::string& /*type*/, const std::string& /*context*/) override {
        return { &m_cert_store }; // 使用系统信任根
    }

    std::vector<Botan::X509_Certificate> cert_chain(
        const std::vector<std::string>& /*cert_key_types*/,
        const std::vector<Botan::AlgorithmIdentifier>& /*cert_signature_schemes*/,
        const std::string& /*type*/,
        const std::string& /*context*/) override {
        return {}; // 无客户端证书
    }

    std::shared_ptr<Botan::Private_Key> private_key_for(
        const Botan::X509_Certificate& /*cert*/,
        const std::string& /*type*/,
        const std::string& /*context*/) override {
        return nullptr;
    }

private:
    Botan::System_Certificate_Store m_cert_store;
};

// =============== 主程序 ===============
int main(int argc, char** argv) {
    const std::string host = (argc > 1) ? argv[1] : "example.com";
    const std::string port = (argc > 2) ? argv[2] : "443";

    int sock = connect_tcp(host, port);
    if (sock < 0) return 1;

    try {
        auto rng        = std::make_shared<Botan::AutoSeeded_RNG>();
        auto callbacks  = std::make_shared<Callbacks>(sock);
        auto sessionMgr = std::make_shared<Botan::TLS::Session_Manager_In_Memory>(rng);
        auto creds      = std::make_shared<Client_Credentials>();

        // 使用默认策略，兼容性更好
        auto policy     = std::make_shared<Botan::TLS::Policy>();

        // 目标服务器信息（用于 SNI/证书主机名校验）
        Botan::TLS::Server_Information server_info(host, static_cast<uint16_t>(std::stoi(port)));

        // 可选：声明我们接受的 ALPN（简化版本）
        std::vector<std::string> alpn = {"http/1.1"};

        // 使用 TLS 1.2（兼容性最好）
        const auto version = Botan::TLS::Protocol_Version::TLS_V12;

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

        while (!client.is_closed()) {
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
            } else if (FD_ISSET(sock, &readfds)) {
                ssize_t got = ::recv(sock, netbuf.data(), netbuf.size(), 0);
                if (got > 0) {
                    client.received_data(std::span<const uint8_t>(netbuf.data(), static_cast<size_t>(got)));
                } else if (got == 0) {
                    // TCP 对端关闭。通知 Botan 我们收不到更多数据了。
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
                std::string req =
                    "GET / HTTP/1.1\r\n"
                    "Host: " + host + "\r\n"
                    "User-Agent: botan-tls-example/1.0\r\n"
                    "Accept: */*\r\n"
                    "Connection: close\r\n"
                    "\r\n";
                client.send(reinterpret_cast<const uint8_t*>(req.data()), req.size());
                request_sent = true;
            }
        }

        if (!peer_closed) {
            // 主动优雅关闭（如果还没收对端 FIN）
            try {
                client.close();
            } catch (...) {
                // 忽略关闭过程中的异常
            }
        }

        ::close(sock);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        ::close(sock);
        return 2;
    }
}
