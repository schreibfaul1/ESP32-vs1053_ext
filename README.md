# ESP32 vs1053_ext
Whith this class You can easily build a MiniWebRadio in Adruino or Eclipse SDK.
I have found the Originalcode by EdZelf ESP32 Webradio.
The code is extended with a WiFi-client. You can play many stations, m3u,mp3,aac.
Also You can play mp3-files.<br>
The class provides optional events:<br>
vs1053_showstreaminfo &nbsp;&nbsp;&nbsp; Shows th connexted URL<br>
vs1053_showstreamtitle &nbsp;&nbsp;&nbsp; The played title<br>
vs1053_showstation &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; The name of the connected station<br>
vs1053_info &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp;&nbsp;  Additional information for debugging<br>

```
#include "Arduino.h"
#include <SPI.h>
#include <WiFi.h>
#include "vs1053_ext.h"

// Digital I/O used
#define VS1053_CS     2
#define VS1053_DCS    4
#define VS1053_DREQ   36

String ssid =     "Wolles-POWERLINE";
String password = "xxxx";

int volume=15;

VS1053 mp3(VS1053_CS, VS1053_DCS, VS1053_DREQ);

//The setup function is called once at startup of the sketch
void setup() {

    Serial.begin(115200);
    SPI.begin();
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    while (WiFi.status() != WL_CONNECTED) delay(1500);
    mp3.begin();
    mp3.setVolume(volume);
    mp3.connecttohost("edge.audio.3qsdn.com/senderkw-mp3");
}

// The loop function is called in an endless loop
void loop()
{
    mp3.loop();
}

// optional:
void vs1053_info(const char *info) {                // called from vs1053
    Serial.print("DEBUG:       ");
    Serial.print(info);                                 // debug infos
}

void vs1053_showstation(const char *info){          // called from vs1053
    Serial.print("STATION:     ");
    Serial.println(info);                                 // Show station name
}
void vs1053_showstreamtitle(const char *info){      // called from vs1053
    Serial.print("STREAMTITLE: ");
    Serial.print(info);                                 // Show title
}
void vs1053_showstreaminfo(const char *info){           // called from vs1053
    Serial.print("STREAMINFO:  ");
    Serial.print(info);                                   // Show streaminfo
}
```

ESP32 developerboard connections



