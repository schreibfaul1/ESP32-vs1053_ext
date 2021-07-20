/*
 *  vs1053_ext.h
 *
 *  Created on: Jul 09.2017
 *  Updated on: Jul 20 2021
 *      Author: Wolle
 */

#ifndef _vs1053_ext
#define _vs1053_ext

#include "Arduino.h"
#include "base64.h"
#include "SPI.h"
#include "SD.h"
#include "SD_MMC.h"
#include "SPIFFS.h"
#include "FS.h"
#include "FFat.h"
#include "WiFiClient.h"
#include "WiFiClientSecure.h"

extern __attribute__((weak)) void vs1053_info(const char*);
extern __attribute__((weak)) void vs1053_showstreamtitle(const char*);
extern __attribute__((weak)) void vs1053_showstation(const char*);
extern __attribute__((weak)) void vs1053_showstreaminfo(const char*);
extern __attribute__((weak)) void vs1053_id3data(const char*); //ID3 metadata
extern __attribute__((weak)) void vs1053_eof_mp3(const char*);
extern __attribute__((weak)) void vs1053_eof_speech(const char*);
extern __attribute__((weak)) void vs1053_bitrate(const char*);
extern __attribute__((weak)) void vs1053_commercial(const char*);
extern __attribute__((weak)) void vs1053_icyurl(const char*);
extern __attribute__((weak)) void vs1053_lasthost(const char*);

class VS1053{

private:
    WiFiClient client;
    WiFiClientSecure clientsecure;
    File audiofile;


private:
    enum : int { VS1053_NONE, VS1053_HEADER , VS1053_DATA, VS1053_METADATA, VS1053_PLAYLISTINIT,
                 VS1053_PLAYLISTHEADER,  VS1053_PLAYLISTDATA, VS1053_SWM, VS1053_OGG};
    enum : int { FORMAT_NONE = 0, FORMAT_M3U = 1, FORMAT_PLS = 2, FORMAT_ASX = 3};

private:
    uint8_t       cs_pin ;                        	// Pin where CS line is connected
    uint8_t       dcs_pin ;                       	// Pin where DCS line is connected
    uint8_t       dreq_pin ;                      	// Pin where DREQ line is connected
    uint8_t       curvol ;                        	// Current volume setting 0..100%

    const uint8_t vs1053_chunk_size = 32 ;
    // SCI Register
    const uint8_t SCI_MODE          = 0x0 ;
    const uint8_t SCI_STATUS        = 0x1 ;
    const uint8_t SCI_BASS          = 0x2 ;
    const uint8_t SCI_CLOCKF        = 0x3 ;
    const uint8_t SCI_DECODE_TIME   = 0x4 ;
    const uint8_t SCI_AUDATA        = 0x5 ;
    const uint8_t SCI_WRAM          = 0x6 ;
    const uint8_t SCI_WRAMADDR      = 0x7 ;
    const uint8_t SCI_HDAT0         = 0x8 ;
    const uint8_t SCI_HDAT1         = 0x9 ;
    const uint8_t SCI_AIADDR        = 0xA ;
    const uint8_t SCI_VOL           = 0xB ;
    const uint8_t SCI_AICTRL0       = 0xC ;
    const uint8_t SCI_AICTRL1       = 0xD ;
    const uint8_t SCI_AICTRL2       = 0xE ;
    const uint8_t SCI_AICTRL3       = 0xF ;
    // SCI_MODE bits
    const uint8_t SM_SDINEW         = 11 ;        	// Bitnumber in SCI_MODE always on
    const uint8_t SM_RESET          = 2 ;        	// Bitnumber in SCI_MODE soft reset
    const uint8_t SM_CANCEL         = 3 ;         	// Bitnumber in SCI_MODE cancel song
    const uint8_t SM_TESTS          = 5 ;         	// Bitnumber in SCI_MODE for tests
    const uint8_t SM_LINE1          = 14 ;        	// Bitnumber in SCI_MODE for Line input

    SPISettings     VS1053_SPI;                     // SPI settings for this slave

    char chbuf[256];
    char path[256];

    uint8_t  m_ringbuf[0x5000]; // 20480d           // Ringbuffer for mp3 stream
    char     m_line[512];                           // stores plsLine or metaLine
    char     m_lastHost[256];                       // Store the last URL to a webstream
    uint8_t  m_rev=0;                               // Revision
    uint8_t  m_playlistFormat = 0;                  // M3U, PLS, ASX
    const uint16_t m_ringbfsiz=sizeof(m_ringbuf);   // Ringbuffer size
    uint16_t m_rbwindex=0;                          // Ringbuffer writeindex
    uint16_t m_rbrindex=0;                          // Ringbuffer readindex
    uint16_t m_ringspace=0;                         // Ringbuffer free space
    uint16_t m_rcount=0;                            // Ringbuffer used space
    int             m_id3Size=0;                    // length id3 tag
    bool            m_f_ssl=false;
    uint8_t         m_endFillByte ;                 // Byte to send when stopping song
    uint16_t        m_datamode=0;                   // Statemaschine
    String          m_metaline ;                    // Readable line in metadata
    String          m_mp3title;                     // Name of the mp3 file
    String          m_icyurl="";                    // Store ie icy-url if received
    String          m_st_remember="";               // Save the last streamtitle
    bool            m_chunked = false ;             // Station provides chunked transfer
    bool            m_ctseen=false;                 // First line of header seen or not
    bool            m_firstchunk=true;              // First chunk as input
    int             m_LFcount;                      // Detection of end of header
    uint32_t        m_chunkcount = 0 ;              // Counter for chunked transfer
    uint32_t        m_metaint = 0;                  // Number of databytes between metadata
    int             m_bitrate = 0;                  // Bitrate in kb/sec
    int16_t         m_btp=0;                        // Bytes to play
    int             m_metacount=0;                  // Number of bytes in metadata
    String          m_icyname ;                     // Icecast station name
    String          m_icystreamtitle ;              // Streamtitle from metadata
    bool            m_firstmetabyte=false;          // True if first metabyte (counter)
    bool            m_f_running = false;
    bool            m_f_localfile = false ;         // Play from local mp3-file
    bool            m_f_webstream = false ;         // Play from URL
    bool            m_f_ogg=false;                  // Set if oggstream
    bool            m_f_stream_ready=false;         // Set after connecttohost and first streamdata are available
    bool            m_f_unsync = false;
    bool            m_f_exthdr = false;             // ID3 extended header

    const char volumetable[22]={   0,50,60,65,70,75,80,82,84,86,
                                  88,90,91,92,93,94,95,96,97,98,99,100}; //22 elements
protected:
    inline void DCS_HIGH() {(dcs_pin&0x20) ? GPIO.out1_w1ts.data = 1 << (dcs_pin - 32) : GPIO.out_w1ts = 1 << dcs_pin;}
	inline void DCS_LOW()  {(dcs_pin&0x20) ? GPIO.out1_w1tc.data = 1 << (dcs_pin - 32) : GPIO.out_w1tc = 1 << dcs_pin;}
	inline void CS_HIGH()  {( cs_pin&0x20) ? GPIO.out1_w1ts.data = 1 << ( cs_pin - 32) : GPIO.out_w1ts = 1 <<  cs_pin;}
    inline void CS_LOW()   {( cs_pin&0x20) ? GPIO.out1_w1tc.data = 1 << ( cs_pin - 32) : GPIO.out_w1tc = 1 <<  cs_pin;}
    inline void await_data_request() {while(!digitalRead(dreq_pin)) NOP();}	  // Very short delay
    inline bool data_request()     {return(digitalRead(dreq_pin) == HIGH);}
	
    void     control_mode_on();
    void     control_mode_off();
    void     data_mode_on();
    void     data_mode_off();
    uint16_t read_register ( uint8_t _reg ) ;
    void     write_register ( uint8_t _reg, uint16_t _value );
    void     sdi_send_buffer ( uint8_t* data, size_t len ) ;
    size_t   sendBytes(uint8_t* data, size_t len);
    void     sdi_send_fillers ( size_t length ) ;
    void     wram_write ( uint16_t address, uint16_t data ) ;
    uint16_t wram_read ( uint16_t address ) ;
    void     handlebyte(uint8_t b);
    void     showstreamtitle ( const char *ml, bool full );
    bool     chkhdrline ( const char* str );
    void     startSong() ;                               // Prepare to start playing. Call this each
                                                         // time a new song starts.
    void     stopSong() ;                                // Finish playing a song. Call this after
                                                         // the last playChunk call.
    String   urlencode(String str);
    void     readID3Metadata();
    void     processLocalFile();
    void     processWebStream();
    void     processPlayListData();
    void     UTF8toASCII(char* str);
    void     setDefaults();

    


public:
    // Constructor.  Only sets pin values.  Doesn't touch the chip.  Be sure to call begin()!
    VS1053 ( uint8_t _cs_pin, uint8_t _dcs_pin, uint8_t _dreq_pin ) ;
    ~VS1053();

    void     begin() ;                                  // Begin operation.  Sets pins correctly,
                                                        // and prepares SPI bus.
    void     stop_mp3client();
    void     setVolume(uint8_t vol);                    // Set the player volume.Level from 0-21, higher is louder.
    void     setTone(uint8_t* rtone);                   // Set the player baas/treble, 4 nibbles for treble gain/freq and bass gain/freq
    uint8_t  getVolume();                               // Get the current volume setting, higher is louder.
    void     printDetails();                            // Print configuration details to serial output.
    bool     printVersion();                            // Print ID and version of vs1053 chip
    void     softReset() ;                              // Do a soft reset
    void 	 loop();
    uint16_t ringused();
    bool     connecttohost(String host);
    bool     connecttohost(const char* host, const char* user = "", const char* pwd = "");
    bool	 connecttoSD(String sdfile);
    bool     connecttoSD(const char* sdfile);
    bool     connecttoFS(fs::FS &fs, const char* path);
    bool     connecttospeech(String speech, String lang);
    uint32_t getFileSize();
    uint32_t getFilePos();
    bool     setFilePos(uint32_t pos);

    // implement several function with respect to the index of string
    bool startsWith (const char* base, const char* str) { return (strstr(base, str) - base) == 0;}
    bool endsWith (const char* base, const char* str) {
        int blen = strlen(base);
        int slen = strlen(str);
        return (blen >= slen) && (0 == strcmp(base + blen - slen, str));
    }
    int indexOf (const char* base, const char* str, int startIndex) {
        int result;
        int baselen = strlen(base);
        if (strlen(str) > baselen || startIndex > baselen) result = -1;
        else {
            char* pos = strstr(base + startIndex, str);
            if (pos == NULL) result = -1;
            else result = pos - base;
        }
        return result;
    }
    int lastIndexOf(const char* base, const char* str) {
        int res = -1, result = -1;
        int lenBase = strlen(base);
        int lenStr  = strlen(str);
        if(lenStr > lenBase) {return -1;} // str should not longer than base
        for(int i=0; i<(lenBase - lenStr); i++){
            res = indexOf(base, str, i);
            if(res > result) result = res;
        }
        return result;
    }

    inline uint8_t  getDatamode(){return m_datamode;}
    inline void     setDatamode(uint8_t dm){m_datamode=dm;}
    inline uint32_t streamavail() {if(m_f_ssl==false) return client.available(); else return clientsecure.available();}
};

#endif
