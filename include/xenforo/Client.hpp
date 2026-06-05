#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "xenforo/HttpClient.hpp"
#include "xenforo/Types.hpp"

namespace xenforo {

class ApiError : public std::runtime_error {
public:
    ApiError(long status, std::string code, std::string message)
        : std::runtime_error(message),
          status_code(status),
          error_code(std::move(code)) {}

    long status_code = 0;
    std::string error_code;
};

struct Config {
    // With or without a trailing "/api", e.g. "https://example.com/community".
    std::string base_url;

    // For login validation you need a SUPER USER key.
    std::string api_key;

    // Optional default context user (XF-Api-User), super user keys only.
    std::optional<int> default_api_user;

    bool verify_ssl = true;
    long timeout_seconds = 30;
};

class Client {
public:
    explicit Client(Config config);

    // POST /auth
    AuthResult authenticate(const std::string& login,
                            const std::string& password);

    // POST /auth/from-session
    AuthResult auth_from_session(
        const std::optional<std::string>& session_id,
        const std::optional<std::string>& remember_cookie);

    // POST /auth/login-token
    LoginToken create_login_token(int user_id,
                                  const std::optional<std::string>& return_url = std::nullopt,
                                  bool remember = false);

    // GET /users/{id}
    User get_user(int user_id);

    // GET /users/find-name?username=...
    std::optional<User> find_user_by_name(const std::string& username);

    // GET /me
    User get_me(std::optional<int> as_user = std::nullopt);

    void set_upgrade_definitions(std::vector<UpgradeDefinition> defs) {
        upgrades_ = std::move(defs);
    }

    // Active/inactive only, resolved via group membership (no expiry date).
    std::vector<ActiveUpgrade> get_user_upgrades(int user_id);

    bool has_upgrade(int user_id, int upgrade_id);

    // Detailed upgrades incl. start/end dates via a custom add-on endpoint
    // (bundled RetteDasCode/UserUpgradeAPI). {user_id} is substituted in path.
    std::vector<ActiveUpgrade> get_user_upgrades_detailed(
        int user_id,
        const std::string& endpoint_path = "user-upgrades/{user_id}",
        std::optional<int> as_user = std::nullopt);

    nlohmann::json get_custom(const std::string& path,
                              const Params& query = {},
                              std::optional<int> as_user = std::nullopt);

    // Generic request. `path` is relative to /api; throws ApiError on non-2xx.
    nlohmann::json request(const std::string& method, const std::string& path,
                           const Params& params = {},
                           std::optional<int> as_user = std::nullopt);

private:
    Headers build_headers(std::optional<int> as_user) const;
    std::string make_url(const std::string& path) const;
    nlohmann::json parse_or_throw(const HttpResponse& response) const;

    Config config_;
    std::string api_base_;
    HttpClient http_;
    std::vector<UpgradeDefinition> upgrades_;
};

}  // namespace xenforo
