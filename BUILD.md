# Building AC8 InputMapper

Windows x64, Visual Studio 2022 or newer with Desktop development with C++, and a Windows 10/11 SDK are required. No game files or third-party code are required to build.

Run `build.cmd` from a normal command prompt. It locates MSVC using vswhere, builds `build\compatibility\dinput8.dll`, runs the legacy and multi-source regression executables, and verifies the six exports and ordinals. Run `build-controller-list.cmd` to build `build\controller-list\List-Controllers.exe`. Both use the static MSVC runtime (/MT).

Tests use synthetic GUIDs with the known-good VIRPIL assignments. They do not launch AC8 or acquire controllers. The runtime version guard stores SHA-256 compatibility fingerprints, not game instruction bytes or extracted profiles. Offsets and fingerprints restrict translation to the previously validated executable build; unsupported versions fail open to native input. The observer-derived forwarding implementation here is independently written and has no dependency on the separate research project.

Offline single-player only. EAC compatibility has not been validated or approved. MIT licensed, copyright 2026 Fred Barry.

After both builds, run `powershell -NoProfile -ExecutionPolicy Bypass -File package-release.ps1`. It creates the end-user ZIP under `dist/` and refuses to overwrite existing release output. The regression fixture contains synthetic GUIDs, not a usable personal configuration.

The multi-source tests use mock COM devices, including already-hooked shared vtables. They cover additional-device setup/data formats, normalized axis ranges, bounded reacquisition, failed reads, focus suspension, retries, cleanup and INI conflicts. They do not acquire real controllers. Windows user32.lib is now linked for foreground/window checks; no extra runtime DLL needs to be distributed.
