# Changelog

## 0.2.0-experimental

- Add up to 14 additional physical input devices feeding the same two X56 roles and existing 19 actions.
- Retain action/button/axis syntax in per-device binding sections; existing Version=1 configurations remain supported.
- Rename runtime configuration to AC8InputMapper.ini and documented offline settings to AC8InputMapper_Offline.json.
- Add foreground nonexclusive acquisition, bounded reacquisition and retries, failed-read neutrality, and owned-device cleanup.
- Publish independent MSVC source/build scripts and legacy/multi-device regression tests. Replace extracted guard byte arrays with SHA-256 compatibility fingerprints.
- Keep compiled runtime files in Release assets rather than the source tree.
- Local tests pass; live additional-device, focus and reconnect validation remains pending.
- Offline single-player only. EAC is not validated or approved.

## 0.1.0-experimental

- Initial experimental DirectInput compatibility build.
- Translates two physical DirectInput devices into AC8's existing X56 stick/throttle channels.
- Supports 19 role-specific named assignments.
- Includes a controller InstanceGUID inventory utility.
- Tested with VIRPIL WarBRD base + Constellation Alpha grip and MongoosT-50CM3 throttle in menus and flight.
- Offline single-player testing only; no EAC-protected-play approval asserted.
