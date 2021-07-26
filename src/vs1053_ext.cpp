/*
 *  vs1053_ext.cpp
 *
 *  Created on: Jul 09.2017
 *  Updated on: Jul 26 2021
 *      Author: Wolle
 */

#include "vs1053_ext.h"

//---------------------------------------------------------------------------------------------------------------------
AudioBuffer::AudioBuffer(size_t maxBlockSize) {
    // if maxBlockSize isn't set use defaultspace (1600 bytes) is enough for aac and mp3 player
    if(maxBlockSize) m_resBuffSizeRAM  = maxBlockSize;
    if(maxBlockSize) m_maxBlockSize = maxBlockSize;
}

AudioBuffer::~AudioBuffer() {
    if(m_buffer)
        free(m_buffer);
    m_buffer = NULL;
}

size_t AudioBuffer::init() {
    if(m_buffer) free(m_buffer);
    m_buffer = NULL;
    if(psramInit()) {
        // PSRAM found, AudioBuffer will be allocated in PSRAM
        m_buffSize = m_buffSizePSRAM;
        if(m_buffer == NULL) {
            m_buffer = (uint8_t*) ps_calloc(m_buffSize, sizeof(uint8_t));
            m_buffSize = m_buffSizePSRAM - m_resBuffSizePSRAM;
            if(m_buffer == NULL) {
                // not enough space in PSRAM, use ESP32 Flash Memory instead
                m_buffer = (uint8_t*) calloc(m_buffSize, sizeof(uint8_t));
                m_buffSize = m_buffSizeRAM - m_resBuffSizeRAM;
            }
        }
    } else {  // no PSRAM available, use ESP32 Flash Memory"
        m_buffSize = m_buffSizeRAM;
        m_buffer = (uint8_t*) calloc(m_buffSize, sizeof(uint8_t));
        m_buffSize = m_buffSizeRAM - m_resBuffSizeRAM;
    }
    if(!m_buffer)
        return 0;
    resetBuffer();
    return m_buffSize;
}

void AudioBuffer::changeMaxBlockSize(uint16_t mbs){
    m_maxBlockSize = mbs;
    return;
}

uint16_t AudioBuffer::getMaxBlockSize(){
    return m_maxBlockSize;
}

size_t AudioBuffer::freeSpace() {
    if(m_readPtr >= m_writePtr) {
        m_freeSpace = (m_readPtr - m_writePtr);
    } else {
        m_freeSpace = (m_endPtr - m_writePtr) + (m_readPtr - m_buffer);
    }
    if(m_f_start)
        m_freeSpace = m_buffSize;
    return m_freeSpace - 1;
}

size_t AudioBuffer::writeSpace() {
    if(m_readPtr >= m_writePtr) {
        m_writeSpace = (m_readPtr - m_writePtr - 1); // readPtr must not be overtaken
    } else {
        if(getReadPos() == 0)
            m_writeSpace = (m_endPtr - m_writePtr - 1);
        else
            m_writeSpace = (m_endPtr - m_writePtr);
    }
    if(m_f_start)
        m_writeSpace = m_buffSize - 1;
    return m_writeSpace;
}

size_t AudioBuffer::bufferFilled() {
    if(m_writePtr >= m_readPtr) {
        m_dataLength = (m_writePtr - m_readPtr);
    } else {
        m_dataLength = (m_endPtr - m_readPtr) + (m_writePtr - m_buffer);
    }
    return m_dataLength;
}

void AudioBuffer::bytesWritten(size_t bw) {
    m_writePtr += bw;
    if(m_writePtr == m_endPtr) {
        m_writePtr = m_buffer;
    }
    if(bw && m_f_start)
        m_f_start = false;
}

void AudioBuffer::bytesWasRead(size_t br) {
    m_readPtr += br;
    if(m_readPtr >= m_endPtr) {
        size_t tmp = m_readPtr - m_endPtr;
        m_readPtr = m_buffer + tmp;
    }
}

uint8_t* AudioBuffer::getWritePtr() {
    return m_writePtr;
}

uint8_t* AudioBuffer::getReadPtr() {
    size_t len = m_endPtr - m_readPtr;
    if(len < m_maxBlockSize) { // be sure the last frame is completed
        memcpy(m_endPtr, m_buffer, m_maxBlockSize - len);  // cpy from m_buffer to m_endPtr with len
    }
return m_readPtr;
}

void AudioBuffer::resetBuffer() {
    m_writePtr = m_buffer;
    m_readPtr = m_buffer;
    m_endPtr = m_buffer + m_buffSize;
    m_f_start = true;
    // memset(m_buffer, 0, m_buffSize); //Clear Inputbuffer
}

uint32_t AudioBuffer::getWritePos() {
    return m_writePtr - m_buffer;
}

uint32_t AudioBuffer::getReadPos() {
    return m_readPtr - m_buffer;
}
//---------------------------------------------------------------------------------------------------------------------
// **** VS1053 Impl ****
//---------------------------------------------------------------------------------------------------------------------
VS1053::VS1053(uint8_t _cs_pin, uint8_t _dcs_pin, uint8_t _dreq_pin) :
        cs_pin(_cs_pin), dcs_pin(_dcs_pin), dreq_pin(_dreq_pin)
{
    clientsecure.setInsecure();                 // update to ESP32 Arduino version 1.0.5-rc05 or higher
    m_endFillByte=0;
    curvol=50;
    m_LFcount=0;
}
VS1053::~VS1053(){
    // destructor
}
//---------------------------------------------------------------------------------------------------------------------
void VS1053::initInBuff() {
    static bool f_already_done = false;
    if(!f_already_done) {
        size_t size = InBuff.init();
        if(size == m_buffSizeRAM - m_resBuffSizeRAM) {
            sprintf(chbuf, "PSRAM not found, inputBufferSize: %u bytes", size - 1);
            if(vs1053_info)  vs1053_info(chbuf);
            f_already_done = true;
        }
        if(size == m_buffSizePSRAM - m_resBuffSizePSRAM) {
            sprintf(chbuf, "PSRAM found, inputBufferSize: %u bytes", size - 1);
            if(vs1053_info) vs1053_info(chbuf);
            f_already_done = true;
        }
    }
    changeMaxBlockSize(4096);
}
//---------------------------------------------------------------------------------------------------------------------
void VS1053::control_mode_off()
{
    CS_HIGH();                                  // End control mode
    SPI.endTransaction();                       // Allow other SPI users
}
void VS1053::control_mode_on()
{
    SPI.beginTransaction(VS1053_SPI);           // Prevent other SPI users
    DCS_HIGH();                                 // Bring slave in control mode
    CS_LOW();
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
//---------------------------------------------------------------------------------------------------------------------
uint16_t VS1053::read_register(uint8_t _reg)
{
    uint16_t result=0;
    control_mode_on();
    SPI.write(3);                                           // Read operation
    SPI.write(_reg);                                        // Register to write (0..0xF)
    // Note: transfer16 does not seem to work
    result=(SPI.transfer(0xFF) << 8) | (SPI.transfer(0xFF));  // Read 16 bits data
    await_data_request();                                   // Wait for DREQ to be HIGH again
    control_mode_off();
    return result;
}
//---------------------------------------------------------------------------------------------------------------------
void VS1053::write_register(uint8_t _reg, uint16_t _value)
{
    control_mode_on();
    SPI.write(2);                                           // Write operation
    SPI.write(_reg);                                        // Register to write (0..0xF)
    SPI.write16(_value);                                    // Send 16 bits data
    await_data_request();
    control_mode_off();
}
//---------------------------------------------------------------------------------------------------------------------
size_t VS1053::sendBytes(uint8_t* data, size_t len){
    size_t chunk_length = 0;                                // Length of chunk 32 byte or shorter
    size_t bytesDecoded = 0;

    data_mode_on();
    while(len){                                             // More to do?
        if(!digitalRead(dreq_pin)) break;
        chunk_length = len;
        if(len > vs1053_chunk_size){
            chunk_length = vs1053_chunk_size;
        }
        SPI.writeBytes(data, chunk_length);
        data         += chunk_length;
        len          -= chunk_length;
        bytesDecoded += chunk_length;        
    }
    data_mode_off();
    return bytesDecoded;
}
//---------------------------------------------------------------------------------------------------------------------
void VS1053::sdi_send_buffer(uint8_t* data, size_t len)
{
    size_t chunk_length;                                    // Length of chunk 32 byte or shorter

    data_mode_on();
    while(len){                                             // More to do?

        await_data_request();                               // Wait for space available
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
//---------------------------------------------------------------------------------------------------------------------
void VS1053::sdi_send_fillers(size_t len){

    size_t chunk_length;                                    // Length of chunk 32 byte or shorter

    data_mode_on();
    while(len)                                              // More to do?
    {
        await_data_request();                               // Wait for space available
        chunk_length=len;
        if(len > vs1053_chunk_size){
            chunk_length=vs1053_chunk_size;
        }
        len-=chunk_length;
        while(chunk_length--){
            SPI.write(m_endFillByte);
        }
    }
    data_mode_off();
}
//---------------------------------------------------------------------------------------------------------------------
void VS1053::wram_write(uint16_t address, uint16_t data){

    write_register(SCI_WRAMADDR, address);
    write_register(SCI_WRAM, data);
}
//---------------------------------------------------------------------------------------------------------------------
uint16_t VS1053::wram_read(uint16_t address){

    write_register(SCI_WRAMADDR, address);                  // Start reading from WRAM
    return read_register(SCI_WRAM);                         // Read back result
}
//---------------------------------------------------------------------------------------------------------------------
void VS1053::begin(){

    pinMode(dreq_pin, INPUT);                               // DREQ is an input
    pinMode(cs_pin, OUTPUT);                                // The SCI and SDI signals
    pinMode(dcs_pin, OUTPUT);
    DCS_HIGH();
    CS_HIGH();
    delay(100);

    // Init SPI in slow mode (0.2 MHz)
    VS1053_SPI=SPISettings(200000, MSBFIRST, SPI_MODE0);
//    printDetails("Right after reset/startup \n");
    delay(20);
    //printDetails ("20 msec after reset");
    //testComm("Slow SPI,Testing VS1053 read/write registers... \n");
    // Most VS1053 modules will start up in midi mode.  The result is that there is no audio
    // when playing MP3.  You can modify the board, but there is a more elegant way:
    wram_write(0xC017, 3);                                  // GPIO DDR=3
    wram_write(0xC019, 0);                                  // GPIO ODATA=0
    delay(100);
    //printDetails ("After test loop");
    softReset();                                            // Do a soft reset
    // Switch on the analog parts
    write_register(SCI_AUDATA, 44100 + 1);                  // 44.1kHz + stereo
    // The next clocksetting allows SPI clocking at 5 MHz, 4 MHz is safe then.
    write_register(SCI_CLOCKF, 6 << 12);                    // Normal clock settings multiplyer 3.0=12.2 MHz
    //SPI Clock to 4 MHz. Now you can set high speed SPI clock.
    VS1053_SPI=SPISettings(4000000, MSBFIRST, SPI_MODE0);
    write_register(SCI_MODE, _BV (SM_SDINEW) | _BV(SM_LINE1));
    //testComm("Fast SPI, Testing VS1053 read/write registers again... \n");
    delay(10);
    await_data_request();
    m_endFillByte=wram_read(0x1E06) & 0xFF;
//    sprintf(chbuf, "endFillByte is %X", endFillByte);
//    if(vs1053_info) vs1053_info(chbuf);
//    printDetails("After last clocksetting \n");
    loadUserCode(); // load in VS1053B if you want to play flac
    delay(100);
}
//---------------------------------------------------------------------------------------------------------------------
void VS1053::setVolume(uint8_t vol){
    // Set volume.  Both left and right.
    // Input value is 0..21.  21 is the loudest.
    // Clicking reduced by using 0xf8 to 0x00 as limits.
    uint16_t value;                                         // Value to send to SCI_VOL

    if(vol > 21) vol=21;

    if(vol != curvol){
        curvol = vol;                                       // #20       
        vol=volumetable[vol];                               // Save for later use
        value=map(vol, 0, 100, 0xF8, 0x00);                 // 0..100% to one channel
        value=(value << 8) | value;
        write_register(SCI_VOL, value);                     // Volume left and right
    }
}
//---------------------------------------------------------------------------------------------------------------------
void VS1053::setTone(uint8_t *rtone){                       // Set bass/treble (4 nibbles)

    // Set tone characteristics.  See documentation for the 4 nibbles.
    uint16_t value=0;                                       // Value to send to SCI_BASS
    int i;                                                  // Loop control

    for(i=0; i < 4; i++)
            {
        value=(value << 4) | rtone[i];                      // Shift next nibble in
    }
    write_register(SCI_BASS, value);                        // Volume left and right
}
//---------------------------------------------------------------------------------------------------------------------
uint8_t VS1053::getVolume()                                 // Get the currenet volume setting.
{
    return curvol;
}
//----------------------------------------------------------------------------------------------------------------------
void VS1053::startSong()
{
    sdi_send_fillers(2052);
}
//---------------------------------------------------------------------------------------------------------------------
void VS1053::stopSong()
{
    uint16_t modereg;                                       // Read from mode register
    int i;                                                  // Loop control

    m_f_localfile = false;
    m_f_webfile = false;
    m_f_webstream = false;

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
            sprintf(chbuf, "Song stopped correctly after %d msec", i * 10);
            if(vs1053_info) vs1053_info(chbuf);
            return;
        }
        delay(10);
    }
    if(vs1053_info) vs1053_info("Song stopped incorrectly!");
    printDetails();
}
//---------------------------------------------------------------------------------------------------------------------
void VS1053::softReset()
{
    write_register(SCI_MODE, _BV (SM_SDINEW) | _BV(SM_RESET));
    delay(10);
    await_data_request();
}
//---------------------------------------------------------------------------------------------------------------------
void VS1053::printDetails(){
    uint16_t regbuf[16];
    uint8_t i;
    String reg, tmp;

    String regName[16] = {
        "MODE       ", "STATUS     ", "BASS       ", "CLOCKF     ",
        "DECODE_TIME", "AUDATA     ", "WRAM       ", "WRAMADDR   ",
        "HDAT0      ", "HDAT1      ", "AIADDR     ", "VOL        ",
        "AICTRL0    ", "AICTRL1    ", "AICTRL2    ", "AICTRL3    ",
    };

    if(vs1053_info) vs1053_info("REG         Contents   bin   hex");
    if(vs1053_info) vs1053_info("----------- ---------------- ---");
    for(i=0; i <= SCI_AICTRL3; i++){
        regbuf[i]=read_register(i);
    }
    for(i=0; i <= SCI_AICTRL3; i++){
        reg=regName[i]+ " ";
        tmp=String(regbuf[i],2); while(tmp.length()<16) tmp="0"+tmp; // convert regbuf to binary string
        reg=reg+tmp +" ";
        tmp=String(regbuf[i],16); tmp.toUpperCase(); while(tmp.length()<4) tmp="0"+tmp; // conv to hex
        reg=reg+tmp;
        if(vs1053_info) vs1053_info(reg.c_str());
    }
}
//---------------------------------------------------------------------------------------------------------------------
bool VS1053::printVersion(){
    boolean flag=true;
    uint16_t reg1=0, reg2=0;
    reg1=wram_read(0x1E00);
    reg2=wram_read(0x1E01);
    if((reg1==0xFFFF)&&(reg2==0xFFFF)) flag=false; // all high?, seems not connected
    if((reg1==0x0000)&&(reg2==0x0000)) flag=false; // all low?,  not proper connected (no SCK?)
    if(flag==false){reg1=0; reg2=0;}
    sprintf(chbuf, "chipID = %d%d", reg1, reg2);
    if(vs1053_info) vs1053_info(chbuf);
    reg1=wram_read(0x1E02) & 0xFF;
    if(reg1==0xFF) {reg1=0; flag=false;} // version too high
    sprintf(chbuf, "version = %d", reg1);
    if(vs1053_info) vs1053_info(chbuf);
    return flag;
}
//---------------------------------------------------------------------------------------------------------------------
bool VS1053::chkhdrline(const char* str){
    char b;                                            // Byte examined
    int len=0;                                         // Lengte van de string

    while((b= *str++)){                                // Search to end of string
        len++;                                         // Update string length
        if( !isalpha(b)){                              // Alpha (a-z, A-Z)
            if(b != '-'){                              // Minus sign is allowed
                if((b == ':') || (b == ';')){          // Found a colon or semicolon?
                    return ((len > 5) && (len < 200)); // Yes, okay if length is okay
                }
                else{
                    return false;                      // Not a legal character
                }
            }
        }
    }
    return false;                                      // End of string without colon
}
//---------------------------------------------------------------------------------------------------------------------
void VS1053::showstreamtitle(const char *ml, bool full){
    // example for ml:
    // StreamTitle='Oliver Frank - Mega Hitmix';StreamUrl='www.radio-welle-woerthersee.at';
    // or adw_ad='true';durationMilliseconds='10135';adId='34254';insertionType='preroll';

    int16_t pos1=0, pos2=0, pos3=0, pos4=0;
    String mline=ml, st="", su="", ad="", artist="", title="", icyurl="";
    //log_i("%s",mline.c_str());
    pos1=mline.indexOf("StreamTitle=");
    if(pos1!=-1){                                       // StreamTitle found
        pos1=pos1+12;
        st=mline.substring(pos1);                       // remove "StreamTitle="
//      log_i("st_orig %s", st.c_str());
        if(st.startsWith("'{")){
            // special codig like '{"t":"\u041f\u0438\u043a\u043d\u0438\u043a - \u0418...."m":"mdb","lAU":0,"lAuU":18}
            pos2= st.indexOf('"', 8);                   // end of '{"t":".......", seek for double quote at pos 8
            st=st.substring(0, pos2);
            pos2= st.lastIndexOf('"');
            st=st.substring(pos2+1);                    // remove '{"t":"
            pos2=0;
            String uni="";
            String st1="";
            uint16_t u=0;
            uint8_t v=0, w=0;
            for(int i=0; i<st.length(); i++){
                if(pos2>1) pos2++;
                if((st[i]=='\\')&&(pos2==0)) pos2=1;    // backslash found
                if((st[i]=='u' )&&(pos2==1)) pos2=2;    // "\u" found
                if(pos2>2) uni=uni+st[i];               // next 4 values are unicode
                if(pos2==0) st1+=st[i];                 // normal character
                if(pos2>5){
                    pos2=0;
                    u=strtol(uni.c_str(), 0, 16);       // convert hex to int
                    v=u/64 + 0xC0; st1+=char(v);        // compute UTF-8
                    w=u%64 + 0x80; st1+=char(w);
                     //log_i("uni %i  %i", v, w );
                    uni="";
                }
            }
            log_i("st1 %s", st1.c_str());
            st=st1;
        }
        else{
            // normal coding
            if(st.indexOf('&')!=-1){                // maybe html coded
                st.replace("&Auml;", "Ä" ); st.replace("&auml;", "ä"); //HTML -> ASCII
                st.replace("&Ouml;", "Ö" ); st.replace("&ouml;", "o");
                st.replace("&Uuml;", "Ü" ); st.replace("&uuml;", "ü");
                st.replace("&szlig;","ß" ); st.replace("&amp;",  "&");
                st.replace("&quot;", "\""); st.replace("&lt;",   "<");
                st.replace("&gt;",   ">" ); st.replace("&apos;", "'");
            }
            pos2= st.indexOf(';',1);                // end of StreamTitle, first occurence of ';'
            if(pos2!=-1) st=st.substring(0,pos2);   // extract StreamTitle
            if(st.startsWith("'")) st=st.substring(1,st.length()-1); // if exists remove ' at the begin and end
            pos3=st.lastIndexOf(" - ");             // separator artist - title
            if(pos3!=-1){                           // found separator? yes
                artist=st.substring(0,pos3);        // artist not used yet
                title=st.substring(pos3+3);         // title not used yet
            }
        }

        if(m_st_remember!=st){ // show only changes
            if(vs1053_showstreamtitle) vs1053_showstreamtitle(st.c_str());
        }

        m_st_remember=st;
        st="StreamTitle=" + st;
        if(vs1053_info) vs1053_info(st.c_str());
    }
    pos4=mline.indexOf("StreamUrl=");
    if(pos4!=-1){                               // StreamUrl found
        pos4=pos4+10;
        su=mline.substring(pos4);               // remove "StreamUrl="
        pos2= su.indexOf(';',1);                // end of StreamUrl, first occurence of ';'
        if(pos2!=-1) su=su.substring(0,pos2);   // extract StreamUrl
        if(su.startsWith("'")) su=su.substring(1,su.length()-1); // if exists remove ' at the begin and end
        su="StreamUrl=" + su;
        if(vs1053_info) vs1053_info(su.c_str());
    }
    pos2=mline.indexOf("adw_ad=");              // advertising,
    if(pos2!=-1){
       ad=mline.substring(pos2);
       if(vs1053_info) vs1053_info(ad.c_str());
       pos2=mline.indexOf("durationMilliseconds=");
       if(pos2!=-1){
    	  pos2+=22;
    	  mline=mline.substring(pos2);
    	  mline=mline.substring(0, mline.indexOf("'")-3); // extract duration in sec
    	  if(vs1053_commercial) vs1053_commercial(mline.c_str());
       }
    }
    if(!full){
        m_icystreamtitle="";                    // Unknown type
        return;                                 // Do not show
    }
    if(pos1 == -1 && pos4 == -1){
        // Info probably from playlist
        st = mline;
        if(vs1053_showstreamtitle) vs1053_showstreamtitle(st.c_str());
        st = "Streamtitle: " + st;
        if(vs1053_info) vs1053_info(st.c_str());
    }
}
//---------------------------------------------------------------------------------------------------------------------
void VS1053::handlebyte(uint8_t b){
    String lcml;                                                // Lower case metaline
    static String ct;                                           // Contents type
    static String host;
 
    if(m_datamode == VS1053_HEADER)                             // Handle next byte of MP3 header
    {


        if((b > 0x7F) ||                                        // Ignore unprintable characters
                (b == '\r') ||                                  // Ignore CR
                (b == '\0'))                                    // Ignore NULL
                {
            // Yes, ignore
        }
        else if(b == '\n'){                                     // Linefeed ?
            m_LFcount++;                                        // Count linefeeds

            if(chkhdrline(m_metaline.c_str())){                 // Reasonable input?
                lcml=m_metaline;                                // Use lower case for compare
                lcml.toLowerCase();
                lcml.trim();
                if(lcml.indexOf("content-type:") >= 0){         // Line with "Content-Type: xxxx/yyy"
                    if(lcml.indexOf("audio") >= 0){             // Is ct audio?
                        m_f_ctseen=true;                        // Yes, remember seeing this
                        ct=m_metaline.substring(13);            // Set contentstype. Not used yet
                        ct.trim();
                        sprintf(chbuf, "%s seen.", ct.c_str());
                        if(vs1053_info) vs1053_info(chbuf);
                    }
                    if(lcml.indexOf("ogg") >= 0){               // Is ct ogg?
                        m_f_ctseen=true;                        // Yes, remember seeing this
                        ct=m_metaline.substring(13);
                        ct.trim();
                        sprintf(chbuf, "%s seen.", ct.c_str());
                        if(vs1053_info) vs1053_info(chbuf);
                        m_metaint=0;                            // ogg has no metadata
                        m_bitrate=0;
                        m_icyname=="";
                        m_f_ogg=true;
                    }
                }
                else if(lcml.startsWith("location:")){
                    host=m_metaline.substring(lcml.indexOf("http"),lcml.length());// use metaline instead lcml
                    if(host.indexOf("&")>0)host=host.substring(0,host.indexOf("&")); // remove parameter
                    sprintf(chbuf, "redirect to new host %s", host.c_str());
                    if(vs1053_info) vs1053_info(chbuf);
                    connecttohost(host);
                }
                else if(lcml.startsWith("icy-br:")){
                    m_bitrate=m_metaline.substring(7).toInt();  // Found bitrate tag, read the bitrate
                    sprintf(chbuf,"%d", m_bitrate);
                    if(vs1053_bitrate) vs1053_bitrate(chbuf);
                }
                else if(lcml.startsWith("icy-metaint:")){
                    m_metaint=m_metaline.substring(12).toInt(); // Found metaint tag, read the value
                    //log_i("m_metaint=%i",m_metaint);
                    if(m_metaint==0) m_metaint=16000;           // if no set to default
                }
                else if(lcml.startsWith("icy-name:")){
                    m_icyname=m_metaline.substring(9);          // Get station name
                    m_icyname.trim();                           // Remove leading and trailing spaces
                    sprintf(chbuf, "icy-name=%s", m_icyname.c_str());
                    if(vs1053_info) vs1053_info(chbuf);
                    if(m_icyname!=""){
                        if(vs1053_showstation) vs1053_showstation(m_icyname.c_str());
                    }
//                    for(int z=0; z<m_icyname.length();z++) log_e("%i",m_icyname[z]);
                }
                else if(lcml.startsWith("transfer-encoding:")){
                    // Station provides chunked transfer
                    if(m_metaline.endsWith("chunked")){
                        m_f_chunked=true;
                        if(vs1053_info) vs1053_info("chunked data transfer");
                        m_chunkcount=0;                         // Expect chunkcount in DATA
                    }
                }
                else if(lcml.startsWith("icy-url:")){
                    m_icyurl=m_metaline.substring(8);             // Get the URL
                    m_icyurl.trim();
                    if(vs1053_icyurl) vs1053_icyurl(m_icyurl.c_str());
                }
                else{
                    // all other
                    sprintf(chbuf, "%s", m_metaline.c_str());
                    if(vs1053_info) vs1053_info(chbuf);
                }
            }
            m_metaline="";                                      // Reset this line
            if((m_LFcount == 2) && m_f_ctseen){                   // Some data seen and a double LF?
                if(m_icyname==""){if(vs1053_showstation) vs1053_showstation("");} // no icyname available
                if(m_bitrate==0){if(vs1053_bitrate) vs1053_bitrate("");} // no bitrate received
                if(m_f_ogg==true){
                    m_datamode=VS1053_OGG;                      // Overwrite m_datamode
                    sprintf(chbuf, "Switch to OGG, bitrate is %d, metaint is %d", m_bitrate, m_metaint); // Show bitrate and metaint
                    if(vs1053_info) vs1053_info(chbuf);
                    String lasthost=m_lastHost;
                    uint idx=lasthost.indexOf('?');
                    if(idx>0) lasthost=lasthost.substring(0, idx);
                    if(vs1053_lasthost) vs1053_lasthost(lasthost.c_str());
                    m_f_ogg=false;
                }
                else{
                    m_datamode=VS1053_DATA;                         // Expecting data now
                    sprintf(chbuf, "Switch to DATA, bitrate is %d, metaint is %d", m_bitrate, m_metaint); // Show bitrate and metaint
                    if(vs1053_info) vs1053_info(chbuf);
                    String lasthost=m_lastHost;
                    uint idx=lasthost.indexOf('?');
                    if(idx>0) lasthost=lasthost.substring(0, idx);
                    if(vs1053_lasthost) vs1053_lasthost(lasthost.c_str());
                }
                startSong();                                    // Start a new song
                delay(1000);
            }
        }
        else
        {
            m_metaline+=(char)b;                                // Normal character, put new char in metaline
            m_LFcount=0;                                        // Reset double CRLF detection
        }
        return;
    }
    if(m_datamode == VS1053_METADATA)                           // Handle next byte of metadata
    {
        if(m_firstmetabyte)                                     // First byte of metadata?
        {
            m_firstmetabyte=false;                              // Not the first anymore
            m_metacount=b * 16 + 1;                             // New count for metadata including length byte
            if(m_metacount > 1){
                sprintf(chbuf, "Metadata block %d bytes",        // Most of the time there are zero bytes of metadata
                        m_metacount-1);
                if(vs1053_info) vs1053_info(chbuf);
           }
            m_metaline="";                                      // Set to empty
        }
        else
        {
            m_metaline+=(char)b;                                // Normal character, put new char in metaline
        }
        if(--m_metacount == 0){
            if(m_metaline.length()){                            // Any info present?

                // metaline contains artist and song name.  For example:
                // "StreamTitle='Don McLean - American Pie';StreamUrl='';"
                // Sometimes it is just other info like:
                // "StreamTitle='60s 03 05 Magic60s';StreamUrl='';"
                // Isolate the StreamTitle, remove leading and trailing quotes if present.
                //log_i("ST %s", m_metaline.c_str());
            	if( !m_f_localfile) showstreamtitle(m_metaline.c_str(), true);         // Show artist and title if present in metadata
            }
            if(m_metaline.length() > 1500){                     // Unlikely metaline length?
                if(vs1053_info) vs1053_info("Metadata block to long! Skipping all Metadata from now on.");
                m_metaint=16000;                                // Probably no metadata
                m_metaline="";                                  // Do not waste memory on this
            }
            m_datamode=VS1053_DATA;                             // Expecting data
        }
    }
    return;
}
//---------------------------------------------------------------------------------------------------------------------
size_t VS1053::ringused(){
    return (InBuff.bufferFilled());                                      // Free space available
}
//---------------------------------------------------------------------------------------------------------------------
void VS1053::loop(){
    // - localfile - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_f_localfile) {                                      // Playing file fron SPIFFS or SD?
        processLocalFile();
    }
    // - webstream - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_f_webstream) {                                      // Playing file from URL?
        //if(!m_f_running) return;
        if(m_datamode == VS1053_PLAYLISTINIT || m_datamode == VS1053_PLAYLISTHEADER || m_datamode == VS1053_PLAYLISTDATA){
            processPlayListData();
            return;
        }
        if(m_datamode == VS1053_HEADER){
            processAudioHeaderData();
            return;
        }
        if(m_datamode == VS1053_DATA){
            processWebStream();
            return;
        }
    }
    return;
}
//---------------------------------------------------------------------------------------------------------------------
void VS1053::processLocalFile() {

    if(!(audiofile && m_f_running && m_f_localfile)) return;

    int bytesDecoded = 0;
    uint32_t bytesCanBeWritten = 0;
    uint32_t bytesCanBeRead = 0;
    int32_t bytesAddedToBuffer = 0;
    static bool f_stream;

    if(m_f_firstCall) {  // runs only one time per connection, prepare for start
        m_f_firstCall = false;
        f_stream = false;
        return;
    }
    bytesCanBeWritten = InBuff.writeSpace();
    //----------------------------------------------------------------------------------------------------
    // some files contain further data after the audio block (e.g. pictures).
    // In that case, the end of the audio block is not the end of the file. An 'eof' has to be forced.
    if((m_controlCounter == 100) && (m_contentlength > 0)) { // fileheader was read
           if(bytesCanBeWritten + getFilePos() >= m_contentlength){
               if(m_contentlength > getFilePos()) bytesCanBeWritten = m_contentlength - getFilePos();
               else bytesCanBeWritten = 0;
           }
    }
    //----------------------------------------------------------------------------------------------------

    bytesAddedToBuffer = audiofile.read(InBuff.getWritePtr(), bytesCanBeWritten);
    if(bytesAddedToBuffer > 0) {
        InBuff.bytesWritten(bytesAddedToBuffer);
    }

//    if(psramFound() && bytesAddedToBuffer >4096)
//        vTaskDelay(2);// PSRAM has a bottleneck in the queue, so wait a little bit

    if(bytesAddedToBuffer == -1) bytesAddedToBuffer = 0; // read error? eof?
    bytesCanBeRead = InBuff.bufferFilled();
    if(bytesCanBeRead > InBuff.getMaxBlockSize()) bytesCanBeRead = InBuff.getMaxBlockSize();
    if(bytesCanBeRead == InBuff.getMaxBlockSize()) { // mp3 or aac frame complete?
        if(!f_stream) {
            f_stream = true;
            if(vs1053_info) vs1053_info("stream ready");
        }
        if(m_controlCounter != 100){
            // if(m_codec == CODEC_WAV){
            //     int res = read_WAV_Header(InBuff.getReadPtr(), bytesCanBeRead);
            //     if(res >= 0) bytesDecoded = res;
            //     else{ // error, skip header
            //         m_controlCounter = 100;
            //     }
            // }
            if(m_codec == CODEC_MP3){
                int res = read_MP3_Header(InBuff.getReadPtr(), bytesCanBeRead);
                if(res >= 0) bytesDecoded = res;
                else{ // error, skip header
                    m_controlCounter = 100;
                }
            }
            // if(m_codec == CODEC_M4A){
            //     int res = read_M4A_Header(InBuff.getReadPtr(), bytesCanBeRead);
            //     if(res >= 0) bytesDecoded = res;
            //     else{ // error, skip header
            //         m_controlCounter = 100;
            //     }
            // }
            if(m_codec == CODEC_AAC){
                // stream only, no header
                m_audioDataSize = getFileSize();
                m_controlCounter = 100;
            }

            // if(m_codec == CODEC_FLAC){
            //     int res = read_FLAC_Header(InBuff.getReadPtr(), bytesCanBeRead);
            //     if(res >= 0) bytesDecoded = res;
            //     else{ // error, skip header
            //         stopSong();
            //         m_controlCounter = 100;
            //     }
            // }
        }
        else {
            bytesDecoded = sendBytes(InBuff.getReadPtr(), bytesCanBeRead);
        }
        if(bytesDecoded > 0) {InBuff.bytesWasRead(bytesDecoded); return;}
        return;
    }
    if(!bytesAddedToBuffer) {  // eof
        bytesCanBeRead = InBuff.bufferFilled();
        if(bytesCanBeRead > 200){
            if(bytesCanBeRead > InBuff.getMaxBlockSize()) bytesCanBeRead = InBuff.getMaxBlockSize();
            bytesDecoded = sendBytes(InBuff.getReadPtr(), bytesCanBeRead); // play last chunk(s)
            if(bytesDecoded > 0){
                InBuff.bytesWasRead(bytesDecoded);
                return;
            }
        }
        InBuff.resetBuffer();

        // if(m_f_loop  && f_stream){  //eof
        //     sprintf(chbuf, "loop from: %u to: %u", getFilePos(), m_audioDataStart);  //TEST loop
        //     if(audio_info) audio_info(chbuf);
        //     setFilePos(m_audioDataStart);
        //     if(m_codec == CODEC_FLAC) FLACDecoderReset();
        //     /*
        //         The current time of the loop mode is not reset,
        //         which will cause the total audio duration to be exceeded.
        //         For example: current time   ====progress bar====>  total audio duration
        //                         3:43        ====================>        3:33
        //     */
        //     m_audioCurrentTime = 0;
        //     return;
        // } //TEST loop

        f_stream = false;
        m_f_localfile = false;

        char *afn =strdup(audiofile.name()); // store temporary the name


        stopSong();
        sprintf(chbuf, "End of file \"%s\"", afn);
        if(vs1053_info) vs1053_info(chbuf);
        if(vs1053_eof_mp3) vs1053_eof_mp3(afn);
        if(afn) free(afn);
    }
}
//---------------------------------------------------------------------------------------------------------------------
void VS1053::processWebStream(){
    const uint16_t  maxFrameSize = InBuff.getMaxBlockSize();
    int32_t         availableBytes = 0;                         // available bytes in stream
    uint16_t        bcs  = 0;                                   // bytes can current send
    static bool     f_tmr_1s;   
    static bool     f_stream;                                   // first audio data received
    static int      bytesDecoded;   
    static uint32_t byteCounter;                                // count received data
    static uint32_t chunksize;                                  // chunkcount read from stream
    static uint32_t tmr_1s;                                     // timer 1 sec
    static uint32_t loopCnt;                                    // count loops if clientbuffer is empty
    static uint32_t metacount;                                  // counts down bytes between metadata


    // first call, set some values to default - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_f_firstCall) { // runs only ont time per connection, prepare for start
        m_f_firstCall = false;
        f_stream = false;
        byteCounter = 0;
        chunksize = 0;
        bytesDecoded = 0;
        loopCnt = 0;
        tmr_1s = millis();
        m_t0 = millis();
        metacount = m_metaint;
    }

    // timer, triggers every second - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if((tmr_1s + 1000) < millis()) {
        f_tmr_1s = true;                                        // flag will be set every second for one loop only
        tmr_1s = millis();
    }

    // if we have chunked data transfer: get the chunksize- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_f_chunked && !m_chunkcount) { // Expecting a new chunkcount?
        int b;
        if(!m_f_ssl) b = client.read();
        else         b = clientsecure.read();

        if(b < 1) return;
        if(b == '\r') return;
        if(b == '\n'){ m_chunkcount = chunksize;  chunksize = 0; return;}

        // We have received a hexadecimal character.  Decode it and add to the result.
        b = toupper(b) - '0';                       // Be sure we have uppercase
        if(b > 9) b = b - 7;                        // Translate A..F to 10..15
        chunksize = (chunksize << 4) + b;
        return;
    }

    // if we have metadata: get them - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(!metacount && !m_f_swm){
        int16_t b = 0;
        if(!m_f_ssl) b = client.read();
        else         b = clientsecure.read();
        if(b >= 0) {
            if(m_f_chunked) m_chunkcount--;
            if(readMetadata(b)) metacount = m_metaint;
        }
        return;
    }

    // now we can get the pure audio data - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_f_ssl == false) availableBytes = client.available();            // available from stream
    if(m_f_ssl == true)  availableBytes = clientsecure.available();      // available from stream

    // if the buffer can't filled for several seconds try a new connection  - - - - - - - - - - - - - - - - - - - - - -
    if(f_stream && !availableBytes){
        loopCnt++;
        if(loopCnt > 200000) {              // wait several seconds
            loopCnt = 0;
            if(vs1053_info) vs1053_info("Stream lost -> try new connection");
            connecttohost(m_lastHost);
        }
    }

    if(availableBytes) loopCnt = 0;

    // buffer fill routine  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(true) { // statement has no effect
        uint32_t bytesCanBeWritten = InBuff.writeSpace();
        if(!m_f_swm)    bytesCanBeWritten = min(metacount,  bytesCanBeWritten);
        if(m_f_chunked) bytesCanBeWritten = min(m_chunkcount, bytesCanBeWritten);

        int16_t bytesAddedToBuffer = 0;

        if(psramFound()) if(bytesCanBeWritten > 4096) bytesCanBeWritten = 4096; // PSRAM throttle

        if(m_f_webfile){
            // normally there is nothing to do here, if byteCounter == contentLength
            // then the file is completely read, but:
            // m4a files can have more data  (e.g. pictures ..) after the audio Block
            // therefore it is bad to read anything else (this can generate noise)
            if(byteCounter + bytesCanBeWritten >= m_contentlength) bytesCanBeWritten = m_contentlength - byteCounter;
        }

        if(m_f_ssl == false) bytesAddedToBuffer = client.read(InBuff.getWritePtr(), bytesCanBeWritten);
        else                 bytesAddedToBuffer = clientsecure.read(InBuff.getWritePtr(), bytesCanBeWritten);

        if(bytesAddedToBuffer > 0) {
            if(m_f_webfile)             byteCounter  += bytesAddedToBuffer;  // Pull request #42
            if(!m_f_swm)                metacount  -= bytesAddedToBuffer;
            if(m_f_chunked)             m_chunkcount -= bytesAddedToBuffer;
            InBuff.bytesWritten(bytesAddedToBuffer);
        }

        if(InBuff.bufferFilled() > maxFrameSize && !f_stream) {  // waiting for buffer filled
            f_stream = true;  // ready to play the audio data
            uint16_t filltime = millis() - m_t0;
            if(vs1053_info) vs1053_info("stream ready");
            sprintf(chbuf, "buffer filled in %d ms", filltime);
            if(vs1053_info) vs1053_info(chbuf);
        }
        if(!f_stream) return;
    }

    // // if we have a webfile, read the file header first - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_f_webfile && m_controlCounter != 100){
        if(InBuff.bufferFilled() < maxFrameSize) return;
         if(m_codec == CODEC_WAV){
            m_controlCounter = 100;
        }
        if(m_codec == CODEC_MP3){
            int res = read_MP3_Header(InBuff.getReadPtr(), InBuff.bufferFilled());
            if(res >= 0) bytesDecoded = res;
            else{m_controlCounter = 100;} // error, skip header
        }
        if(m_codec == CODEC_M4A){
            m_controlCounter = 100;
        }
        if(m_codec == CODEC_FLAC){
            m_controlCounter = 100;
        }
        InBuff.bytesWasRead(bytesDecoded);
        return;
    }

    // play audio data - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    if((InBuff.bufferFilled() >= maxFrameSize) && (f_stream == true)) { // fill > framesize?
        bytesDecoded = sendBytes(InBuff.getReadPtr(), maxFrameSize);
        InBuff.bytesWasRead(bytesDecoded);
    }

    // have we reached the end of the webfile?  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(f_stream == true) {
        if(m_f_webfile && (byteCounter >= m_contentlength - 10) && (InBuff.bufferFilled() < maxFrameSize)) {
            // it is stream from fileserver with known content-length? and
            // everything is received?  and
            // the buff is almost empty?, issue #66 then comes to an end
            stopSong(); // Correct close when play known length sound #74 and before callback #112
            sprintf(chbuf, "End of webstream: \"%s\"", m_lastHost);
            if(vs1053_info) vs1053_info(chbuf);
            if(vs1053_eof_stream) vs1053_eof_stream(m_lastHost);
        }
    }
}
//---------------------------------------------------------------------------------------------------------------------
void VS1053::processPlayListData() {

    int av = 0;
    if(!m_f_ssl) av = client.available();
    else         av = clientsecure.available();
    if(av < 1) return;

    char pl[256]; // playlistline
    uint8_t b = 0;
    int16_t pos = 0;

    static bool f_entry = false;                            // entryflag for asx playlist
    static bool f_title = false;                            // titleflag for asx playlist
    static bool f_ref   = false;                            // refflag   for asx playlist

    while(true){
        if(!m_f_ssl)  b = client.read();
        else          b = clientsecure.read();
        if(b == 0xff) b = '\n'; // no more to read? send new line
        if(b == '\n') {pl[pos] = 0; break;}
        if(b < 0x20 || b > 0x7E) continue;
        pl[pos] = b;
        pos++;
        if(pos == 255){pl[pos] = '\0'; log_e("headerline oberflow"); break;}
    }

    if(strlen(pl) == 0 && m_datamode == VS1053_PLAYLISTHEADER) {
        if(vs1053_info) vs1053_info("Switch to PLAYLISTDATA");
        m_datamode = VS1053_PLAYLISTDATA;                    // Expecting data now
        return;
    }


    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_datamode == VS1053_PLAYLISTINIT) {                  // Initialize for receive .m3u file
        // We are going to use metadata to read the lines from the .m3u file
        // Sometimes this will only contain a single line
        f_entry = false;
        f_title = false;
        f_ref   = false;
        m_datamode = VS1053_PLAYLISTHEADER;                  // Handle playlist data
        if(vs1053_info) vs1053_info("Read from playlist");
    } // end AUDIO_PLAYLISTINIT

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_datamode == VS1053_PLAYLISTHEADER) {                // Read header

        sprintf(chbuf, "Playlistheader: %s", pl);           // Show playlistheader
        if(vs1053_info) vs1053_info(chbuf);

        int pos = indexOf(pl, "404 Not Found", 0);
        if(pos >= 0) {
            m_datamode = VS1053_NONE;
            if(vs1053_info) vs1053_info("Error 404 Not Found");
            stopSong();
            return;
        }

        pos = indexOf(pl, "404 File Not Found", 0);
        if(pos >= 0) {
            m_datamode = VS1053_NONE;
            if(vs1053_info) vs1053_info("Error 404 File Not Found");
            stopSong();
            return;
        }

        pos = indexOf(pl, ":", 0);                          // lowercase all letters up to the colon
        if(pos >= 0) {
            for(int i=0; i<pos; i++) {
                pl[i] = toLowerCase(pl[i]);
            }
        }
        if(startsWith(pl, "location:")) {
            const char* host;
            pos = indexOf(pl, "http", 0);
            host = (pl + pos);
            sprintf(chbuf, "redirect to new host %s", host);
            if(vs1053_info) vs1053_info(chbuf);
            connecttohost(host);
        }
        return;
    } // end AUDIO_PLAYLISTHEADER

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_datamode == VS1053_PLAYLISTDATA) {                  // Read next byte of .m3u file data
        sprintf(chbuf, "Playlistdata: %s", pl);             // Show playlistdata
        if(vs1053_info) vs1053_info(chbuf);

        // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
        if(m_playlistFormat == FORMAT_M3U) {

            if(indexOf(pl, "#EXTINF:", 0) >= 0) {           // Info?
               pos = indexOf(pl, ",", 0);                   // Comma in this line?
               if(pos > 0) {
                   // Show artist and title if present in metadata
                   if(vs1053_info) vs1053_info(pl + pos + 1);
               }
               return;
           }
           if(startsWith(pl, "#")) {                        // Commentline?
               return;
           }

           pos = indexOf(pl, "http://:@", 0); // ":@"??  remove that!
           if(pos >= 0) {
               sprintf(chbuf, "Entry in playlist found: %s", (pl + pos + 9));
               connecttohost(pl + pos + 9);
               return;
           }
           sprintf(chbuf, "Entry in playlist found: %s", pl);
           if(vs1053_info) vs1053_info(chbuf);
           pos = indexOf(pl, "http", 0);                    // Search for "http"
           const char* host;
           if(pos >= 0) {                                   // Does URL contain "http://"?
               host = (pl + pos);
               connecttohost(host);
           }                                                // Yes, set new host
           return;
        } //m3u

        // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
        if(m_playlistFormat == FORMAT_PLS) {
            if(startsWith(pl, "File1")) {
                pos = indexOf(pl, "http", 0);                   // File1=http://streamplus30.leonex.de:14840/;
                if(pos >= 0) {                                  // yes, URL contains "http"?
                    memcpy(m_lastHost, pl + pos, strlen(pl) + 1);   // http://streamplus30.leonex.de:14840/;
                    // Now we have an URL for a stream in host.
                    f_ref = true;
                }
            }
            if(startsWith(pl, "Title1")) {                      // Title1=Antenne Tirol
                const char* plsStationName = (pl + 7);
                if(vs1053_showstation) vs1053_showstation(plsStationName);
                sprintf(chbuf, "StationName: \"%s\"", plsStationName);
                if(vs1053_info) vs1053_info(chbuf);
                f_title = true;
            }
            if(startsWith(pl, "Length1")) f_title = true;               // if no Title is available
            if((f_ref == true) && (strlen(pl) == 0)) f_title = true;

            if(f_ref && f_title) {                                      // we have both StationName and StationURL
                log_i("connect to new host %s", m_lastHost);
                connecttohost(m_lastHost);                              // Connect to it
            }
            return;
        } // pls

        // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
        if(m_playlistFormat == FORMAT_ASX) { // Advanced Stream Redirector
            if(indexOf(pl, "<entry>", 0) >= 0) f_entry = true;      // found entry tag (returns -1 if not found)
            if(f_entry) {
                if(indexOf(pl, "ref href", 0) > 0) {                // <ref href="http://87.98.217.63:24112/stream" />
                    pos = indexOf(pl, "http", 0);
                    if(pos > 0) {
                        char* plsURL = (pl + pos);                  // http://87.98.217.63:24112/stream" />
                        int pos1 = indexOf(plsURL, "\"", 0);        // http://87.98.217.63:24112/stream
                        if(pos1 > 0) {
                            plsURL[pos1] = 0;
                        }
                        memcpy(m_lastHost, plsURL, strlen(plsURL)); // save url in array
                        log_i("m_plsURL = %s",pl);
                        // Now we have an URL for a stream in host.
                        f_ref = true;
                    }
                }
                pos = indexOf(pl, "<title>", 0);
                if(pos < 0) pos = indexOf(pl, "<Title>", 0);
                if(pos >= 0) {
                    char* plsStationName = (pl + pos + 7);          // remove <Title>
                    pos = indexOf(plsStationName, "</", 0);
                    if(pos >= 0){
                            *(plsStationName +pos) = 0;             // remove </Title>
                    }
                    if(vs1053_showstation) vs1053_showstation(plsStationName);
                    sprintf(chbuf, "StationName: \"%s\"", plsStationName);
                    if(vs1053_info) vs1053_info(chbuf);
                    f_title = true;
                }
            } //entry
            if(f_ref && f_title) {   //we have both StationName and StationURL
                connecttohost(m_lastHost);                          // Connect to it
            }
        }  //asx
        return;
    } // end AUDIO_PLAYLISTDATA
}
//---------------------------------------------------------------------------------------------------------------------
void VS1053::processAudioHeaderData() {

    int av = 0;
    if(!m_f_ssl) av=client.available();
    else         av= clientsecure.available();
    if(av <= 0) return;

    char hl[256]; // headerline
    uint8_t b = 0;
    uint8_t pos = 0;
    int16_t idx = 0;

    while(true){
        if(!m_f_ssl) b = client.read();
        else         b = clientsecure.read();
        if(b == '\n') break;
        if(b == '\r') hl[pos] = 0;
        if(b < 0x20 || b > 0x7E) continue;
        hl[pos] = b;
        pos++;
        if(pos == 255){hl[pos] = '\0'; log_e("headerline oberflow"); break;}
    }

    if(!pos && m_f_ctseen){  // audio header complete?
        m_datamode = VS1053_DATA;                         // Expecting data now
        sprintf(chbuf, "Switch to DATA, metaint is %d", m_metaint);
        if(vs1053_info) vs1053_info(chbuf);
        memcpy(chbuf, m_lastHost, strlen(m_lastHost)+1);
        uint idx = indexOf(chbuf, "?", 0);
        if(idx > 0) chbuf[idx] = 0;
        if(vs1053_lasthost) vs1053_lasthost(chbuf);
        delay(50);  // #77
        return;
    }
    if(!pos){
        stopSong();
        log_e("can't see content in audioHeaderData");
        return;
    }

    idx = indexOf(hl, ":", 0); // lowercase all letters up to the colon
    if(idx >= 0) {
        for(int i=0; i< idx; i++) {
            hl[i] = toLowerCase(hl[i]);
        }
    }

    if(indexOf(hl, "content-type:", 0) >= 0) {
        if(parseContentType(hl)) m_f_ctseen = true;
    }
    else if(startsWith(hl, "location:")) {
        int pos = indexOf(hl, "http", 0);
        const char* c_host = (hl + pos);
        sprintf(chbuf, "redirect to new host \"%s\"", c_host);
        if(vs1053_info) vs1053_info(chbuf);
        connecttohost(c_host);
    }
    else if(startsWith(hl, "set-cookie:")    ||
            startsWith(hl, "pragma:")        ||
            startsWith(hl, "expires:")       ||
            startsWith(hl, "cache-control:") ||
            startsWith(hl, "icy-pub:")       ||
            startsWith(hl, "accept-ranges:") ){
        ; // do nothing
    }
    else if(startsWith(hl, "connection:")) {
        if(indexOf(hl, "close", 0) >= 0) {; /* do nothing */}
    }
    else if(startsWith(hl, "icy-genre:")) {
        ; // do nothing Ambient, Rock, etc
    }
    else if(startsWith(hl, "icy-br:")) {
        const char* c_bitRate = (hl + 7);
        int32_t br = atoi(c_bitRate); // Found bitrate tag, read the bitrate in Kbit
        m_bitrate = br;
        sprintf(chbuf, "%d", m_bitrate);
        if(vs1053_bitrate) vs1053_bitrate(chbuf);
    }
    else if(startsWith(hl, "icy-metaint:")) {
        const char* c_metaint = (hl + 12);
        int32_t i_metaint = atoi(c_metaint);
        m_metaint = i_metaint;
        if(m_metaint) m_f_swm = false     ;                            // Multimediastream
    }
    else if(startsWith(hl, "icy-name:")) {
        char* c_icyname = (hl + 9); // Get station name
        idx = 0;
        while(c_icyname[idx] == ' '){idx++;} c_icyname += idx;        // Remove leading spaces
        idx = strlen(c_icyname);
        while(c_icyname[idx] == ' '){idx--;} c_icyname[idx + 1] = 0;  // Remove trailing spaces

        if(strlen(c_icyname) > 0) {
            sprintf(chbuf, "icy-name: %s", c_icyname);
            if(vs1053_info) vs1053_info(chbuf);
            if(vs1053_showstation) vs1053_showstation(c_icyname);
        }
    }
    else if(startsWith(hl, "content-length:")) {
        const char* c_cl = (hl + 15);
        int32_t i_cl = atoi(c_cl);
        m_contentlength = i_cl;
        m_f_webfile = true; // Stream comes from a fileserver
        sprintf(chbuf, "content-length: %i", m_contentlength);
        if(vs1053_info) vs1053_info(chbuf);
    }
    else if((startsWith(hl, "transfer-encoding:"))){
        if(endsWith(hl, "chunked") || endsWith(hl, "Chunked") ) {     // Station provides chunked transfer
            m_f_chunked = true;
            if(vs1053_info) vs1053_info("chunked data transfer");
            m_chunkcount = 0;                                         // Expect chunkcount in DATA
        }
    }
    else if(startsWith(hl, "icy-url:")) {
        const char* icyurl = (hl + 8);
        idx = 0;
        while(icyurl[idx] == ' ') {idx ++;} icyurl += idx;            // remove leading blanks
        sprintf(chbuf, "icy-url: %s", icyurl);
        if(vs1053_info) vs1053_info(chbuf);
        if(vs1053_icyurl) vs1053_icyurl(icyurl);
    }
    else if(startsWith(hl, "www-authenticate:")) {
        if(vs1053_info) vs1053_info("authentification failed, wrong credentials?");
        m_f_running = false;
        stopSong();
    }
    else {
        if(isascii(hl[0]) && hl[0] >= 0x20) {  // all other
            sprintf(chbuf, "%s", hl);
            if(vs1053_info) vs1053_info(chbuf);
        }
    }
    return;
}
//---------------------------------------------------------------------------------------------------------------------
bool VS1053::readMetadata(uint8_t b) {

    static uint16_t pos_ml = 0;                          // determines the current position in metaline
    static uint16_t metalen = 0;
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(!metalen) {                                       // First byte of metadata?
        metalen = b * 16 + 1;                            // New count for metadata including length byte
        if(metalen >512){
            if(vs1053_info) vs1053_info("Metadata block to long! Skipping all Metadata from now on.");
            m_f_swm = true;                              // expect stream without metadata
        }
        pos_ml = 0; chbuf[pos_ml] = 0;                   // Prepare for new line
    }
    else {
        chbuf[pos_ml] = (char) b;                        // Put new char in metaline
        if(pos_ml < 510) pos_ml ++;
        chbuf[pos_ml] = 0;
        if(pos_ml == 509) log_i("metaline overflow in AUDIO_METADATA! metaline=%s", chbuf) ;
        if(pos_ml == 510) { ; /* last current char in b */}

    }
    if(--metalen == 0) {
        if(strlen(chbuf)) {                             // Any info present?
            // metaline contains artist and song name.  For example:
            // "StreamTitle='Don McLean - American Pie';StreamUrl='';"
            // Sometimes it is just other info like:
            // "StreamTitle='60s 03 05 Magic60s';StreamUrl='';"
            // Isolate the StreamTitle, remove leading and trailing quotes if present.
            // log_i("ST %s", metaline);

            int pos = indexOf(chbuf, "song_spot", 0);    // remove some irrelevant infos
            if(pos > 3) {                                // e.g. song_spot="T" MediaBaseId="0" itunesTrackId="0"
                chbuf[pos] = 0;
            }
            if(!m_f_localfile) showstreamtitle(chbuf, true);   // Show artist and title if present in metadata
        }
        return true ;
    }
    return false;// end_METADATA
}
//---------------------------------------------------------------------------------------------------------------------
bool VS1053::parseContentType(const char* ct) {
    bool ct_seen = false;
    if(indexOf(ct, "audio", 0) >= 0) {        // Is ct audio?
        ct_seen = true;                       // Yes, remember seeing this
        if(indexOf(ct, "mpeg", 13) >= 0) {
            m_codec = CODEC_MP3;
            sprintf(chbuf, "%s, format is mp3", ct);
            if(vs1053_info) vs1053_info(chbuf); //ok is likely mp3
        }
        else if(indexOf(ct, "mp3", 13) >= 0) {
            m_codec = CODEC_MP3;
            sprintf(chbuf, "%s, format is mp3", ct);
            if(vs1053_info) vs1053_info(chbuf);
        }
        else if(indexOf(ct, "aac", 13) >= 0) {
            m_codec = CODEC_AAC;
            sprintf(chbuf, "%s, format is aac", ct);
            if(vs1053_info) vs1053_info(chbuf);
        }
        else if(indexOf(ct, "mp4", 13) >= 0) {      // audio/mp4a, audio/mp4a-latm
            m_codec = CODEC_M4A;
            sprintf(chbuf, "%s, format is aac", ct);
            if(vs1053_info) vs1053_info(chbuf);
        }
        else if(indexOf(ct, "m4a", 13) >= 0) {      // audio/x-m4a
            m_codec = CODEC_M4A;
            sprintf(chbuf, "%s, format is aac", ct);
            if(vs1053_info) vs1053_info(chbuf);
        }
        else if(indexOf(ct, "wav", 13) >= 0) {      // audio/x-wav
            m_codec = CODEC_WAV;
            sprintf(chbuf, "%s, format is wav", ct);
            if(vs1053_info) vs1053_info(chbuf);
        }
        else if(indexOf(ct, "ogg", 13) >= 0) {
            m_codec = CODEC_OGG;
            sprintf(chbuf, "ContentType %s found", ct);
            if(vs1053_info) vs1053_info(chbuf);
        }
        else if(indexOf(ct, "flac", 13) >= 0) {     // audio/flac, audio/x-flac
            m_codec = CODEC_FLAC;
            sprintf(chbuf, "%s, format is flac", ct);
            if(vs1053_info) vs1053_info(chbuf);
        }
        else {
            m_f_running = false;
            sprintf(chbuf, "%s, unsupported audio format", ct);
            if(vs1053_info) vs1053_info(chbuf);
        }
    }
    if(indexOf(ct, "application", 0) >= 0) {  // Is ct application?
        ct_seen = true;                       // Yes, remember seeing this
        uint8_t pos = indexOf(ct, "application", 0);
        if(indexOf(ct, "ogg", 13) >= 0) {
            m_codec = CODEC_OGG;
            sprintf(chbuf, "ContentType %s found", ct + pos);
            if(vs1053_info) vs1053_info(chbuf);
        }
    }
    return ct_seen;
}
//---------------------------------------------------------------------------------------------------------------------
void VS1053::stop_mp3client(){
    int v=read_register(SCI_VOL);
    audiofile.close();
    m_f_localfile=false;
    m_f_webstream=false;
    write_register(SCI_VOL, 0);                         // Mute while stopping

    client.flush();                                     // Flush stream client
    client.stop();                                      // Stop stream client
    write_register(SCI_VOL, v);
}
//---------------------------------------------------------------------------------------------------------------------
void VS1053::setDefaults(){
    // initializationsequence
    stopSong();
    initInBuff();                                           // initialize InputBuffer if not already done
    InBuff.resetBuffer();
    m_f_ctseen=false;                                       // Contents type not seen yet
    m_metaint=0;                                            // No metaint yet
    m_LFcount=0;                                            // For detection end of header
    m_bitrate=0;                                            // Bitrate still unknown
    m_f_firstCall = true;                                   // InitSequence for processWebstream and processLokalFile
    m_metaline="";                                          // No metadata yet
    m_icyname="";                                           // No StationName yet
    m_st_remember="";                                       // Delete the last streamtitle
    m_bitrate=0;                                            // No bitrate yet
    m_f_firstchunk=true;                                    // First chunk expected
    m_f_chunked=false;                                      // Assume not chunked
    m_f_ssl=false;
    m_f_swm = true;
    m_f_webfile = false;
    m_f_webstream = false;
    m_f_localfile = false;
}
//---------------------------------------------------------------------------------------------------------------------
bool VS1053::connecttohost(String host){
    return connecttohost(host.c_str());
}
//---------------------------------------------------------------------------------------------------------------------
bool VS1053::connecttohost(const char* host, const char* user, const char* pwd) {
    // user and pwd for authentification only, can be empty

    if(strlen(host) == 0) {
        if(vs1053_info) vs1053_info("Hostaddress is empty");
        return false;
    }
    setDefaults();

    sprintf(chbuf, "Connect to new host: \"%s\"", host);
    if(vs1053_info) vs1053_info(chbuf);

    // authentification
    String toEncode = String(user) + ":" + String(pwd);
    String authorization = base64::encode(toEncode);

    // initializationsequence
    int16_t pos_colon;                                        // Position of ":" in hostname
    int16_t pos_ampersand;                                    // Position of "&" in hostname
    uint16_t port = 80;                                       // Port number for host
    String extension = "/";                                   // May be like "/mp3" in "skonto.ls.lv:8002/mp3"
    String hostwoext = "";                                    // Host without extension and portnumber
    String headerdata = "";
    m_f_webstream = true;
    setDatamode(VS1053_HEADER);                               // Handle header

    if(startsWith(host, "http://")) {
        host = host + 7;
        m_f_ssl = false;
    }

    if(startsWith(host, "https://")) {
        host = host +8;
        m_f_ssl = true;
        port = 443;
    }

    String s_host = host;
    s_host.trim();

    // Is it a playlist?
    if(s_host.endsWith(".m3u")) {m_playlistFormat = FORMAT_M3U; m_datamode = VS1053_PLAYLISTINIT;}
    if(s_host.endsWith(".pls")) {m_playlistFormat = FORMAT_PLS; m_datamode = VS1053_PLAYLISTINIT;}
    if(s_host.endsWith(".asx")) {m_playlistFormat = FORMAT_ASX; m_datamode = VS1053_PLAYLISTINIT;}

    // In the URL there may be an extension, like noisefm.ru:8000/play.m3u&t=.m3u
    pos_colon = s_host.indexOf("/");                                  // Search for begin of extension
    if(pos_colon > 0) {                                               // Is there an extension?
        extension = s_host.substring(pos_colon);                      // Yes, change the default
        hostwoext = s_host.substring(0, pos_colon);                   // Host without extension
    }
    // In the URL there may be a portnumber
    pos_colon = s_host.indexOf(":");                                  // Search for separator
    pos_ampersand = s_host.indexOf("&");                              // Search for additional extensions
    if(pos_colon >= 0) {                                              // Portnumber available?
        if((pos_ampersand == -1) or (pos_ampersand > pos_colon)) {    // Portnumber is valid if ':' comes before '&' #82
            port = s_host.substring(pos_colon + 1).toInt();           // Get portnumber as integer
            hostwoext = s_host.substring(0, pos_colon);               // Host without portnumber
        }
    }
    sprintf(chbuf, "Connect to \"%s\" on port %d, extension \"%s\"", hostwoext.c_str(), port, extension.c_str());
    if(vs1053_info) vs1053_info(chbuf);

    extension.replace(" ", "%20");

    String resp = String("GET ") + extension + String(" HTTP/1.1\r\n")
                + String("Host: ") + hostwoext + String("\r\n")
                + String("Icy-MetaData:1\r\n")
                + String("Authorization: Basic " + authorization + "\r\n")
                + String("Connection: close\r\n\r\n");

    const uint32_t TIMEOUT_MS{250};
    if(m_f_ssl == false) {
        uint32_t t = millis();
        if(client.connect(hostwoext.c_str(), port, TIMEOUT_MS)) {
            client.setNoDelay(true);
            client.print(resp);
            uint32_t dt = millis() - t;
            sprintf(chbuf, "Connected to server in %u ms", dt);
            if(vs1053_info) vs1053_info(chbuf);

            memcpy(m_lastHost, s_host.c_str(), s_host.length()+1);               // Remember the current s_host
            m_f_running = true;
            return true;
        }
    }

    const uint32_t TIMEOUT_MS_SSL{2700};
    if(m_f_ssl == true) {
        uint32_t t = millis();
        if(clientsecure.connect(hostwoext.c_str(), port, TIMEOUT_MS_SSL)) {
            clientsecure.setNoDelay(true);
            // if(audio_info) audio_info("SSL/TLS Connected to server");
            clientsecure.print(resp);
            uint32_t dt = millis() - t;
            sprintf(chbuf, "SSL has been established in %u ms, free Heap: %u bytes", dt, ESP.getFreeHeap());
            if(vs1053_info) vs1053_info(chbuf);
            memcpy(m_lastHost, s_host.c_str(), s_host.length()+1);               // Remember the current s_host
            m_f_running = true;
            return true;
        }
    }
    sprintf(chbuf, "Request %s failed!", s_host.c_str());
    if(vs1053_info) vs1053_info(chbuf);
    if(vs1053_showstation) vs1053_showstation("");
    if(vs1053_showstreamtitle) vs1053_showstreamtitle("");
    m_lastHost[0] = 0;
    return false;
}
//---------------------------------------------------------------------------------------------------------------------
void VS1053::loadUserCode(void) {
  int i = 0;

  while (i<sizeof(flac_plugin)/sizeof(flac_plugin[0])) {
    unsigned short addr, n, val;
    addr = flac_plugin[i++];
    n = flac_plugin[i++];
    if (n & 0x8000U) { /* RLE run, replicate n samples */
      n &= 0x7FFF;
      val = flac_plugin[i++];
      while (n--) {
        write_register(addr, val);
      }
    } else {           /* Copy run, copy n samples */
      while (n--) {
        val = flac_plugin[i++];
        write_register(addr, val);
      }
    }
  }
}
//---------------------------------------------------------------------------------------------------------------------
void VS1053::UTF8toASCII(char* str){

    const uint8_t ascii[60] = {
    //129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148  // UTF8(C3)
    //                Ä    Å    Æ    Ç         É                                       Ñ                  // CHAR
      000, 000, 000, 142, 143, 146, 128, 000, 144, 000, 000, 000, 000, 000, 000, 000, 165, 000, 000, 000, // ASCII
    //149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168
    //      Ö                             Ü              ß    à                   ä    å    æ         è
      000, 153, 000, 000, 000, 000, 000, 154, 000, 000, 225, 133, 000, 000, 000, 132, 134, 145, 000, 138,
    //169, 170, 171, 172. 173. 174. 175, 176, 177, 179, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188
    //      ê    ë    ì         î    ï         ñ    ò         ô         ö              ù         û    ü
      000, 136, 137, 141, 000, 140, 139, 000, 164, 149, 000, 147, 000, 148, 000, 000, 151, 000, 150, 129};

    uint16_t i = 0, j=0, s = 0;
    bool f_C3_seen = false;

    while(str[i] != 0) {                                     // convert UTF8 to ASCII
        if(str[i] == 195){                                   // C3
            i++;
            f_C3_seen = true;
            continue;
        }
        str[j] = str[i];
        if(str[j] > 128 && str[j] < 189 && f_C3_seen == true) {
            s = ascii[str[j] - 129];
            if(s != 0) str[j] = s;                         // found a related ASCII sign
            f_C3_seen = false;
        }
        i++; j++;
    }
    str[j] = 0;
}
//---------------------------------------------------------------------------------------------------------------------
bool VS1053::connecttoSD(String sdfile){
    return connecttoFS(SD, sdfile.c_str());
}

bool VS1053::connecttoSD(const char* sdfile){
    return connecttoFS(SD, sdfile);
}

bool VS1053::connecttoFS(fs::FS &fs, const char* path) {

    if(strlen(path)>255) return false;

    char audioName[256];

    setDefaults(); // free buffers an set defaults

    memcpy(audioName, path, strlen(path)+1);
    if(audioName[0] != '/'){
        for(int i = 255; i > 0; i--){
            audioName[i] = audioName[i-1];
        }
        audioName[0] = '/';
    }
    if(endsWith(audioName, "\n")) audioName[strlen(audioName) -1] = 0;

    sprintf(chbuf, "Reading file: \"%s\"", audioName);
    if(vs1053_info) {vTaskDelay(2); vs1053_info(chbuf);}

    if(fs.exists(audioName)) {
        audiofile = fs.open(audioName);
    } else {
        UTF8toASCII(audioName);
        if(fs.exists(audioName)) {
            audiofile = fs.open(audioName);
        }
    }

    if(!audiofile) {
        if(vs1053_info) {vTaskDelay(2); vs1053_info("Failed to open file for reading");}
        return false;
    }

    m_f_localfile = true;
    m_file_size = audiofile.size();//TEST loop

    String afn = (String) audiofile.name();                   // audioFileName

    afn.toLowerCase();
    if(afn.endsWith(".mp3")) {      // MP3 section
        m_codec = CODEC_MP3;
        m_f_running = true;
        return true;
    } // end MP3 section

    // if(afn.endsWith(".m4a")) {      // M4A section, iTunes
    //     m_codec = CODEC_M4A;
    //     m_f_running = true;
    //     return true;
    // } // end M4A section

    if(afn.endsWith(".aac")) {      // AAC section, without FileHeader
        m_codec = CODEC_AAC;
        m_f_running = true;
        return true;
    } // end AAC section

    // if(afn.endsWith(".wav")) {      // WAVE section
    //     m_codec = CODEC_WAV;
    //     m_f_running = false;
    //     return true;
    // } // end WAVE section

    // if(afn.endsWith(".flac")) {     // FLAC section
    //     m_codec = CODEC_FLAC;
    //     m_f_running = false;
    //      return true;
    // } // end FLAC section

    sprintf(chbuf, "The %s format is not supported", afn.c_str() + afn.lastIndexOf(".") + 1);
    if(vs1053_info) vs1053_info(chbuf);
    audiofile.close();
    return false;
}
//---------------------------------------------------------------------------------------------------------------------
bool VS1053::connecttospeech(String speech, String lang){
    String host="translate.google.com.vn";
    String path="/translate_tts";
    m_f_localfile=false;
    m_f_webstream=false;
    m_f_ssl=true;

    stopSong();
    stop_mp3client();                           // Disconnect if still connected
    clientsecure.stop(); clientsecure.flush();  // release memory if allocated

    String tts=   path + "?ie=UTF-8&q=" + urlencode(speech) +
                  "&tl=" + lang + "&client=tw-ob";

    String resp = String("GET ") + tts + String(" HTTP/1.1\r\n") +
                  String("Host: ") + host + String("\r\n") +
                  String("User-Agent: GoogleTTS for ESP32/1.0.0\r\n") +
                  String("Accept-Encoding: identity\r\n") +
                  String("Accept: text/html\r\n") +
                  String("Connection: close\r\n\r\n");

    if (!clientsecure.connect(host.c_str(), 443)) {
        Serial.println("Connection failed");
        return false;
    }
    clientsecure.print(resp);

    while (clientsecure.connected()) {  // read the header
        String line = clientsecure.readStringUntil('\n');
        line+="\n";
        if (line == "\r\n") break;
    }

    uint8_t mp3buff[32];
    startSong();
    while(clientsecure.available() > 0) {
        uint8_t bytesread = clientsecure.readBytes(mp3buff, 32);
        sdi_send_buffer(mp3buff, bytesread);
    }
    clientsecure.stop();  clientsecure.flush();
    if(vs1053_eof_speech) vs1053_eof_speech(speech.c_str());
    return true;
}
//---------------------------------------------------------------------------------------------------------------------
void VS1053::unicode2utf8(char* buff, uint32_t len){
    // converts unicode in UTF-8, buff contains the string to be converted up to len
    // range U+1 ... U+FFFF
    uint8_t* tmpbuff = (uint8_t*)malloc(len * 2);
    if(!tmpbuff) {log_e("out of memory"); return;}
    bool bitorder = false;
    uint16_t j = 0;
    uint16_t k = 0;
    uint16_t m = 0;
    uint8_t uni_h = 0;
    uint8_t uni_l = 0;

    while(m < len - 1) {
        if((buff[m] == 0xFE) && (buff[m + 1] == 0xFF)) {
            bitorder = true;
            j = m + 2;
        }  // LSB/MSB
        if((buff[m] == 0xFF) && (buff[m + 1] == 0xFE)) {
            bitorder = false;
            j = m + 2;
        }  // MSB/LSB
        m++;
    } // seek for last bitorder
    m = 0;
    if(j > 0) {
        for(k = j; k < len; k += 2) {
            if(bitorder == true) {
                uni_h = (uint8_t)buff[k];
                uni_l = (uint8_t)buff[k + 1];
            }
            else {
                uni_l = (uint8_t)buff[k];
                uni_h = (uint8_t)buff[k + 1];
            }

            uint16_t uni_hl = ((uni_h << 8) | uni_l);

            if (uni_hl < 0X80){
                tmpbuff[m] = uni_l;
                m++;
            }
            else if (uni_hl < 0X800) {
                tmpbuff[m]= ((uni_hl >> 6) | 0XC0);
                m++;
                tmpbuff[m] =((uni_hl & 0X3F) | 0X80);
                m++;
            }
            else {
                tmpbuff[m] = ((uni_hl >> 12) | 0XE0);
                m++;
                tmpbuff[m] = (((uni_hl >> 6) & 0X3F) | 0X80);
                m++;
                tmpbuff[m] = ((uni_hl & 0X3F) | 0X80);
                m++;
            }
        }
    }
    buff[m] = 0;
    memcpy(buff, tmpbuff, m);
    free(tmpbuff);
}
//---------------------------------------------------------------------------------------------------------------------
int VS1053::read_MP3_Header(uint8_t *data, size_t len) {

    static size_t headerSize;
    static size_t id3Size;
    static uint8_t ID3version;
    static int ehsz = 0;
    static String tag = "";
    static char frameid[5];
    static size_t framesize = 0;
    static bool compressed = false;
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == 0){      /* read ID3 tag and ID3 header size */
        if(m_f_localfile){
            m_contentlength = getFileSize();
            ID3version = 0;
            sprintf(chbuf, "Content-Length: %u", m_contentlength);
            if(vs1053_info) vs1053_info(chbuf);
        }
        m_controlCounter ++;
        headerSize = 0;
        ehsz = 0;
        if(specialIndexOf(data, "ID3", 4) != 0) { // ID3 not found
            if(vs1053_info) vs1053_info("file has no mp3 tag, skip metadata");
            m_audioDataSize = m_contentlength;
            sprintf(chbuf, "Audio-Length: %u", m_audioDataSize);
            if(vs1053_info) vs1053_info(chbuf);
            return -1; // error, no ID3 signature found
        }
        ID3version = *(data + 3);
        switch(ID3version){
            case 2:
                m_f_unsync = (*(data + 5) & 0x80);
                m_f_exthdr = false;
                break;
            case 3:
            case 4:
                m_f_unsync = (*(data + 5) & 0x80); // bit7
                m_f_exthdr = (*(data + 5) & 0x40); // bit6 extended header
                break;
        };
        id3Size = bigEndian(data + 6, 4, 7); //  ID3v2 size  4 * %0xxxxxxx (shift left seven times!!)
        id3Size += 10;

        // Every read from now may be unsync'd
        sprintf(chbuf, "ID3 framesSize: %i", id3Size);
        if(vs1053_info) vs1053_info(chbuf);

        sprintf(chbuf, "ID3 version: 2.%i", ID3version);
        if(vs1053_info) vs1053_info(chbuf);

        if(ID3version == 2){
            m_controlCounter = 10;
        }
        headerSize = id3Size;
        headerSize -= 10;

        return 10;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == 1){      // compute extended header size if exists
        m_controlCounter ++;
        if(m_f_exthdr) {
            if(vs1053_info) vs1053_info("ID3 extended header");
            ehsz =  bigEndian(data, 4);
            headerSize -= 4;
            ehsz -= 4;
            return 4;
        }
        else{
            if(vs1053_info) vs1053_info("ID3 normal frames");
            return 0;
        }
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == 2){      // skip extended header if exists
        if(ehsz > 256) {
            ehsz -=256;
            headerSize -= 256;
            return 256;} // Throw it away
        else           {
            m_controlCounter ++;
            headerSize -= ehsz;
            return ehsz;} // Throw it away
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == 3){      // read a ID3 frame, get the tag
        if(headerSize == 0){
            m_controlCounter = 99;
            return 0;
        }
        m_controlCounter ++;
        frameid[0] = *(data + 0);
        frameid[1] = *(data + 1);
        frameid[2] = *(data + 2);
        frameid[3] = *(data + 3);
        frameid[4] = 0;
        tag = frameid;
        headerSize -= 4;
        if(frameid[0] == 0 && frameid[1] == 0 && frameid[2] == 0 && frameid[3] == 0) {
            // We're in padding
            m_controlCounter = 98;  // all ID3 metadata processed
        }
        return 4;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == 4){  // get the frame size
        m_controlCounter = 6;

        if(ID3version == 4){
            framesize = bigEndian(data, 4, 7); // << 7
        }
        else {
            framesize = bigEndian(data, 4);  // << 8
        }
        headerSize -= 4;
        uint8_t flag = *(data + 4); // skip 1st flag
        (void) flag;
        headerSize--;
        compressed = (*(data + 5)) & 0x80; // Frame is compressed using [#ZLIB zlib] with 4 bytes for 'decompressed
                                           // size' appended to the frame header.
        headerSize--;
        uint32_t decompsize = 0;
        if(compressed){
            log_i("iscompressed");
            decompsize = bigEndian(data + 6, 4);
            headerSize -= 4;
            (void) decompsize;
            log_i("decompsize=%u", decompsize);
            return 6 + 4;
        }
        return 6;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == 5){      // If the frame is larger than 256 bytes, skip the rest
        if(framesize > 256){
            framesize -= 256;
            headerSize -= 256;
            return 256;
        }
        else {
            m_controlCounter = 3; // check next frame
            headerSize -= framesize;
            return framesize;
        }
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == 6){      // Read the value
        m_controlCounter = 5;       // only read 256 bytes
        char value[256];
        char ch = *(data + 0);
        bool isUnicode = (ch==1) ? true : false;

        if(tag == "APIC") { // a image embedded in file, passing it to external function
            log_i("framesize=%i", framesize);
            isUnicode = false;
            if(m_f_localfile){
                size_t pos = id3Size - headerSize;
                if(vs1053_id3image) vs1053_id3image(audiofile, pos, framesize);
            }
            return 0;
        }

        size_t fs = framesize;
        if(fs >255) fs = 255;
        for(int i=0; i<fs; i++){
            value[i] = *(data + i);
        }
        framesize -= fs;
        headerSize -= fs;
        value[fs] = 0;
        if(isUnicode && fs > 1) {
            unicode2utf8(value, fs);   // convert unicode to utf-8 U+0020...U+07FF
        }
        if(!isUnicode){
            uint16_t j = 0, k = 0;
            j = 0;
            k = 0;
            while(j < fs) {
                if(value[j] == 0x0A) value[j] = 0x20; // replace LF by space
                if(value[j] > 0x1F) {
                    value[k] = value[j];
                    k++;
                }
                j++;
            } //remove non printables
            if(k>0) value[k] = 0; else value[0] = 0; // new termination
        }
        showID3Tag(tag, value);
        return fs;
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    // -- section V2.2 only , greater Vers above ----
    if(m_controlCounter == 10){ // frames in V2.2, 3bytes identifier, 3bytes size descriptor
        frameid[0] = *(data + 0);
        frameid[1] = *(data + 1);
        frameid[2] = *(data + 2);
        frameid[3] = 0;
        tag = frameid;
        headerSize -= 3;
        size_t len = bigEndian(data + 3, 3);
        headerSize -= 3;
        headerSize -= len;
        char value[256];
        size_t tmp = len;
        if(tmp > 254) tmp = 254;
        memcpy(value, (data + 7), tmp);
        value[tmp+1] = 0;
        chbuf[0] = 0;

        showID3Tag(tag, value);
        if(len == 0) m_controlCounter = 98;

        return 3 + 3 + len;
    }
    // -- end section V2.2 -----------

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == 98){ // skip all ID3 metadata (mostly spaces)
        if(headerSize > 256) {
            headerSize -=256;
            return 256;
        } // Throw it away
        else           {
            m_controlCounter = 99;
            return headerSize;
        } // Throw it away
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_controlCounter == 99){ //  exist another ID3tag?
        m_audioDataStart += id3Size;
        vTaskDelay(30);
        if((*(data + 0) == 'I') && (*(data + 1) == 'D') && (*(data + 2) == '3')) {
            m_controlCounter = 0;
            return 0;
        }
        else {
            m_controlCounter = 100; // ok
            m_audioDataSize = m_contentlength - m_audioDataStart;
            sprintf(chbuf, "Audio-Length: %u", m_audioDataSize);
            if(vs1053_info) vs1053_info(chbuf);

            return 0;
        }
    }
    return 0;
}
//---------------------------------------------------------------------------------------------------------------------
void VS1053::showID3Tag(String tag, const char* value){

    chbuf[0] = 0;
    // V2.2
    if(tag == "CNT") sprintf(chbuf, "Play counter: %s", value);
    // if(tag == "COM") sprintf(chbuf, "Comments: %s", value);
    if(tag == "CRA") sprintf(chbuf, "Audio encryption: %s", value);
    if(tag == "CRM") sprintf(chbuf, "Encrypted meta frame: %s", value);
    if(tag == "ETC") sprintf(chbuf, "Event timing codes: %s", value);
    if(tag == "EQU") sprintf(chbuf, "Equalization: %s", value);
    if(tag == "IPL") sprintf(chbuf, "Involved people list: %s", value);
    if(tag == "PIC") sprintf(chbuf, "Attached picture: %s", value);
    if(tag == "SLT") sprintf(chbuf, "Synchronized lyric/text: %s", value);
    // if(tag == "TAL") sprintf(chbuf, "Album/Movie/Show title: %s", value);
    if(tag == "TBP") sprintf(chbuf, "BPM (Beats Per Minute): %s", value);
    if(tag == "TCM") sprintf(chbuf, "Composer: %s", value);
    if(tag == "TCO") sprintf(chbuf, "Content type: %s", value);
    if(tag == "TCR") sprintf(chbuf, "Copyright message: %s", value);
    if(tag == "TDA") sprintf(chbuf, "Date: %s", value);
    if(tag == "TDY") sprintf(chbuf, "Playlist delay: %s", value);
    if(tag == "TEN") sprintf(chbuf, "Encoded by: %s", value);
    if(tag == "TFT") sprintf(chbuf, "File type: %s", value);
    if(tag == "TIM") sprintf(chbuf, "Time: %s", value);
    if(tag == "TKE") sprintf(chbuf, "Initial key: %s", value);
    if(tag == "TLA") sprintf(chbuf, "Language(s): %s", value);
    if(tag == "TLE") sprintf(chbuf, "Length: %s", value);
    if(tag == "TMT") sprintf(chbuf, "Media type: %s", value);
    if(tag == "TOA") sprintf(chbuf, "Original artist(s)/performer(s): %s", value);
    if(tag == "TOF") sprintf(chbuf, "Original filename: %s", value);
    if(tag == "TOL") sprintf(chbuf, "Original Lyricist(s)/text writer(s): %s", value);
    if(tag == "TOR") sprintf(chbuf, "Original release year: %s", value);
    if(tag == "TOT") sprintf(chbuf, "Original album/Movie/Show title: %s", value);
    if(tag == "TP1") sprintf(chbuf, "Lead artist(s)/Lead performer(s)/Soloist(s)/Performing group: %s", value);
    if(tag == "TP2") sprintf(chbuf, "Band/Orchestra/Accompaniment: %s", value);
    if(tag == "TP3") sprintf(chbuf, "Conductor/Performer refinement: %s", value);
    if(tag == "TP4") sprintf(chbuf, "Interpreted, remixed, or otherwise modified by: %s", value);
    if(tag == "TPA") sprintf(chbuf, "Part of a set: %s", value);
    if(tag == "TPB") sprintf(chbuf, "Publisher: %s", value);
    if(tag == "TRC") sprintf(chbuf, "ISRC (International Standard Recording Code): %s", value);
    if(tag == "TRD") sprintf(chbuf, "Recording dates: %s", value);
    if(tag == "TRK") sprintf(chbuf, "Track number/Position in set: %s", value);
    if(tag == "TSI") sprintf(chbuf, "Size: %s", value);
    if(tag == "TSS") sprintf(chbuf, "Software/hardware and settings used for encoding: %s", value);
    if(tag == "TT1") sprintf(chbuf, "Content group description: %s", value);
    if(tag == "TT2") sprintf(chbuf, "Title/Songname/Content description: %s", value);
    if(tag == "TT3") sprintf(chbuf, "Subtitle/Description refinement: %s", value);
    if(tag == "TXT") sprintf(chbuf, "Lyricist/text writer: %s", value);
    if(tag == "TXX") sprintf(chbuf, "User defined text information frame: %s", value);
    if(tag == "TYE") sprintf(chbuf, "Year: %s", value);
    if(tag == "UFI") sprintf(chbuf, "Unique file identifier: %s", value);
    if(tag == "ULT") sprintf(chbuf, "Unsychronized lyric/text transcription: %s", value);
    if(tag == "WAF") sprintf(chbuf, "Official audio file webpage: %s", value);
    if(tag == "WAR") sprintf(chbuf, "Official artist/performer webpage: %s", value);
    if(tag == "WAS") sprintf(chbuf, "Official audio source webpage: %s", value);
    if(tag == "WCM") sprintf(chbuf, "Commercial information: %s", value);
    if(tag == "WCP") sprintf(chbuf, "Copyright/Legal information: %s", value);
    if(tag == "WPB") sprintf(chbuf, "Publishers official webpage: %s", value);
    if(tag == "WXX") sprintf(chbuf, "User defined URL link frame: %s", value);

    // V2.3 V2.4 tags
    // if(tag == "COMM") sprintf(chbuf, "Comment: %s", value);
    if(tag == "OWNE") sprintf(chbuf, "Ownership: %s", value);
    // if(tag == "PRIV") sprintf(chbuf, "Private: %s", value);
    if(tag == "SYLT") sprintf(chbuf, "SynLyrics: %s", value);
    if(tag == "TALB") sprintf(chbuf, "Album: %s", value);
    if(tag == "TBPM") sprintf(chbuf, "BeatsPerMinute: %s", value);
    if(tag == "TCMP") sprintf(chbuf, "Compilation: %s", value);
    if(tag == "TCOM") sprintf(chbuf, "Composer: %s", value);
    if(tag == "TCON") sprintf(chbuf, "ContentType: %s", value);
    if(tag == "TCOP") sprintf(chbuf, "Copyright: %s", value);
    if(tag == "TDAT") sprintf(chbuf, "Date: %s", value);
    if(tag == "TEXT") sprintf(chbuf, "Lyricist: %s", value);
    if(tag == "TIME") sprintf(chbuf, "Time: %s", value);
    if(tag == "TIT1") sprintf(chbuf, "Grouping: %s", value);
    if(tag == "TIT2") sprintf(chbuf, "Title: %s", value);
    if(tag == "TIT3") sprintf(chbuf, "Subtitle: %s", value);
    if(tag == "TLAN") sprintf(chbuf, "Language: %s", value);
    if(tag == "TLEN") sprintf(chbuf, "Length (ms): %s", value);
    if(tag == "TMED") sprintf(chbuf, "Media: %s", value);
    if(tag == "TOAL") sprintf(chbuf, "OriginalAlbum: %s", value);
    if(tag == "TOPE") sprintf(chbuf, "OriginalArtist: %s", value);
    if(tag == "TORY") sprintf(chbuf, "OriginalReleaseYear: %s", value);
    if(tag == "TPE1") sprintf(chbuf, "Artist: %s", value);
    if(tag == "TPE2") sprintf(chbuf, "Band: %s", value);
    if(tag == "TPE3") sprintf(chbuf, "Conductor: %s", value);
    if(tag == "TPE4") sprintf(chbuf, "InterpretedBy: %s", value);
    if(tag == "TPOS") sprintf(chbuf, "PartOfSet: %s", value);
    if(tag == "TPUB") sprintf(chbuf, "Publisher: %s", value);
    if(tag == "TRCK") sprintf(chbuf, "Track: %s", value);
    if(tag == "TSSE") sprintf(chbuf, "SettingsForEncoding: %s", value);
    if(tag == "TRDA") sprintf(chbuf, "RecordingDates: %s", value);
    if(tag == "TXXX") sprintf(chbuf, "UserDefinedText: %s", value);
    if(tag == "TYER") sprintf(chbuf, "Year: %s", value);
    if(tag == "USER") sprintf(chbuf, "TermsOfUse: %s", value);
    if(tag == "USLT") sprintf(chbuf, "Lyrics: %s", value);
    if(tag == "WOAR") sprintf(chbuf, "OfficialArtistWebpage: %s", value);
    if(tag == "XDOR") sprintf(chbuf, "OriginalReleaseTime: %s", value);

    if(chbuf[0] != 0) if(vs1053_id3data) vs1053_id3data(chbuf);
}
//---------------------------------------------------------------------------------------------------------------------
uint32_t VS1053::getFileSize(){
    if (!audiofile) return 0;
    return audiofile.size();
}
//---------------------------------------------------------------------------------------------------------------------
uint32_t VS1053::getFilePos(){
    if (!audiofile) return 0;
    return audiofile.position();
}
//---------------------------------------------------------------------------------------------------------------------
bool VS1053::setFilePos(uint32_t pos){
    if (!audiofile) return false;
    return audiofile.seek(pos);
}
//---------------------------------------------------------------------------------------------------------------------
String VS1053::urlencode(String str)
{
    String encodedString="";
    char c;
    char code0;
    char code1;
    for (int i =0; i < str.length(); i++){
        c=str.charAt(i);
        if (c == ' ') encodedString+= '+';
        else if (isalnum(c)) encodedString+=c;
        else{
            code1=(c & 0xf)+'0';
            if ((c & 0xf) >9) code1=(c & 0xf) - 10 + 'A';
            c=(c>>4)&0xf;
            code0=c+'0';
            if (c > 9) code0=c - 10 + 'A';
            encodedString+='%';
            encodedString+=code0;
            encodedString+=code1;
        }
    }
    return encodedString;
}
