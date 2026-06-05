// Example: validate a login against XenForo and check user upgrades.
//
// Build (see README) and run with env vars:
//   XF_URL=https://example.com/community
//   XF_API_KEY=<super user key>
//
// NEVER hard-code your API key or ship it to a client. Keep the super user key
// on a trusted server only.

#include <cstdlib>
#include <iostream>
#include <string>

#include "xenforo/Client.hpp"

namespace {
std::string env_or(const char* name, const std::string& fallback) {
    const char* v = std::getenv(name);
    return v ? std::string(v) : fallback;
}
}  // namespace

int main(int argc, char** argv) {
    xenforo::Config config;
    config.base_url = env_or("XF_URL", "https://example.com/community");
    config.api_key = env_or("XF_API_KEY", "");

    if (config.api_key.empty()) {
        std::cerr << "Set XF_API_KEY (a super user key) first.\n";
        return 1;
    }

    xenforo::Client client(config);

    // Map your forum's upgrades to the (hidden) user groups they grant.
    client.set_upgrade_definitions({
        {/*upgrade_id*/ 1, "Premium", /*group_id*/ 10},
        {/*upgrade_id*/ 2, "VIP", /*group_id*/ 11},
    });

    const std::string login = (argc > 1) ? argv[1] : "testuser";
    const std::string password = (argc > 2) ? argv[2] : "secret";

    try {
        const auto auth = client.authenticate(login, password);
        if (!auth.success || !auth.user) {
            std::cout << "Login failed: " << auth.error << "\n";
            return 2;
        }

        const auto& user = *auth.user;
        std::cout << "Logged in as " << user.username << " (#" << user.user_id
                  << "), state=" << user.user_state << "\n";

        std::cout << "Upgrades:\n";
        for (const auto& up : client.get_user_upgrades(user.user_id)) {
            std::cout << "  - " << up.name << ": "
                      << (up.active ? "ACTIVE" : "inactive") << "\n";
        }
    } catch (const xenforo::ApiError& e) {
        std::cerr << "API error [" << e.status_code << "/" << e.error_code
                  << "]: " << e.what() << "\n";
        return 3;
    } catch (const xenforo::TransportError& e) {
        std::cerr << "Transport error: " << e.what() << "\n";
        return 4;
    }

    return 0;
}
