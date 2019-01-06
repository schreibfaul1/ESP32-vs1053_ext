/*
 *  vs1053_ext.cpp
 *
 *  Created on: Jul 09.2017
 *  Updated on: Dec 30 2018
 *      Author: Wolle
 */

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
    SPI.write(3);                                // Read operation
    SPI.write(_reg);                             // Register to write (0..0xF)
    // Note: transfer16 does not seem to work
    result=(SPI.transfer(0xFF) << 8) | (SPI.transfer(0xFF));  // Read 16 bits data
    await_data_request();                        // Wait for DREQ to be HIGH again
    control_mode_off();
    return result;
}
//---------------------------------------------------------------------------------------------------------------------
void VS1053::write_register(uint8_t _reg, uint16_t _value)
{
    control_mode_on();
    SPI.write(2);                                // Write operation
    SPI.write(_reg);                             // Register to write (0..0xF)
    SPI.write16(_value);                         // Send 16 bits data
    await_data_request();
    control_mode_off();
}
//---------------------------------------------------------------------------------------------------------------------
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
//---------------------------------------------------------------------------------------------------------------------
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
//---------------------------------------------------------------------------------------------------------------------
void VS1053::wram_write(uint16_t address, uint16_t data){

    write_register(SCI_WRAMADDR, address);
    write_register(SCI_WRAM, data);
}
//---------------------------------------------------------------------------------------------------------------------
uint16_t VS1053::wram_read(uint16_t address){

    write_register(SCI_WRAMADDR, address);       // Start reading from WRAM
    return read_register(SCI_WRAM);              // Read back result
}
//---------------------------------------------------------------------------------------------------------------------
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
//    sprintf(sbuf, "endFillByte is %X", endFillByte);
//    if(vs1053_info) vs1053_info(sbuf);
//    printDetails("After last clocksetting \n");
    delay(100);
}
//---------------------------------------------------------------------------------------------------------------------
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
//---------------------------------------------------------------------------------------------------------------------
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
//---------------------------------------------------------------------------------------------------------------------
uint8_t VS1053::getVolume()                              // Get the currenet volume setting.
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
            sprintf(sbuf, "Song stopped correctly after %d msec", i * 10);
            if(vs1053_info) vs1053_info(sbuf);
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
//    String bit_rep[16] = {
//        [ 0] = "0000", [ 1] = "0001", [ 2] = "0010", [ 3] = "0011",
//        [ 4] = "0100", [ 5] = "0101", [ 6] = "0110", [ 7] = "0111",
//        [ 8] = "1000", [ 9] = "1001", [10] = "1010", [11] = "1011",
//        [12] = "1100", [13] = "1101", [14] = "1110", [15] = "1111",
//    };
    String regName[16] = {
        [ 0] = "MODE       ", [ 1] = "STATUS     ", [ 2] = "BASS       ", [ 3] = "CLOCKF     ",
        [ 4] = "DECODE_TIME", [ 5] = "AUDATA     ", [ 6] = "WRAM       ", [ 7] = "WRAMADDR   ",
        [ 8] = "HDAT0      ", [ 9] = "HDAT1      ", [10] = "AIADDR     ", [11] = "VOL        ",
        [12] = "AICTRL0    ", [13] = "AICTRL1    ", [14] = "AICTRL2    ", [15] = "AICTRL3    ",
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
    sprintf(sbuf, "chipID = %d%d", reg1, reg2);
    if(vs1053_info) vs1053_info(sbuf);
    reg1=wram_read(0x1E02) & 0xFF;
    if(reg1==0xFF) {reg1=0; flag=false;} // version too high
    sprintf(sbuf, "version = %d", reg1);
    if(vs1053_info) vs1053_info(sbuf);
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
    if(pos1==-1 && pos4==-1){
        // Info probably from playlist
        st=mline;
        if(vs1053_showstreamtitle) vs1053_showstreamtitle(st.c_str());
        st="Streamtitle: " + st;
        if(vs1053_info) vs1053_info(sbuf);
    }
}
//---------------------------------------------------------------------------------------------------------------------
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
                if(lcml.indexOf("content-type:") >= 0){         // Line with "Content-Type: xxxx/yyy"
                    if(lcml.indexOf("audio") >= 0){             // Is ct audio?
                        m_ctseen=true;                          // Yes, remember seeing this
                        ct=m_metaline.substring(13);            // Set contentstype. Not used yet
                        ct.trim();
                        sprintf(sbuf, "%s seen.", ct.c_str());
                        if(vs1053_info) vs1053_info(sbuf);
                    }
                    if(lcml.indexOf("ogg") >= 0){               // Is ct ogg?
                        m_ctseen=true;                          // Yes, remember seeing this
                        ct=m_metaline.substring(13);
                        ct.trim();
                        sprintf(sbuf, "%s seen.", ct.c_str());
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
                    sprintf(sbuf, "redirect to new host %s", host.c_str());
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
                    //log_i("m_metaint=%i",m_metaint);
                    if(m_metaint==0) m_metaint=16000;           // if no set to default
                }
                else if(lcml.startsWith("icy-name:")){
                    m_icyname=m_metaline.substring(9);          // Get station name
                    m_icyname.trim();                           // Remove leading and trailing spaces
                    sprintf(sbuf, "icy-name=%s", m_icyname.c_str());
                    if(vs1053_info) vs1053_info(sbuf);
                    if(m_icyname!=""){
                        if(vs1053_showstation) vs1053_showstation(m_icyname.c_str());
                    }
//                    for(int z=0; z<m_icyname.length();z++) log_e("%i",m_icyname[z]);
                }
                else if(lcml.startsWith("transfer-encoding:")){
                    // Station provides chunked transfer
                    if(m_metaline.endsWith("chunked")){
                        m_chunked=true;
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
                    sprintf(sbuf, "%s", m_metaline.c_str());
                    if(vs1053_info) vs1053_info(sbuf);
                }
            }
            m_metaline="";                                      // Reset this line
            if((m_LFcount == 2) && m_ctseen){                   // Some data seen and a double LF?
                if(m_icyname==""){if(vs1053_showstation) vs1053_showstation("");} // no icyname available
                if(m_bitrate==0){if(vs1053_bitrate) vs1053_bitrate("");} // no bitrate received
                if(m_f_ogg==true){
                    m_datamode=VS1053_OGG;                      // Overwrite m_datamode
                    sprintf(sbuf, "Switch to OGG, bitrate is %d, metaint is %d", m_bitrate, m_metaint); // Show bitrate and metaint
                    if(vs1053_info) vs1053_info(sbuf);
                    String lasthost=m_lastHost;
                    uint idx=lasthost.indexOf('?');
                    if(idx>0) lasthost=lasthost.substring(0, idx);
                    if(vs1053_lasthost) vs1053_lasthost(lasthost.c_str());
                    m_f_ogg=false;
                }
                else{
                    m_datamode=VS1053_DATA;                         // Expecting data now
                    sprintf(sbuf, "Switch to DATA, bitrate is %d, metaint is %d", m_bitrate, m_metaint); // Show bitrate and metaint
                    if(vs1053_info) vs1053_info(sbuf);
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
                sprintf(sbuf, "Metadata block %d bytes",        // Most of the time there are zero bytes of metadata
                        m_metacount-1);
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
                if(vs1053_info) vs1053_info("Metadata block to long! Skipping all Metadata from now on.");
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
        if(vs1053_info) vs1053_info("Read from playlist");
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
            sprintf(sbuf, "Playlistheader: %s", m_metaline.c_str());  // Show playlistheader
            if(vs1053_info) vs1053_info(sbuf);
            lcml=m_metaline;                                // Use lower case for compare
            lcml.toLowerCase();
            lcml.trim();
            if(lcml.startsWith("location:")){
                host=m_metaline.substring(lcml.indexOf("http"),lcml.length());// use metaline instead lcml
                if(host.indexOf("&")>0)host=host.substring(0,host.indexOf("&")); // remove parameter
                sprintf(sbuf, "redirect to new host %s", host.c_str());
                if(vs1053_info) vs1053_info(sbuf);
                connecttohost(host);
            }
            m_metaline="";                                      // Ready for next line
            if(m_LFcount == 2)
                    {
                if(vs1053_info) vs1053_info("Switch to PLAYLISTDATA");
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
            sprintf(sbuf, "Playlistdata: %s", m_metaline.c_str());  // Show playlistdata
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
                sprintf(sbuf, "Entry %d in playlist found: %s", playlistcnt, m_metaline.c_str());
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
                    sprintf(sbuf, "StationName: %s", m_plsStationName.c_str());
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
                        sprintf(sbuf, "StationName: %s", m_plsStationName.c_str());
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
//---------------------------------------------------------------------------------------------------------------------
uint16_t VS1053::ringused(){
    return (m_rcount);                                      // Free space available
}
//---------------------------------------------------------------------------------------------------------------------
void VS1053::loop(){

    uint16_t part=0;                                        // part at the end of the ringbuffer
    uint16_t bcs=0;                                         // bytes can current send
    uint16_t maxchunk=0x1000;                               // max number of bytes to read, 4096d is enough
    uint16_t btp=0;                                         // bytes to play
    int16_t  res=0;                                         // number of bytes getting from client
    uint32_t av=0;                                          // available in stream (uin16_t is to small by playing from SD)
    static uint16_t rcount=0;                               // max bytes handover to the player
    static uint32_t chunksize=0;                            // Chunkcount read from stream
    static uint32_t count=0;                                // Bytecounter between metadata
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
             sprintf(sbuf,"End of mp3file %s", m_mp3title.c_str());
             if(vs1053_info) vs1053_info(sbuf);
             if(vs1053_eof_mp3) vs1053_eof_mp3(m_mp3title.c_str());
         }
    }
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if(m_f_webstream){                                      // Playing file from URL?
        if(m_ssl==false) av=client.available();// Available from stream
        if(m_ssl==true)  av=clientsecure.available();// Available from stream
        if(av){
            m_ringspace=m_ringbfsiz - m_rcount;
            part=m_ringbfsiz - m_rbwindex;                  // Part of length to xfer
            if(part>m_ringspace)part=m_ringspace;
            if(m_ssl==false) res=client.read(m_ringbuf+ m_rbwindex, part);   // Copy first part
            if(m_ssl==true)  res=clientsecure.read(m_ringbuf+ m_rbwindex, part);   // Copy first part
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
            if(btp){  //bytes to play
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
            if(bcs){ // bytes can send
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
                    if(vs1053_info) vs1053_info("Stream lost -> try new connection");
                    connecttohost(m_lastHost);} // try a new connection
            }
            else i=0;
        }
    } // end if(webstream)
}
//---------------------------------------------------------------------------------------------------------------------
void VS1053::stop_mp3client(){
    int v=read_register(SCI_VOL);
    mp3file.close();
    m_f_localfile=false;
    m_f_webstream=false;
    write_register(SCI_VOL, 0);  // Mute while stopping
//    while(client.connected())
//    {
//        if(vs1053_info) vs1053_info("Stopping client"); // Stop connection to host
//        client.flush();
//        client.stop();
//        delay(500);
//    }
    client.flush();                                       // Flush stream client
    client.stop();                                        // Stop stream client
    write_register(SCI_VOL, v);
}
//---------------------------------------------------------------------------------------------------------------------
bool VS1053::connecttohost(String host){

    int inx;                                              // Position of ":" in hostname
    int port=80;                                          // Port number for host
    String extension="/";                                 // May be like "/mp3" in "skonto.ls.lv:8002/mp3"
    String hostwoext;                                     // Host without extension and portnumber
    String headerdata="";
    stopSong();
    stop_mp3client();                                     // Disconnect if still connected
    m_f_localfile=false;
    m_f_webstream=true;
    if(m_lastHost!=host){                                 // New host or reconnection?
        m_f_stream_ready=false;
        m_lastHost=host;                                  // Remember the current host
    }
    sprintf(sbuf, "Connect to new host: %s", host.c_str());
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
    m_st_remember="";                                       // Delete the last streamtitle
    m_bitrate=0;                                            // No bitrate yet
    m_firstchunk=true;                                      // First chunk expected
    m_chunked=false;                                        // Assume not chunked
    m_ssl=false;
    setDatamode(VS1053_HEADER);                             // Handle header

    if(host.startsWith("http://")) {host=host.substring(7); m_ssl=false;}
    if(host.startsWith("https://")){host=host.substring(8); m_ssl=true; port=443;}
    clientsecure.stop(); clientsecure.flush(); // release memory

    if(host.endsWith(".m3u")||
            host.endsWith(".pls")||
                 host.endsWith("asx")){                     // Is it an m3u or pls or asx playlist?
        m_playlist=host;                                    // Save copy of playlist URL
        m_datamode=VS1053_PLAYLISTINIT;                     // Yes, start in PLAYLIST mode
        if(m_playlist_num == 0){                            // First entry to play?
            m_playlist_num=1;                               // Yes, set index
        }
        sprintf(sbuf, "Playlist request, entry %d", m_playlist_num); // Most of the time there are zero bytes of metadata
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
    sprintf(sbuf, "Connect to %s on port %d, extension %s",
            hostwoext.c_str(), port, extension.c_str());
    if(vs1053_info) vs1053_info(sbuf);
    if(vs1053_showstreaminfo) vs1053_showstreaminfo(sbuf);

    String resp=String("GET ") + extension +
                String(" HTTP/1.1\r\n") +
                String("Host: ") + hostwoext +
                String("\r\n") +
                String("Icy-MetaData:1\r\n") +
                String("Connection: close\r\n\r\n");

    if(m_ssl==false){
        if(client.connect(hostwoext.c_str(), port)){
            if(vs1053_info) vs1053_info("Connected to server");
            client.print(resp);
            return true;
        }
    }
    if(m_ssl==true){
        if(clientsecure.connect(hostwoext.c_str(), port)){
            if(vs1053_info) vs1053_info("SSL/TLS Connected to server");
            clientsecure.print(resp);
            return true;
        }
    }

    sprintf(sbuf, "Request %s failed!", host.c_str());
    if(vs1053_info) vs1053_info(sbuf);
    if(vs1053_showstation) vs1053_showstation("");
    if(vs1053_showstreamtitle) vs1053_showstreamtitle("");
    if(vs1053_showstreaminfo) vs1053_showstreaminfo("");
    return false;
}
//---------------------------------------------------------------------------------------------------------------------
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
    stop_mp3client();                           // Disconnect if still connected
    clientsecure.stop(); clientsecure.flush();  // release memory if allocated
    m_f_localfile=true;
    m_f_webstream=false;
    if(!sdfile.startsWith("/")) sdfile="/"+sdfile;
    while(sdfile[i] != 0){                      //convert UTF8 to ASCII
        path[i]=sdfile[i];
        if(path[i] > 195){
            s=ascii[path[i]-196];
            if(s!=0) path[i]=s;                 // found a related ASCII sign
        } i++;
    }
    path[i]=0;
    m_mp3title=sdfile.substring(sdfile.lastIndexOf('/') + 1, sdfile.length());
    showstreamtitle(m_mp3title.c_str(), true);
    sprintf(sbuf, "Reading file: %s", path);
    if(vs1053_info) vs1053_info(sbuf);
    fs::FS &fs=SD;
    mp3file=fs.open(path);
    if( !mp3file){
        if(vs1053_info) vs1053_info("Failed to open file for reading");
        return false;
    }
    mp3file.readBytes(sbuf, 10);
        if ((sbuf[0] != 'I') || (sbuf[1] != 'D') || (sbuf[2] != '3')) {
            if(vs1053_info) vs1053_info("file has no mp3 tag, skip metadata");
            setFilePos(0);
            return true;
        }
        m_rev = sbuf[3];
        switch (m_rev) {
        case 2:
            m_f_unsync = (sbuf[5] & 0x80);
            m_f_exthdr = false;
            break;
        case 3:
        case 4:
            m_f_unsync = (sbuf[5] & 0x80); // bit7
            m_f_exthdr = (sbuf[5] & 0x40); // bit6 extended header
            break;
        };

        m_id3Size  = sbuf[6]; m_id3Size = m_id3Size << 7;
        m_id3Size |= sbuf[7]; m_id3Size = m_id3Size << 7;
        m_id3Size |= sbuf[8]; m_id3Size = m_id3Size << 7;
        m_id3Size |= sbuf[9];

        // Every read from now may be unsync'd
        sprintf(sbuf, "ID3 version=%i", m_rev);
        if(vs1053_info) vs1053_info(sbuf);
        sprintf(sbuf,"ID3 framesSize=%i", m_id3Size);
        if(vs1053_info) vs1053_info(sbuf);
        readID3Metadata();
    return true;
}
//---------------------------------------------------------------------------------------------------------------------
bool VS1053::connecttospeech(String speech, String lang){
    String host="translate.google.com";
    String path="/translate_tts";
    m_f_localfile=false;
    m_f_webstream=false;
    m_ssl=true;

    stopSong();
    stop_mp3client();                           // Disconnect if still connected
    clientsecure.stop(); clientsecure.flush();  // release memory if allocated

    String resp=   String("GET / HTTP/1.0\r\n") +
                   String("Host: ") + host + String("\r\n") +
                   String("User-Agent: GoogleTTS for ESP32/1.0.0\r\n") +
                   String("Accept-Encoding: identity\r\n") +
                   String("Accept: text/html\r\n\r\n");

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

    String tkkFunc;
    char ch;
    do {  // search for tkk:
        tkkFunc = "";
        clientsecure.readBytes(&ch, 1);
        if (ch != 't') continue;
        tkkFunc += String(ch);
        clientsecure.readBytes(&ch, 1);
        if (ch != 'k') continue;
        tkkFunc += String(ch);
        clientsecure.readBytes(&ch, 1);
        if (ch != 'k') continue;
        tkkFunc += String(ch);
        clientsecure.readBytes(&ch, 1);
        if (ch != ':') continue;
        tkkFunc += String(ch);
    } while(tkkFunc.length() < 4);
    tkkFunc +=  clientsecure.readStringUntil(',');  // "tkk='xxxxxxxxx.yyyyyyyyy'"
    tkkFunc = tkkFunc.substring(5 /* length of "tkk='" */, tkkFunc.lastIndexOf('\''));
//    log_i("tkk=%s", tkkFunc.c_str());

    // create token
    int periodPos = tkkFunc.indexOf('.');
    String key1 = tkkFunc.substring(0,periodPos);
    String key2 = tkkFunc.substring(periodPos + 1);
    long long int a, b;
    a = b = strtoll(key1.c_str(), NULL, 10);

    int f;
    int len = strlen(speech.c_str());
    for (f = 0; f < len; f++) {
      a += speech[f];
      a = XL(a, "+-a^+6");
    }
    a = XL(a, "+-3^+b+-f");
    a = a ^ (strtoll(key2.c_str(), NULL, 10));
    if (0 > a) {
      a = (a & 2147483647) + 2147483648;
    }
    a = a % 1000000;
    String token=String(lltoa(a, 10)) + '.' + lltoa(a ^ b, 10);

    int i,j;
    const char* t = speech.c_str();
    for(i=0,j=0;i<strlen(t);i++) {
      if (t[i] < 0x80 || t[i] > 0xbf) {
        j++;
      }
    }
//    log_i("Token=%s", token.c_str());

    String tts= String("https://") + host + path +
                        "?ie=UTF-8&q=" + urlencode(speech) +
                        "&tl=" + lang +
                        "&textlen=" + String(j) +
                        "&tk=" + token +
                        "&total=1&idx=0&client=t&prev=input&ttsspeed=1";

    clientsecure.stop();  clientsecure.flush();

    resp=   String("GET ") + tts + String("HTTP/1.1\r\n") +
            String("Host: ") + host + String("\r\n") +
            String("Connection: close\r\n\r\n");

    if (!clientsecure.connect(host.c_str(), 443)) {
        Serial.println("Connection failed");
        return false;
    }
    clientsecure.print(resp);

    while (clientsecure.connected()) {
        String line = clientsecure.readStringUntil('\n');
        line+="\n";
//      if(vs1053_info) vs1053_info(line.c_str());
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
void VS1053::readID3Metadata(){
    char frameid[5];
    int framesize=0;
    bool compressed;
    char value[256];
    bool bitorder=false;
    uint8_t uni_h=0;
    uint8_t uni_l=0;
    String tag="";
    if (m_f_exthdr) {
        if(vs1053_info) vs1053_info("ID3 extended header");
        int ehsz = (mp3file.read() << 24) | (mp3file.read() << 16)
                | (mp3file.read() << 8) | (mp3file.read());
        m_id3Size -= 4;
        for (int j = 0; j < ehsz - 4; j++) {
            mp3file.read();
            m_id3Size--;
        } // Throw it away
    } else
        if(vs1053_info) vs1053_info("ID3 normal frames");

    do {
        frameid[0] = mp3file.read();
        frameid[1] = mp3file.read();
        frameid[2] = mp3file.read();
        m_id3Size -= 3;
        if   (m_rev == 2) frameid[3] = 0;
        else {frameid[3] = mp3file.read(); m_id3Size--;}
        frameid[4]=0; // terminate the string
        tag=frameid;
        if(frameid[0]==0 && frameid[1]==0 && frameid[2]==0 && frameid[3]==0){
            // We're in padding
            while (m_id3Size != 0){mp3file.read(); m_id3Size--;}
        }
        else{
            if(m_rev==2){
                framesize = (mp3file.read() << 16) | (mp3file.read() << 8) | (mp3file.read());
                m_id3Size -= 3;
                compressed = false;
            }
            else{
                framesize = (mp3file.read() << 24) | (mp3file.read() << 16) | (mp3file.read() << 8) | (mp3file.read());
                m_id3Size -= 4;
                mp3file.read(); // skip 1st flag
                m_id3Size--;
                compressed = mp3file.read() & 0x80;
                m_id3Size--;
            }
            if(compressed){
                log_i("iscompressed");
                int decompsize=(mp3file.read()<<24) | (mp3file.read()<<16) | (mp3file.read()<<8) | (mp3file.read());
                m_id3Size -= 4;
                (void) decompsize;
                for (int j = 0; j < framesize; j++){
                    mp3file.read();
                    m_id3Size--;
                }
            }
            // Read the value
            uint32_t i=0; uint16_t j=0, k=0, m=0;
            bool isUnicode;
            if(framesize>0){
                isUnicode = (mp3file.read() == 1) ? true : false;
                m_id3Size--;
                if(framesize<256){
                    mp3file.readBytes(value, framesize-1); m_id3Size-=framesize-1;
                    i=framesize-1; value[framesize-1]=0;
                }
                else{
                    if(tag=="APIC"){ // a image embedded in file, skip it
                        //log_i("it's a image");
                        setFilePos(getFilePos()+framesize-1); m_id3Size-=framesize-1;
                    }
                    else{
                        // store the first 255 bytes in buffer and cut the remains
                        mp3file.readBytes(value, 255); m_id3Size-=255;
                        value[255]=0; i=255;
                        // big block, skip it
                        setFilePos(getFilePos()+framesize-1-255); m_id3Size-=framesize-1;
                    }
                }
                if(isUnicode){  // convert unicode to utf-8 U+0020...U+07FF
                    j=0; m=0;
                    while(m<i-1){
                        if((value[m]==0xFE)&&(value[m+1]==0xFF)){bitorder=true; j=m+2;}// MSB/LSB
                        if((value[m]==0xFF)&&(value[m+1]==0xFE)){bitorder=false; j=m+2;}//LSB/MSB
                        m++;
                    } // seek for last bitorder
                    m=0;
                    if(j>0){
                        for(k=j; k<i-1; k+=2){
                            if(bitorder==true){uni_h=value[k]; uni_l=value[k+1];}
                            else              {uni_l=value[k]; uni_h=value[k+1];}
                            uint16_t uni_hl=(uni_h<<8)+uni_l;
                            uint8_t utf8_h=(uni_hl>>6); // div64
                            uint8_t utf8_l=uni_l;
                            if(utf8_h>3){
                                utf8_h+=0xC0;
                                if     (uni_l<0x40) utf8_l=uni_l+0x80;
                                else if(uni_l<0x80) utf8_l=uni_l+=0x40;
                                else if(uni_l<0xC0) utf8_l=uni_l;
                                else    utf8_l=uni_l-0x40;
                            }
                            if(utf8_h>3) {value[m]= utf8_h; m++;}
                            value[m]=utf8_l; m++;
                        }
                    }value[m]=0; i=m;
                }
            }
            sbuf[0]=0; j=0; k=0;
            while(j<i){if(value[j]>0x19){value[k]=value[j]; k++;}else{i--;} j++;} //remove non printables
            value[i]=0; // new termination
            // Revision 2
            if(tag=="CNT") sprintf(sbuf, "Play counter: %s", value);
            if(tag=="COM") sprintf(sbuf, "Comments: %s", value);
            if(tag=="CRA") sprintf(sbuf, "Audio encryption: %s", value);
            if(tag=="CRM") sprintf(sbuf, "Encrypted meta frame: %s", value);
            if(tag=="ETC") sprintf(sbuf, "Event timing codes: %s", value);
            if(tag=="EQU") sprintf(sbuf, "Equalization: %s", value);
            if(tag=="IPL") sprintf(sbuf, "Involved people list: %s", value);
            if(tag=="PIC") sprintf(sbuf, "Attached picture: %s", value);
            if(tag=="SLT") sprintf(sbuf, "Synchronized lyric/text: %s", value);
            if(tag=="TAL") sprintf(sbuf, "Album/Movie/Show title: %s", value);
            if(tag=="TBP") sprintf(sbuf, "BPM (Beats Per Minute): %s", value);
            if(tag=="TCM") sprintf(sbuf, "Composer: %s", value);
            if(tag=="TCO") sprintf(sbuf, "Content type: %s", value);
            if(tag=="TCR") sprintf(sbuf, "Copyright message: %s", value);
            if(tag=="TDA") sprintf(sbuf, "Date: %s", value);
            if(tag=="TDY") sprintf(sbuf, "Playlist delay: %s", value);
            if(tag=="TEN") sprintf(sbuf, "Encoded by: %s", value);
            if(tag=="TFT") sprintf(sbuf, "File type: %s", value);
            if(tag=="TIM") sprintf(sbuf, "Time: %s", value);
            if(tag=="TKE") sprintf(sbuf, "Initial key: %s", value);
            if(tag=="TLA") sprintf(sbuf, "Language(s): %s", value);
            if(tag=="TLE") sprintf(sbuf, "Length: %s", value);
            if(tag=="TMT") sprintf(sbuf, "Media type: %s", value);
            if(tag=="TOA") sprintf(sbuf, "Original artist(s)/performer(s): %s", value);
            if(tag=="TOF") sprintf(sbuf, "Original filename: %s", value);
            if(tag=="TOL") sprintf(sbuf, "Original Lyricist(s)/text writer(s): %s", value);
            if(tag=="TOR") sprintf(sbuf, "Original release year: %s", value);
            if(tag=="TOT") sprintf(sbuf, "Original album/Movie/Show title: %s", value);
            if(tag=="TP1") sprintf(sbuf, "Lead artist(s)/Lead performer(s)/Soloist(s)/Performing group: %s", value);
            if(tag=="TP2") sprintf(sbuf, "Band/Orchestra/Accompaniment: %s", value);
            if(tag=="TP3") sprintf(sbuf, "Conductor/Performer refinement: %s", value);
            if(tag=="TP4") sprintf(sbuf, "Interpreted, remixed, or otherwise modified by: %s", value);
            if(tag=="TPA") sprintf(sbuf, "Part of a set: %s", value);
            if(tag=="TPB") sprintf(sbuf, "Publisher: %s", value);
            if(tag=="TRC") sprintf(sbuf, "ISRC (International Standard Recording Code): %s", value);
            if(tag=="TRD") sprintf(sbuf, "Recording dates: %s", value);
            if(tag=="TRK") sprintf(sbuf, "Track number/Position in set: %s", value);
            if(tag=="TSI") sprintf(sbuf, "Size: %s", value);
            if(tag=="TSS") sprintf(sbuf, "Software/hardware and settings used for encoding: %s", value);
            if(tag=="TT1") sprintf(sbuf, "Content group description: %s", value);
            if(tag=="TT2") sprintf(sbuf, "Title/Songname/Content description: %s", value);
            if(tag=="TT3") sprintf(sbuf, "Subtitle/Description refinement: %s", value);
            if(tag=="TXT") sprintf(sbuf, "Lyricist/text writer: %s", value);
            if(tag=="TXX") sprintf(sbuf, "User defined text information frame: %s", value);
            if(tag=="TYE") sprintf(sbuf, "Year: %s", value);
            if(tag=="UFI") sprintf(sbuf, "Unique file identifier: %s", value);
            if(tag=="ULT") sprintf(sbuf, "Unsychronized lyric/text transcription: %s", value);
            if(tag=="WAF") sprintf(sbuf, "Official audio file webpage: %s", value);
            if(tag=="WAR") sprintf(sbuf, "Official artist/performer webpage: %s", value);
            if(tag=="WAS") sprintf(sbuf, "Official audio source webpage: %s", value);
            if(tag=="WCM") sprintf(sbuf, "Commercial information: %s", value);
            if(tag=="WCP") sprintf(sbuf, "Copyright/Legal information: %s", value);
            if(tag=="WPB") sprintf(sbuf, "Publishers official webpage: %s", value);
            if(tag=="WXX") sprintf(sbuf, "User defined URL link frame: %s", value);
            // Revision 3
            if(tag=="COMM") sprintf(sbuf, "Comment: %s", value);
            if(tag=="OWNE") sprintf(sbuf, "Ownership: %s", value);
            if(tag=="PRIV") sprintf(sbuf, "Private: %s", value);
            if(tag=="SYLT") sprintf(sbuf, "SynLyrics: %s", value);
            if(tag=="TALB") sprintf(sbuf, "Album: %s", value);
            if(tag=="TBPM") sprintf(sbuf, "BeatsPerMinute: %s", value);
            if(tag=="TCMP") sprintf(sbuf, "Compilation: %s", value);
            if(tag=="TCOM") sprintf(sbuf, "Composer: %s", value);
            if(tag=="TCOP") sprintf(sbuf, "Copyright: %s", value);
            if(tag=="TDAT") sprintf(sbuf, "Date: %s", value);
            if(tag=="TEXT") sprintf(sbuf, "Lyricist: %s", value);
            if(tag=="TIME") sprintf(sbuf, "Time: %s", value);
            if(tag=="TIT1") sprintf(sbuf, "Grouping: %s", value);
            if(tag=="TIT2") sprintf(sbuf, "Title: %s", value);
            if(tag=="TIT3") sprintf(sbuf, "Subtitle: %s", value);
            if(tag=="TLAN") sprintf(sbuf, "Language: %s", value);
            if(tag=="TLEN") sprintf(sbuf, "Length: %s", value);
            if(tag=="TMED") sprintf(sbuf, "Media: %s", value);
            if(tag=="TOAL") sprintf(sbuf, "OriginalAlbum: %s", value);
            if(tag=="TOPE") sprintf(sbuf, "OriginalArtist: %s", value);
            if(tag=="TORY") sprintf(sbuf, "OriginalReleaseYear: %s", value);
            if(tag=="TPE1") sprintf(sbuf, "Artist: %s", value);
            if(tag=="TPE2") sprintf(sbuf, "Band: %s", value);
            if(tag=="TPE3") sprintf(sbuf, "Conductor: %s", value);
            if(tag=="TPE4") sprintf(sbuf, "InterpretedBy: %s", value);
            if(tag=="TPOS") sprintf(sbuf, "PartOfSet: %s", value);
            if(tag=="TPUB") sprintf(sbuf, "Publisher: %s", value);
            if(tag=="TRCK") sprintf(sbuf, "Track: %s", value);
            if(tag=="TRDA") sprintf(sbuf, "RecordingDates: %s", value);
            if(tag=="TXXX") sprintf(sbuf, "UserDefinedText: %s", value);
            if(tag=="TYER") sprintf(sbuf, "Year: %s", value);
            if(tag=="USER") sprintf(sbuf, "TermsOfUse: %s", value);
            if(tag=="USLT") sprintf(sbuf, "Lyrics: %s", value);
            if(tag=="XDOR") sprintf(sbuf, "OriginalReleaseTime: %s", value);
            if(sbuf[0]!=0) if(vs1053_id3data) vs1053_id3data(sbuf);
        }
    } while (m_id3Size > 0);
}
//---------------------------------------------------------------------------------------------------------------------
uint32_t VS1053::getFileSize(){
    if (!mp3file) return 0;
    return mp3file.size();
}
//---------------------------------------------------------------------------------------------------------------------
uint32_t VS1053::getFilePos(){
    if (!mp3file) return 0;
    return mp3file.position();
}
//---------------------------------------------------------------------------------------------------------------------
bool VS1053::setFilePos(uint32_t pos){
    if (!mp3file) return false;
    return mp3file.seek(pos);
}
//---------------------------------------------------------------------------------------------------------------------
long long int VS1053::XL (long long int a, const char* b) {
  int len = strlen(b);
  for (int c = 0; c < len - 2; c += 3) {
    int  d = (long long int)b[c + 2];
    d = d >= 97 ? d - 87 : d - 48;
    d = (b[c + 1] == '+' ? a >> d : a << d);
    a = b[c] == '+' ? (a + d) & 4294967295 : a ^ d;
  }
  return a;
}
//---------------------------------------------------------------------------------------------------------------------
char* VS1053::lltoa(long long val, int base){

    static char buf[64] = {0};
    static char chn=0;
    int i = 62;
    int sign = (val < 0);
    if(sign) val = -val;

    if(val == 0) return &chn;

    for(; val && i ; --i, val /= base) {
        buf[i] = "0123456789abcdef"[val % base];
    }

    if(sign) {
        buf[i--] = '-';
    }
    return &buf[i+1];
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
