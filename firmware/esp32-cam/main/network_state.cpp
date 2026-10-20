#include "network_state.h"
#include "config.h"

// Reuse a fixed packet buffer to avoid repeated heap allocations.
char buffer[128];

// Destination for camera UDP packets. Control discovers the PC separately.
// Set the video address in config.h for the current network.
const char *videoDestination = VIDEO_DESTINATION_IP;

// UDP sockets for control commands and video streaming.
WiFiUDP udpControl;
WiFiUDP udpVideo;

// Last time a valid MOV or HB arrived from the PC. Silence for >1 s stops the
// motors; >5 s triggers another UDP discovery packet.
unsigned long lastHeartbeat = 0;
unsigned long wifiLostTimestamp = 0;
bool trackingLostWifi = false;
unsigned long lastReconnectAttempt = 0;
