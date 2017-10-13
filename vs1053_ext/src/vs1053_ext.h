
#ifndef _vs1053_ext
#define _vs1053_ext

#include "Arduino.h"
#include "SPI.h"
#include "WiFi.h"
#include "SD.h"
#include "FS.h"

extern __attribute__((weak)) void vs1053_info(const char*);
extern __attribute__((weak)) void vs1053_showstreamtitle(const char*);
extern __attribute__((weak)) void vs1053_showstation(const char*);
extern __attribute__((weak)) void vs1053_showstreaminfo(const char*);
extern __attribute__((weak)) void vs1053_eof_mp3(const char*);

#define VS1053_INIT            1  //const for datamode
#define VS1053_HEADER          2
#define VS1053_DATA            4
#define VS1053_METADATA        8
#define VS1053_PLAYLISTINIT   16
#define VS1053_PLAYLISTHEADER 32
#define VS1053_PLAYLISTDATA   64
#define VS1053_STOPREQD      128
#define VS1053_STOPPED       256









class VS1053
{
  private:
	WiFiClient client;                  			// An instance of the client
	File mp3file;
  private:
	uint8_t       cs_pin ;                        	// Pin where CS line is connected
    uint8_t       dcs_pin ;                       	// Pin where DCS line is connected
    uint8_t       dreq_pin ;                      	// Pin where DREQ line is connected
    uint8_t       curvol ;                        	// Current volume setting 0..100%
    const uint8_t vs1053_chunk_size = 32 ;
    // SCI Register
    const uint8_t SCI_MODE          = 0x0 ;
    const uint8_t SCI_BASS          = 0x2 ;
    const uint8_t SCI_CLOCKF        = 0x3 ;
    const uint8_t SCI_AUDATA        = 0x5 ;
    const uint8_t SCI_WRAM          = 0x6 ;
    const uint8_t SCI_WRAMADDR      = 0x7 ;
    const uint8_t SCI_AIADDR        = 0xA ;
    const uint8_t SCI_VOL           = 0xB ;
    const uint8_t SCI_AICTRL0       = 0xC ;
    const uint8_t SCI_AICTRL1       = 0xD ;
    const uint8_t SCI_num_registers = 0xF ;
    // SCI_MODE bits
    const uint8_t SM_SDINEW         = 11 ;        	// Bitnumber in SCI_MODE always on
    const uint8_t SM_RESET          = 2 ;        	// Bitnumber in SCI_MODE soft reset
    const uint8_t SM_CANCEL         = 3 ;         	// Bitnumber in SCI_MODE cancel song
    const uint8_t SM_TESTS          = 5 ;         	// Bitnumber in SCI_MODE for tests
    const uint8_t SM_LINE1          = 14 ;        	// Bitnumber in SCI_MODE for Line input
    SPISettings   VS1053_SPI ;                    	// SPI settings for this slave
    uint8_t       endFillByte ;                   	// Byte to send when stopping song
    char sbuf[256];
    char path[256];
    uint32_t t0;

	const uint16_t   ringbfsiz=20480;			  	// Ringbuffer for smooth playing
	uint8_t*         ringbuf=0 ;                  	// Ringbuffer for VS1053
	uint16_t         rbwindex = 0 ;               	// Fill pointer in ringbuffer
	uint16_t         rbrindex = ringbfsiz - 1 ;   	// Emptypointer in ringbuffer
	uint16_t         rcount = 0 ;                 	// Number of bytes in ringbuffer
	uint16_t		 datamode=0;
	String           metaline ;                     // Readable line in metadata
	String           mp3title;                      // Name of the mp3 file
	bool             chunked = false ;              // Station provides chunked transfer
	int              chunkcount = 0 ;               // Counter for chunked transfer
	int metaint = 0;                    			// Number of databytes between metadata
	int bitrate = 0;                    			// Bitrate in kb/sec
	uint32_t totalcount = 0;            			// Counter mp3 data
	int datacount=0;                      			// Counter databytes before metadata
	int metacount=0;                      			// Number of bytes in metadata
	String           icyname ;                      // Icecast station name
	String           icystreamtitle ;               // Streamtitle from metadata
	String           playlist ;                     // The URL of the specified playlist
	int8_t           playlist_num = 0 ;             // Nonzero for selection from playlist
	bool             hostreq = false ;              // Request for new host
	bool             localfile = false ;            // Play from local mp3-file or not
	bool             plsFile=false;                 // Set if URL is known
	bool             plsTitle=false;                // Set if StationName is knowm
	String           plsURL;
	String           plsStationName;
	const char volumetable[22]={   0,50,60,65,70,75,80,82,84,86,
	    						  88,90,91,92,93,94,95,96,97,98,99,100}; //22 elements
  protected:
	inline void DCS_HIGH() {GPIO.out_w1ts = (1 << dcs_pin);}
	inline void DCS_LOW()  {GPIO.out_w1tc = (1 << dcs_pin);}
	inline void CS_HIGH() {GPIO.out_w1ts = (1 << cs_pin);}
	inline void CS_LOW()  {GPIO.out_w1tc = (1 << cs_pin);}
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
    void 	 handlebyte ( uint8_t b, bool force );
    void 	 putring ( uint8_t* buf, uint16_t len );
    uint8_t  getring();
    void 	 emptyring();
    void 	 showstreamtitle ( const char *ml, bool full );
    void 	 handlebyte_ch ( uint8_t b, bool force=false );
    uint16_t ringspace();

    bool 	 chkhdrline ( const char* str );
    void 	 stop_mp3client ();
    inline bool data_request() const
    {
      return ( digitalRead ( dreq_pin ) == HIGH ) ;
    }


  public:
    // Constructor.  Only sets pin values.  Doesn't touch the chip.  Be sure to call begin()!
    VS1053 ( uint8_t _cs_pin, uint8_t _dcs_pin, uint8_t _dreq_pin ) ;
    ~VS1053();

    void     begin() ;                                   // Begin operation.  Sets pins correctly,
                                                         // and prepares SPI bus.
    void     startSong() ;                               // Prepare to start playing. Call this each
                                                         // time a new song starts.
    void     playChunk ( uint8_t* data, size_t len ) ;   // Play a chunk of data.  Copies the data to
                                                         // the chip.  Blocks until complete.
    void     stopSong() ;                                // Finish playing a song. Call this after
                                                         // the last playChunk call.
    void     setVolume ( uint8_t vol ) ;                 // Set the player volume.Level from 0-100,
                                                         // higher is louder.
    void     setTone ( uint8_t* rtone ) ;                // Set the player baas/treble, 4 nibbles for
                                                         // treble gain/freq and bass gain/freq
    uint8_t  getVolume() ;                               // Get the current volume setting.
                                                         // higher is louder.
    void     printDetails ( const char *header ) ;       // Print configuration details to serial output.
    void     softReset() ;                               // Do a soft reset
    bool     testComm ( const char *header ) ;           // Test communication with module
    void 	 loop();
    bool     connecttohost(String host);
    bool	 connecttoSD(String sdfile);
    inline uint16_t ringused(){
   		return rcount ;                     	// Return number of bytes available for getring()
   	}
    inline uint8_t getDatamode(){
       	return datamode;
       }
    inline void setDatamode(uint8_t dm){
       	datamode=dm;
       }
    inline uint32_t streamavail() {return client.available();}
} ;

#endif
