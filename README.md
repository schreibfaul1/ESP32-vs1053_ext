# ESP32 vs1053_ext
With this library You can easily build a MiniWebRadio in Adruino or Eclipse SDK.
I have found the Originalcode by EdZelf ESP32 Webradio.
The code is extended with a WiFi-client. This library can play many radiostations up to 320kb/s.
Chunked data transfer is supported. Playlists can be m3u, pls or asx, dataformat can be mp3, wma, aac, or ogg,
asx playlists must contains only audio but no additional videodata.
Also it can play mp3-files from SD Card and from text using Google Translate Service (thanks to horihiro, included esp8266-google-tts library)<br>
The class provides optional events:<br>
vs1053_showstreaminfo &nbsp;&nbsp;&nbsp; shows th connexted URL<br>
vs1053_showstreamtitle &nbsp;&nbsp;&nbsp; the played title<br>
vs1053_showstation &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; the name of the connected station<br>
vs1053_info &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; additional information for debugging<br>
vs1053_bitrate &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; this is the bitrate of the set station<br>
vs1053_eof_mp3 &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; mp3 player reaches the end of file<br>
vs1053_eof_stream  &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; end of web file<br>
vs1053_commercial  &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; is there a commercial at the begin, show the duration in seconds<br>
vs1053_icyurl &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; if the station have a homepage, show the URL<br>
vs1053_lasthost &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; contains the really connected URL (originally may be changed by redirection)

``` c++
#include "Arduino.h"
#include <SPI.h>
#include <WiFi.h>
#include "vs1053_ext.h"

// Digital I/O used
#define VS1053_CS      2
#define VS1053_DCS     4
#define VS1053_DREQ   36

#define VS1053_MOSI   23
#define VS1053_MISO   19
#define VS1053_SCK    18

String ssid =     "Wolles-POWERLINE";
String password = "xxxx";

int volume=15;

VS1053 mp3(VS1053_CS, VS1053_DCS, VS1053_DREQ, VSPI, VS1053_MOSI, VS1053_MISO, VS1053_SCK);

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
    //  mp3.loadUserCode(); // FLAC plugin
    mp3.setVolume(volume);
    mp3.connecttohost("streambbr.ir-media-tec.com/berlin/mp3-128/vtuner_web_mp3/");
    //mp3.connecttohost("stream.landeswelle.de/lwt/mp3-192");                 // mp3 192kb/s
    //mp3.connecttohost("http://radio.hear.fi:8000/hear.ogg");                // ogg
    //mp3.connecttohost("tophits.radiomonster.fm/320.mp3");                   // bitrate 320k
    //mp3.connecttohost("http://star.jointil.net/proxy/jrn_beat?mp=/stream"); // chunked data transfer
    //mp3.connecttohost("http://stream.srg-ssr.ch/rsp/aacp_48.asx");          // asx
    //mp3.connecttohost("www.surfmusic.de/m3u/100-5-das-hitradio,4529.m3u");  // m3u
    //mp3.connecttohost("https://raw.githubusercontent.com/schreibfaul1/ESP32-audioI2S/master/additional_info/Testfiles/Pink-Panther.wav"); // webfile
    //mp3.connecttohost("http://stream.revma.ihrhls.com/zc5060/hls.m3u8");    // HLS
    //mp3.connecttohost("https://live-cdn.sr.se/pool2/p2musik/p2musik.isml/p2musik-audio=192000.m3u8"); // HLS transport stream
    //mp3.connecttoFS(SD, "320k_test.mp3"); // SD card, local file
    //mp3.connecttospeech("Wenn die Hunde schlafen, kann der Wolf gut Schafe stehlen.", "de");
}

// The loop function is called in an endless loop
void loop()
{
    mp3.loop();
}

// next code is optional:
void vs1053_info(const char *info) {                // called from vs1053
    Serial.print("DEBUG:        ");
    Serial.println(info);                           // debug infos
}
void vs1053_showstation(const char *info){          // called from vs1053
    Serial.print("STATION:      ");
    Serial.println(info);                           // Show station name
}
void vs1053_showstreamtitle(const char *info){      // called from vs1053
    Serial.print("STREAMTITLE:  ");
    Serial.println(info);                           // Show title
}
void vs1053_showstreaminfo(const char *info){       // called from vs1053
    Serial.print("STREAMINFO:   ");
    Serial.println(info);                           // Show streaminfo
}
void vs1053_eof_mp3(const char *info){              // called from vs1053
    Serial.print("vs1053_eof:   ");
    Serial.print(info);                             // end of mp3 file (filename)
}
void vs1053_bitrate(const char *br){                // called from vs1053
    Serial.print("BITRATE:      ");
    Serial.println(String(br)+"kBit/s");            // bitrate of current stream
}
void vs1053_commercial(const char *info){           // called from vs1053
    Serial.print("ADVERTISING:  ");
    Serial.println(String(info)+"sec");             // info is the duration of advertising
}
void vs1053_icyurl(const char *info){               // called from vs1053
    Serial.print("Homepage:     ");
    Serial.println(info);                           // info contains the URL
}
void vs1053_eof_speech(const char *info){           // called from vs1053
    Serial.print("end of speech:");
    Serial.println(info);
}
void vs1053_lasthost(const char *info){             // really connected URL
    Serial.print("lastURL:      ");
    Serial.println(info);
}

```
Breadboard
![Breadboard](https://github.com/schreibfaul1/ESP32-vs1053_ext/blob/master/additional%20info/Breadboard.jpg)

ESP32 developerboard connections
![Connections](https://github.com/schreibfaul1/ESP32-vs1053_ext/blob/master/additional%20info/ESP32_dev_board.jpg)

Tested with this mp3 module
![mp3 module](https://github.com/schreibfaul1/ESP32-vs1053_ext/blob/master/additional%20info/MP3_Board.gif)


