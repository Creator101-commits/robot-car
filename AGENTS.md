# Robot Car

A 4-wheel Arduino UNO robot car with Bluetooth control from a custom
Android app, IR remote control, ultrasonic obstacle avoidance and
infrared line tracking.

## Build commands

```bash
# Firmware host tests: RobotCarCore decision logic, no Arduino needed.
make -C firmware/test

# Compile the UNO sketch (needs arduino-cli and the AVR core).
arduino-cli core install arduino:avr@1.8.8
arduino-cli lib install Servo@1.1.3
arduino-cli compile --fqbn arduino:avr:uno \
  --libraries firmware/libraries firmware/robot_car

# Android app (JDK 17, Gradle wrapper is committed).
cd android-app
./gradlew assembleDebug testDebugUnitTest lint
```

## Platform support

- Firmware targets Arduino UNO (ATmega328P) on `arduino:avr` 1.8.8.
  Host tests compile `firmware/libraries/RobotCarCore` with
  `g++ -std=c++17 -Wall -Wextra -Werror -pedantic` on macOS and Linux.
- Android app: minSdk 21, compileSdk 37 (minor 2), targetSdk 37,
  JDK 17, Gradle 9.9.1 wrapper, AGP 9.5.1. Dependency versions are the
  newest that still support minSdk 21.

## Key conventions

- `firmware/robot_car/robot_car.ino` is Arduino glue only: pins,
  setup/loop, sensor reads and motor writes. All decision logic lives in
  `firmware/libraries/RobotCarCore/` as Arduino-free functions that take
  readings and return `MotorCommand` structs, so it is host-testable.
- The Bluetooth wire protocol is `%X#` frames. The command table
  (F forward, B backward, L left, R right, S stop, T trace, A
  avoidance) is shared by `RobotCarCore`, `CommandProtocol.java` and the
  README protocol table. Keep all three in sync.
- `firmware/libraries/IRremote/` is a custom NEC IR decoder plus keymap
  (provenance unknown, see README). `Servo` comes from the Arduino
  library manager, not this repo.
- Android dependencies resolve through `android-app/local.properties`
  (`sdk.dir`) or `ANDROID_HOME`; the file is gitignored.

## Rules

- Default branch is `main`; never force-push it or rewrite shared history.
- Ask before adding any dependency or package manager.
- Never fabricate data, benchmarks, logs or results.
- Done means the relevant tests actually ran and passed; never skip or mock them to finish.
- No hardware-behavior claims without hardware evidence: say what was
  verified on the host or in compilation, and what still needs a real
  car.
- No emojis anywhere: code, comments, commits, READMEs, docs.
- No em dashes (the long dash, U+2014) anywhere: code, comments, commits, READMEs, docs. Use a comma, colon, parentheses, or a new sentence instead.
- Keep README in sync with code changes, following its existing template.
- Comments are brief and explain why, only when the code doesn't already say it.

## Commits

- Conventional Commits: `type(scope): summary`. Types: `feat`, `fix`, `refactor`, `perf`, `test`, `docs`, `build`, `ci`, `chore`; scope is optional.
- Summary in imperative mood, lowercase, no trailing period, at most 72 characters.
- One logical change per commit; keep formatting-only changes separate.
- Add a body only when the reason is not obvious: explain why, not what, wrapped at 72.
- Breaking changes: `type!:` in the summary plus a `BREAKING CHANGE:` footer.
- Commit only when asked. Never commit secrets, build output, or failing code.
