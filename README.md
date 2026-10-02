# AC8 InputMapper

**Experimental third-party HOTAS input mapping for ACE COMBAT 8 on Windows.**

AC8 InputMapper translates two DirectInput anchor controllers, plus optional additional devices, into the game's existing two X56 stick/throttle input roles. It is intended to make unsupported HOTAS hardware usable without UE4SS, Lua scripts, or runtime F5/F6 reloads.

> [!WARNING]
> **Experimental — offline single-player only.** This build uses an in-process DirectInput DLL and is **not approved for Easy Anti-Cheat protected play**. Remove the compatibility DLL and restore your normal launch configuration before returning to protected/online play.

## Current status

The 0.1.0 experimental build has been tested with:

- VIRPIL WarBRD base with Constellation Alpha grip.
- VIRPIL MongoosT-50CM3 throttle
- ACE COMBAT 8 menus and in-flight controls

The 0.2.0 experimental source adds additional input devices. Local legacy and multi-device tests pass; live pedal input, focus recovery and reconnect behavior still need validation. Other hardware is unverified.

The mapper retains two distinct anchor devices and the same **19 output actions**, with up to 14 additional physical input sources:

**Stick:** Gun, MSL, Weapon, Target, View, D-pad Up/Right/Down/Left, Pitch, Roll, Yaw, Camera Pitch, Camera Yaw  
**Throttle:** Radar, Flare, AutoPilot, Pause, Throttle

The current build maps those inputs onto AC8's native X56 control channels. Stock X56 context aliases are preserved—for example, Gun also acts as menu confirm and MSL as menu back.

## Quick start

1. Read **[SETUP.md](SETUP.md)** before installing.
2. Run `tools\List-Controllers.cmd` from the release package to find each controller's DirectInput **InstanceGUID**.
3. Configure your devices and named actions in `AC8InputMapper.ini`.
4. Copy only `dinput8.dll` and `AC8InputMapper.ini` into:
   `Game\Binaries\Win64`
5. Use the documented offline launch configuration and test in campaign/free flight.
6. Restart AC8 after changing the INI.

A minimal configuration looks like:

```ini
[Compatibility]
Version=1
Enabled=1

[Stick]
InstanceGUID={YOUR-STICK-INSTANCE-GUID}

[Throttle]
InstanceGUID={YOUR-THROTTLE-INSTANCE-GUID}

[Stick.Bindings]
Pitch=-Y
Roll=X
Yaw=Z
Gun=Button1
MSL=Button2

[Throttle.Bindings]
Throttle=Rx
Flare=Button7
```

Use only axes reported for your hardware. `Button1` means physical button 1; prefix an axis with `-` to reverse it.

## Additional devices in 0.2

The binding syntax stays the same. Set `Version=2`, remove `Yaw` from `[Stick.Bindings]` or set it to `None`, and add:

```ini
[Device.Pedals]
InstanceGUID={YOUR-PEDALS-INSTANCE-GUID}

[Device.Pedals.Bindings]
Yaw=Z
```

Choose the actual axis reported by your pedals; use `-Z` if reversed. Existing `Version=1` INIs remain supported using the new filename. Additional device sections can supply any existing action to either output role, with one active source per action. See **[MULTI-DEVICE.txt](MULTI-DEVICE.txt)** for examples and the live test checklist.

## Build from source

The repository contains independently written source, build scripts and regression tests. See **[BUILD.md](BUILD.md)**. Build artifacts and personal configuration files are excluded from source control; use Release assets for prebuilt binaries.

## Important limitations

This is an early compatibility build, not a general-purpose remapper. It currently requires two distinct controller InstanceGUIDs, does not support native POV tokens or Slider1/Slider2, retains role-specific keys in the original stick/throttle sections, and does not support live configuration reload. Game updates may invalidate the executable guards used by a release.

External axes require an unused physical axis object on their destination anchor for capability metadata. Missing or unreadable additional sources contribute centered axes and released buttons; there is no held-last-value or silent fallback. Additional devices use foreground, nonexclusive acquisition, and reconnect recovery remains unvalidated on hardware.

Each launch creates an `ac8-compat-*.log` beside the DLL. Logs can contain controller identity/input information, so review them before sharing.

## Downloads

Use the **Releases** section for packaged experimental builds. The repository documentation describes the project; release ZIPs contain the runtime DLL, blank INI, controller-list utility, hashes, license, and setup notes.

## Why this exists

ACE COMBAT 8 currently recognizes a limited set of flight-stick profiles. AC8 InputMapper is an independent interoperability project that translates other DirectInput hardware into an input profile the game already understands.

It is not affiliated with, endorsed by, or supported by Bandai Namco Entertainment, Bandai Namco Aces, PROJECT ACES, Logitech/Saitek, Thrustmaster, VIRPIL Controls, or Epic Games/Easy Anti-Cheat. Product names and trademarks belong to their respective owners.

## License

AC8 InputMapper is released under the **[MIT License](LICENSE)**.

Copyright © 2026 Fred Barry.

---

Created by **Fred Barry**, author of *[Cybersecurity for Everyday Life](https://www.amazon.com/dp/B0H61FWMCD)*. I also write fantasy as **F. Lee Cooper** — [fleecooper.just-serendipity.com](https://fleecooper.just-serendipity.com).
