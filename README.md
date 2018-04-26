# ESP32 vs1053_ext
With this library You can easily build a MiniWebRadio in Adruino or Eclipse SDK.
I have found the Originalcode by EdZelf ESP32 Webradio.
The code is extended with a WiFi-client. This library can play many radiostations up to 320kb/s.
Chunked data transfer is supported. Playlists can be m3u, pls or asx, dataformat can be mp3, wma, aac, or ogg,
asx playlists must contains only audio but no additional videodata.
Also it can play mp3-files from SD Card.<br>
The class provides optional events:<br>
vs1053_showstreaminfo &nbsp;&nbsp;&nbsp; shows th connexted URL<br>
vs1053_showstreamtitle &nbsp;&nbsp;&nbsp; the played title<br>
vs1053_showstation &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; the name of the connected station<br>
vs1053_info &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; additional information for debugging<br>
vs1053_bitrate &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; this is the bitrate of the set station<br>
vs1053_eof_mp3 &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; mp3 player reaches the end of file<br>
vs1053_commercial  &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; is there a commercial at the begin, show the duration in seconds<br>
vs1053_icyurl &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; if the station have a homepage, show the URL

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
    mp3.connecttohost("streambbr.ir-media-tec.com/berlin/mp3-128/vtuner_web_mp3/");
        //mp3.connecttohost("stream.landeswelle.de/lwt/mp3-192"); // mp3 192kb/s
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
void vs1053_eof_mp3(const char *info){              // called from vs1053
      Serial.print("vs1053_eof: ");
      Serial.print(info);                           // end of mp3 file (filename)
}
void vs1053_bitrate(const char *br){		    // called from vs1053
      Serial.print("BITRATE: ");
      Serial.println(String(br)+"kBit/s");          // bitrate of current stream
}
void vs1053_commercial(const char *info){           // called from vs1053
    Serial.print("ADVERTISING: ");
    Serial.println(String(info)+"sec");             // info is the duration of advertising
}
void vs1053_icyurl(const char *info){               // called from vs1053
    Serial.print("Homepage: ");  
    Serial.println(info);                           // info contains the URL
}
```
Breadboard
![Breadboard](https://github.com/schreibfaul1/ESP32-vs1053_ext/blob/master/additional%20info/Breadboard.jpg)

ESP32 developerboard connections
![Connections](https://github.com/schreibfaul1/ESP32-vs1053_ext/blob/master/additional%20info/ESP32_dev_board.jpg)

Tested with this mp3 module
![mp3 module](https://github.com/schreibfaul1/ESP32-vs1053_ext/blob/master/additional%20info/MP3_Board.gif)


