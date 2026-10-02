# AC8 InputMapper

**Experimental third-party HOTAS input mapping for ACE COMBAT 8 on Windows.**

AC8 InputMapper translates two DirectInput controllers into the game's existing X56 stick/throttle input channels. It is intended to make unsupported HOTAS hardware usable without UE4SS, Lua scripts, or runtime F5/F6 reloads.

> [!WARNING]
> **Experimental — offline single-player only.** This build uses an in-process DirectInput DLL and is **not approved for Easy Anti-Cheat protected play**. Remove the compatibility DLL and restore your normal launch configuration before returning to protected/online play.

## Current status

The 0.1.0 experimental build has been tested with:

- VIRPIL WarBRD stick
- VIRPIL MongoosT-50CM3 throttle
- ACE COMBAT 8 menus and in-flight controls

Other controllers are currently unverified.

The mapper supports two distinct DirectInput devices and **19 role-specific assignments**:

**Stick:** Gun, MSL, Weapon, Target, View, D-pad Up/Right/Down/Left, Pitch, Roll, Yaw, Camera Pitch, Camera Yaw  
**Throttle:** Radar, Flare, AutoPilot, Pause, Throttle

The current build maps those inputs onto AC8's native X56 control channels. Stock X56 context aliases are preserved—for example, Gun also acts as menu confirm and MSL as menu back.

## Quick start

1. Read **[SETUP.md](SETUP.md)** before installing.
2. Run `tools\List-Controllers.cmd` from the release package to find each controller's DirectInput **InstanceGUID**.
3. Configure your devices and named actions in `AC8HOTAS.ini`.
4. Copy only `dinput8.dll` and `AC8HOTAS.ini` into:
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

## Important limitations

This is an early compatibility build, not a general-purpose remapper. It currently requires two distinct controller InstanceGUIDs, does not support native POV tokens or Slider1/Slider2, does not allow actions to move freely between stick and throttle roles, and does not support live configuration reload. Game updates may invalidate the executable guards used by a release.

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

Created by **Fred Barry**. I do, and write about Computer Security, https://www.amazon.com/Cybersecurity-Everyday-Life-Computer-Security-ebook/dp/B0H61FWMCD/ref=sr_1_1?s=books&sr=1-1. If that's not your speed, I also write fantasy as **F. Lee Cooper** — [fleecooper.just-serendipity.com](https://fleecooper.just-serendipity.com).
