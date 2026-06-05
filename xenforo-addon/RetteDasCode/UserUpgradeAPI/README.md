# RetteDasCode / UserUpgradeAPI

A small XenForo 2.2+ add-on that exposes a user's **active user upgrades**
(including `start_date` and `end_date`) via the REST API. This fills the gap
where XenForo has no native endpoint for reading user upgrades, and lets the
[XenforoCpp](../../../README.md) wrapper show the **remaining duration** of an
upgrade.

## Endpoint

```
GET /api/user-upgrades/{user_id}
```

Headers:

- `XF-Api-Key: <super user key>`
- `XF-Api-User: <some user id>` (optional; super user keys only)

Response:

```json
{
  "user_id": 123,
  "now": 1719490000,
  "upgrades": [
    {
      "user_upgrade_id": 1,
      "user_upgrade_record_id": 42,
      "title": "Premium",
      "start_date": 1717500000,
      "end_date": 1720092000,
      "is_permanent": false,
      "expired": false,
      "remaining_seconds": 602000,
      "remaining_days": 6,
      "remaining_human": "6d 23h"
    }
  ]
}
```

The endpoint computes the remaining duration server-side:

- `end_date` of `0` means the upgrade is **permanent** (never expires).
- `is_permanent` / `expired` are convenience booleans.
- `remaining_seconds` / `remaining_days` are `null` for permanent upgrades.
- `remaining_human` is a ready-to-display string (`"6d 23h"`, `"permanent"`,
  `"expired"`).

## File structure

```
RetteDasCode/UserUpgradeAPI/
├── addon.json                         # add-on metadata
├── Setup.php                          # install/upgrade/uninstall (no DB changes)
├── Api/
│   └── Controller/
│       └── UserUpgrade.php            # the API controller (actionGet)
├── _data/
│   └── routes.xml                     # registers the API route
└── README.md
```

## Installation

1. Copy the `RetteDasCode/UserUpgradeAPI` folder into your forum's
   `src/addons/` directory, so you end up with
   `src/addons/RetteDasCode/UserUpgradeAPI/addon.json`.
2. In the Admin Control Panel go to **Add-ons**, find *User Upgrade API* and
   click **Install**.
3. Make sure you have a **Super user** API key under
   **Setup > API keys** with at least the `user:read` scope.

### Registering the route manually (alternative)

If the route from `_data/routes.xml` is not picked up, add it by hand:

- **Admin CP > Development > Routes > Add route**
- Route type: **API**
- Route prefix: `user-upgrades`
- Route format: `:int<user_id>/`
- Controller: `RetteDasCode\UserUpgradeAPI:UserUpgrade`

> The Development tab is only visible when `config.development.enabled` is `true`
> in `src/config.php`.

## Using it from XenforoCpp

```cpp
xenforo::Client client(config);

// The add-on serves /api/user-upgrades/{user_id}
auto upgrades = client.get_user_upgrades_detailed(userId, "user-upgrades/{user_id}");
for (const auto& up : upgrades) {
    // up.name, up.active, up.is_permanent()
    // up.remaining_human()  -> "12d 3h", "permanent", "expired"
}
```

## Notes

- Only **currently active** upgrades are returned. XenForo moves expired
  upgrades into `xf_user_upgrade_expired`; query that table too if you want
  history.
- Keep the super user key on a trusted backend. Never ship it inside a
  desktop/mobile client.
