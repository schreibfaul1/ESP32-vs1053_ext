#include "vs1053_ext.h"

VS1053::VS1053(uint8_t _cs_pin, uint8_t _dcs_pin, uint8_t _dreq_pin) :
        cs_pin(_cs_pin), dcs_pin(_dcs_pin), dreq_pin(_dreq_pin)
{
    endFillByte=0;
    curvol=50;
    t0=0;
    ringbuf=(uint8_t *)malloc(ringbfsiz);       // Create ring buffer
}
VS1053::~VS1053(){
    free(ringbuf);
}
//---------------------------------------------------------------------------------------
void VS1053::control_mode_on()
{
    SPI.beginTransaction(VS1053_SPI);           // Prevent other SPI users
    DCS_HIGH();                                 // Bring slave in control mode
    CS_LOW();
}
void VS1053::control_mode_off()
{
    CS_HIGH();                                  // End control mode
    SPI.endTransaction();                       // Allow other SPI users
}
void VS1053::data_mode_on()
{
    SPI.beginTransaction(VS1053_SPI);           // Prevent other SPI users
    CS_HIGH();                                  // Bring slave in data mode
    DCS_LOW();
}
void VS1053::data_mode_off()
{
    //digitalWrite(dcs_pin, HIGH);              // End data mode
    DCS_HIGH();
    SPI.endTransaction();                       // Allow other SPI users
}
//---------------------------------------------------------------------------------------
uint16_t VS1053::read_register(uint8_t _reg)
{
    uint16_t result=0;
    control_mode_on();
    SPI.write(3);                                // Read operation
    SPI.write(_reg);                             // Register to write (0..0xF)
    // Note: transfer16 does not seem to work
    result=(SPI.transfer(0xFF) << 8) | (SPI.transfer(0xFF));  // Read 16 bits data
    await_data_request();                        // Wait for DREQ to be HIGH again
    control_mode_off();
    return result;
}
//---------------------------------------------------------------------------------------
void VS1053::write_register(uint8_t _reg, uint16_t _value)
{
    control_mode_on();
    SPI.write(2);                                // Write operation
    SPI.write(_reg);                             // Register to write (0..0xF)
    SPI.write16(_value);                         // Send 16 bits data
    await_data_request();
    control_mode_off();
}
//---------------------------------------------------------------------------------------
void VS1053::sdi_send_buffer(uint8_t* data, size_t len)
{
    size_t chunk_length;                         // Length of chunk 32 byte or shorter

    data_mode_on();
    while(len){                                  // More to do?

        await_data_request();                    // Wait for space available
        chunk_length=len;
        if(len > vs1053_chunk_size){
            chunk_length=vs1053_chunk_size;
        }
        len-=chunk_length;
        SPI.writeBytes(data, chunk_length);
        data+=chunk_length;
    }
    data_mode_off();
}
//---------------------------------------------------------------------------------------
void VS1053::sdi_send_fillers(size_t len){

    size_t chunk_length;                         // Length of chunk 32 byte or shorter

    data_mode_on();
    while(len)                                   // More to do?
    {
        await_data_request();                    // Wait for space available
        chunk_length=len;
        if(len > vs1053_chunk_size){
            chunk_length=vs1053_chunk_size;
        }
        len-=chunk_length;
        while(chunk_length--){
            SPI.write(endFillByte);
        }
    }
    data_mode_off();
}
//---------------------------------------------------------------------------------------
void VS1053::wram_write(uint16_t address, uint16_t data){

    write_register(SCI_WRAMADDR, address);
    write_register(SCI_WRAM, data);
}
//---------------------------------------------------------------------------------------
uint16_t VS1053::wram_read(uint16_t address){

    write_register(SCI_WRAMADDR, address);       // Start reading from WRAM
    return read_register(SCI_WRAM);              // Read back result
}
//---------------------------------------------------------------------------------------
bool VS1053::testComm(const char *header){

    // Test the communication with the VS1053 module.  The result wille be returned.
    // If DREQ is low, there is problably no VS1053 connected.  Pull the line HIGH
    // in order to prevent an endless loop waiting for this signal.  The rest of the
    // software will still work, but readbacks from VS1053 will fail.
    int i;                                       // Loop control
    uint16_t r1, r2, cnt=0;
    uint16_t delta=300;                          // 3 for fast SPI

    if( !digitalRead(dreq_pin)){

        if(vs1053_info) vs1053_info("VS1053 not properly installed!\n");
        // Allow testing without the VS1053 module
        pinMode(dreq_pin, INPUT_PULLUP);         // DREQ is now input with pull-up
        return false;                            // return bad result
    }
    // Further TESTING.  Check if SCI bus can write and read without errors.
    // We will use the volume setting for this.
    // Will give warnings on serial output if DEBUG is active.
    // A maximum of 20 errors will be reported.
    if(strstr(header, "Fast")){

        delta=3;                                        // Fast SPI, more loops
    }
    if(vs1053_info) vs1053_info(header);                // Show a header
    for(i=0; (i < 0xFFFF) && (cnt < 20); i+=delta){

        write_register(SCI_VOL, i);                     // Write data to SCI_VOL
        r1=read_register(SCI_VOL);                      // Read back for the first time
        r2=read_register(SCI_VOL);                      // Read back a second time
        if(r1 != r2 || i != r1 || i != r2){             // Check for 2 equal reads

            sprintf(sbuf, "VS1053 error retry SB:%04X R1:%04X R2:%04X \n", i, r1, r2);
            if(vs1053_info) vs1053_info(sbuf);
            cnt++;
            delay(10);
        }
    }
    return (cnt == 0);                                 // Return the result
}
//---------------------------------------------------------------------------------------
void VS1053::begin(){

    pinMode(dreq_pin, INPUT);                          // DREQ is an input
    pinMode(cs_pin, OUTPUT);                           // The SCI and SDI signals
    pinMode(dcs_pin, OUTPUT);
    digitalWrite(dcs_pin, HIGH);                       // Start HIGH for SCI en SDI
    digitalWrite(cs_pin, HIGH);
    delay(100);

    // Init SPI in slow mode (0.2 MHz)
    VS1053_SPI=SPISettings(200000, MSBFIRST, SPI_MODE0);
    printDetails("Right after reset/startup \n");
    delay(20);
    //printDetails ("20 msec after reset");
    testComm("Slow SPI,Testing VS1053 read/write registers... \n");
    // Most VS1053 modules will start up in midi mode.  The result is that there is no audio
    // when playing MP3.  You can modify the board, but there is a more elegant way:
    wram_write(0xC017, 3);                             // GPIO DDR=3
    wram_write(0xC019, 0);                             // GPIO ODATA=0
    delay(100);
    //printDetails ("After test loop");
    softReset();                                       // Do a soft reset
    // Switch on the analog parts
    write_register(SCI_AUDATA, 44100 + 1);             // 44.1kHz + stereo
    // The next clocksetting allows SPI clocking at 5 MHz, 4 MHz is safe then.
    write_register(SCI_CLOCKF, 6 << 12);               // Normal clock settings multiplyer 3.0=12.2 MHz
    //SPI Clock to 4 MHz. Now you can set high speed SPI clock.
    VS1053_SPI=SPISettings(4000000, MSBFIRST, SPI_MODE0);
    write_register(SCI_MODE, _BV (SM_SDINEW) | _BV(SM_LINE1));
    testComm("Fast SPI, Testing VS1053 read/write registers again... \n");
    delay(10);
    await_data_request();
    endFillByte=wram_read(0x1E06) & 0xFF;
    sprintf(sbuf, "endFillByte is %X \n", endFillByte);
    if(vs1053_info) vs1053_info(sbuf);
    printDetails("After last clocksetting \n");
    delay(100);
}
//---------------------------------------------------------------------------------------
void VS1053::setVolume(uint8_t vol){
    // Set volume.  Both left and right.
    // Input value is 0..21.  21 is the loudest.
    // Clicking reduced by using 0xf8 to 0x00 as limits.
    uint16_t value;                                      // Value to send to SCI_VOL

    if(vol > 21) vol=21;
    vol=volumetable[vol];
    if(vol != curvol){
        curvol=vol;                                      // Save for later use
        value=map(vol, 0, 100, 0xF8, 0x00);              // 0..100% to one channel
        value=(value << 8) | value;
        write_register(SCI_VOL, value);                  // Volume left and right
    }
}
//---------------------------------------------------------------------------------------
void VS1053::setTone(uint8_t *rtone){                    // Set bass/treble (4 nibbles)

    // Set tone characteristics.  See documentation for the 4 nibbles.
    uint16_t value=0;                                    // Value to send to SCI_BASS
    int i;                                               // Loop control

    for(i=0; i < 4; i++)
            {
        value=(value << 4) | rtone[i];                   // Shift next nibble in
    }
    write_register(SCI_BASS, value);                     // Volume left and right
}
//---------------------------------------------------------------------------------------
uint8_t VS1053::getVolume()                              // Get the currenet volume setting.
{
    return curvol;
}
//---------------------------------------------------------------------------------------
void VS1053::startSong()
{
    sdi_send_fillers(2052);
}
//---------------------------------------------------------------------------------------
void VS1053::playChunk (uint8_t* data, size_t len)
{
  sdi_send_buffer (data, len);
}
//---------------------------------------------------------------------------------------
void VS1053::stopSong()
{
    uint16_t modereg;                     // Read from mode register
    int i;                                // Loop control

    sdi_send_fillers(2052);
    delay(10);
    write_register(SCI_MODE, _BV (SM_SDINEW) | _BV(SM_CANCEL));
    for(i=0; i < 200; i++)
            {
        sdi_send_fillers(32);
        modereg=read_register(SCI_MODE);  // Read status
        if((modereg & _BV(SM_CANCEL)) == 0)
                {
            sdi_send_fillers(2052);
            sprintf(sbuf, "Song stopped correctly after %d msec \n", i * 10);
            if(vs1053_info) vs1053_info(sbuf);
            return;
        }
        delay(10);
    }
    printDetails("Song stopped incorrectly! \n");
}
//---------------------------------------------------------------------------------------
void VS1053::softReset()
{
    write_register(SCI_MODE, _BV (SM_SDINEW) | _BV(SM_RESET));
    delay(10);
    await_data_request();
}
//---------------------------------------------------------------------------------------
void VS1053::printDetails(const char *header){
    uint16_t regbuf[16];
    uint8_t i;

    if(vs1053_info) vs1053_info(header);
    if(vs1053_info) vs1053_info("REG   Contents \n");
    if(vs1053_info) vs1053_info("---   ----- \n");
    for(i=0; i <= SCI_num_registers; i++)
            {
        regbuf[i]=read_register(i);
    }
    for(i=0; i <= SCI_num_registers; i++)
            {
        delay(5);
        sprintf(sbuf, "%3X - %5X \n", i, regbuf[i]);
        if(vs1053_info) vs1053_info(sbuf);
    }
}
//---------------------------------------------------------------------------------------
uint16_t VS1053::ringspace(){
    return (ringbfsiz - rcount);                                 // Free space available
}
//---------------------------------------------------------------------------------------
void VS1053::putring(uint8_t* buf, uint16_t len){
    uint16_t partl;                                              // Partial length to xfer

    if(len){                                                     // anything to do?
        // First see if we must split the transfer.  We cannot write past the ringbuffer end.
        if((rbwindex + len) >= ringbfsiz){
            partl=ringbfsiz - rbwindex;                          // Part of length to xfer
            memcpy(ringbuf + rbwindex, buf, partl);              // Copy next part
            rbwindex=0;
            rcount+=partl;                                       // Adjust number of bytes
            buf+=partl;                                          // Point to next free byte
            len-=partl;                                          // Adjust rest length
        }
        if(len){                                                 // Rest to do?
            memcpy(ringbuf + rbwindex, buf, len);                // Copy full or last part
            rbwindex+=len;                                       // Point to next free byte
            rcount+=len;                                         // Adjust number of bytes
        }
    }
}
//---------------------------------------------------------------------------------------
uint8_t VS1053::getring()
{
    // Assume there is always something in the bufferpace.  See ringavail()
    if(++rbrindex == ringbfsiz){      // Increment pointer and
        rbrindex=0;                    // wrap at end
    }
    rcount--;                          // Count is now one less
    return *(ringbuf + rbrindex);      // return the oldest byte
}
//---------------------------------------------------------------------------------------
void VS1053::emptyring()
{
    rbwindex=0;                      // Reset ringbuffer administration
    rbrindex=ringbfsiz - 1;
    rcount=0;
}
//---------------------------------------------------------------------------------------
bool VS1053::chkhdrline(const char* str){
    char b;                                            // Byte examined
    int len=0;                                         // Lengte van de string

    while((b= *str++)){                                // Search to end of string
        len++;                                         // Update string length
        if( !isalpha(b)){                              // Alpha (a-z, A-Z)
            if(b != '-'){                              // Minus sign is allowed
                if((b == ':') || (b == ';')){          // Found a colon or semicolon?
                    return ((len > 5) && (len < 50));  // Yes, okay if length is okay
                }
                else{
                    return false;                      // Not a legal character
                }
            }
        }
    }
    return false;                                      // End of string without colon
}
//---------------------------------------------------------------------------------------
void VS1053::showstreamtitle(const char *ml, bool full){
    char* p1;
    char* p2;
    char streamtitle[150];                             // Streamtitle from metadata

    if(strstr(ml, "StreamTitle=")){
        sprintf(sbuf, "Streamtitle found %d bytes\n", strlen(ml));
        //for(int i=0; i<strlen(ml); i++) log_e("%c",ml[i]);
        if(vs1053_info) vs1053_info(sbuf);
        p1=(char*)ml + 12;                             // Begin of artist and title
        if((p2=strstr(ml, ";"))){                      // Search for end of title
            if( *p1 == '\''){                          // Surrounded by quotes?
                p1++;
                p2--;
            }
            *p2='\0';                                  // Strip the rest of the line
        }
        // Save last part of string as streamtitle.  Protect against buffer overflow
        strncpy(streamtitle, p1, sizeof(streamtitle));
        streamtitle[sizeof(streamtitle) - 1]='\0';
    }
    else if(full)
    {
        // Info probably from playlist
        strncpy(streamtitle, ml, sizeof(streamtitle));
        streamtitle[sizeof(streamtitle) - 1]='\0';
    }
    else
    {
        icystreamtitle="";                             // Unknown type
        return;                                        // Do not show
    }
    // Save for status request from browser and for MQTT
    icystreamtitle=streamtitle;
    if((p1=strstr(streamtitle, " - ")))                // look for artist/title separator
    {
        *p1++='\n';                                    // Found: replace 3 characters by newline
        p2=p1 + 2;
        if( *p2 == ' ')                                // Leading space in title?
                {
            p2++;
        }
        strcpy(p1, p2);                                // Shift 2nd part of title 2 or 3 places
    }
    sprintf(sbuf, "Streamtitle: %s\n", streamtitle);
    if(vs1053_info) vs1053_info(sbuf);
    if(vs1053_showstreamtitle) vs1053_showstreamtitle(streamtitle);
}
//---------------------------------------------------------------------------------------
void VS1053::handlebyte_ch(uint8_t b, bool force){

    static int chunksize=0;                                // Chunkcount read from stream

    if(chunked && !force &&                                // Test op DATA handling
            (datamode & (VS1053_DATA | VS1053_METADATA | VS1053_PLAYLISTDATA)))
            {
        if(chunkcount == 0)                                // Expecting a new chunkcount?
                {
            if(b == '\r'){                                 // Skip CR
                return;
            }
            else if(b == '\n'){                            // LF ?
                chunkcount=chunksize;                      // Yes, set new count
                //log_e("chunksize %i", chunksize);
                chunksize=0;                               // For next decode
                return;
            }
            // We have received a hexadecimal character.  Decode it and add to the result.
            b=toupper(b) - '0';                            // Be sure we have uppercase
            if(b > 9) b = b - 7;                           // Translate A..F to 10..15
            chunksize=(chunksize << 4) + b;
        }
        else
        {
            handlebyte(b, force);                          // Normal data byte
            chunkcount--;                                  // Update count to next chunksize block
        }
    }
    else
    {
        handlebyte(b, force);                              // Normal handling of this byte
    }
}
//---------------------------------------------------------------------------------------
void VS1053::handlebyte(uint8_t b, bool force){
    static uint16_t playlistcnt;                           // Counter to find right entry in playlist
    static bool firstmetabyte;                             // True if first metabyte (counter)
    static int LFcount;                                    // Detection of end of header
    static __attribute__((aligned(4)))  uint8_t buf[32];   // Buffer for chunk
    static int bufcnt=0;                                   // Data in chunk
    static bool firstchunk=true;                           // First chunk as input
    String lcml;                                           // Lower case metaline
    static String ct;                                      // Contents type
    String host;
    static bool ctseen=false;                              // First line of header seen or not
    int inx;                                               // Pointer in metaline
    int i;                                                 // Loop control

    if(datamode == VS1053_INIT)                            // Initialize for header receive
    {
        ctseen=false;                                      // Contents type not seen yet
        metaint=0;                                         // No metaint found
        LFcount=0;                                         // For detection end of header
        bitrate=0;                                         // Bitrate still unknown
        if(vs1053_info) vs1053_info("Switch to HEADER\n");
        datamode=VS1053_HEADER;                            // Handle header
        totalcount=0;                                      // Reset totalcount
        metaline="";                                       // No metadata yet
        icyname="";                                        // No StationName yet
        firstchunk=true;                                   // First chunk expected
    }
    if(datamode == VS1053_DATA)                            // Handle next byte of MP3/Ogg data
    {
        buf[bufcnt++]=b;                                   // Save byte in chunkbuffer
        if(bufcnt == sizeof(buf) || force){                // Buffer full?
            if(firstchunk){
                firstchunk=false;
                if(vs1053_info) vs1053_info("First chunk:\n");  // Header for printout of first chunk
                for(i=0; i < 32; i+=8){           // Print 4 lines
                    sprintf(sbuf, "%02X %02X %02X %02X %02X %02X %02X %02X\n",
                            buf[i], buf[i + 1], buf[i + 2], buf[i + 3],
                            buf[i + 4], buf[i + 5], buf[i + 6], buf[i + 7]);
                    if(vs1053_info) vs1053_info(sbuf);
                }
            }
            playChunk(buf, bufcnt);                        // Yes, send to player
            bufcnt=0;                                      // Reset count
        }
        totalcount++;                                      // Count number of bytes, ignore overflow
        if(metaint != 0) {                                  // No METADATA on Ogg streams or mp3 files
            if(--datacount == 0){                           // End of datablock?
                 if(bufcnt){                                 // Yes, still data in buffer?
                    playChunk(buf, bufcnt);                // Yes, send to player
                    bufcnt=0;                              // Reset count
                }
                datamode=VS1053_METADATA;
                if(localfile==true) datamode=VS1053_DATA;  // there are no metadata
                firstmetabyte=true;                        // Expecting first metabyte (counter)
            }
        }
        return;
    }
    if(datamode == VS1053_HEADER)                          // Handle next byte of MP3 header
    {
        if((b > 0x7F) ||                                   // Ignore unprintable characters
                (b == '\r') ||                             // Ignore CR
                (b == '\0'))                               // Ignore NULL
                {
            // Yes, ignore
        }
        else if(b == '\n'){                                // Linefeed ?
            LFcount++;                                     // Count linefeeds
            if(chkhdrline(metaline.c_str())){              // Reasonable input?
                lcml=metaline;                             // Use lower case for compare
                lcml.toLowerCase();
                sprintf(sbuf, "%s\n", metaline.c_str());
                if(vs1053_info) vs1053_info(sbuf);         // Yes, Show it
                if(lcml.indexOf("content-type") >= 0){     // Line with "Content-Type: xxxx/yyy"
                    ctseen=true;                           // Yes, remember seeing this
                    ct=metaline.substring(14);             // Set contentstype. Not used yet
                    //ct.trim;
                    sprintf(sbuf, "%s seen.\n", ct.c_str());
                    if(vs1053_info) vs1053_info(sbuf);
                }
                if(lcml.startsWith("location:")){
                    host=metaline.substring(lcml.indexOf("http://")+7,lcml.length());// use metaline instead lcml
                    if(host.indexOf("&")>0)host=host.substring(0,host.indexOf("&")); // remove parameter
                    log_i("redirect to host %s", host.c_str());
                    connecttohost(host);
                }

                if(lcml.startsWith("icy-br:")){
                    bitrate=metaline.substring(7).toInt(); // Found bitrate tag, read the bitrate
                    if(bitrate == 0){                      // For Ogg br is like "Quality 2"
                        bitrate=87;                        // Dummy bitrate
                    }
                }
                else if(metaline.startsWith("icy-metaint:")){
                    metaint=metaline.substring(12).toInt();// Found metaint tag, read the value
                }
                else if(lcml.startsWith("icy-name:")){
                    icyname=metaline.substring(9);         // Get station name
                    icyname.trim();                        // Remove leading and trailing spaces
                    if(icyname!=""){
                        if(vs1053_showstation) vs1053_showstation(icyname.c_str());
                    }
//                    for(int z=0; z<icyname.length();z++) log_e("%i",icyname[z]);
                }
                else if(lcml.startsWith("transfer-encoding:"))
                        {
                    // Station provides chunked transfer
                    if(metaline.endsWith("chunked"))
                            {
                        chunked=true;                      // Remember chunked transfer mode
                        log_i("chunked data transfer");
                        chunkcount=0;                      // Expect chunkcount in DATA
                    }
                }
            }
            metaline="";                                   // Reset this line
            if((LFcount == 2) && ctseen){                   // Some data seen and a double LF?
                sprintf(sbuf, "Switch to DATA, bitrate is %d, metaint is %d\n", bitrate, metaint); // Show bitrate and metaint
                if(vs1053_info) vs1053_info(sbuf);
                // if no icyname available show defaultname from nvs
                if(icyname==""){if(vs1053_showstation) vs1053_showstation("");}
                datamode=VS1053_DATA;                      // Expecting data now
                datacount=metaint;                         // Number of bytes before first metadata
                bufcnt=0;                                  // Reset buffer count
                startSong();                               // Start a new song
            }
        }
        else
        {
            metaline+=(char)b;                             // Normal character, put new char in metaline
            LFcount=0;                                     // Reset double CRLF detection
        }
        return;
    }
    if(datamode == VS1053_METADATA)                        // Handle next byte of metadata
    {
        if(firstmetabyte)                                  // First byte of metadata?
        {
            firstmetabyte=false;                           // Not the first anymore
            metacount=b * 16 + 1;                          // New count for metadata including length byte
            if(metacount > 1)
                    {

                sprintf(sbuf, "Metadata block %d bytes\n", // Most of the time there are zero bytes of metadata
                        metacount - 1);
                if(vs1053_info) vs1053_info(sbuf);

            }
            metaline="";                                   // Set to empty
        }
        else
        {
            metaline+=(char)b;                             // Normal character, put new char in metaline
        }
        if(--metacount == 0)
                {
            if(metaline.length())                          // Any info present?
            {
                // metaline contains artist and song name.  For example:
                // "StreamTitle='Don McLean - American Pie';StreamUrl='';"
                // Sometimes it is just other info like:
                // "StreamTitle='60s 03 05 Magic60s';StreamUrl='';"
                // Isolate the StreamTitle, remove leading and trailing quotes if present.
                if( !localfile) showstreamtitle(metaline.c_str(), true);         // Show artist and title if present in metadata
            }
            if(metaline.length() > 1500)                   // Unlikely metaline length?
                    {
                if(vs1053_info) vs1053_info("Metadata block to long! Skipping all Metadata from now on.\n");
                metaint=0;                                 // Probably no metadata
                metaline="";                               // Do not waste memory on this
            }
            datacount=metaint;                             // Reset data count
            bufcnt=0;                                      // Reset buffer count
            datamode=VS1053_DATA;                          // Expecting data

        }
    }
    if(datamode == VS1053_PLAYLISTINIT)                    // Initialize for receive .m3u file
    {
        // We are going to use metadata to read the lines from the .m3u file
        // Sometimes this will only contain a single line
        metaline="";                                       // Prepare for new line
        LFcount=0;                                         // For detection end of header
        datamode=VS1053_PLAYLISTHEADER;                    // Handle playlist data
        playlistcnt=1;                                     // Reset for compare
        totalcount=0;                                      // Reset totalcount
        if(vs1053_info) vs1053_info("Read from playlist\n");
    }
    if(datamode == VS1053_PLAYLISTHEADER){                 // Read header
        if((b > 0x7F) ||                                   // Ignore unprintable characters
                (b == '\r') ||                             // Ignore CR
                (b == '\0'))                               // Ignore NULL
                {
            // Yes, ignore
        }
        else if(b == '\n')                                 // Linefeed ?
                {
            LFcount++;                                     // Count linefeeds
            sprintf(sbuf, "Playlistheader: %s\n", metaline.c_str());  // Show playlistheader
            if(vs1053_info) vs1053_info(sbuf);
            metaline="";                                   // Ready for next line
            if(LFcount == 2)
                    {
                if(vs1053_info) vs1053_info("Switch to PLAYLISTDATA\n");
                datamode=VS1053_PLAYLISTDATA;              // Expecting data now
                return;
            }
        }
        else
        {
            metaline+=(char)b;                             // Normal character, put new char in metaline
            LFcount=0;                                     // Reset double CRLF detection
        }
    }
    if(datamode == VS1053_PLAYLISTDATA)                    // Read next byte of .m3u file data
    {
        t0=millis();
        if((b > 0x7F) ||                                   // Ignore unprintable characters
                (b == '\r') ||                             // Ignore CR
                (b == '\0'))                               // Ignore NULL
                { /* Yes, ignore */ }

        else if(b == '\n'){                              // Linefeed or end of string?
            sprintf(sbuf, "Playlistdata: %s\n", metaline.c_str());  // Show playlistdata
            if(vs1053_info) vs1053_info(sbuf);
            if(playlist.endsWith("m3u")){
                if(metaline.length() < 5) {                     // Skip short lines
                    metaline="";                               // Flush line
                    return;}
                if(metaline.indexOf("#EXTINF:") >= 0){          // Info?
                    if(playlist_num == playlistcnt){            // Info for this entry?
                        inx=metaline.indexOf(",");             // Comma in this line?
                        if(inx > 0){
                            // Show artist and title if present in metadata
                            showstreamtitle(metaline.substring(inx + 1).c_str(), true);}}}
                if(metaline.startsWith("#")){                   // Commentline?
                    metaline="";
                    return;}                                    // Ignore commentlines
                // Now we have an URL for a .mp3 file or stream.  Is it the rigth one?
                //if(metaline.indexOf("&")>0)metaline=host.substring(0,metaline.indexOf("&"));
                sprintf(sbuf, "Entry %d in playlist found: %s\n", playlistcnt, metaline.c_str());
                if(vs1053_info) vs1053_info(sbuf);
                if(metaline.indexOf("&")){
                    metaline=metaline.substring(0, metaline.indexOf("&"));}
                if(playlist_num == playlistcnt){
                    inx=metaline.indexOf("http://");           // Search for "http://"
                    if(inx >= 0){                              // Does URL contain "http://"?
                        host=metaline.substring(inx + 7);}     // Yes, remove it and set host
                    else{
                        host=metaline;}                        // Yes, set new host
                    log_i("connecttohost %s", host.c_str());
                    connecttohost(host);                       // Connect to it
                }
                metaline="";
                host=playlist;                                 // Back to the .m3u host
                playlistcnt++;                                 // Next entry in playlist
            } //m3u
            if(playlist.endsWith("pls")){
                if(metaline.startsWith("File1")){
                    inx=metaline.indexOf("http://");           // Search for "http://"
                    if(inx >= 0){                              // Does URL contain "http://"?
                        plsURL=metaline.substring(inx + 7);    // Yes, remove it
                        if(plsURL.indexOf("&")>0)plsURL=plsURL.substring(0,plsURL.indexOf("&")); // remove parameter
                        // Now we have an URL for a .mp3 file or stream in host.

                        plsFile=true;
                    }
                }
                if(metaline.startsWith("Title1")){
                    plsStationName=metaline.substring(7);
                    if(vs1053_showstation) vs1053_showstation(plsStationName.c_str());
                    sprintf(sbuf, "StationName: %s\n", plsStationName.c_str());
                    if(vs1053_info) vs1053_info(sbuf);
                    plsTitle=true;
                }
                if(metaline.startsWith("Length1")) plsTitle=true; // if no Title is available
                if((plsFile==true)&&(metaline.length()==0)) plsTitle=true;
                metaline="";
                if(plsFile && plsTitle){ //we have both StationName and StationURL
                    plsFile=false; plsTitle=false;
                    log_i("connecttohost %s", host.c_str());
                    connecttohost(plsURL); // Connect to it

                }
            }
        }
        else
        {
            metaline+=(char)b;                             // Normal character, add it to metaline
        }
        return;
    }
}
//---------------------------------------------------------------------------------------
void VS1053::loop(){
    static uint8_t tmpbuff[1024];                         // Input buffer for mp3 stream
    static boolean f_once=false;
    static boolean f_mp3_end=false;
    uint32_t maxchunk;                                    // Max number of bytes to read
    int res=0;                                            // Result reading from mp3 stream
    uint32_t rs;                                          // Free space in ringbuffer
    uint32_t av;                                          // Available in stream

    if(f_mp3_end==true){ // wait of empty ringbuffer
        if(rcount==0){
            sprintf(sbuf,"End of mp3file %s\n",mp3title.c_str());
            if(vs1053_info) vs1053_info(sbuf);
            if(vs1053_eof_mp3) vs1053_eof_mp3(mp3title.c_str());
            f_mp3_end=false;
        }
    }

    // Try to keep the ringbuffer filled up by adding as much bytes as possible
    if(datamode & (VS1053_INIT | VS1053_HEADER | VS1053_DATA | VS1053_METADATA | // Test op playing
            VS1053_PLAYLISTINIT | VS1053_PLAYLISTHEADER | VS1053_PLAYLISTDATA)){
        rs=ringspace();                                   // Get free ringbuffer space
        if(rs >= sizeof(tmpbuff)){                        // Need to fill the ringbuffer?
            maxchunk=sizeof(tmpbuff);                     // Reduce byte count for this mp3loop()
            if(localfile){                                // Playing file from SD card?
                av=mp3file.available();                   // Bytes left in file
                if(av == 0){
                    if(f_once==false){
                        f_mp3_end=true;
                        f_once=true;
                    }
                    mp3file.close();
                }
                else f_once=false;
                if(av < maxchunk) maxchunk=av;            // Reduce byte count for this mp3loop()
                if(maxchunk){                             // Anything to read?
                    res=mp3file.read(tmpbuff, maxchunk);  // Read a block of data
                }
            }
            else{
                av=client.available();                    // Available from stream

                if(av < maxchunk) maxchunk=av;            // Limit read size
                if(maxchunk){                             // Anything to read?
                    res=client.read(tmpbuff, maxchunk);   // Read a number of bytes from the stream
                    if(res<0) log_e("can't read from client");
                }
            }
            if(res>0) putring(tmpbuff, res);              // Transfer to ringbuffer
        }
    }
    while(data_request() && ringused())                   // Try to keep VS1053 filled
    {
        handlebyte_ch(getring());                         // Yes, handle it

    }
    if(datamode == VS1053_PLAYLISTDATA){
        if(t0+49<millis()) {
            log_i("terminate metaline after 50ms");
            handlebyte_ch('\n');          // no more ch form client? send lf
        }
    }
    if(datamode == VS1053_STOPREQD){                      // STOP requested?

        if(vs1053_info) vs1053_info("STOP requested\n");

        stop_mp3client();                                 // Disconnect if still connected
        mp3file.close();
        handlebyte_ch(0, true);                           // Force flush of buffer
        setVolume(0);                                     // Mute
        stopSong();                                       // Stop playing
        emptyring();                                      // Empty the ringbuffer
        metaint=0;                                        // No metaint known now
        datamode=VS1053_STOPPED;                          // Yes, state becomes STOPPED
        if(vs1053_info) vs1053_info("VS1053 stopped\n");
        delay(500);
    }
}
//---------------------------------------------------------------------------------------
void VS1053::stop_mp3client(){
    while(client.connected())
    {
        if(vs1053_info) vs1053_info("Stopping client\n"); // Stop connection to host
        client.flush();
        client.stop();
        delay(500);
    }
    client.flush();                                       // Flush stream client
    client.stop();                                        // Stop stream client
}
//---------------------------------------------------------------------------------------
bool VS1053::connecttohost(String host){

    int inx;                                              // Position of ":" in hostname
    //char*       pfs ;                                   // Pointer to formatted string
    int port=80;                                          // Port number for host
    String extension="/";                                 // May be like "/mp3" in "skonto.ls.lv:8002/mp3"
    String hostwoext;                                     // Host without extension and portnumber

    stop_mp3client();                                     // Disconnect if still connected
    emptyring();
    localfile=false;
    sprintf(sbuf, "Connect to new host: %s\n", host.c_str());
    if(vs1053_info) vs1053_info(sbuf);

    setDatamode(VS1053_INIT);                             // Start default in metamode
    chunked=false;                                        // Assume not chunked

    if(host.endsWith(".m3u")|| host.endsWith(".pls")){    // Is it an m3u or pls playlist?
        playlist=host;                                    // Save copy of playlist URL
        datamode=VS1053_PLAYLISTINIT;                     // Yes, start in PLAYLIST mode
        if(playlist_num == 0){                             // First entry to play?
            playlist_num=1;                               // Yes, set index
        }
        sprintf(sbuf, "Playlist request, entry %d\n", playlist_num); // Most of the time there are zero bytes of metadata
        if(vs1053_info) vs1053_info(sbuf);

    }


    // In the URL there may be an extension, like noisefm.ru:8000/play.m3u&t=.m3u
    inx=host.indexOf("/");                                // Search for begin of extension
    if(inx > 0){                                           // Is there an extension?
        extension=host.substring(inx);                    // Yes, change the default
        hostwoext=host.substring(0, inx);                 // Host without extension

    }
    // In the URL there may be a portnumber
    inx=host.indexOf(":");                                // Search for separator
    if(inx >= 0)                                          // Portnumber available?
            {
        port=host.substring(inx + 1).toInt();             // Get portnumber as integer
        hostwoext=host.substring(0, inx);                 // Host without portnumber
    }
    sprintf(sbuf, "Connect to %s on port %d, extension %s\n",
            hostwoext.c_str(), port, extension.c_str());
    if(vs1053_info) vs1053_info(sbuf);
    if(vs1053_showstreaminfo) vs1053_showstreaminfo(sbuf);
    if(client.connect(hostwoext.c_str(), port)){
        if(vs1053_info) vs1053_info("Connected to server\n");
        // This will send the request to the server. Request metadata.
        client.print(String("GET ") +
                extension +
                String(" HTTP/1.1\r\n") +
                String("Host: ") +
                hostwoext +
                String("\r\n") +
                String("Icy-MetaData:1\r\n") +
                String("Connection: close\r\n\r\n"));
        return true;
    }
    sprintf(sbuf, "Request %s failed!\n", host.c_str());
    if(vs1053_info) vs1053_info(sbuf);
    if(vs1053_showstation) vs1053_showstation("");
    if(vs1053_showstreamtitle) vs1053_showstreamtitle("");
    if(vs1053_showstreaminfo) vs1053_showstreaminfo("");
    return false;
}
//---------------------------------------------------------------------------------------
bool VS1053::connecttoSD(String sdfile){

    ;
    uint16_t i=0, j=0;

    stopSong();
    stop_mp3client();                                    // Disconnect if still connected
    emptyring();
    localfile=true;

    while(sdfile[i] != 0){  //convert ISO8859-1 to ASCII
        path[j]=sdfile[i];
        if(path[j] == 228) path[j]=132; // ä
        if(path[j] == 246) path[j]=148; // ö
        if(path[j] == 252) path[j]=129; // ü
        if(path[j] == 196) path[j]=142; // Ä
        if(path[j] == 214) path[j]=153; // Ö
        if(path[j] == 220) path[j]=154; // Ü
        if(path[j] == 223) path[j]=225; // ß
        j++;
        i++;
    }
    path[j]=0;
    mp3title=sdfile.substring(sdfile.lastIndexOf('/') + 1, sdfile.length());
    showstreamtitle(mp3title.c_str(), true);
    sprintf(sbuf, "Reading file: %s\n", path);
    if(vs1053_info) vs1053_info(sbuf);
    fs::FS &fs=SD;
    mp3file=fs.open(path);
    if( !mp3file){
        if(vs1053_info) vs1053_info("Failed to open file for reading\n");
        return false;
    }

    return true;
}
