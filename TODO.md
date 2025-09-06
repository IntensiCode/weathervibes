Weather Vibes – Refactor Roadmap
================================

This document tracks refactor ideas, quality improvements, and concrete tasks.

Architecture
------------
- [x] Split monolithic files into focused modules (weather_display, weather_update, preferences_handler, etc.)
- [x] Separate configuration management (applet_config.c) from UI handling
- [ ] Encapsulate applet state: make `WeatherApplet` opaque (typedef in `applet.h`, struct in `core.c`).
- [ ] Add asynchronous fetch pipeline using `GTask` and main-loop handoff.
- [ ] Normalize provider inputs (prefer lat/lon + locale); centralize geocoding + cache.
- [ ] Replace ad‑hoc error strings with a structured `GError` domain for providers/network.
- [ ] Track in‑flight updates; debounce rapid preference changes.

Networking
----------
- [ ] Replace `popen("curl …")` with `libsoup` (or `libcurl`) for safe HTTPS, timeouts, redirects, proxies.
- [ ] Quote/escape all URLs when shelling (if subprocess remains); otherwise remove shelling entirely.
- [ ] Enforce HTTPS endpoints everywhere; update GeoIP and other URLs.
- [ ] Centralize timeouts, retries, and backoff in `network.c`.
- [ ] Cap maximum response size to avoid excessive memory usage.

UI/UX
-----
- [x] Use emoji icons directly in systray (removed GTK theme icon dependencies)
- [x] Unified emoji display in both systray and detail popup
- [x] Left-click toggles detail popup (easier open/close)
- [ ] Determine day/night from sunrise/sunset data; fallback to local time only if missing.
- [ ] Support wind/pressure unit selection (m/s, km/h, mph; hPa, inHg).
- [ ] Add accessible names/descriptions; keyboard activation for popup (Space/Enter).
- [ ] Replace deprecated `GtkActionGroup` with `GAction` + `GMenuModel` (as supported by MATE panel).

Configuration
-------------
- [ ] Consider migrating to `GSettings`; otherwise keep INI with stronger validation and clamping.
- [ ] Mask API keys in logs; never print full secrets.
- [ ] Normalize provider API key handling (map keyed by provider enum).

Data Model
----------
- [x] Removed redundant `condition_icon` field (emoji derived from condition enum)
- [x] Centralized emoji mapping in weather_conditions.c
- [ ] Replace sentinel `-999.0` with validity flags/bitfield.
- [ ] Include timezone/offset; format times respecting locale and 12/24h preferences.
- [ ] Extend with precipitation fields (rate, type, probability) when provider supports them.

Concurrency and Safety
----------------------
- [ ] Ensure all `g_app_context` accesses are guarded by its mutex (including config, last fetch times, cached city).
- [ ] All UI updates run on the GTK main thread (use `g_main_context_invoke/g_idle_add`).
- [ ] Add `GCancellable` for in‑flight operations; cancel on destroy/provider change.

Providers
---------
- [x] All providers now properly set condition enum and use shared emoji function
- [ ] Consolidate JSON helpers for safe extraction (numbers/strings/arrays) to reduce duplication.
- [ ] Add capability flags (needs API key, supports UV/feels_like, wind_dir type) to drive dynamic UI.
- [ ] Handle rate limiting with provider-specific backoff.

Error Handling
--------------
- [ ] Define error domains (e.g., `WEATHER_ERROR`, `NETWORK_ERROR`, `PROVIDER_ERROR`).
- [ ] Map errors to concise, localized messages for tooltip/popup.
- [ ] Keep last error in state; avoid repetitive/noisy logs.

Logging
-------
- [ ] Move logs to XDG state dir: `~/.local/state/weather-vibes/weather.log` (or `$XDG_STATE_HOME`).
- [ ] Simple rotation: size cap and keep N files.
- [ ] Configurable log level; never log secrets.

Internationalization
--------------------
- [ ] Add gettext; wrap user-visible strings in `_()` and add PO scaffolding.
- [ ] Format times with locale; respect 12/24h where available.

Build/Project Hygiene
---------------------
- [ ] Add brief module headers (responsibility + public API) to new `.c` files.
- [ ] Add `make check` with GLib test framework for providers, config round‑trip, icon mapping.
- [ ] Add CI (GitHub Actions) to build and run tests on Ubuntu.
- [ ] Optional: add `clang-format` config; do not enforce if it conflicts with existing style.

Security
--------
- [ ] Prefer HTTPS for all services; update Nominatim UA per policy; cache geocoding.
- [ ] Remove shell invocations from network path; avoid argument injection and environment dependence.

Smaller Code Fixes
------------------
- [ ] `core.c`: call `config_free()` on destroy; consolidate cleanup paths.
- [ ] `ui.c`: factor string formatting helpers for repeated tooltip text.
- [ ] `preferences.c`: remove cast warnings; avoid printing full API keys; single definition of callback data.
- [ ] `network.c`: parameterize timeouts; add retry with jitter; surface status codes.
- [ ] `config.c`: clamp intervals (min/max); robust enum validation; debounce save on changes.
- [ ] `logger.c`: tune GLib vs file logging to reduce duplication at INFO+.

Validation
----------
- Build and run `make test_providers` to sanity‑check providers.
- Manual test panel applet: verify UI remains responsive during fetch; close popup while fetching; change provider mid‑fetch to confirm cancellation.
- Log: confirm no secrets are printed; inspect error messages for clarity.
