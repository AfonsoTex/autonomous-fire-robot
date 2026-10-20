#pragma once
// Controls the motors through the L293D H-bridge.

// GPIO2 receives movement permission from Raspberry BCM GPIO23; share GND.
// Do not use microSD on this pin. Disconnect this signal when programming the ESP.
// Set -1 to leave the input unconfigured and keep the motors stopped.
constexpr int RASPBERRY_PERMISSION_PIN = 2;

bool movement_allowed();

void setup_motor_pins();
void pwm_channel_setup();
void stop_motors();
void motor_logic(float speed, float steering);
// Store the latest valid MOV, including while the Raspberry holds the car stopped.
bool process_movement_command(const char* data);
bool has_movement_command();
// Forget old movement after a network outage; a fresh MOV is required to resume.
void clear_movement_command();
// Apply the stored command only with a live PC connection and Raspberry permission.
void update_movement(bool communicationReady);
