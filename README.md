# MobiladorID AML

AML compatibility mod for the supplied `app-game-release.apk` target.

## Source-derived target

The target APK uses its own `libNezukoSA-MP.so` / `libsamp.so`. This project does **not** replace either library.
The mod waits for the target library and searches its mapped image for the exact `0.3.7-R4` version literal.
When found, it changes that literal to `0.3.7` in place.

This is intentionally content/signature based rather than a fixed address, so normal ASLR relocation does not change the scan location.

## Input preservation

No mouse, keyboard, controller, touch, or `sendDirectInput` hooks are installed.
The mod only patches the version literal.

## Configuration

AML config section:

```ini
[Settings]
Enabled=1
ReportedVersion=0.3.7
```

The reported value must not be longer than the original `0.3.7-R4` literal.

## Build

GitHub Actions builds both `armeabi-v7a` and `arm64-v8a` and publishes the resulting `.so` files as an artifact.
A pushed tag also creates release assets.

## Verification

The version-literal patch is an inference from the supplied APKs. It must be verified against the ordinary SA-MP server.
If changing the reported version does not change the server's classification, the server is using another ClientJoin field; this mod intentionally does not guess or alter that field.
