#include "vs1053_ext.h"

VS1053::VS1053(uint8_t _cs_pin, uint8_t _dcs_pin, uint8_t _dreq_pin) :
        cs_pin(_cs_pin), dcs_pin(_dcs_pin), dreq_pin(_dreq_pin)
{
    m_endFillByte=0;
    curvol=50;
    m_t0=0;
    m_LFcount=0;
}
VS1053::~VS1053(){
    // destructor
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
            SPI.write(m_endFillByte);
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
void VS1053::begin(){

    pinMode(dreq_pin, INPUT);                          // DREQ is an input
    pinMode(cs_pin, OUTPUT);                           // The SCI and SDI signals
    pinMode(dcs_pin, OUTPUT);
    digitalWrite(dcs_pin, HIGH);                       // Start HIGH for SCI en SDI
    digitalWrite(cs_pin, HIGH);
    delay(100);

    // Init SPI in slow mode (0.2 MHz)
    VS1053_SPI=SPISettings(200000, MSBFIRST, SPI_MODE0);
//    printDetails("Right after reset/startup \n");
    delay(20);
    //printDetails ("20 msec after reset");
    //testComm("Slow SPI,Testing VS1053 read/write registers... \n");
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
    //testComm("Fast SPI, Testing VS1053 read/write registers again... \n");
    delay(10);
    await_data_request();
    m_endFillByte=wram_read(0x1E06) & 0xFF;
//    sprintf(sbuf, "endFillByte is %X \n", endFillByte);
//    if(vs1053_info) vs1053_info(sbuf);
//    printDetails("After last clocksetting \n");
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
    for(i=0; i <= SCI_AICTRL3; i++)
            {
        regbuf[i]=read_register(i);
    }
    for(i=0; i <= SCI_AICTRL3; i++)
            {
        delay(5);
        sprintf(sbuf, "%3X - %5X \n", i, regbuf[i]);
        if(vs1053_info) vs1053_info(sbuf);
    }
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
//---------------------------------------------------------------------------------------
void VS1053::showstreamtitle(const char *ml, bool full){
    // example for ml:
    // StreamTitle='Oliver Frank - Mega Hitmix';StreamUrl='www.radio-welle-woerthersee.at';
    // or adw_ad='true';durationMilliseconds='10135';adId='34254';insertionType='preroll';

    int8_t pos1=0, pos2=0, pos3=0, pos4=0;
    String mline=ml, st="", su="", ad="", artist="", title="", icyurl="";
    static String st_remember="";
    //log_i("%s",mline.c_str());
    pos1=mline.indexOf("StreamTitle=");
    if(pos1!=-1){                               // StreamTitle found
        pos1=pos1+12;
        st=mline.substring(pos1);               // remove "StreamTitle="
        if(st.indexOf('&')!=-1){                // maybe html coded
            st.replace("&Auml;", "�" ); st.replace("&auml;", "�"); //HTML -> ASCII
            st.replace("&Ouml;", "�" ); st.replace("&ouml;", "�");
            st.replace("&Uuml;", "�" ); st.replace("&uuml;", "�");
            st.replace("&szlig;","�" ); st.replace("&amp;",  "&");
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
        if(st_remember!=st){ // show only changes
            if(vs1053_showstreamtitle) vs1053_showstreamtitle(st.c_str());
        }

        st_remember=st;
        st="StreamTitle=" + st + '\n';
        if(vs1053_info) vs1053_info(st.c_str());
    }
    pos4=mline.indexOf("StreamUrl=");
    if(pos4!=-1){                               // StreamUrl found
        pos4=pos4+10;
        su=mline.substring(pos4);               // remove "StreamUrl="
        pos2= su.indexOf(';',1);                // end of StreamUrl, first occurence of ';'
        if(pos2!=-1) su=su.substring(0,pos2);   // extract StreamUrl
        if(su.startsWith("'")) su=su.substring(1,su.length()-1); // if exists remove ' at the begin and end
        su="StreamUrl=" + su + '\n';
        if(vs1053_info) vs1053_info(su.c_str());
    }
    pos2=mline.indexOf("adw_ad=");              // advertising,
    if(pos2!=-1){
       ad=mline.substring(pos2);
       ad=ad + '\n';
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
    if(pos1==-1 && pos4==-1){
        // Info probably from playlist
        st=mline;
        if(vs1053_showstreamtitle) vs1053_showstreamtitle(st.c_str());
        st="Streamtitle: " + st + "\n";
        if(vs1053_info) vs1053_info(sbuf);
    }
}
//---------------------------------------------------------------------------------------
void VS1053::handlebyte(uint8_t b){
    static uint16_t playlistcnt;                                // Counter to find right entry in playlist
    String lcml;                                                // Lower case metaline
    static String ct;                                           // Contents type
    static String host;
    int inx;                                                    // Pointer in metaline
    static boolean f_entry=false;                               // entryflag for asx playlist

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
                sprintf(sbuf, "%s\n", m_metaline.c_str());
                if(vs1053_info) vs1053_info(sbuf);              // Yes, Show it
                if(lcml.indexOf("content-type:") >= 0){         // Line with "Content-Type: xxxx/yyy"
                    if(lcml.indexOf("audio") >= 0){             // Is ct audio?
                        m_ctseen=true;                          // Yes, remember seeing this
                        ct=m_metaline.substring(13);            // Set contentstype. Not used yet
                        ct.trim();
                        sprintf(sbuf, "%s seen.\n", ct.c_str());
                        if(vs1053_info) vs1053_info(sbuf);
                    }
                    if(lcml.indexOf("ogg") >= 0){               // Is ct ogg?
                        m_ctseen=true;                          // Yes, remember seeing this
                        ct=m_metaline.substring(13);
                        sprintf(sbuf, "%s seen.\n", ct.c_str());
                        if(vs1053_info) vs1053_info(sbuf);
                        m_metaint=0;                            // ogg has no metadata
                        m_bitrate=0;
                        m_icyname=="";
                        m_f_ogg=true;
                    }
                }
                else if(lcml.startsWith("location:")){
                    host=m_metaline.substring(lcml.indexOf("http"),lcml.length());// use metaline instead lcml
                    if(host.indexOf("&")>0)host=host.substring(0,host.indexOf("&")); // remove parameter
                    sprintf(sbuf, "redirect to new host %s\n", host.c_str());
                    if(vs1053_info) vs1053_info(sbuf);
                    connecttohost(host);
                }
                else if(lcml.startsWith("icy-br:")){
                    m_bitrate=m_metaline.substring(7).toInt();  // Found bitrate tag, read the bitrate
                    sprintf(sbuf,"%d", m_bitrate);
                    if(vs1053_bitrate) vs1053_bitrate(sbuf);
                }
                else if(lcml.startsWith("icy-metaint:")){
                    m_metaint=m_metaline.substring(12).toInt(); // Found metaint tag, read the value
                    //if(m_metaint==0) m_metaint=16000;           // if no set to default
                }
                else if(lcml.startsWith("icy-name:")){
                    m_icyname=m_metaline.substring(9);          // Get station name
                    m_icyname.trim();                           // Remove leading and trailing spaces
                    if(m_icyname!=""){
                        if(vs1053_showstation) vs1053_showstation(m_icyname.c_str());
                    }
//                    for(int z=0; z<m_icyname.length();z++) log_e("%i",m_icyname[z]);
                }
                else if(lcml.startsWith("transfer-encoding:")){
                    // Station provides chunked transfer
                    if(m_metaline.endsWith("chunked")){
                        m_chunked=true;
                        if(vs1053_info) vs1053_info("chunked data transfer\n");
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
                }
            }
            m_metaline="";                                      // Reset this line
            if((m_LFcount == 2) && m_ctseen){                   // Some data seen and a double LF?
                sprintf(sbuf, "Switch to DATA, bitrate is %d, metaint is %d\n", m_bitrate, m_metaint); // Show bitrate and metaint
                if(vs1053_info) vs1053_info(sbuf);
                if(m_icyname==""){if(vs1053_showstation) vs1053_showstation("");} // no icyname available
                if(m_bitrate==0){if(vs1053_bitrate) vs1053_bitrate("");} // no bitrate received
                m_datamode=VS1053_DATA;                         // Expecting data now
                if(m_f_ogg==true){
                    m_datamode=VS1053_OGG;                      // Overwrite m_datamode
                    m_f_ogg=false;
                }
                startSong();                                    // Start a new song
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
                sprintf(sbuf, "Metadata block %d bytes\n",      // Most of the time there are zero bytes of metadata
                        m_metacount - 1);
                if(vs1053_info) vs1053_info(sbuf);
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
                if(vs1053_info) vs1053_info("Metadata block to long! Skipping all Metadata from now on.\n");
                m_metaint=16000;                                // Probably no metadata
                m_metaline="";                                  // Do not waste memory on this
            }
            m_datamode=VS1053_DATA;                             // Expecting data
        }
    }
    if(m_datamode == VS1053_PLAYLISTINIT)                       // Initialize for receive .m3u file
    {
        // We are going to use metadata to read the lines from the .m3u file
        // Sometimes this will only contain a single line
        f_entry=false;                                          // no entry found yet (asx playlist)
        m_metaline="";                                          // Prepare for new line
        m_LFcount=0;                                            // For detection end of header
        m_datamode=VS1053_PLAYLISTHEADER;                       // Handle playlist data
        playlistcnt=1;                                          // Reset for compare
        m_totalcount=0;                                         // Reset totalcount
        if(vs1053_info) vs1053_info("Read from playlist\n");
    }
    if(m_datamode == VS1053_PLAYLISTHEADER){                    // Read header
        if((b > 0x7F) ||                                        // Ignore unprintable characters
                (b == '\r') ||                                  // Ignore CR
                (b == '\0'))                                    // Ignore NULL
                {
            // Yes, ignore
        }
        else if(b == '\n')                                      // Linefeed ?
                {
            m_LFcount++;                                        // Count linefeeds
            sprintf(sbuf, "Playlistheader: %s\n", m_metaline.c_str());  // Show playlistheader
            if(vs1053_info) vs1053_info(sbuf);
            lcml=m_metaline;                                // Use lower case for compare
            lcml.toLowerCase();
            lcml.trim();
            if(lcml.startsWith("location:")){
                 host=m_metaline.substring(lcml.indexOf("http"),lcml.length());// use metaline instead lcml
                if(host.indexOf("&")>0)host=host.substring(0,host.indexOf("&")); // remove parameter
                sprintf(sbuf, "redirect to new host %s\n", host.c_str());
                if(vs1053_info) vs1053_info(sbuf);
                connecttohost(host);
            }
            m_metaline="";                                      // Ready for next line
            if(m_LFcount == 2)
                    {
                if(vs1053_info) vs1053_info("Switch to PLAYLISTDATA\n");
                m_datamode=VS1053_PLAYLISTDATA;                 // Expecting data now
                return;
            }
        }
        else
        {
            m_metaline+=(char)b;                                // Normal character, put new char in metaline
            m_LFcount=0;                                        // Reset double CRLF detection
        }
    }
    if(m_datamode == VS1053_PLAYLISTDATA)                       // Read next byte of .m3u file data
    {
        m_t0=millis();
        if((b > 0x7F) ||                                        // Ignore unprintable characters
                (b == '\r') ||                                  // Ignore CR
                (b == '\0'))                                    // Ignore NULL
                { /* Yes, ignore */ }

        else if(b == '\n'){                                     // Linefeed or end of string?
            sprintf(sbuf, "Playlistdata: %s\n", m_metaline.c_str());  // Show playlistdata
            if(vs1053_info) vs1053_info(sbuf);
            if(m_playlist.endsWith("m3u")){
                if(m_metaline.length() < 5) {                   // Skip short lines
                    m_metaline="";                              // Flush line
                    return;}
                if(m_metaline.indexOf("#EXTINF:") >= 0){        // Info?
                    if(m_playlist_num == playlistcnt){          // Info for this entry?
                        inx=m_metaline.indexOf(",");            // Comma in this line?
                        if(inx > 0){
                            // Show artist and title if present in metadata
                            //if(vs1053_showstation) vs1053_showstation(m_metaline.substring(inx + 1).c_str());
                            if(vs1053_info) vs1053_info(m_metaline.substring(inx + 1).c_str());
                        }
                    }
                }
                if(m_metaline.startsWith("#")){                 // Commentline?
                    m_metaline="";
                    return;}                                    // Ignore commentlines
                // Now we have an URL for a .mp3 file or stream.  Is it the rigth one?
                //if(metaline.indexOf("&")>0)metaline=host.substring(0,metaline.indexOf("&"));
                sprintf(sbuf, "Entry %d in playlist found: %s\n", playlistcnt, m_metaline.c_str());
                if(vs1053_info) vs1053_info(sbuf);
                if(m_metaline.indexOf("&")){
                    m_metaline=m_metaline.substring(0, m_metaline.indexOf("&"));}
                if(m_playlist_num == playlistcnt){
                    inx=m_metaline.indexOf("http://");          // Search for "http://"
                    if(inx >= 0){                               // Does URL contain "http://"?
                        host=m_metaline.substring(inx + 7);}    // Yes, remove it and set host
                    else{
                        host=m_metaline;}                       // Yes, set new host
                    //log_i("connecttohost %s", host.c_str());
                    connecttohost(host);                        // Connect to it
                }
                m_metaline="";
                host=m_playlist;                                // Back to the .m3u host
                playlistcnt++;                                  // Next entry in playlist
            } //m3u
            if(m_playlist.endsWith("pls")){
                if(m_metaline.startsWith("File1")){
                    inx=m_metaline.indexOf("http://");          // Search for "http://"
                    if(inx >= 0){                               // Does URL contain "http://"?
                        m_plsURL=m_metaline.substring(inx + 7); // Yes, remove it
                        if(m_plsURL.indexOf("&")>0)m_plsURL=m_plsURL.substring(0,m_plsURL.indexOf("&")); // remove parameter
                        // Now we have an URL for a .mp3 file or stream in host.

                        m_f_plsFile=true;
                    }
                }
                if(m_metaline.startsWith("Title1")){
                    m_plsStationName=m_metaline.substring(7);
                    if(vs1053_showstation) vs1053_showstation(m_plsStationName.c_str());
                    sprintf(sbuf, "StationName: %s\n", m_plsStationName.c_str());
                    if(vs1053_info) vs1053_info(sbuf);
                    m_f_plsTitle=true;
                }
                if(m_metaline.startsWith("Length1")) m_f_plsTitle=true; // if no Title is available
                if((m_f_plsFile==true)&&(m_metaline.length()==0)) m_f_plsTitle=true;
                m_metaline="";
                if(m_f_plsFile && m_f_plsTitle){    //we have both StationName and StationURL
                    m_f_plsFile=false; m_f_plsTitle=false;
                    //log_i("connecttohost %s", m_plsURL.c_str());
                    connecttohost(m_plsURL);        // Connect to it
                }
            }//pls
            if(m_playlist.endsWith("asx")){
                String ml=m_metaline;
                ml.toLowerCase();                               // use lowercases
                if(ml.indexOf("<entry>")>=0) f_entry=true;      // found entry tag (returns -1 if not found)
                if(f_entry){
                    if(ml.indexOf("ref href")>0){
                        inx=ml.indexOf("http://");
                        if(inx>0){
                            m_plsURL=m_metaline.substring(inx + 7); // Yes, remove it
                            if(m_plsURL.indexOf('"')>0)m_plsURL=m_plsURL.substring(0,m_plsURL.indexOf('"')); // remove rest
                            // Now we have an URL for a stream in host.
                            m_f_plsFile=true;
                        }
                    }
                    if(ml.indexOf("<title>")>=0){
                        m_plsStationName=m_metaline.substring(7);
                        if(m_plsURL.indexOf('<')>0)m_plsURL=m_plsURL.substring(0,m_plsURL.indexOf('<')); // remove rest
                        if(vs1053_showstation) vs1053_showstation(m_plsStationName.c_str());
                        sprintf(sbuf, "StationName: %s\n", m_plsStationName.c_str());
                        if(vs1053_info) vs1053_info(sbuf);
                        m_f_plsTitle=true;
                    }
                }//entry
                m_metaline="";
                if(m_f_plsFile && m_f_plsTitle){   //we have both StationName and StationURL
                    m_f_plsFile=false; m_f_plsTitle=false;
                    //log_i("connecttohost %s", host.c_str());
                    connecttohost(m_plsURL);        // Connect to it
                }
            }//asx
        }
        else
        {
            m_metaline+=(char)b;                            // Normal character, add it to metaline
        }
        return;
    }
}
//---------------------------------------------------------------------------------------
uint16_t VS1053::ringused(){
    return (m_rcount);                                      // Free space available
}
//---------------------------------------------------------------------------------------
void VS1053::loop(){

    uint16_t part=0;                                        // part at the end of the ringbuffer
    uint16_t bcs=0;                                         // bytes can current send
    uint16_t maxchunk=0x1000;                               // max number of bytes to read, 4096d is enough
    uint16_t btp=0;                                         // bytes to play
    int16_t  res=0;                                         // number of bytes getting from client
    uint32_t av=0;                                          // available in stream (uin16_t is to small by playing from SD)
    static uint16_t rcount=0;                               // max bytes handover to the player
    static uint16_t chunksize=0;                            // Chunkcount read from stream
    static uint16_t count=0;                                // Bytecounter between metadata
    static uint32_t i=0;                                    // Count loops if ringbuffer is empty

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_f_localfile){                                      // Playing file from SD card?
         av=mp3file.available();                            // Bytes left in file
         if(av < maxchunk) maxchunk=av;                     // Reduce byte count for this mp3loop()
         if(maxchunk){                                      // Anything to read?
             m_btp=mp3file.read(m_ringbuf, maxchunk);       // Read a block of data
             sdi_send_buffer(m_ringbuf,m_btp);
         }
         if(av == 0){                                       // No more data from SD Card
             mp3file.close();
             m_f_localfile=false;
             sprintf(sbuf,"End of mp3file %s\n",m_mp3title.c_str());
             if(vs1053_info) vs1053_info(sbuf);
             if(vs1053_eof_mp3) vs1053_eof_mp3(m_mp3title.c_str());
         }
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_f_webstream){                                      // Playing file from URL?
        av=client.available();// Available from stream
        if(av){
            m_ringspace=m_ringbfsiz - m_rcount;
            part=m_ringbfsiz - m_rbwindex;                  // Part of length to xfer
            if(part>m_ringspace)part=m_ringspace;
            res=client.read(m_ringbuf+ m_rbwindex, part);   // Copy first part
            if(res>0){
                m_rcount+=res;
                m_rbwindex+=res;
            }
            if(m_rbwindex==m_ringbfsiz) m_rbwindex=0;
        }

        if(m_datamode == VS1053_PLAYLISTDATA){
            if(m_t0+49<millis()) {
                //log_i("terminate metaline after 50ms");     // if mo data comes from host
                handlebyte('\n');                           // send LF
            }
        }
        if(m_chunked==false){rcount=m_rcount;}
        else{
            while(m_rcount){
                if((m_chunkcount+rcount) == 0|| m_firstchunk){             // Expecting a new chunkcount?
                    uint8_t b =m_ringbuf[m_rbrindex];
                    if(b=='\r'){}
                    else if(b=='\n'){
                        m_chunkcount=chunksize;
                        m_firstchunk=false;
                        rcount=0;
                        chunksize=0;
                    }
                    else{
                        // We have received a hexadecimal character.  Decode it and add to the result.
                        b=toupper(b) - '0';                         // Be sure we have uppercase
                        if(b > 9) b = b - 7;                        // Translate A..F to 10..15
                        chunksize=(chunksize << 4) + b;
                    }
                    if(++m_rbrindex == m_ringbfsiz){        // Increment pointer and
                        m_rbrindex=0;                       // wrap at end
                    }
                    m_rcount--;

                }
                else break;
            }
            if(rcount==0){ //all bytes consumed?
                if(m_chunkcount>m_rcount){
                    m_chunkcount-=m_rcount;
                    rcount=m_rcount;
                }
                else{
                    rcount=m_chunkcount;
                    m_chunkcount-=rcount;
                }
            }
        }

        //*******************************************************************************

        if(m_datamode==VS1053_OGG){
            if(rcount>1024) btp=1024; else btp=rcount;  // reduce chunk thereby the ringbuffer can be proper fillied
            if(btp){
                rcount-=btp;
                if((m_rbrindex + btp) >= m_ringbfsiz){
                    part=m_ringbfsiz - m_rbrindex;
                    sdi_send_buffer(m_ringbuf+ m_rbrindex, part);
                    m_rbrindex=0;
                    m_rcount-=part;
                    btp-=part;
                }
                if(btp){                                         // Rest to do?
                    sdi_send_buffer(m_ringbuf+ m_rbrindex, btp); // Copy full or last part
                    m_rbrindex+=btp;                             // Point to next free byte
                    m_rcount-=btp;                               // Adjust number of bytes
                }
            } return;
        }


        if(m_datamode==VS1053_DATA){
            if(rcount>1024)btp=1024;  else btp=rcount;  // reduce chunk thereby the ringbuffer can be proper fillied
            if(count>btp){bcs=btp; count-=bcs;} else{bcs=count; count=0;}
            if(bcs){
              rcount-=bcs;
                // First see if we must split the transfer.  We cannot write past the ringbuffer end.
                if((m_rbrindex + bcs) >= m_ringbfsiz){
                    part=m_ringbfsiz - m_rbrindex;              // Part of length to xfer
                    sdi_send_buffer(m_ringbuf+ m_rbrindex, part);  // Copy first part
                    m_rbrindex=0;
                    m_rcount-=part;                             // Adjust number of bytes
                    bcs-=part;                                  // Adjust rest length
                }
                if(bcs){                                        // Rest to do?
                    sdi_send_buffer(m_ringbuf+ m_rbrindex, bcs); // Copy full or last part
                    m_rbrindex+=bcs;                            // Point to next free byte
                    m_rcount-=bcs;                              // Adjust number of bytes
                }
                if(count==0){
                    m_datamode=VS1053_METADATA;
                    m_firstmetabyte=true;
                }
            }
        }
        else{ //!=DATA
            while(rcount){
                handlebyte(m_ringbuf[m_rbrindex]);
                if(++m_rbrindex == m_ringbfsiz){                // Increment pointer and
                    m_rbrindex=0;                               // wrap at end
                }
                rcount--;
                // call handlebyte>connecttohost can set m_rcount to zero (empty ringbuff)
                if(m_rcount>0) m_rcount--;                   // no underrun
                if(m_rcount==0)rcount=0; // exit this while()
                if(m_datamode==VS1053_DATA){
                    count=m_metaint;
                    if(m_metaint==0) m_datamode=VS1053_OGG; // is likely no ogg but a stream without metadata, can be mms
                    break;
                }
            }
        }
        if((m_f_stream_ready==false)&&(ringused()!=0)){ // first streamdata recognised
            m_f_stream_ready=true;
        }
        if(m_f_stream_ready==true){
            if(ringused()==0){  // empty buffer, broken stream or bad bitrate?
                i++;
                if(i>150000){    // wait several seconds
                    i=0;
                    if(vs1053_info) vs1053_info("Stream lost -> try new connection\n");
                    connecttohost(m_lastHost);} // try a new connection
            }
            else i=0;
        }
    } // end if(webstream)
}
//---------------------------------------------------------------------------------------
void VS1053::stop_mp3client(){
    int v=read_register(SCI_VOL);
    mp3file.close();
    m_f_localfile=false;
    m_f_webstream=false;
    write_register(SCI_VOL, 0);  // Mute while stopping
//    while(client.connected())
//    {
//        if(vs1053_info) vs1053_info("Stopping client\n"); // Stop connection to host
//        client.flush();
//        client.stop();
//        delay(500);
//    }
    client.flush();                                       // Flush stream client
    client.stop();                                        // Stop stream client
    write_register(SCI_VOL, v);
}
//---------------------------------------------------------------------------------------
bool VS1053::connecttohost(String host){

    int inx;                                              // Position of ":" in hostname
    int port=80;                                          // Port number for host
    String extension="/";                                 // May be like "/mp3" in "skonto.ls.lv:8002/mp3"
    String hostwoext;                                     // Host without extension and portnumber
    boolean ssl=false;

    stopSong();
    stop_mp3client();                                     // Disconnect if still connected
    m_f_localfile=false;
    m_f_webstream=true;
    if(m_lastHost!=host){                                 // New host or reconnection?
        m_f_stream_ready=false;
        m_lastHost=host;                                  // Remember the current host
    }
    sprintf(sbuf, "Connect to new host: %s\n", host.c_str());
    if(vs1053_info) vs1053_info(sbuf);

    // initializationsequence
    m_rcount=0;                                             // Empty ringbuff
    m_rbrindex=0;
    m_rbwindex=0;
    m_ctseen=false;                                         // Contents type not seen yet
    m_metaint=0;                                            // No metaint yet
    m_LFcount=0;                                            // For detection end of header
    m_bitrate=0;                                            // Bitrate still unknown
    m_totalcount=0;                                         // Reset totalcount
    m_metaline="";                                          // No metadata yet
    m_icyname="";                                           // No StationName yet
    m_bitrate=0;                                            // No bitrate yet
    m_firstchunk=true;                                      // First chunk expected
    m_chunked=false;                                        // Assume not chunked
    setDatamode(VS1053_HEADER);                             // Handle header

    if(host.startsWith("http://")) {host=host.substring(7); ssl=false;}
    if(host.startsWith("https://")){host=host.substring(8); ssl=true;}
    // ssl not supported yet because lack of memory

    if(host.endsWith(".m3u")||
            host.endsWith(".pls")||
                 host.endsWith("asx")){                     // Is it an m3u or pls or asx playlist?
        m_playlist=host;                                    // Save copy of playlist URL
        m_datamode=VS1053_PLAYLISTINIT;                     // Yes, start in PLAYLIST mode
        if(m_playlist_num == 0){                            // First entry to play?
            m_playlist_num=1;                               // Yes, set index
        }
        sprintf(sbuf, "Playlist request, entry %d\n", m_playlist_num); // Most of the time there are zero bytes of metadata
        if(vs1053_info) vs1053_info(sbuf);
    }

    // In the URL there may be an extension, like noisefm.ru:8000/play.m3u&t=.m3u
    inx=host.indexOf("/");                                  // Search for begin of extension
    if(inx > 0){                                            // Is there an extension?
        extension=host.substring(inx);                      // Yes, change the default
        hostwoext=host.substring(0, inx);                   // Host without extension

    }
    // In the URL there may be a portnumber
    inx=host.indexOf(":");                                  // Search for separator
    if(inx >= 0){                                           // Portnumber available?
        port=host.substring(inx + 1).toInt();               // Get portnumber as integer
        hostwoext=host.substring(0, inx);                   // Host without portnumber
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

    const uint8_t ascii[60]={
          //196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215,   ISO
            142, 143, 146, 128, 000, 144, 000, 000, 000, 000, 000, 000, 000, 165, 000, 000, 000, 000, 153, 000, //ASCII
          //216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235,   ISO
            000, 000, 000, 000, 154, 000, 000, 225, 133, 000, 000, 000, 132, 143, 145, 135, 138, 130, 136, 137, //ASCII
          //236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255    ISO
            000, 161, 140, 139, 000, 164, 000, 162, 147, 000, 148, 000, 000, 000, 163, 150, 129, 000, 000, 152};//ASCII


    uint16_t i=0, s=0;

    stopSong();
    stop_mp3client();                                    // Disconnect if still connected
    m_f_localfile=true;
    m_f_webstream=false;
    while(sdfile[i] != 0){  //convert UTF8 to ASCII
        path[i]=sdfile[i];
        if(path[i] > 195){
            s=ascii[path[i]-196];
            if(s!=0) path[i]=s; // found a related ASCII sign
        } i++;
    }
    path[i]=0;
    m_mp3title=sdfile.substring(sdfile.lastIndexOf('/') + 1, sdfile.length());
    showstreamtitle(m_mp3title.c_str(), true);
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
