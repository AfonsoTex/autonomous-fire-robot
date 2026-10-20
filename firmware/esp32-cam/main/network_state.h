#pragma once
// Declares shared network state for the main loop, Wi-Fi manager, and camera.

#include <WiFi.h>
#include <WiFiUdp.h>

// The PC server listens for control on port 1883 and video on port 1884.
// IP picks the machine, port picks which program on it.
#define SERVER_PORT 1883
#define VIDEO_PORT 1884

// Reuse a fixed packet buffer to avoid repeated heap allocations.
extern char buffer[128];

// Destination for camera UDP packets. Control discovers the PC separately.
// Set the video address in config.h for the current network.
extern const char *videoDestination;

// UDP sockets for control commands and video streaming.
extern WiFiUDP udpControl;
extern WiFiUDP udpVideo;

// Last time a valid MOV or HB arrived from the PC. Silence for >1 s stops the
// motors; >5 s triggers another UDP discovery packet.
extern unsigned long lastHeartbeat;
extern unsigned long wifiLostTimestamp;
extern bool trackingLostWifi;
extern unsigned long lastReconnectAttempt;
