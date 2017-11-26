# ESP32 vs1053_ext
With this class You can easily build a MiniWebRadio in Adruino or Eclipse SDK.
I have found the Originalcode by EdZelf ESP32 Webradio.
The code is extended with a WiFi-client. This library can play many radiostations up to 320kb/s.
Chunked data transfer is supported. Playlists can be m3u or pls, dataformat can be mp3, aac, or ogg.
Also it can play mp3-files.<br>
The class provides optional events:<br>
vs1053_showstreaminfo &nbsp;&nbsp;&nbsp; Shows th connexted URL<br>
vs1053_showstreamtitle &nbsp;&nbsp;&nbsp; The played title<br>
vs1053_showstation &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; The name of the connected station<br>
vs1053_info &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp;&nbsp;  Additional information for debugging<br>

``` c++
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
    //SD.begin();
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    while (WiFi.status() != WL_CONNECTED) delay(1500);
    mp3.begin();
    mp3.setVolume(volume);
    mp3.connecttohost("stream.landeswelle.de/lwt/mp3-192"); // mp3 192kb/s
	//mp3.connecttohost("listen.ai-radio.org:8000/320.ogg?cc=DE&now=1511557873.987&");  // ogg
	//mp3.connecttohost("tophits.radiomonster.fm/320.mp3");  //bitrate 320k
	//mp3.connecttohost("hellwegradiowest.radiovonhier.de/high/stream.mp3"); // Transfer Encoding: chunked
	//mp3.connecttoSD("/mp3files/320k_test.mp3"); // SD card
}

// The loop function is called in an endless loop
void loop()
{
    mp3.loop();
}

// optional:
void vs1053_info(const char *info) {                // called from vs1053
    Serial.print("DEBUG:       ");
    Serial.print(info);                             // debug infos
}
void vs1053_showstation(const char *info){          // called from vs1053
    Serial.print("STATION:     ");
    Serial.println(info);                           // Show station name
}
void vs1053_showstreamtitle(const char *info){      // called from vs1053
    Serial.print("STREAMTITLE: ");
    Serial.print(info);                             // Show title
}
void vs1053_showstreaminfo(const char *info){       // called from vs1053
    Serial.print("STREAMINFO:  ");
    Serial.print(info);                             // Show streaminfo
}
```

ESP32 developerboard connections
![Connections](https://github.com/schreibfaul1/ESP32-vs1053_ext/blob/master/additional%20info/ESP32_dev_board.jpg)

Tested with this mp3 module
![mp3 module](https://github.com/schreibfaul1/ESP32-vs1053_ext/blob/master/additional%20info/MP3_Board.gif)


