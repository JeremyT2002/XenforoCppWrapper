#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace xenforo {

// Minimal projection of a XenForo "User" entity. The raw JSON is kept around in
// `raw` so callers can reach fields that aren't mapped explicitly.
struct User {
    int user_id = 0;
    std::string username;
    std::string email;
    std::string user_state;            // e.g. "valid", "email_confirm"
    bool is_banned = false;
    int user_group_id = 0;             // primary group
    std::vector<int> secondary_group_ids;
    nlohmann::json raw;

    // True if the user belongs to `group_id` either as primary or secondary
    // group. This is the building block for the "upgrade = hidden group"
    // pattern recommended by XenForo (there is no native upgrade read API).
    bool in_group(int group_id) const;

    static User from_json(const nlohmann::json& j);
};

// Result of POST /auth (login + password validation, super user key only).
struct AuthResult {
    bool success = false;
    std::optional<User> user;          // populated on success
    std::string error;                 // populated on failure
    nlohmann::json raw;
};

// A short-lived token that logs a visitor into a specific account.
struct LoginToken {
    std::string login_token;
    std::string login_url;             // ready-to-use URL when provided by XF
    int64_t expiry_date = 0;
    nlohmann::json raw;
};

// Describes one "upgrade" mapped to a XenForo user group. Configure these for
// your forum so the client can translate group membership into upgrade state.
struct UpgradeDefinition {
    int upgrade_id = 0;                // your own identifier / XF user_upgrade_id
    std::string name;
    int group_id = 0;                  // hidden group granted by the upgrade
};

// Resolved upgrade state for a user.
struct ActiveUpgrade {
    int upgrade_id = 0;
    std::string name;
    int group_id = 0;
    bool active = false;
};

}  // namespace xenforo
