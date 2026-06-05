#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace xenforo {

struct User {
    int user_id = 0;
    std::string username;
    std::string email;
    std::string user_state;
    bool is_banned = false;
    int user_group_id = 0;
    std::vector<int> secondary_group_ids;
    nlohmann::json raw;

    bool in_group(int group_id) const;

    static User from_json(const nlohmann::json& j);
};

struct AuthResult {
    bool success = false;
    std::optional<User> user;
    std::string error;
    nlohmann::json raw;
};

struct LoginToken {
    std::string login_token;
    std::string login_url;
    int64_t expiry_date = 0;
    nlohmann::json raw;
};

struct UpgradeDefinition {
    int upgrade_id = 0;
    std::string name;
    int group_id = 0;                  // hidden group granted by the upgrade
};

struct ActiveUpgrade {
    int upgrade_id = 0;
    std::string name;
    int group_id = 0;
    bool active = false;
    int64_t start_date = 0;
    int64_t end_date = 0;              // 0 == permanent

    bool is_permanent() const { return end_date == 0; }

    // 0 if expired, -1 if permanent.
    int64_t remaining_seconds(int64_t now = 0) const;

    bool expired(int64_t now = 0) const;

    // "12d 3h", "permanent" or "expired".
    std::string remaining_human(int64_t now = 0) const;
};

}  // namespace xenforo
