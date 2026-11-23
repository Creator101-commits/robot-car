#pragma once

#include <stddef.h>
#include <stdint.h>

// Arduino-free decision logic for the robot car. The .ino sketch owns all
// pin I/O, timing and the serial/Bluetooth framing; everything here is a
// pure function of its inputs so it can be tested on the host.

namespace robotcar {

// Pin mapping on the Arduino Uno (A0=14, A1=15, A2=16).
namespace pins {
constexpr int kIrReceiver = 12;
constexpr int kServo = 16;          // A2
constexpr int kUltrasonicEcho = 14; // A0
constexpr int kUltrasonicTrig = 15; // A1
constexpr int kLineLeft = 9;
constexpr int kLineCenter = 10;
constexpr int kLineRight = 11;
constexpr int kMotorDirA1 = 2;
constexpr int kMotorDirA2 = 4;
constexpr int kMotorDirB1 = 7;
constexpr int kMotorDirB2 = 8;
constexpr int kMotorPwmA = 5;
constexpr int kMotorPwmB = 6;
}  // namespace pins

enum class MotorAction {
    kNone,
    kForward,
    kBackward,
    kRotateLeft,
    kRotateRight,
    kStop,
};

struct MotorCommand {
    MotorAction action;
    int speed;
};

// --- Line following -------------------------------------------------------
// Line sensors read HIGH on the line (Black == 1, White == 0 in the sketch).
struct LineReading {
    bool left_black;
    bool center_black;
    bool right_black;
};

constexpr int kLineForwardSpeed = 130;
constexpr int kLinePivotSpeed = 180;
constexpr int kLineTurnSpeed = 160;

// Decision table ported from Infrared_Tracing(). All-white returns kNone:
// the original writes no motor pins in that case, so the last motion
// persists. Every other combination maps to exactly one action.
MotorCommand line_follow_command(const LineReading& reading);

// --- Obstacle avoidance ---------------------------------------------------
constexpr float kAvoidDistanceCm = 20.0f;
constexpr int kAvoidCruiseSpeed = 150;
constexpr int kAvoidTurnSpeed = 255;

// checkdistance() divides the echo pulse by 58; a timeout yields 0, which the
// original treats as "clear" because the trigger requires 0 < d <= 20.
bool avoidance_triggered(float front_distance_cm);

// On a tie (right <= left) the original rotates left.
MotorAction avoidance_turn(float left_distance_cm, float right_distance_cm);

// Full decision: clear -> cruise forward at 150; triggered -> turn at 255.
// The servo sweep that produces left/right readings stays in the sketch.
MotorCommand avoidance_command(float front_cm, float left_cm, float right_cm);

// --- Bluetooth protocol ---------------------------------------------------
// Wire format "%X#": '%' at index 0, command char at index 1, '#' last,
// total length at most 4. Characters between index 2 and the end are ignored.
enum class BtFrameKind { kEmpty, kTooLong, kMalformed, kCommand };

struct BtMessage {
    BtFrameKind kind;
    char command;  // valid only when kind == kCommand
};

constexpr size_t kBtMaxLen = 4;

BtMessage parse_bt_message(const char* data, size_t len);

// F/B/L/R/S are one-shot: the sketch clears the buffer after dispatch.
// T/A re-run every loop while the buffer persists; Z and unknown commands
// leave the buffer untouched as well.
bool bt_consumes_message(char command);

// One-shot motor action for a consumed command; kNone for everything else.
MotorCommand bt_one_shot(char command);

constexpr int kBtDriveHoldMs = 500;   // F, B
constexpr int kBtRotateHoldMs = 300;  // L, R
constexpr int kBtForwardSpeed = 255;
constexpr int kBtRotateSpeed = 230;

// --- Infrared remote ------------------------------------------------------
// Keycodes are the bytes the NEC decoder exposes via IRremote::getCode(),
// matching the "up/down/ok/left/right" rows of normal_ir_keymap.
constexpr uint8_t kIrKeyUp = 0x46;
constexpr uint8_t kIrKeyDown = 0x15;
constexpr uint8_t kIrKeyOk = 0x40;
constexpr uint8_t kIrKeyLeft = 0x44;
constexpr uint8_t kIrKeyRight = 0x43;

// Maps a keycode to the pending mode char the sketch executes next loop
// ('f', 'b', 'l', 'r', 's'), or '\0' when the key has no binding.
char ir_pending_for_keycode(uint8_t keycode);

// Pending-mode state. armed mirrors IR_Mode_Flag: once any IR key arrives it
// stays set until a well-formed Bluetooth frame preempts it.
struct IrState {
    char pending;
    bool armed;
};

void ir_note_keycode(IrState& state, uint8_t keycode);

// Call for every well-formed "%..#" frame up to kBtMaxLen, before dispatch.
// Returns true when it stopped the car and cleared the pending mode, which is
// what the original does inside the framed-message branch.
bool ir_preempt_for_frame(IrState& state);

// Executes and clears the pending mode once, like the switch in
// IR_remote_control(). kNone for ' ' or unknown pending values.
MotorCommand ir_execute_pending(IrState& state);

constexpr int kIrPulseMs = 300;

}  // namespace robotcar
