#pragma once

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace xenforo {

// Thrown when the HTTP transport layer itself fails (DNS, TLS, timeouts, ...).
class TransportError : public std::runtime_error {
public:
    explicit TransportError(const std::string& message)
        : std::runtime_error(message) {}
};

struct HttpResponse {
    long status_code = 0;
    std::string body;
    std::map<std::string, std::string> headers;

    bool ok() const { return status_code >= 200 && status_code < 300; }
};

using Params = std::vector<std::pair<std::string, std::string>>;
using Headers = std::vector<std::pair<std::string, std::string>>;

// RAII wrapper around libcurl. Not thread-safe; use one instance per thread.
class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;
    HttpClient(HttpClient&&) noexcept;
    HttpClient& operator=(HttpClient&&) noexcept;

    void set_verify_ssl(bool verify) { verify_ssl_ = verify; }
    void set_timeout_seconds(long seconds) { timeout_seconds_ = seconds; }

    HttpResponse get(const std::string& url, const Headers& headers,
                     const Params& query = {});
    HttpResponse post(const std::string& url, const Headers& headers,
                      const Params& form = {});
    HttpResponse del(const std::string& url, const Headers& headers,
                     const Params& query = {});

    std::string url_encode(const std::string& value) const;

private:
    HttpResponse perform(const std::string& method, const std::string& url,
                         const Headers& headers, const Params& params,
                         bool params_in_body);

    std::string encode_params(const Params& params) const;

    void* curl_ = nullptr;
    bool verify_ssl_ = true;
    long timeout_seconds_ = 30;
};

}  // namespace xenforo
