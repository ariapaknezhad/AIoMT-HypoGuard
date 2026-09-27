/* =====================================================
   config.example.h
   -----------------------------------------------------
   Copy this file to "config.h" and fill in your own
   credentials. Never commit the real "config.h" file —
   it is already excluded via .gitignore.
   ===================================================== */

#ifndef CONFIG_H
#define CONFIG_H

// ---------- WiFi ----------
#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// ---------- ThingSpeak ----------
#define THINGSPEAK_CHANNEL_NUMBER 0000000UL      // your channel number
#define THINGSPEAK_WRITE_API_KEY  "YOUR_WRITE_API_KEY"

#endif
