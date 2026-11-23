#include "RobotCarCore.h"

namespace robotcar {

MotorCommand line_follow_command(const LineReading& r) {
    const bool l = r.left_black;
    const bool c = r.center_black;
    const bool rr = r.right_black;
    if (!l && c && !rr) return {MotorAction::kForward, kLineForwardSpeed};
    if (l && c && !rr) return {MotorAction::kRotateLeft, kLinePivotSpeed};
    if (l && !c && !rr) return {MotorAction::kRotateLeft, kLineTurnSpeed};
    if (!l && !c && rr) return {MotorAction::kRotateRight, kLineTurnSpeed};
    if (!l && c && rr) return {MotorAction::kRotateRight, kLinePivotSpeed};
    if (l && c && rr) return {MotorAction::kStop, 0};
    return {MotorAction::kNone, 0};
}

bool avoidance_triggered(float front_distance_cm) {
    return front_distance_cm > 0.0f && front_distance_cm <= kAvoidDistanceCm;
}

MotorAction avoidance_turn(float left_distance_cm, float right_distance_cm) {
    return right_distance_cm <= left_distance_cm ? MotorAction::kRotateLeft
                                                 : MotorAction::kRotateRight;
}

MotorCommand avoidance_command(float front_cm, float left_cm, float right_cm) {
    if (!avoidance_triggered(front_cm)) {
        return {MotorAction::kForward, kAvoidCruiseSpeed};
    }
    return {avoidance_turn(left_cm, right_cm), kAvoidTurnSpeed};
}

BtMessage parse_bt_message(const char* data, size_t len) {
    if (len == 0) return {BtFrameKind::kEmpty, '\0'};
    if (len > kBtMaxLen) return {BtFrameKind::kTooLong, '\0'};
    if (data[0] == '%' && data[len - 1] == '#') {
        return {BtFrameKind::kCommand, data[1]};
    }
    return {BtFrameKind::kMalformed, '\0'};
}

bool bt_consumes_message(char command) {
    switch (command) {
        case 'F':
        case 'B':
        case 'L':
        case 'R':
        case 'S':
            return true;
        default:
            return false;
    }
}

MotorCommand bt_one_shot(char command) {
    switch (command) {
        case 'F':
            return {MotorAction::kForward, kBtForwardSpeed};
        case 'B':
            return {MotorAction::kBackward, kBtForwardSpeed};
        case 'L':
            return {MotorAction::kRotateLeft, kBtRotateSpeed};
        case 'R':
            return {MotorAction::kRotateRight, kBtRotateSpeed};
        case 'S':
            return {MotorAction::kStop, 0};
        default:
            return {MotorAction::kNone, 0};
    }
}

char ir_pending_for_keycode(uint8_t keycode) {
    switch (keycode) {
        case kIrKeyUp:
            return 'f';
        case kIrKeyDown:
            return 'b';
        case kIrKeyLeft:
            return 'l';
        case kIrKeyRight:
            return 'r';
        case kIrKeyOk:
            return 's';
        default:
            return '\0';
    }
}

void ir_note_keycode(IrState& state, uint8_t keycode) {
    const char pending = ir_pending_for_keycode(keycode);
    if (pending != '\0') {
        state.pending = pending;
        state.armed = true;
    }
}

bool ir_preempt_for_frame(IrState& state) {
    if (!state.armed) return false;
    state.pending = ' ';
    state.armed = false;
    return true;
}

MotorCommand ir_execute_pending(IrState& state) {
    switch (state.pending) {
        case 'f':
            state.pending = ' ';
            return {MotorAction::kForward, kBtForwardSpeed};
        case 'b':
            state.pending = ' ';
            return {MotorAction::kBackward, kBtForwardSpeed};
        case 'l':
            state.pending = ' ';
            return {MotorAction::kRotateLeft, kBtForwardSpeed};
        case 'r':
            state.pending = ' ';
            return {MotorAction::kRotateRight, kBtForwardSpeed};
        case 's':
            state.pending = ' ';
            return {MotorAction::kStop, 0};
        default:
            return {MotorAction::kNone, 0};
    }
}

}  // namespace robotcar
