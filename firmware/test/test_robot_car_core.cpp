// Characterization tests for the original robot-car logic, now living in
// RobotCarCore. Plain asserts, no framework; built by the Makefile with
// g++ -std=c++17 -Wall -Wextra -Werror -pedantic.

#include <cstdio>
#include <cstring>

#include "RobotCarCore.h"

using namespace robotcar;

static int failures = 0;
static int checks = 0;

#define CHECK(cond)                                                        \
  do {                                                                     \
    ++checks;                                                              \
    if (!(cond)) {                                                         \
      ++failures;                                                          \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
    }                                                                      \
  } while (0)

static void check_cmd(const char* what, int line, MotorCommand got,
                      MotorAction action, int speed) {
  ++checks;
  if (got.action != action || got.speed != speed) {
    ++failures;
    std::printf("FAIL %s:%d: %s -> action %d speed %d, expected %d/%d\n",
                __FILE__, line, what, (int)got.action, got.speed,
                (int)action, speed);
  }
}

#define CHECK_CMD(expr, action, speed)                                     \
  check_cmd(#expr, __LINE__, (expr), (action), (speed))

static BtMessage parse(const char* s) {
  return parse_bt_message(s, std::strlen(s));
}

// --- Bluetooth framing ----------------------------------------------------

static void test_bt_framing() {
  CHECK(parse("").kind == BtFrameKind::kEmpty);
  CHECK(parse("%F#").kind == BtFrameKind::kCommand);
  CHECK(parse("%F#").command == 'F');
  CHECK(parse("%S#").command == 'S');
  CHECK(parse("%A#").command == 'A');
  CHECK(parse("%T#").command == 'T');
  CHECK(parse("%Z#").command == 'Z');
  // Up to 4 chars total; junk between command and '#' is ignored.
  CHECK(parse("%FJ#").kind == BtFrameKind::kCommand);
  CHECK(parse("%FJ#").command == 'F');
  CHECK(parse("%AB#").command == 'A');
  // "%#" is a framed message whose command char is '#'.
  CHECK(parse("%#").kind == BtFrameKind::kCommand);
  CHECK(parse("%#").command == '#');
  // 5+ chars is rejected and stopped by the caller.
  CHECK(parse("%FABC").kind == BtFrameKind::kTooLong);
  CHECK(parse("%FABC").kind != BtFrameKind::kCommand);
  // Missing markers are malformed and leave the buffer untouched.
  CHECK(parse("F#").kind == BtFrameKind::kMalformed);
  CHECK(parse("%F").kind == BtFrameKind::kMalformed);
  CHECK(parse("abc").kind == BtFrameKind::kMalformed);
  CHECK(parse("%").kind == BtFrameKind::kMalformed);
  CHECK(parse("#").kind == BtFrameKind::kMalformed);
}

// --- Bluetooth dispatch ---------------------------------------------------

static void test_bt_dispatch() {
  CHECK(bt_consumes_message('F'));
  CHECK(bt_consumes_message('B'));
  CHECK(bt_consumes_message('L'));
  CHECK(bt_consumes_message('R'));
  CHECK(bt_consumes_message('S'));
  CHECK(!bt_consumes_message('T'));  // tracing re-runs while buffered
  CHECK(!bt_consumes_message('A'));  // avoidance re-runs while buffered
  CHECK(!bt_consumes_message('Z'));
  CHECK(!bt_consumes_message('#'));
  CHECK(!bt_consumes_message('\0'));

  CHECK_CMD(bt_one_shot('F'), MotorAction::kForward, 255);
  CHECK_CMD(bt_one_shot('B'), MotorAction::kBackward, 255);
  CHECK_CMD(bt_one_shot('L'), MotorAction::kRotateLeft, 230);
  CHECK_CMD(bt_one_shot('R'), MotorAction::kRotateRight, 230);
  CHECK_CMD(bt_one_shot('S'), MotorAction::kStop, 0);
  CHECK_CMD(bt_one_shot('T'), MotorAction::kNone, 0);
  CHECK_CMD(bt_one_shot('A'), MotorAction::kNone, 0);
  CHECK_CMD(bt_one_shot('Z'), MotorAction::kNone, 0);
  CHECK_CMD(bt_one_shot('Q'), MotorAction::kNone, 0);

  CHECK(kBtDriveHoldMs == 500);
  CHECK(kBtRotateHoldMs == 300);
  CHECK(kBtForwardSpeed == 255);
  CHECK(kBtRotateSpeed == 230);
}

// --- Line following ---------------------------------------------------------
// Original decision table from Infrared_Tracing(), Black == 1.

static void test_line_follow() {
  // (left, center, right) -> action
  LineReading center_only{false, true, false};
  CHECK_CMD(line_follow_command(center_only),
            MotorAction::kForward, kLineForwardSpeed);    // 130
  CHECK_CMD(line_follow_command({true, true, false}),
            MotorAction::kRotateLeft, kLinePivotSpeed);   // 180
  CHECK_CMD(line_follow_command({true, false, false}),
            MotorAction::kRotateLeft, kLineTurnSpeed);    // 160
  CHECK_CMD(line_follow_command({false, false, true}),
            MotorAction::kRotateRight, kLineTurnSpeed);   // 160
  CHECK_CMD(line_follow_command({false, true, true}),
            MotorAction::kRotateRight, kLinePivotSpeed);  // 180
  CHECK_CMD(line_follow_command({true, true, true}),
            MotorAction::kStop, 0);
  // All white: the original writes nothing, motion persists.
  CHECK_CMD(line_follow_command({false, false, false}),
            MotorAction::kNone, 0);
}

// --- Obstacle avoidance -----------------------------------------------------

static void test_avoidance() {
  // Trigger window: 0 < d <= 20 cm. Zero (sensor timeout) is "clear".
  CHECK(!avoidance_triggered(0.0f));
  CHECK(!avoidance_triggered(-5.0f));
  CHECK(avoidance_triggered(0.1f));
  CHECK(avoidance_triggered(20.0f));
  CHECK(!avoidance_triggered(20.1f));
  CHECK(!avoidance_triggered(150.0f));

  // Turn direction: right <= left rotates left (tie goes left).
  CHECK(avoidance_turn(10.0f, 5.0f) == MotorAction::kRotateLeft);
  CHECK(avoidance_turn(5.0f, 10.0f) == MotorAction::kRotateRight);
  CHECK(avoidance_turn(7.0f, 7.0f) == MotorAction::kRotateLeft);

  CHECK_CMD(avoidance_command(50.0f, 0.0f, 0.0f),
            MotorAction::kForward, kAvoidCruiseSpeed);   // 150
  CHECK_CMD(avoidance_command(0.0f, 0.0f, 0.0f),
            MotorAction::kForward, kAvoidCruiseSpeed);   // timeout = clear
  CHECK_CMD(avoidance_command(15.0f, 10.0f, 5.0f),
            MotorAction::kRotateLeft, kAvoidTurnSpeed);  // 255
  CHECK_CMD(avoidance_command(15.0f, 5.0f, 10.0f),
            MotorAction::kRotateRight, kAvoidTurnSpeed);
}

// --- IR remote ---------------------------------------------------------------

static void test_ir_keycodes() {
  CHECK(ir_pending_for_keycode(kIrKeyUp) == 'f');
  CHECK(ir_pending_for_keycode(kIrKeyDown) == 'b');
  CHECK(ir_pending_for_keycode(kIrKeyLeft) == 'l');
  CHECK(ir_pending_for_keycode(kIrKeyRight) == 'r');
  CHECK(ir_pending_for_keycode(kIrKeyOk) == 's');
  CHECK(ir_pending_for_keycode(0x16) == '\0');  // "1" has no binding
  CHECK(ir_pending_for_keycode(0x00) == '\0');
  CHECK(ir_pending_for_keycode(0xFF) == '\0');
}

static void test_ir_state_machine() {
  IrState s{' ', false};

  // Idle: nothing pending, nothing executes.
  CHECK_CMD(ir_execute_pending(s), MotorAction::kNone, 0);

  // A bound key sets pending + armed; it executes on the next pass.
  ir_note_keycode(s, kIrKeyUp);
  CHECK(s.pending == 'f');
  CHECK(s.armed);
  CHECK_CMD(ir_execute_pending(s), MotorAction::kForward, 255);
  CHECK(s.pending == ' ');
  CHECK(s.armed);  // armed stays set until a BT frame preempts

  // Second key while armed replaces pending.
  ir_note_keycode(s, kIrKeyLeft);
  CHECK(s.pending == 'l');
  CHECK_CMD(ir_execute_pending(s), MotorAction::kRotateLeft, 255);

  // Unbound key changes nothing.
  ir_note_keycode(s, 0x16);
  CHECK(s.pending == ' ');
  CHECK(s.armed);

  // Pending IR action is preempted by any well-formed BT frame.
  ir_note_keycode(s, kIrKeyDown);
  CHECK(s.pending == 'b');
  CHECK(ir_preempt_for_frame(s));
  CHECK(s.pending == ' ');
  CHECK(!s.armed);
  CHECK_CMD(ir_execute_pending(s), MotorAction::kNone, 0);
  // Once disarmed, frames do not report a preemption.
  CHECK(!ir_preempt_for_frame(s));

  // 's' maps to a bare stop with no motion pulse.
  ir_note_keycode(s, kIrKeyOk);
  CHECK(s.pending == 's');
  CHECK_CMD(ir_execute_pending(s), MotorAction::kStop, 0);

  CHECK(kIrPulseMs == 300);
}

// --- Pins ---------------------------------------------------------------------

static void test_pins() {
  CHECK(pins::kIrReceiver == 12);
  CHECK(pins::kServo == 16);           // A2
  CHECK(pins::kUltrasonicEcho == 14);  // A0
  CHECK(pins::kUltrasonicTrig == 15);  // A1
  CHECK(pins::kLineLeft == 9);
  CHECK(pins::kLineCenter == 10);
  CHECK(pins::kLineRight == 11);
  CHECK(pins::kMotorDirA1 == 2);
  CHECK(pins::kMotorDirA2 == 4);
  CHECK(pins::kMotorDirB1 == 7);
  CHECK(pins::kMotorDirB2 == 8);
  CHECK(pins::kMotorPwmA == 5);
  CHECK(pins::kMotorPwmB == 6);
}

int main() {
  test_bt_framing();
  test_bt_dispatch();
  test_line_follow();
  test_avoidance();
  test_ir_keycodes();
  test_ir_state_machine();
  test_pins();
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
