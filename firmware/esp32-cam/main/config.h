#pragma once
#define VIDEO_DESTINATION_IP  "192.168.1.65"
// Always broadcast HELLO on the local network so the Python server can
// discover the ESP32 even if the PC's IP changes.
// Video uses VIDEO_DESTINATION_IP; update it when the PC's address changes.
#define CONTROL_DISCOVERY_IP "255.255.255.255"
#define AP_SSID     "YOUR_AP_SSID"
#define AP_PASSWORD "YOUR_AP_PASSWORD"
