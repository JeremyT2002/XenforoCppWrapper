# XenforoCpp

A lightweight, modern C++17 wrapper for the [XenForo REST API](https://docs.xenforo.com/api).
Designed to connect your own app to a XenForo forum's **login** and **user upgrades**.

## Features

- `authenticate(login, password)` – validates credentials via `POST /auth`
- `auth_from_session(...)` – SSO helper via `POST /auth/from-session`
- `create_login_token(user_id)` – one-time login token via `POST /auth/login-token`
- `get_user(id)`, `find_user_by_name(name)`, `get_me()`
- `get_user_upgrades(id)` / `has_upgrade(id, upgrade_id)` – upgrade detection via user groups
- Generic `request(method, path, params)` for any endpoint

## Important notes about the XenForo API

1. **A super user key is required.** Login validation (`/auth`) is, per XenForo,
   only available with a **super user API key**. Create one in the ACP under
   *Setup > API keys*.
2. **Never ship the API key to the client.** The key belongs on a trusted
   server. A desktop/mobile app should talk to your own backend instead of
   using the key directly.
3. **No native endpoint for user upgrades.** XenForo provides no REST route to
   read active user upgrades. The recommended approach (also suggested by the
   XF community): create a (hidden) **user group** per upgrade that the upgrade
   grants, then check group membership. That is exactly what
   `get_user_upgrades()` does. Alternatively, use `get_custom()` to call your
   own add-on endpoint.

## Dependencies

- A C++17 compiler
- [libcurl](https://curl.se/libcurl/)
- [nlohmann/json](https://github.com/nlohmann/json) (automatically downloaded via CMake `FetchContent` when needed)

If `libcurl` is not found on your system, the build will automatically fetch and
build it from source (using the native Schannel TLS backend on Windows, so no
OpenSSL dependency is required).

### Windows (recommended: vcpkg)

```powershell
vcpkg install curl nlohmann-json
```

## Building

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
# With vcpkg, also add:
#   -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

This produces the `xenforo` library and (optionally) `login_example`.

## Usage

```cpp
#include "xenforo/Client.hpp"

xenforo::Config config;
config.base_url = "https://example.com/community";  // "/api" is appended
config.api_key  = "<your super user key>";

xenforo::Client client(config);

// Map your upgrades to (hidden) user groups:
client.set_upgrade_definitions({
    {1, "Premium", 10},  // upgrade_id, name, group_id
    {2, "VIP",     11},
});

auto auth = client.authenticate("username", "password");
if (auth.success && auth.user) {
    int uid = auth.user->user_id;
    for (const auto& up : client.get_user_upgrades(uid)) {
        // up.name, up.active
    }
    bool premium = client.has_upgrade(uid, /*upgrade_id*/ 1);
}
```

## Showing remaining duration

Group membership can only tell you whether an upgrade is **active or not** — it
has no expiry date. To show the **remaining duration**, you need the `end_date`
from XenForo's `xf_user_upgrade_active` table, which has no native REST route.

Expose it with a tiny custom add-on endpoint, then read it via
`get_user_upgrades_detailed()`:

```cpp
// Returns ActiveUpgrade entries with start_date / end_date populated.
for (const auto& up : client.get_user_upgrades_detailed(userId)) {
    // up.active           -> still valid?
    // up.is_permanent()   -> never expires?
    // up.remaining_seconds() -> seconds left (0 = expired, -1 = permanent)
    // up.remaining_human()   -> "12d 3h", "permanent", "expired"
}
```

The C++ side expects the endpoint to return either a bare array or
`{ "upgrades": [ ... ] }`, where each item has `user_upgrade_id`, `title`,
`start_date` and `end_date` (Unix timestamps; `end_date` of 0/null = permanent).

### Bundled XenForo add-on

A ready-to-install add-on that provides exactly this endpoint ships in
[`xenforo-addon/RetteDasCode/UserUpgradeAPI`](xenforo-addon/RetteDasCode/UserUpgradeAPI/README.md).
It serves:

```
GET /api/user-upgrades/{user_id}
```

which is the default path used by `get_user_upgrades_detailed()`. Copy it into
your forum's `src/addons/` directory and install it from the Admin Control
Panel. See its README for details.

> Note: the endpoint returns currently **active** upgrades. Expired ones are
> moved out of `xf_user_upgrade_active` by XenForo; query
> `xf_user_upgrade_expired` too if you also want history.

## Recommended architecture for your app

```
[Your app]  --HTTPS-->  [Your backend (uses XenforoCpp + super user key)]  --REST-->  [XenForo]
```

This keeps the super user key secret while letting you relay login/upgrade
status to your app without exposing the key.

## License

MIT
