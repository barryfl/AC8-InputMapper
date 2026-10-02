# Setup

> [!WARNING]
> **Experimental — offline single-player only.** The current build is an in-process DirectInput DLL and has not been approved for Easy Anti-Cheat protected play. Do not return to protected/online play with the compatibility DLL installed.

## 1. Offline launch configuration

If you already have a working offline launch setup, retain it.

The following procedure is based on community-documented offline AC8 mod setup. It is not an assurance from EAC or Bandai Namco.

With AC8 closed, use Steam's **Manage → Browse local files** to open the installation root. In the `EasyAntiCheat` folder, duplicate `Settings.json` as `AC8HOTAS_Offline.json`. Leave the original intact.

In the duplicate, add this top-level JSON entry:

```json
"allow_null_client": "true"
```

If the key already exists, change its value rather than duplicating it.

In **Steam → ACE COMBAT 8 → Properties → General → Launch Options**, save any previous options and enter:

```text
cmd /d /c "set EOS_USE_ANTICHEATCLIENTNULL=1&& %command% -anticheat_settings=AC8HOTAS_Offline.json"
```

Use this setup only for offline campaign/free flight. If offline launching fails, stop and inspect the configuration rather than launching protected play with the compatibility DLL present.

## 2. Prepare the game folder

Open `Game\Binaries\Win64` and close the game before making changes.

If `dinput8.dll` already exists there, determine what installed it and back it up outside the game folder. AC8 InputMapper cannot share that filename with another DirectInput proxy/mod loader.

Disable older UE4SS-based HOTAS setups if installed. Disabling a Lua mod alone does not unload UE4SS.

## 3. Find controller InstanceGUIDs

Connect the stick and throttle. With AC8 closed, run `List-Controllers.cmd` from the release package.

The utility displays controller names, InstanceGUID, ProductGUID, button counts and available axes, and writes `controllers.txt` beside itself.

Copy the complete **InstanceGUID**, including braces, for each device. Do **not** use ProductGUID, VID/PID, Device Manager hardware ID, or a Steam controller identifier.

`joy.cpl` is useful for identifying physical button numbers. The INI is one-based: physical button 1 is `Button1`.

## 4. Install

Copy only these runtime files into `Game\Binaries\Win64`:

```text
dinput8.dll
AC8HOTAS.ini
```

No UE4SS or Lua files are required by this runtime.

## 5. Configure AC8HOTAS.ini

Set each device once:

```ini
[Stick]
InstanceGUID={YOUR-STICK-INSTANCE-GUID}

[Throttle]
InstanceGUID={YOUR-THROTTLE-INSTANCE-GUID}
```

Buttons use `ButtonN`. Axes use `X`, `Y`, `Z`, `Rx`, `Ry`, or `Rz`; prefix with `-` to reverse an axis. Use only axes reported by the controller inventory. `None` leaves an action unassigned.

| Role | Type | Actions |
| --- | --- | --- |
| Stick | Buttons | Gun, MSL, Weapon, Target, View, DPadUp, DPadRight, DPadDown, DPadLeft |
| Stick | Axes | Pitch, Roll, Yaw, CameraPitch, CameraYaw |
| Throttle | Buttons | Radar, Flare, AutoPilot, Pause |
| Throttle | Axis | Throttle |

Current limitations: D-pad bindings require `ButtonN` inputs; native POV tokens and Slider1/Slider2 are not supported. Actions cannot yet move freely between stick/throttle sections. Two distinct device InstanceGUIDs are required.

After replacing both GUID placeholders and configuring bindings, set `Enabled=1` under `[Compatibility]`. Leave `Version=1`.

Restart AC8 after every INI change.

## 6. First test

For the first test, disconnect other natively supported HOTAS devices to avoid overlapping input.

At the menu, test confirm/back and mapped D-pad directions. In flight, verify pitch, roll, yaw, throttle endpoints, Target, Weapon, Gun, MSL, View and camera axes.

The stock X56 profile has context-dependent aliases: Gun also confirms menus, MSL also backs out, and D-pad directions share hat channels. AC8 InputMapper preserves those aliases.

## 7. Logs

Each launch creates `ac8-compat-<PID>-<QPC>.log` beside `dinput8.dll`.

For troubleshooting, retain the log and describe the physical input and visible game response. Logs may contain controller/device identity information; review them before sharing.

## 8. Return to normal protected play

Quit AC8 and remove the AC8 InputMapper `dinput8.dll` from the game folder. Remove or disable other research/mod DLL loaders as appropriate.

Restore your previous Steam launch options or clear the offline command. Steam's **Verify integrity** can restore shipped files but may leave extra mod DLLs behind, so remove those separately before returning to protected/online play.
