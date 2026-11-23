# REVIEW

## Verification log

- `make -C firmware/test`: 112 checks, 0 failures
  (`g++ -std=c++17 -Wall -Wextra -Werror -pedantic`).
- `arduino-cli compile --fqbn arduino:avr:uno --libraries
  firmware/libraries firmware/robot_car`: 9,124 bytes flash (28% of
  32,256), 551 bytes RAM (26% of 2,048).
- `cd android-app && ./gradlew assembleDebug testDebugUnitTest lint`
  (JDK 17, Gradle 9.9.1, AGP 9.5.1, compileSdk 37.2): BUILD SUCCESSFUL;
  3 JUnit tests, 0 failures; lint 0 errors, 4 informational version
  warnings.
- Em-dash/emoji scan over all changed files: clean.

## Rebuild notes

- The original sketch did not compile as committed: `IR_remote.h` was
  missing, then `Servo.h` after the IR files were supplied. Both are
  now proper libraries under `firmware/libraries/` (`IRremote`,
  `Servo` via the library manager).
- Decision logic was extracted into the Arduino-free
  `firmware/libraries/RobotCarCore/` library; `robot_car.ino` keeps
  only pin I/O, sensor reads and motor writes. `firmware/test/`
  characterizes the original behavior with plain asserts.
- `IRremote` is a custom NEC decoder plus keymap with no license
  header; provenance unknown (likely kit/tutorial-derived).
- Android: Gradle wrapper generated at 9.9.1, AGP 9.5.1,
  compileSdk 37 (minor 2), targetSdk 37, minSdk 21 kept. Runtime
  permission is `BLUETOOTH_CONNECT` on API 31+ only; `BLUETOOTH_SCAN`
  and location are not declared because the app never discovers
  devices. `CommandProtocol.java` is the app's single source of
  command bytes, asserted by JUnit tests against the same table the
  firmware tests pin.
- CI (`.github/workflows/ci.yml`) runs the host tests, the UNO compile
  and the Android build/test/lint on push and PR.

## Behavior changes from the original sketch

- IR key reads: the original called `ir.getCode()` up to 5 times per
  loop in its if/else chain, and `IRremote::loop()` clears `irRead`
  about 1 ms after a decode, so reads within one iteration could see
  different values and some keypresses could be missed. The rebuild
  reads the code once per loop (deterministic); kept as-is.
- Keymap bounds: `getKeyMap`/`getIrKey` iterated `KEY_MAX` (21) over a
  17-entry table and could read past the end; now bounded by the real
  table sizes (`fix(irremote)`).
- Compile fixes: missing `IR_remote.h`/`Servo.h` resolved via proper
  libraries (the sketch never compiled as committed).
- Launcher icons: none existed; generated PNGs added so resource
  linking succeeds.
- Status string: hardcoded "Connected" text (with an em dash) became
  the `status_connected_named` string resource.
- Distance units: ultrasonic distances are whole centimeters,
  truncated like the original `volatile int` globals via
  `distance_cm_from_echo_us` (`(int)(echo_us / 58.00)`); a float
  version of this logic was corrected in `fix(firmware)`.

## Notes

- On-car driving, sensor calibration, real Bluetooth pairing and IR
  hardware are unverified; see the README "Not verified" section.
- The Bluetooth command table (`%X#` frames: F/B/L/R/S/T/A) is shared
  by `RobotCarCore`, `CommandProtocol.java` and the README; unknown
  command chars parse as a no-op that keeps the buffer.
