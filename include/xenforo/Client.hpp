#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "xenforo/HttpClient.hpp"
#include "xenforo/Types.hpp"

namespace xenforo {

// Thrown when XenForo returns a non-2xx status with a structured error body.
class ApiError : public std::runtime_error {
public:
    ApiError(long status, std::string code, std::string message)
        : std::runtime_error(message),
          status_code(status),
          error_code(std::move(code)) {}

    long status_code = 0;
    std::string error_code;  // XenForo machine-readable error code, if present
};

struct Config {
    // Base forum URL, with or without a trailing "/api". Examples:
    //   "https://example.com/community"
    //   "https://example.com/community/api"
    std::string base_url;

    // The XF-Api-Key value. For login validation you need a SUPER USER key.
    std::string api_key;

    // Optional default context user (XF-Api-User). Only meaningful for super
    // user keys. Individual calls can override this.
    std::optional<int> default_api_user;

    bool verify_ssl = true;
    long timeout_seconds = 30;
};

class Client {
public:
    explicit Client(Config config);

    // --- Authentication ----------------------------------------------------

    // Validate a login (username or email) and password.
    // POST /auth  -- requires a super user key.
    AuthResult authenticate(const std::string& login,
                            const std::string& password);

    // Resolve an active user from XF session/remember cookies (SSO helper).
    // POST /auth/from-session -- requires a super user key.
    AuthResult auth_from_session(
        const std::optional<std::string>& session_id,
        const std::optional<std::string>& remember_cookie);

    // Generate a one-time login token for a user.
    // POST /auth/login-token -- requires a super user key.
    LoginToken create_login_token(int user_id,
                                  const std::optional<std::string>& return_url = std::nullopt,
                                  bool remember = false);

    // --- Users -------------------------------------------------------------

    // GET /users/{id}
    User get_user(int user_id);

    // GET /users/find-name?username=...  (returns the best match, if any)
    std::optional<User> find_user_by_name(const std::string& username);

    // GET /me  -- the user the request is authenticated as.
    User get_me(std::optional<int> as_user = std::nullopt);

    // --- User upgrades -----------------------------------------------------
    // XenForo has no native endpoint to read user upgrades. The supported
    // approach is to back each upgrade with a (hidden) user group and check
    // group membership. Configure your upgrades and use these helpers.

    void set_upgrade_definitions(std::vector<UpgradeDefinition> defs) {
        upgrades_ = std::move(defs);
    }

    // Resolve all configured upgrades for a user via group membership.
    std::vector<ActiveUpgrade> get_user_upgrades(int user_id);

    // Convenience: is a specific configured upgrade active for the user?
    bool has_upgrade(int user_id, int upgrade_id);

    // Escape hatch for a custom addon endpoint that returns upgrade JSON.
    // Performs GET on the given path (relative to /api) and returns the body.
    nlohmann::json get_custom(const std::string& path,
                              const Params& query = {},
                              std::optional<int> as_user = std::nullopt);

    // --- Low level ---------------------------------------------------------
    // Generic request returning parsed JSON. `path` is relative to /api,
    // e.g. "users/1". Throws ApiError on non-2xx responses.
    nlohmann::json request(const std::string& method, const std::string& path,
                           const Params& params = {},
                           std::optional<int> as_user = std::nullopt);

private:
    Headers build_headers(std::optional<int> as_user) const;
    std::string make_url(const std::string& path) const;
    nlohmann::json parse_or_throw(const HttpResponse& response) const;

    Config config_;
    std::string api_base_;  // normalised "<base>/api"
    HttpClient http_;
    std::vector<UpgradeDefinition> upgrades_;
};

}  // namespace xenforo
