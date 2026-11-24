// Host-only dump of RobotCarCore decisions for tools/render_figure.py.
// Prints one line per input so the figure script plots real firmware
// outputs; this program is never compiled for the Arduino target.

#include <cstdio>

#include "RobotCarCore.h"

using namespace robotcar;

namespace {

const char* action_name(MotorAction action) {
    switch (action) {
        case MotorAction::kNone:
            return "none";
        case MotorAction::kForward:
            return "forward";
        case MotorAction::kBackward:
            return "backward";
        case MotorAction::kRotateLeft:
            return "rotate_left";
        case MotorAction::kRotateRight:
            return "rotate_right";
        case MotorAction::kStop:
            return "stop";
    }
    return "?";
}

}  // namespace

int main() {
    // Line states: bit 2 = left, bit 1 = center, bit 0 = right (l = MSB).
    for (int s = 0; s < 8; ++s) {
        LineReading reading{(s & 4) != 0, (s & 2) != 0, (s & 1) != 0};
        MotorCommand cmd = line_follow_command(reading);
        std::printf(
            "line %d%d%d %s %d\n",
            static_cast<int>(reading.left_black),
            static_cast<int>(reading.center_black),
            static_cast<int>(reading.right_black),
            action_name(cmd.action),
            cmd.speed);
    }
    // Avoidance sweep, 1..60 cm with a self-consistent reading
    // (front = left = right = d).
    for (int d = 1; d <= 60; ++d) {
        MotorCommand cmd = avoidance_command(d, d, d);
        std::printf("avoid %d %s %d\n", d, action_name(cmd.action), cmd.speed);
    }
    return 0;
}
