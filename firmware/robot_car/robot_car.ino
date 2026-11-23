// Robot car firmware: Arduino glue only. All decision logic lives in the
// Arduino-free RobotCarCore library (firmware/libraries/RobotCarCore) so the
// same code runs in the host unit tests.

#include <Servo.h>

#include "IRremote.h"
#include "RobotCarCore.h"

using namespace robotcar;

IRremote ir(pins::kIrReceiver);
Servo servo_A2;

String BT_value;
String BT_value_temp;
IrState ir_state{' ', false};

void Move_Forward(int car_speed) {
  digitalWrite(pins::kMotorDirA1, LOW);
  digitalWrite(pins::kMotorDirA2, HIGH);
  digitalWrite(pins::kMotorDirB1, LOW);
  digitalWrite(pins::kMotorDirB2, HIGH);
  analogWrite(pins::kMotorPwmA, car_speed);
  analogWrite(pins::kMotorPwmB, car_speed);
}

void Move_Backward(int car_speed) {
  digitalWrite(pins::kMotorDirA1, HIGH);
  digitalWrite(pins::kMotorDirA2, LOW);
  digitalWrite(pins::kMotorDirB1, HIGH);
  digitalWrite(pins::kMotorDirB2, LOW);
  analogWrite(pins::kMotorPwmA, car_speed);
  analogWrite(pins::kMotorPwmB, car_speed);
}

void Rotate_Left(int car_speed) {
  digitalWrite(pins::kMotorDirA1, LOW);
  digitalWrite(pins::kMotorDirA2, HIGH);
  digitalWrite(pins::kMotorDirB1, HIGH);
  digitalWrite(pins::kMotorDirB2, LOW);
  analogWrite(pins::kMotorPwmA, car_speed);
  analogWrite(pins::kMotorPwmB, car_speed);
}

void Rotate_Right(int car_speed) {
  digitalWrite(pins::kMotorDirA1, HIGH);
  digitalWrite(pins::kMotorDirA2, LOW);
  digitalWrite(pins::kMotorDirB1, LOW);
  digitalWrite(pins::kMotorDirB2, HIGH);
  analogWrite(pins::kMotorPwmA, car_speed);
  analogWrite(pins::kMotorPwmB, car_speed);
}

void STOP() {
  digitalWrite(pins::kMotorDirA1, HIGH);
  digitalWrite(pins::kMotorDirA2, HIGH);
  digitalWrite(pins::kMotorDirB1, HIGH);
  digitalWrite(pins::kMotorDirB2, HIGH);
  analogWrite(pins::kMotorPwmA, 0);
  analogWrite(pins::kMotorPwmB, 0);
}

void apply_motor(MotorCommand cmd) {
  switch (cmd.action) {
    case MotorAction::kForward:
      Move_Forward(cmd.speed);
      break;
    case MotorAction::kBackward:
      Move_Backward(cmd.speed);
      break;
    case MotorAction::kRotateLeft:
      Rotate_Left(cmd.speed);
      break;
    case MotorAction::kRotateRight:
      Rotate_Right(cmd.speed);
      break;
    case MotorAction::kStop:
      STOP();
      break;
    case MotorAction::kNone:
      break;
  }
}

void Infrared_Tracing() {
  LineReading reading;
  reading.left_black = digitalRead(pins::kLineLeft) == 1;
  reading.center_black = digitalRead(pins::kLineCenter) == 1;
  reading.right_black = digitalRead(pins::kLineRight) == 1;
  apply_motor(line_follow_command(reading));
}

int checkdistance() {
  digitalWrite(pins::kUltrasonicTrig, LOW);
  delayMicroseconds(2);
  digitalWrite(pins::kUltrasonicTrig, HIGH);
  delayMicroseconds(10);
  digitalWrite(pins::kUltrasonicTrig, LOW);
  int distance = distance_cm_from_echo_us(pulseIn(pins::kUltrasonicEcho, HIGH));
  delay(10);
  return distance;
}

void Ultrasonic_Avoidance() {
  int front_distance = checkdistance();
  if (avoidance_triggered(front_distance)) {
    STOP();
    delay(100);
    servo_A2.write(180);
    delay(500);
    int left_distance = checkdistance();
    delay(100);
    servo_A2.write(0);
    delay(500);
    int right_distance = checkdistance();
    delay(100);
    MotorAction turn = avoidance_turn(left_distance, right_distance);
    if (turn == MotorAction::kRotateLeft) {
      Rotate_Left(kAvoidTurnSpeed);
    } else {
      Rotate_Right(kAvoidTurnSpeed);
    }
    servo_A2.write(90);
    delay(300);
  } else {
    Move_Forward(kAvoidCruiseSpeed);
  }
}

void ir_remote_control() {
  MotorCommand pending = ir_execute_pending(ir_state);
  if (pending.action == MotorAction::kStop) {
    STOP();
  } else if (pending.action != MotorAction::kNone) {
    apply_motor(pending);
    delay(kIrPulseMs);
    STOP();
  }
  ir_note_keycode(ir_state, (uint8_t)ir.getCode());
}

void setup() {
  Serial.begin(9600);
  BT_value = "";
  BT_value_temp = "";
  ir_state.pending = ' ';
  ir_state.armed = false;
  pinMode(pins::kLineLeft, INPUT);
  pinMode(pins::kLineCenter, INPUT);
  pinMode(pins::kLineRight, INPUT);
  pinMode(pins::kMotorDirA1, OUTPUT);
  pinMode(pins::kMotorDirA2, OUTPUT);
  pinMode(pins::kMotorDirB1, OUTPUT);
  pinMode(pins::kMotorDirB2, OUTPUT);
  pinMode(pins::kUltrasonicTrig, OUTPUT);
  pinMode(pins::kUltrasonicEcho, INPUT);
  servo_A2.attach(pins::kServo);
}

void loop() {
  while (true) {
    while (Serial.available() > 0) {
      BT_value_temp = BT_value_temp + ((char)(Serial.read()));
      delay(2);
      if (Serial.available() == 0) {
        BT_value = BT_value_temp;
        BT_value_temp = "";
      }
    }
    if (BT_value.length() > 0) {
      Serial.println(BT_value);
      Serial.println(BT_value.length());
      BtMessage msg = parse_bt_message(BT_value.c_str(), BT_value.length());
      if (msg.kind == BtFrameKind::kTooLong) {
        BT_value = "";
        STOP();
      } else if (msg.kind == BtFrameKind::kCommand) {
        if (ir_preempt_for_frame(ir_state)) {
          STOP();
        }
        switch (msg.command) {
          case 'F':
          case 'B':
          case 'L':
          case 'R': {
            MotorCommand cmd = bt_one_shot(msg.command);
            apply_motor(cmd);
            delay((msg.command == 'F' || msg.command == 'B') ? kBtDriveHoldMs
                                                               : kBtRotateHoldMs);
            BT_value = "";
            break;
          }
          case 'T':
            Infrared_Tracing();
            break;
          case 'S':
            BT_value = "";
            STOP();
            break;
          case 'A':
            Ultrasonic_Avoidance();
            break;
          default:
            break;
        }
      }
    } else {
      STOP();
    }
    ir_remote_control();
  }
}
