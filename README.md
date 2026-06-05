# littlefs_tryout

A small demo project for evaluating and experimenting with the `littlefs` embedded filesystem implementation.

## Contents

- `main.c` — example application entry point.
- `build.bat` — build wrapper for the project.
- `external/littlefs/` — upstream LittleFS source, tests, and utilities.

## Build

From the repository root on Windows:

```powershell
./build.bat
```

## Purpose

This repository is intended for:

- learning how `littlefs` integrates into a C project
- trying filesystem operations on emulated block devices
- inspecting LittleFS internals and test cases

## Notes

- The project uses the upstream LittleFS source tree from `external/littlefs`
- `build.bat` should compile the demo application and any required dependencies

Later, folks!