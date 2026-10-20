#include "motor_control.h"
#include <Arduino.h>
#include <stdio.h>   // sscanf
#include <stdlib.h>
#include <string.h>
#include <cmath>

// H-bridge (L293D)
// EN pins carry PWM and set how much power each side gets.
// IN pins are digital only and set the direction, shared by both motors:
// both go forward or both go backward, never one each way.
// Steering comes from feeding the two sides different PWM duty.
const int EN1_pin = 14;   // PWM, left side
const int EN2_pin = 15;   // PWM, right side
const int IN1_pin = 12;   // direction pair: HIGH/LOW = forward
const int IN2_pin = 13;   //                 LOW/HIGH = backward

// LEDC hardware channels for the two PWM outputs.
const int PWM_CHANNEL_1 = 0;
const int PWM_CHANNEL_2 = 1;

// Hardware PWM keeps running by itself. Remember what was actually applied,
// so repeated network commands do not rewrite the same motor outputs.
static bool commandApplied = false;
static float appliedSpeed = 0;
static float appliedSteering = 0;
static bool motorsStopped = false; // Force the first stop to write zero after PWM setup.

// What the PC currently requests can differ from what the motors are doing.
// Keep receiving changes while paused, so releasing permission uses the newest request.
static bool commandReceived = false;
static float requestedSpeed = 0;
static float requestedSteering = 0;

void stop_motors() {
    if (motorsStopped) {
        return;
    }
    ledcWrite(EN1_pin, 0);
    ledcWrite(EN2_pin, 0);
    motorsStopped = true;
    // The previous movement is no longer applied. Once permission returns,
    // update_movement() reapplies the stored request, even if its values did not change.
    commandApplied = false;
}

bool movement_allowed() {
    if (RASPBERRY_PERMISSION_PIN < 0) {
        return false;
    }
    int level = digitalRead(RASPBERRY_PERMISSION_PIN);
    static int previousLevel = -1;
    if (level != previousLevel) {
        if (level == HIGH) {
            Serial.println("[CAR] Raspberry permission HIGH.");
        } else {
            Serial.println("[CAR] Stop: Raspberry permission LOW.");
        }
        previousLevel = level;
    }
    return level == HIGH;
}

void setup_motor_pins() {
    // Without a HIGH permission signal, including a disconnected wire, remain stopped.
    if (RASPBERRY_PERMISSION_PIN >= 0) {
        pinMode(RASPBERRY_PERMISSION_PIN, INPUT_PULLDOWN);
    }
    pinMode(IN1_pin, OUTPUT);
    pinMode(IN2_pin, OUTPUT);

    digitalWrite(IN1_pin, LOW);
    digitalWrite(IN2_pin, LOW);
}

void pwm_channel_setup() {
    // ledcAttachChannel also sets the pin as output, so no pinMode needed.
    ledcAttachChannel(EN1_pin, 1000, 8, PWM_CHANNEL_1);
    ledcAttachChannel(EN2_pin, 1000, 8, PWM_CHANNEL_2);
}

// Turns speed and steering (both -1..1, from the PC) into direction and
// per-side power. IN pins set direction for both motors; the EN duty of
// each side is cut by the steering amount
void motor_logic(float speed, float steering) {
    // A stored PC command must never override the Raspberry's stop signal.
    if (!movement_allowed()) {
        stop_motors();
        return;
    }
    if (commandApplied && speed == appliedSpeed && steering == appliedSteering) {
        return;
    }
    Serial.printf("MOTOR: speed=%.2f steering=%.2f\n", speed, steering);

    // Direction
    if(speed == 0){
        digitalWrite(IN1_pin, LOW);
        digitalWrite(IN2_pin, LOW);
    }
    else if(speed > 0){
        digitalWrite(IN1_pin, HIGH);
        digitalWrite(IN2_pin, LOW);
    }
    else if(speed < 0){
        digitalWrite(IN1_pin, LOW);
        digitalWrite(IN2_pin, HIGH);
    }

    // Convert requested speed and steering into per-side PWM duty.
    int leftDuty = 0;
    int rightDuty = 0;

    if(steering >= 0){
        leftDuty = (int)(speed * 255);
        rightDuty  = (int)(speed * 255 * (1 - steering));
    }
    else{
        rightDuty  = (int)(speed * 255);
        leftDuty = (int)(speed * 255 * (1 + steering));
    }

    ledcWrite(EN1_pin, abs(leftDuty));
    ledcWrite(EN2_pin, abs(rightDuty));
    appliedSpeed = speed;
    appliedSteering = steering;
    commandApplied = true;
    motorsStopped = speed == 0;
    if (motorsStopped) {
        Serial.println("[CAR] Stop: PC requested zero speed.");
    }
    Serial.printf("EN_LEFT=%d EN_RIGHT=%d\n", abs(leftDuty), abs(rightDuty));
}

bool process_movement_command(const char* data) {
    float move = 0.0;
    float direc = 0.0;
    char extra;

    // Read exactly two numbers. Trailing whitespace is allowed, extra text is not.
    if (sscanf(data, "MOV:%f,DIR:%f %c", &move, &direc, &extra) != 2) {
        return false;
    }
    if (!std::isfinite(move) || !std::isfinite(direc) ||
        move < -1 || move > 1 || direc < -1 || direc > 1) {
        return false;
    }
    requestedSpeed = move;
    requestedSteering = direc;
    commandReceived = true;
    return true;
}

bool has_movement_command() {
    return commandReceived;
}

void clear_movement_command() {
    commandReceived = false;
    stop_motors();
}

void update_movement(bool communicationReady) {
    if (!communicationReady) {
        clear_movement_command();
        return;
    }
    if (!commandReceived) {
        stop_motors();
        return;
    }
    // motor_logic checks the GPIO and leaves unchanged PWM running by itself.
    motor_logic(requestedSpeed, requestedSteering);
}
