// ESP32-CAM RC Car
#include "config.h"
#include "network_state.h"
#include "motor_control.h"
#include "wifi_manager.h"
#include "camera_stream.h"

void setup() {
    Serial.begin(115200);
    setup_motor_pins();          // motor direction pins
    pwm_channel_setup();    // motor PWM on LEDC channels 0/1
    stop_motors();
    if (RASPBERRY_PERMISSION_PIN < 0) {
        Serial.println("Raspberry permission pin is not configured (-1). Motors remain stopped.");
    }
    setup_flash_memory();   // init NVS partition
    dump_nvs_to_serial();   // debug: print stored networks

    setup_camera();         // configure and initialize the OV2640 sensor

    // Connects to the Wi-Fi network and starts the video task.
    // This is a blocking function, it only proceeds when Wi-Fi is connected.
    wifi_setup_and_connect();
    // UDP CONTROL INITIALIZATION
    udpControl.begin(SERVER_PORT); // Listens for packets on port 1883
    Serial.printf("UDP Control listening on port %d\n", SERVER_PORT);

    // Broadcast HELLO so the Python server can learn the ESP32's IP and port
    // from the received packet and send control commands back.
    udpControl.beginPacket(CONTROL_DISCOVERY_IP, SERVER_PORT);
    udpControl.print("HELLO");
    udpControl.endPacket();
    
    lastHeartbeat = millis(); 
}

void loop() {
    // Stop even when no packet arrives. Continue receiving the latest requested
    // movement while paused, but apply it only after Raspberry permission returns.
    if (!movement_allowed()) {
        stop_motors();
    }
    // Layer 1 — is WiFi connected?
    // If not: stop the motors and start timing the outage.
    if (WiFi.status() != WL_CONNECTED) {
        clear_movement_command();
        if (!trackingLostWifi) {
            Serial.println("[CAR] Stop: Wi-Fi disconnected.");
            wifiLostTimestamp = millis();
            trackingLostWifi = true;
        }
        if (millis() - wifiLostTimestamp > 10000) ESP.restart();
        return; // Do nothing else if there is no Wi-Fi
    }
    trackingLostWifi = false;

    // Check the previous packet age before receiving another heartbeat. A new HB
    // after a long outage must not revive movement stored before the outage.
    if (millis() - lastHeartbeat > 1000) {
        clear_movement_command();
    }

    // Layer 2 — Read incoming UDP packets
    // Process packets immediately to update lastHeartbeat before testing for timeout.
    int packetSize = udpControl.parsePacket();
    if (packetSize > 0) {
        // Reads the packet directly into the buffer
        int len = udpControl.read(buffer, sizeof(buffer) - 1);
        if (len > 0) {
            buffer[len] = '\0'; // Adds a null terminator to make it a valid string

            if (strncmp(buffer, "MOV:", 4) == 0) {
                if (process_movement_command(buffer)) {
                    lastHeartbeat = millis();
                }
            } else if (strcmp(buffer, "HB\n") == 0 || strcmp(buffer, "HB") == 0) {
                lastHeartbeat = millis();
                if (!has_movement_command()) {
                    // Startup or a network outage cleared the request. Ask for the
                    // PC's current command instead of restarting stale movement.
                    udpControl.beginPacket(udpControl.remoteIP(), udpControl.remotePort());
                    udpControl.print("SEND_MOV");
                    udpControl.endPacket();
                }
            }
        }
    }


    // Layer 3 — Watchdog (Safety Stop & PC Wake-up)
    // If 1000ms (1s) pass without packets (neither MOV nor HB), stop the car for safety.
    static bool commandTimeoutReported = false;
    bool communicationReady = millis() - lastHeartbeat <= 1000;
    update_movement(communicationReady);
    if (!communicationReady) {
        if (!commandTimeoutReported) {
            Serial.println("[CAR] Stop: no control packets for more than 1 second.");
            commandTimeoutReported = true;
        }
    } else {
        commandTimeoutReported = false;
    }

    // If 5000ms (5s) of silence pass, the Python script on the PC might have restarted 
    // and lost our IP. We resend "HELLO" every 2 seconds to wake it up.
    if (millis() - lastHeartbeat > 5000) {
        if (millis() - lastReconnectAttempt > 2000) {
            lastReconnectAttempt = millis();
            Serial.printf("[%lu] Prolonged silence. Sending HELLO to PC...\n", millis());
            
            udpControl.beginPacket(CONTROL_DISCOVERY_IP, SERVER_PORT);
            udpControl.print("HELLO");
            udpControl.endPacket();
        }
    }
}
