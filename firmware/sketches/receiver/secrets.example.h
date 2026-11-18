#pragma once

// Development-only credentials for the bench network.
//
// Copy this file to secrets.h and fill in the local values. secrets.h is
// git-ignored so that Wi-Fi passwords and device tokens are never committed.
// This mechanism is temporary; production devices will be provisioned over the
// local admin interface or with per-device credentials.
#define AK_DEV_WIFI_SSID ""
#define AK_DEV_WIFI_PASSWORD ""
#define AK_DEV_DEVICE_TOKEN ""

// Provisioning AP password. Prefer firmware/.env (AK_AP_PASSWORD), which
// tools/fw-build.sh injects; define it here only when building without the
// scripts. WPA2 requires 8-63 characters.
// #define AK_AP_PASSWORD "replace-with-8-to-63-characters"
