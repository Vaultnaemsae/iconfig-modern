# Tests

These are promoted, offline-first regression suites recovered from the Qt 5/Qt
6 rehabilitation work. Build and run all suites with:

```sh
./dev test
```

Generated binaries and Makefiles live under `build/qt6-release/tests/`.
No physical device is required and no setter is intentionally transmitted.

The restore planner's optional two-file compatibility mode accepts explicit
primary and auxiliary preset paths. Those private fixtures are not part of the
repository; its default path uses a generated synthetic preset.
