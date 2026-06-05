#include "xenforo/HttpClient.hpp"

#include <curl/curl.h>

#include <algorithm>
#include <cctype>
#include <mutex>
#include <sstream>

namespace xenforo {
namespace {

// curl_global_init must be called once before any easy handle is created.
std::once_flag g_curl_init_flag;
void ensure_global_init() {
    std::call_once(g_curl_init_flag, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

size_t write_body_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

size_t header_cb(char* buffer, size_t size, size_t nitems, void* userdata) {
    const size_t total = size * nitems;
    auto* headers = static_cast<std::map<std::string, std::string>*>(userdata);
    std::string line(buffer, total);

    const auto colon = line.find(':');
    if (colon != std::string::npos) {
        std::string key = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        const auto trim = [](std::string& s) {
            const auto not_space = [](unsigned char c) { return !std::isspace(c); };
            s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
            s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
        };
        trim(key);
        trim(value);
        if (!key.empty()) {
            (*headers)[key] = value;
        }
    }
    return total;
}

}  // namespace

HttpClient::HttpClient() {
    ensure_global_init();
    curl_ = curl_easy_init();
    if (curl_ == nullptr) {
        throw TransportError("Failed to initialise libcurl easy handle");
    }
}

HttpClient::~HttpClient() {
    if (curl_ != nullptr) {
        curl_easy_cleanup(static_cast<CURL*>(curl_));
    }
}

HttpClient::HttpClient(HttpClient&& other) noexcept
    : curl_(other.curl_),
      verify_ssl_(other.verify_ssl_),
      timeout_seconds_(other.timeout_seconds_) {
    other.curl_ = nullptr;
}

HttpClient& HttpClient::operator=(HttpClient&& other) noexcept {
    if (this != &other) {
        if (curl_ != nullptr) {
            curl_easy_cleanup(static_cast<CURL*>(curl_));
        }
        curl_ = other.curl_;
        verify_ssl_ = other.verify_ssl_;
        timeout_seconds_ = other.timeout_seconds_;
        other.curl_ = nullptr;
    }
    return *this;
}

std::string HttpClient::url_encode(const std::string& value) const {
    auto* curl = static_cast<CURL*>(curl_);
    char* escaped = curl_easy_escape(curl, value.c_str(),
                                     static_cast<int>(value.size()));
    if (escaped == nullptr) {
        return value;
    }
    std::string result(escaped);
    curl_free(escaped);
    return result;
}

std::string HttpClient::encode_params(const Params& params) const {
    std::ostringstream oss;
    bool first = true;
    for (const auto& [key, value] : params) {
        if (!first) {
            oss << '&';
        }
        first = false;
        oss << url_encode(key) << '=' << url_encode(value);
    }
    return oss.str();
}

HttpResponse HttpClient::perform(const std::string& method,
                                 const std::string& url, const Headers& headers,
                                 const Params& params, bool params_in_body) {
    auto* curl = static_cast<CURL*>(curl_);
    curl_easy_reset(curl);

    std::string final_url = url;
    std::string body;
    if (params_in_body) {
        body = encode_params(params);
    } else if (!params.empty()) {
        final_url += (final_url.find('?') == std::string::npos ? '?' : '&');
        final_url += encode_params(params);
    }

    curl_easy_setopt(curl, CURLOPT_URL, final_url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds_);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, verify_ssl_ ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, verify_ssl_ ? 2L : 0L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "XenforoCpp/0.1");

    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
                         static_cast<long>(body.size()));
    } else if (method == "DELETE") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    }
    // GET is the default after reset.

    struct curl_slist* header_list = nullptr;
    if (params_in_body) {
        header_list = curl_slist_append(
            header_list, "Content-Type: application/x-www-form-urlencoded");
    }
    for (const auto& [key, value] : headers) {
        const std::string h = key + ": " + value;
        header_list = curl_slist_append(header_list, h.c_str());
    }
    if (header_list != nullptr) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
    }

    HttpResponse response;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_body_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_cb);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response.headers);

    const CURLcode rc = curl_easy_perform(curl);
    if (header_list != nullptr) {
        curl_slist_free_all(header_list);
    }

    if (rc != CURLE_OK) {
        throw TransportError(std::string("HTTP request failed: ") +
                             curl_easy_strerror(rc));
    }

    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    response.status_code = status;
    return response;
}

HttpResponse HttpClient::get(const std::string& url, const Headers& headers,
                             const Params& query) {
    return perform("GET", url, headers, query, /*params_in_body=*/false);
}

HttpResponse HttpClient::post(const std::string& url, const Headers& headers,
                              const Params& form) {
    return perform("POST", url, headers, form, /*params_in_body=*/true);
}

HttpResponse HttpClient::del(const std::string& url, const Headers& headers,
                             const Params& query) {
    return perform("DELETE", url, headers, query, /*params_in_body=*/false);
}

}  // namespace xenforo
