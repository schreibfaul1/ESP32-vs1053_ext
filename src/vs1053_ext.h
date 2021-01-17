/*
 *  vs1053_ext.h
 *
 *  Created on: Jul 09.2017
 *  Updated on: Jan 17 2021
 *      Author: Wolle
 */

#ifndef _vs1053_ext
#define _vs1053_ext

#include "Arduino.h"
#include "SPI.h"
#include "SD.h"
#include "FS.h"
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

#define VS1053_HEADER          2    //const for datamode
#define VS1053_DATA            4
#define VS1053_METADATA        8
#define VS1053_PLAYLISTINIT   16
#define VS1053_PLAYLISTHEADER 32
#define VS1053_PLAYLISTDATA   64
#define VS1053_OGG           128

class VS1053
{
  private:
    WiFiClient client;
    WiFiClientSecure clientsecure;
    File mp3file;
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

    char sbuf[256];
    char path[256];

    uint8_t  m_ringbuf[0x5000]; // 20480d           // Ringbuffer for mp3 stream
    uint8_t  m_rev=0;                               // Revision
    const uint16_t m_ringbfsiz=sizeof(m_ringbuf);   // Ringbuffer size
    uint16_t m_rbwindex=0;                          // Ringbuffer writeindex
    uint16_t m_rbrindex=0;                          // Ringbuffer readindex
    uint16_t m_ringspace=0;                         // Ringbuffer free space
    uint16_t m_rcount=0;                            // Ringbuffer used space
    int             m_id3Size=0;                    // length id3 tag
    boolean         m_ssl=false;
    uint32_t        m_t0;                           // Keep alive, end a playlist
    uint8_t         m_endFillByte ;                 // Byte to send when stopping song
    uint16_t        m_datamode=0;                   // Statemaschine
    String          m_metaline ;                    // Readable line in metadata
    String          m_mp3title;                     // Name of the mp3 file
    String          m_lastHost="";                  // Store the last URL to a webstream
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
    uint32_t        m_totalcount = 0;               // Counter mp3 data
    int             m_metacount=0;                  // Number of bytes in metadata
    String          m_icyname ;                     // Icecast station name
    String          m_icystreamtitle ;              // Streamtitle from metadata
    String          m_playlist ;                    // The URL of the specified playlist
    int8_t          m_playlist_num = 0 ;            // Nonzero for selection from playlist
    boolean         m_firstmetabyte=false;          // True if first metabyte (counter)
    boolean         m_f_hostreq = false ;           // Request for new host
    boolean         m_f_localfile = false ;         // Play from local mp3-file
    boolean         m_f_webstream = false ;         // Play from URL
    boolean         m_f_plsFile=false;              // Set if URL is known
    boolean         m_f_plsTitle=false;             // Set if StationName is knowm
    boolean         m_f_ogg=false;                  // Set if oggstream
    boolean         m_f_stream_ready=false;         // Set after connecttohost and first streamdata are available
    boolean         m_f_unsync = false;
    boolean         m_f_exthdr = false;             // ID3 extended header
    String          m_plsURL;
    String          m_plsStationName;
    const char volumetable[22]={   0,50,60,65,70,75,80,82,84,86,
                                  88,90,91,92,93,94,95,96,97,98,99,100}; //22 elements
  protected:
    inline void DCS_HIGH() {GPIO.out_w1ts = (1 << dcs_pin);}
    inline void DCS_LOW()  {GPIO.out_w1tc = (1 << dcs_pin);}
    inline void CS_HIGH()  {GPIO.out_w1ts = (1 << cs_pin);}
    inline void CS_LOW()   {GPIO.out_w1tc = (1 << cs_pin);}
    inline void await_data_request() const
    {
      while ( !digitalRead ( dreq_pin ) )
      {
        NOP() ;                                   	// Very short delay
      }
    }
    void control_mode_on();
    void control_mode_off();
    void data_mode_on();
    void data_mode_off();
    uint16_t read_register ( uint8_t _reg ) ;
    void     write_register ( uint8_t _reg, uint16_t _value );
    void     sdi_send_buffer ( uint8_t* data, size_t len ) ;
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
    inline bool data_request() const
    {
      return ( digitalRead ( dreq_pin ) == HIGH ) ;
    }


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
    bool	 connecttoSD(String sdfile);
    bool     connecttospeech(String speech, String lang);
    uint32_t getFileSize();
    uint32_t getFilePos();
    bool     setFilePos(uint32_t pos);
    inline uint8_t getDatamode(){
       	return m_datamode;
       }
    inline void setDatamode(uint8_t dm){
       	m_datamode=dm;
       }
    inline uint32_t streamavail() {if(m_ssl==false) return client.available(); else return clientsecure.available();}
} ;

#endif
