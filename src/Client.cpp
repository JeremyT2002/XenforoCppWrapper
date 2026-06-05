#include "xenforo/Client.hpp"

#include <algorithm>

namespace xenforo {

namespace {

// Helpers to read optional/loosely-typed JSON fields without throwing.
int as_int(const nlohmann::json& j, const char* key, int fallback = 0) {
    if (j.contains(key) && !j.at(key).is_null()) {
        const auto& v = j.at(key);
        if (v.is_number_integer() || v.is_number_unsigned()) return v.get<int>();
        if (v.is_string()) {
            try {
                return std::stoi(v.get<std::string>());
            } catch (...) {
                return fallback;
            }
        }
    }
    return fallback;
}

std::string as_string(const nlohmann::json& j, const char* key) {
    if (j.contains(key) && j.at(key).is_string()) {
        return j.at(key).get<std::string>();
    }
    return {};
}

bool as_bool(const nlohmann::json& j, const char* key, bool fallback = false) {
    if (j.contains(key) && !j.at(key).is_null()) {
        const auto& v = j.at(key);
        if (v.is_boolean()) return v.get<bool>();
        if (v.is_number()) return v.get<int>() != 0;
    }
    return fallback;
}

}  // namespace

// --- Types -----------------------------------------------------------------

User User::from_json(const nlohmann::json& j) {
    User u;
    u.user_id = as_int(j, "user_id");
    u.username = as_string(j, "username");
    u.email = as_string(j, "email");
    u.user_state = as_string(j, "user_state");
    u.is_banned = as_bool(j, "is_banned");
    u.user_group_id = as_int(j, "user_group_id");
    if (j.contains("secondary_group_ids") && j.at("secondary_group_ids").is_array()) {
        for (const auto& g : j.at("secondary_group_ids")) {
            if (g.is_number()) u.secondary_group_ids.push_back(g.get<int>());
        }
    }
    u.raw = j;
    return u;
}

bool User::in_group(int group_id) const {
    if (user_group_id == group_id) return true;
    return std::find(secondary_group_ids.begin(), secondary_group_ids.end(),
                     group_id) != secondary_group_ids.end();
}

// --- Client ----------------------------------------------------------------

Client::Client(Config config) : config_(std::move(config)) {
    http_.set_verify_ssl(config_.verify_ssl);
    http_.set_timeout_seconds(config_.timeout_seconds);

    std::string base = config_.base_url;
    while (!base.empty() && base.back() == '/') base.pop_back();

    // Append "/api" unless the caller already pointed at it.
    const std::string suffix = "/api";
    const bool ends_with_api =
        base.size() >= suffix.size() &&
        base.compare(base.size() - suffix.size(), suffix.size(), suffix) == 0;
    api_base_ = ends_with_api ? base : base + suffix;
}

std::string Client::make_url(const std::string& path) const {
    std::string p = path;
    while (!p.empty() && p.front() == '/') p.erase(p.begin());
    return api_base_ + "/" + p;
}

Headers Client::build_headers(std::optional<int> as_user) const {
    Headers headers;
    headers.emplace_back("XF-Api-Key", config_.api_key);

    std::optional<int> ctx = as_user ? as_user : config_.default_api_user;
    if (ctx) {
        headers.emplace_back("XF-Api-User", std::to_string(*ctx));
    }
    return headers;
}

nlohmann::json Client::parse_or_throw(const HttpResponse& response) const {
    nlohmann::json body;
    if (!response.body.empty()) {
        body = nlohmann::json::parse(response.body, nullptr, /*allow_exceptions=*/false);
    }

    if (response.ok()) {
        if (body.is_discarded()) {
            throw ApiError(response.status_code, "invalid_json",
                           "Response was not valid JSON");
        }
        return body;
    }

    // Non-2xx: try to surface XenForo's structured error payload.
    std::string code = "http_error";
    std::string message = "HTTP " + std::to_string(response.status_code);
    if (!body.is_discarded() && body.contains("errors") &&
        body.at("errors").is_array() && !body.at("errors").empty()) {
        const auto& err = body.at("errors").front();
        if (err.contains("code") && err.at("code").is_string())
            code = err.at("code").get<std::string>();
        if (err.contains("message") && err.at("message").is_string())
            message = err.at("message").get<std::string>();
    }
    throw ApiError(response.status_code, code, message);
}

nlohmann::json Client::request(const std::string& method,
                               const std::string& path, const Params& params,
                               std::optional<int> as_user) {
    const std::string url = make_url(path);
    const Headers headers = build_headers(as_user);

    HttpResponse response;
    if (method == "GET") {
        response = http_.get(url, headers, params);
    } else if (method == "POST") {
        response = http_.post(url, headers, params);
    } else if (method == "DELETE") {
        response = http_.del(url, headers, params);
    } else {
        throw std::invalid_argument("Unsupported HTTP method: " + method);
    }
    return parse_or_throw(response);
}

// --- Authentication --------------------------------------------------------

AuthResult Client::authenticate(const std::string& login,
                                const std::string& password) {
    const nlohmann::json body =
        request("POST", "auth", {{"login", login}, {"password", password}});

    AuthResult result;
    result.raw = body;
    result.success = as_bool(body, "success");
    if (result.success && body.contains("user") && body.at("user").is_object()) {
        result.user = User::from_json(body.at("user"));
    } else {
        result.error = as_string(body, "error");
        if (result.error.empty()) result.error = "Authentication failed";
    }
    return result;
}

AuthResult Client::auth_from_session(
    const std::optional<std::string>& session_id,
    const std::optional<std::string>& remember_cookie) {
    Params params;
    if (session_id) params.emplace_back("session_id", *session_id);
    if (remember_cookie) params.emplace_back("remember_cookie", *remember_cookie);

    const nlohmann::json body = request("POST", "auth/from-session", params);

    AuthResult result;
    result.raw = body;
    result.success = as_bool(body, "success");
    if (result.success && body.contains("user") && body.at("user").is_object()) {
        result.user = User::from_json(body.at("user"));
    } else {
        result.error = as_string(body, "error");
        if (result.error.empty()) result.error = "Session lookup failed";
    }
    return result;
}

LoginToken Client::create_login_token(
    int user_id, const std::optional<std::string>& return_url, bool remember) {
    Params params;
    params.emplace_back("user_id", std::to_string(user_id));
    params.emplace_back("remember", remember ? "1" : "0");
    if (return_url) params.emplace_back("return_url", *return_url);

    const nlohmann::json body = request("POST", "auth/login-token", params);

    LoginToken token;
    token.raw = body;
    token.login_token = as_string(body, "login_token");
    token.login_url = as_string(body, "login_url");
    if (body.contains("expiry_date") && body.at("expiry_date").is_number()) {
        token.expiry_date = body.at("expiry_date").get<int64_t>();
    }
    return token;
}

// --- Users -----------------------------------------------------------------

User Client::get_user(int user_id) {
    const nlohmann::json body =
        request("GET", "users/" + std::to_string(user_id));
    if (body.contains("user") && body.at("user").is_object()) {
        return User::from_json(body.at("user"));
    }
    throw ApiError(200, "unexpected_response", "No 'user' object in response");
}

std::optional<User> Client::find_user_by_name(const std::string& username) {
    const nlohmann::json body =
        request("GET", "users/find-name", {{"username", username}});
    // XF returns either {"exact": {...}} / {"recommendations": [...]} depending
    // on version; handle the common shapes.
    if (body.contains("exact") && body.at("exact").is_object()) {
        return User::from_json(body.at("exact"));
    }
    if (body.contains("user") && body.at("user").is_object()) {
        return User::from_json(body.at("user"));
    }
    if (body.contains("recommendations") &&
        body.at("recommendations").is_array() &&
        !body.at("recommendations").empty()) {
        return User::from_json(body.at("recommendations").front());
    }
    return std::nullopt;
}

User Client::get_me(std::optional<int> as_user) {
    const nlohmann::json body = request("GET", "me", {}, as_user);
    if (body.contains("me") && body.at("me").is_object()) {
        return User::from_json(body.at("me"));
    }
    if (body.contains("user") && body.at("user").is_object()) {
        return User::from_json(body.at("user"));
    }
    throw ApiError(200, "unexpected_response", "No 'me' object in response");
}

// --- User upgrades ----------------------------------------------------------

std::vector<ActiveUpgrade> Client::get_user_upgrades(int user_id) {
    const User user = get_user(user_id);

    std::vector<ActiveUpgrade> result;
    result.reserve(upgrades_.size());
    for (const auto& def : upgrades_) {
        ActiveUpgrade au;
        au.upgrade_id = def.upgrade_id;
        au.name = def.name;
        au.group_id = def.group_id;
        au.active = user.in_group(def.group_id);
        result.push_back(std::move(au));
    }
    return result;
}

bool Client::has_upgrade(int user_id, int upgrade_id) {
    const auto upgrades = get_user_upgrades(user_id);
    for (const auto& u : upgrades) {
        if (u.upgrade_id == upgrade_id) return u.active;
    }
    return false;
}

nlohmann::json Client::get_custom(const std::string& path, const Params& query,
                                  std::optional<int> as_user) {
    return request("GET", path, query, as_user);
}

}  // namespace xenforo
