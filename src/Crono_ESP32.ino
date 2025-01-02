/*
                Cronómetro MM:SS,d >>> 4x Led Display Control Modules 8x8 MAX7219 
                      https://es.aliexpress.com/item/33038259447.html
                 
                                 --- oOo ---
                                
                              J_RPM >>> http://j-rpm.com/
                            https://www.youtube.com/c/JRPM
                                        (v1.0)
                              >>> Noviembre 2024 <<<
                  
                              >>> HARDWARE <<<
                  LIVE D1 mini ESP32 ESP-32 WiFi + Bluetooth
                https://es.aliexpress.com/item/32816065152.html
                
             4x Led Display Control Modules 8x8 MAX7219 (ROTATE 0)
                https://es.aliexpress.com/item/33038259447.html

            8x8 MAX7219 Led Control Module, 4 in one screen  (ROTATE 90)
               https://es.aliexpress.com/item/4001296969309.html

                           >>> IDE Arduino <<<
                        Model: WEMOS MINI D1 ESP32
       Add URLs: https://dl.espressif.com/dl/package_esp32_index.json
                  https://github.com/espressif/arduino-esp32
                                
 ____________________________________________________________________________________
*/
String HWversion = "(v1.0)"; 
#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <time.h>
///////////////////////////////////////////////////////////
hw_timer_t *timer = NULL;
volatile bool has_expired = false;
bool Mode_CRONO = true;
bool Crono = true;
int Conta = 0;
int Minuto = 0;
int Segundo = 0;
int Decima = 0;
int Centesima = 0;
const int unidadCRONO = 100; // 100 = centesimas, 10 =decimas
const int tiempoRESET = unidadCRONO; 

////////////////////////// MATRIX //////////////////////////////////////////////
#define MAX_DIGITS 20
bool _scroll = false;
bool display_date = false;
bool animated_time = true;
bool show_seconds = true;
byte dig[MAX_DIGITS]={0};
byte digold[MAX_DIGITS]={0};
byte digtrans[MAX_DIGITS]={0};
int dots = 1;
long dotTime = 0;
long clkTime = 0;
int dx=0;
int dy=0;
byte del=0;
int dualChar = 0;
int brightness = 1;  //DUTY CYCLE: 11/32
int h,m,s,d;
String mDay;
long timeConnect;

// Turn on debug statements to the serial output
#define DEBUG  1
#if  DEBUG
#define PRINT(s, x) { Serial.print(F(s)); Serial.print(x); }
#define PRINTS(x) Serial.print(F(x))
#define PRINTX(x) Serial.println(x, HEX)
#else
#define PRINT(s, x)
#define PRINTS(x)
#define PRINTX(x)
#endif

// ESP32 -> Matrix -> ROTATE 90
#define DIN_PIN 12 //32   // ROTATE 90
#define CS_PIN  13 //25   // ROTATE 90   
#define CLK_PIN 14 //27   // ROTATE 90

const int pinPulsa = 4;//14;                                  // Pin GPIO donde está conectado el pulsador
const int pinPulsaFin = 15;//14;                                  // Pin GPIO donde está conectado el pulsador

// ESP32 -> Matrix -> ROTATE 0
//#define DIN_PIN 27   //ROTATE 0
//#define CS_PIN  25   //ROTATE 0 
//#define CLK_PIN 32   //ROTATE 0

#define NUM_MAX 4
#include "max7219.h"
#include "fonts_es.h"
#include <DNSServer.h>
#include <WiFiManager.h>  //https://github.com/tzapu/WiFiManager
WebServer server2(80); 

// Global message buffers shared by Wifi and Scrolling functions
#define BUF_SIZE  512
char curMessage[BUF_SIZE];

const char WebResponse[] = "HTTP/1.1 200 OK\nContent-Type: text/html\r\n";

// Define the number of bytes you want to access (first is index 0)
#define EEPROM_SIZE 14

// Size of buffer used to capture HTTP requests
#define REQ_BUF_SZ   60
char HTTP_req[REQ_BUF_SZ] = {0}; // buffered HTTP request stored as null terminated string

String CurrentTime, CurrentDate, nDay, webpage = "";
bool display_EU = true;
String zone1= "España";
String zone2= "Japan";
bool T_Zone2 = false;
int matrix_speed = 25;

///////////////////////////////////////////////////////////
void IRAM_ATTR timerInterrupcion() {
 has_expired = true;
}
///////////////////////////////////////////////////////////
void setup(){
  Serial.begin(115200);
  timeConnect = millis();

  // Load configuration values from EEPROM memory
  //readConfig();

  //pinMode(LED_BUILTIN, OUTPUT);
  pinMode(pinPulsa, INPUT_PULLUP);                        // Configurar el pin como entrada con resistencia Pull-Up interna
  pinMode(pinPulsaFin, INPUT_PULLUP);                        // Configurar el pin como entrada con resistencia Pull-Up interna

  timer = timerBegin(0, 80, true);                        // Timer 0, divisor de reloj 80
  timerAttachInterrupt(timer, &timerInterrupcion, true);  // Adjuntar la función de manejo de interrupción
  timerAlarmWrite(timer, 1000000/unidadCRONO, true);      // Interrupción cada unidadCRONO segundo
  timerAlarmEnable(timer);                                // Habilitar el Timer
  
  initMAX7219();
  sendCmdAll(CMD_SHUTDOWN,1);
  sendCmdAll(CMD_INTENSITY,brightness);
  sendCmdAll(CMD_DISPLAYTEST, 0);

  //String msg = "  CRONO & NTP "; 
  String msg = "  Colegio";
  /* if (T_Zone2==false) {
    msg = msg + zone1;    
  }else{
    msg = msg + zone2;    
  }
  msg = msg + "  " + HWversion; */
  printStringWithShift(msg.c_str(), matrix_speed);
  delay(1000);

  // Si está pulsado, NO WiFi y entra CRONO
  /* if(digitalRead(pinPulsa) == LOW) {
    reiniciaCrono();
    Mode_CRONO = true;
    PRINTS("\n>>> Modo: CRONO <<<");
  }else {
    Mode_CRONO = false;
    PRINTS("\n>>> Modo: RELOJ <<<");
  } */
  reiniciaCrono();
  Mode_CRONO = false;
  PRINTS("\n>>> Modo: CRONO <<<");
  PRINTS("\n");


  //--------------------------------------------
  // Busca WiFi, asigna IP y sincroniza el RELOJ
  //--------------------------------------------
  //WiFiManager intialisation. Once completed there is no need to repeat the process on the current board
  WiFiManager wifiManager;

  // Salta si está en modo CRONO
  if (Mode_CRONO == false) {
    display_AP_wifi();
  
    // A new OOB ESP32 will have no credentials, so will connect and not need this to be uncommented and compiled in, a used one will, try it to see how it works
    // Uncomment the next line for a new device or one that has not connected to your Wi-Fi before or you want to reset the Wi-Fi connection
    // wifiManager.resetSettings();
    // Then restart the ESP32 and connect your PC to the wireless access point called 'ESP32_AP' or whatever you call it below
    // Next connect to http://192.168.4.1/ and follow instructions to make the WiFi connection
  
    // Set a timeout until configuration is turned off, useful to retry or go to sleep in n-seconds
    wifiManager.setTimeout(180);
    
    //fetches ssid and password and tries to connect, if connections succeeds it starts an access point with the name called "ESP32_AP" and waits in a blocking loop for configuration
    if (!wifiManager.autoConnect("ESP32_AP")) {
      PRINTS("\nFailed to connect and timeout occurred");
      display_AP_wifi();
      display_flash(false);
      //reset_ESP32();
      Mode_CRONO = true;
      PRINTS("\n>>> Modo: CRONO <<<");
    }
    // At this stage the WiFi manager will have successfully connected to a network,
    // or if not will try again in 180-seconds
    //---------------------------------------------------------
    // 
    PRINT("\n>>> Connection Delay(ms): ",millis()-timeConnect);
    
    // Print the IP address
    PRINT("\nUse this URL to connect -> http://",WiFi.localIP());
    PRINTS("/");
    display_ip();
  
    // Syncronize Time and Date
    SetupTime();
    
    // Test WEB
    checkServer();
    curMessage[0] = '\0';
   
    // Set up first message 
    String stringMsg = "Crono_ESP32 " + HWversion + " - RTC: ";
    if (T_Zone2 == false) {
      stringMsg = stringMsg + zone1;    
    }else{
      stringMsg = stringMsg + zone2;    
    }
    stringMsg = stringMsg + " - IP: " + WiFi.localIP().toString() + "\n";
    
    stringMsg.toCharArray(curMessage, BUF_SIZE);
    PRINT("\ncurMessage >>> ", curMessage);
  }

  //-----------------------------------------------
  // Comprueba que el pulsador NO esté presionado
  //-----------------------------------------------
  do {delay(50);
  } while(digitalRead(pinPulsa) == LOW);

  delay(400);
}
///////////////////////////////////////////////////////////
void loop(){
  // Wait for a client to connect and when they do process their requests
  server2.handleClient();
  
  // Selecciona el modo: CRONO/TIME
  if (Mode_CRONO == true) {
    if(has_expired){
       // Tareas a realizar cuando se activa la interrupción del Timer
      testPulsa();
      if (Crono == true) {
        //digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN)); // Alternar el estado del LED
        incrementa();
      }
      ponCrono();
      has_expired = false; 
    }
  }else {
    // Update and refresh of the date and time on the displays
    if (millis() % 60000) UpdateLocalTime();
  
    // Serial display time and date & dots flashing
    if(millis()-dotTime > 500) {
      //PRINT("\n",CurrentTime);
      //PRINT("\n",mDay);
      dotTime = millis();
      dots = !dots;
    }

    ponHora();
    //digitalWrite(LED_BUILTIN, false); // Apaga el LED

    // Show date on matrix display
    if (display_date == true) {
      if(millis()-clkTime > 30000 && !del && dots) { // clock for 30s, then scrolls Date 
        _scroll=true;
        printStringWithShift((String("     ")+ mDay + "           ").c_str(), matrix_speed);
        clkTime = millis();
        _scroll=false;
      }
    }

    // Comprueba si cambia a modo: CRONO
    /* if(digitalRead(pinPulsa) == LOW) {
      reiniciaCrono();
      Mode_CRONO = true;
      do {
        ponCrono();
        delay(1000);
      } while(digitalRead(pinPulsa) == LOW);
    } */
  }
}
///////////////////////////////////////////////////////////
void testPulsa(){
  static bool Cambio = false;

  if(digitalRead(pinPulsa) == LOW) {
    Conta++;

    // STOP >1 Segundo = RESET
    if (Conta > tiempoRESET && Crono == false) {
      reiniciaCrono();
  
    }else if (Cambio == false) {
      Cambio = true;
      if (Crono == false) {
        Crono = true;
        Conta = 0;
      }else {
        Crono = false;
        //digitalWrite(LED_BUILTIN, false); // Apaga el LED
      }
    }

  // NO pulsado
  }else {
    Cambio = false;
    Conta = 0;
  }


  if(digitalRead(pinPulsaFin) == LOW) {
          if (Crono == true) {
            Crono = false;
            //digitalWrite(LED_BUILTIN, false); // Apaga el LED
          }
  }

}
///////////////////////////////////////////////////////////
void reiniciaCrono() {
  Crono = false;
  has_expired = false; 
  //digitalWrite(LED_BUILTIN, false); // Apaga el LED
  Conta = 0;
  Minuto = 0;
  Segundo = 0;
  Decima = 0;
  Centesima = 0;
  ponCrono();
}
///////////////////////////////////////////////////////////
void incrementa(){
  Centesima++;
  if (Centesima < 10) return;
  Centesima = 0;

  Decima++;
  if (Decima > 9) {
    Decima = 0;
    Segundo++;
    if (Segundo > 59) {
      Segundo = 0;
      Minuto++;
        if (Minuto > 99) {
          Minuto = 0;
        }
      }
    }
}
///////////////////////////////////////////////////////////
void ponCrono() {
  if(/* Segundo==0&& */Decima==0&&Centesima==0){
    Serial.print (format2(Minuto));
    Serial.print (F(":"));
    Serial.print (format2(Segundo));
    Serial.print (F(","));
    Serial.print(Decima);
    Serial.println(Centesima);
  }
  matrix_crono();

}
///////////////////////////////////////////////////////////
void ponHora() {
  // Load Time 
  h = (CurrentTime.substring(0,2)).toInt();
  m = (CurrentTime.substring(3,5)).toInt();
  s = (CurrentTime.substring(6,8)).toInt();
  matrix_time();
}
//////////////////////////////////////////////////////////////////////////////
const String format2(int num){
  String f2 = String(num);
  f2 = "0" + f2;
  f2 = f2.substring(f2.length() - 2, f2.length());
  return f2;
}
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void showDigit(char ch, int col, const uint8_t *data){
  if(dy<-8 | dy>8) return;
  int len = pgm_read_byte(data);
  int w = pgm_read_byte(data + 1 + ch * len);
  col += dx;
  for (int i = 0; i < w; i++)
    if(col+i>=0 && col+i<8*NUM_MAX) {
      byte v = pgm_read_byte(data + 1 + ch * len + 1 + i);
      if(!dy) scr[col + i] = v; else scr[col + i] |= dy>0 ? v>>dy : v<<-dy;
    }
}
//////////////////////////////////////////////////////////////////////////////
void setCol(int col, byte v){
  if(dy<-8 | dy>8) return;
  col += dx;
  if(col>=0 && col<8*NUM_MAX)
    if(!dy) scr[col] = v; else scr[col] |= dy>0 ? v>>dy : v<<-dy;
}
//////////////////////////////////////////////////////////////////////////////
int showChar(char ch, const uint8_t *data){
  int len = pgm_read_byte(data);
  int i,w = pgm_read_byte(data + 1 + ch * len);
  for (i = 0; i < w; i++)
    scr[NUM_MAX*8 + i] = pgm_read_byte(data + 1 + ch * len + 1 + i);
  scr[NUM_MAX*8 + i] = 0;
  return w;
}
//////////////////////////////////////////////////////////////////////////////
void showAnimSecClock(){
  byte digPos[6]={0,5,11,16,23,28};
  int digHt = 12;
  int num = 6; 
  int i;
  if(del==0) {
    del = digHt;
    for(i=0; i<num; i++) digold[i] = dig[i];
    dig[0] = h/10 ? h/10 : 10;
    dig[1] = h%10;
    dig[2] = m/10;
    dig[3] = m%10;
    dig[4] = s/10;
    dig[5] = s%10;
    for(i=0; i<num; i++)  digtrans[i] = (dig[i]==digold[i]) ? 0 : digHt;
  } else
    del--;
  
  clr();
  for(i=0; i<num; i++) {
    if(digtrans[i]==0) {
      dy=0;
      showDigit(dig[i], digPos[i], dig4x8r);
    } else {
      dy = digHt-digtrans[i];
      showDigit(digold[i], digPos[i], dig4x8r);
      dy = -digtrans[i];
      showDigit(dig[i], digPos[i], dig4x8r);
      digtrans[i]--;
    }
  }
  dy=0;
  setCol(9,1 ? B00100100 : 0);
  setCol(21,1 ? B00100100 : 0);
  refreshAll();
  server2.handleClient();
}
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
unsigned char convertSpanish(unsigned char _c){
  unsigned char c = _c;
  if(c==195) {
    dualChar = c;
    return 0;
  }
  // UTF8
  if(dualChar) {
    switch(_c) {
      case 161: c = 1+'~'; break;   // 'á'
      case 169: c = 2+'~'; break;   // 'é'
      case 173: c = 3+'~'; break;   // 'í'
      case 179: c = 4+'~'; break;   // 'ó'
      case 186: c = 5+'~'; break;   // 'ú'
      case 188: c = 6+'~'; break;   // 'ü'
      case 156: c = 7+'~'; break;   // 'Ü'
      case 177: c = 8+'~'; break;   // 'ñ'
      case 145: c = 9+'~'; break;   // 'Ñ'
      case 167: c = 10+'~'; break;  // 'ç'
      case 135: c = 11+'~'; break;  // 'Ç'
      case 129: c = 12+'~'; break;  // 'Á'
      case 137: c = 13+'~'; break;  // 'É'
      case 141: c = 14+'~'; break;  // 'Í'
      case 147: c = 15+'~'; break;  // 'Ó'
      case 154: c = 16+'~'; break;  // 'Ú'
      default:  break;
    }
    dualChar = 0;
    return c;
  }    
  // ANSI
  switch(_c) {
    case 225: c = 1+'~'; break;   // 'á'
    case 233: c = 2+'~'; break;   // 'é'
    case 237: c = 3+'~'; break;   // 'í'
    case 243: c = 4+'~'; break;   // 'ó'
    case 250: c = 5+'~'; break;   // 'ú'
    case 252: c = 6+'~'; break;   // 'ü'
    case 220: c = 7+'~'; break;   // 'Ü'
    case 241: c = 8+'~'; break;   // 'ñ'
    case 209: c = 9+'~'; break;   // 'Ñ'
    case 231: c = 10+'~'; break;  // 'ç'
    case 199: c = 11+'~'; break;  // 'Ç'
    case 193: c = 12+'~'; break;  // 'Á'
    case 201: c = 13+'~'; break;  // 'É'
    case 205: c = 14+'~'; break;  // 'Í'
    case 211: c = 15+'~'; break;  // 'Ó'
    case 218: c = 16+'~'; break;  // 'Ú'
    default:  break;
  }
  return c;
}
//////////////////////////////////////////////////////////////////////////////
void printCharWithShift(unsigned char c, int shiftDelay) {
  // To check WiFi inputs faster 
  shiftDelay = shiftDelay / 4;
  
  c = convertSpanish(c);
  if (c < ' ' || c > '~'+23) return;
  c -= 32;
  int w = showChar(c, font);
  for (int i=0; i<w+1; i++) {
    server2.handleClient();
    delay(shiftDelay);
    server2.handleClient();
    delay(shiftDelay);
    server2.handleClient();
    delay(shiftDelay);
    server2.handleClient();
    delay(shiftDelay);
    scrollLeft();
    refreshAll();
  }
}
//////////////////////////////////////////////////////////////////////////////
void printStringWithShift(const char* s, int shiftDelay){
  while (*s) {
    printCharWithShift(*s, shiftDelay);
    s++;
  }
}
//////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
const char *err2Str(wl_status_t code){
  switch (code){
  case WL_IDLE_STATUS:    return("IDLE");           break; // WiFi is in process of changing between statuses
  case WL_NO_SSID_AVAIL:  return("NO_SSID_AVAIL");  break; // case configured SSID cannot be reached
  case WL_CONNECTED:      return("CONNECTED");      break; // successful connection is established
  case WL_CONNECT_FAILED: return("PASSWORD_ERROR"); break; // password is incorrect
  case WL_DISCONNECTED:   return("CONNECT_FAILED"); break; // module is not configured in station mode
  default: return("??");
  }
}
/////////////////////////////////////////////////////////////////
uint8_t htoi(char c) {
  c = toupper(c);
  if ((c >= '0') && (c <= '9')) return(c - '0');
  if ((c >= 'A') && (c <= 'F')) return(c - 'A' + 0xa);
  return(0);
}
/////////////////////////////////////////////////////////////////
////////////////////////////////////////////////
// Check clock config
////////////////////////////////////////////////
void _display_crono_start() {
  Crono = true;
  PRINTS("\n-> DISPLAY_CRONO_START");
  responseWeb();
}
/////////////////////////////////////////////////////////////////
void _display_crono_stop() {
  Crono = false;
  //digitalWrite(LED_BUILTIN, false); // Apaga el LED
  PRINTS("\n-> DISPLAY_CRONO_STOP");
  responseWeb();
}
/////////////////////////////////////////////////////////////////
void _display_crono_init() {
  reiniciaCrono();
  PRINTS("\n-> DISPLAY_CRONO_INIT");
  responseWeb();
}
/////////////////////////////////////////////////////////////////
void _display_mode_time() {
  Mode_CRONO = false;
  PRINTS("\n-> DISPLAY_MODE_TIME");
  responseWeb();
}
/////////////////////////////////////////////////////////////////
void _display_mode_crono() {
  reiniciaCrono();
  Mode_CRONO = true;
  PRINTS("\n-> DISPLAY_MODE_CRONO");
  responseWeb();
}
/////////////////////////////////////////////////////////////////
void _display_mode_usa() {
  display_EU = false;
  PRINTS("\n-> DISPLAY_MODE_USA");
  display_mode_toggle();
  responseWeb();
}
/////////////////////////////////////////////////////////////////
void _display_mode_eu() {
  display_EU = true;
  PRINTS("\n-> DISPLAY_MODE_EU");
  display_mode_toggle();
  responseWeb();
}
/////////////////////////////////////////////////////////////////
void _time_anim() {
  animated_time = true;
  PRINTS("\n-> TIME_ANIM");
  display_time_mode();
  responseWeb();
}
/////////////////////////////////////////////////////////////////
void _time_normal() {
  animated_time = false;
  PRINTS("\n-> TIME_NORMAL");
  display_time_mode();
  responseWeb();
}
/////////////////////////////////////////////////////////////////
void _time_minute() {
  show_seconds = false;
  PRINTS("\n-> TIME_MINUTE");
  display_time_view();
  responseWeb();
}
/////////////////////////////////////////////////////////////////
void _time_second() {
  show_seconds = true;
  PRINTS("\n-> TIME_SECOND");
  display_time_view();
  responseWeb();
}
/////////////////////////////////////////////////////////////////
void _display_date() {
  display_date = true;
  PRINTS("\n-> DISPLAY_DATE");
  display_date_toggle();
  responseWeb();
}
/////////////////////////////////////////////////////////////////
void _display_no_date() {
  display_date = false;
  PRINTS("\n-> DISPLAY_NO_DATE");
  display_date_toggle();
  responseWeb();
}
/////////////////////////////////////////////////////////////////
void _bright_0() {
  PRINTS("\n-> BRIGHT=0");
  brightness = 0;     //DUTY CYCLE: 1/32 (MIN) 
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_1() {
  PRINTS("\n-> BRIGHT=1");
  brightness = 1;    
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_2() {
  PRINTS("\n-> BRIGHT=2");
  brightness = 2;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_3() {
  PRINTS("\n-> BRIGHT=3");
  brightness = 3;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_4() {
  PRINTS("\n-> BRIGHT=4");
  brightness = 4;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_5() {
  PRINTS("\n-> BRIGHT=5");
  brightness = 5;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_6() {
  PRINTS("\n-> BRIGHT=6");
  brightness = 6;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_7() {
  PRINTS("\n-> BRIGHT=7");
  brightness = 7;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_8() {
  PRINTS("\n-> BRIGHT=8");
  brightness = 8;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_9() {
  PRINTS("\n-> BRIGHT=9");
  brightness = 9;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_10() {
  PRINTS("\n-> BRIGHT=10");
  brightness = 10;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_11() {
  PRINTS("\n-> BRIGHT=11");
  brightness = 11;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_12() {
  PRINTS("\n-> BRIGHT=12");
  brightness = 12;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_13() {
  PRINTS("\n-> BRIGHT=13");
  brightness = 13;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_14() {
  PRINTS("\n-> BRIGHT=14");
  brightness = 14;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_15() {
  PRINTS("\n-> BRIGHT=15");
  brightness = 15;     //DUTY CYCLE: 31/32 (MAX)
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _save_bright(){
  brightness_matrix();
  responseWeb();
}
/////////////////////////////////////////////////////////////////
void _restart_1() {
  T_Zone2=false;
  PRINT("\n>>> SYNC Time: ",zone1);
  set_Zone2();
  _restart();
}
/////////////////////////////////////////////////////////////////
void _restart_2() {
  T_Zone2=true;
  PRINT("\n>>> SYNC Time: ",zone2);
  set_Zone2();
  _restart();
}
/////////////////////////////////////////////////////////////////
void _restart() {
  PRINTS("\n-> RESTART");
  web_reset_ESP32();
}
/////////////////////////////////////////////////////////////////
void _reset_wifi() {
  PRINTS("\n-> RESET_WIFI");
  reset_wifi();
}
/////////////////////////////////////////////////////////////////
void _home() {
  PRINTS("\n-> HOME");
  responseWeb();
}
/////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void display_mode_toggle() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(0, display_EU);
  end_Eprom();
}
//////////////////////////////////////////////////////////////
void display_date_toggle() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(1, display_date);
  end_Eprom();
}
//////////////////////////////////////////////////////////////////////////////
void brightness_matrix() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(2, brightness);
  sendCmdAll(CMD_INTENSITY,brightness);
  end_Eprom();
}
//////////////////////////////////////////////////////////////
void display_time_mode() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(3, animated_time);
  end_Eprom();
}
//////////////////////////////////////////////////////////////
void display_time_view() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(4, show_seconds);
  end_Eprom();
}
//////////////////////////////////////////////////////////////
void display_matrix_speed() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(5, matrix_speed);
  end_Eprom();
}
//////////////////////////////////////////////////////////////
void set_Zone2() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(13, T_Zone2);
  end_Eprom();
}
//////////////////////////////////////////////////////////////
void end_Eprom() {
  del=0;
  dots=1;
  EEPROM.commit();
  EEPROM.end();
}
//////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
void responseWeb(){
  PRINTS("\nS_RESPONSE");
  NTP_Clock_home_page();
}
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
void checkServer(){
  server2.begin();  // Start the WebServer
  PRINTS("\nWebServer started");
  ////////////////////////////////////////////////////////////////////////
  // Define what happens when a client requests attention
  ////////////////////////////////////////////////////////////////////////
  server2.on("/", _home);
  server2.on("/CRONO_START", _display_crono_start); 
  server2.on("/CRONO_STOP", _display_crono_stop); 
  server2.on("/CRONO_INIT", _display_crono_init); 
  server2.on("/TIME", _display_mode_time); 
  server2.on("/CRONO", _display_mode_crono); 
  server2.on("/DISPLAY_MODE_USA", _display_mode_usa); 
  server2.on("/DISPLAY_MODE_EU", _display_mode_eu); 
  server2.on("/TIME_ANIM", _time_anim); 
  server2.on("/TIME_NORMAL", _time_normal); 
  server2.on("/TIME_MINUTE", _time_minute); 
  server2.on("/TIME_SECOND", _time_second); 
  server2.on("/DISPLAY_DATE", _display_date);
  server2.on("/DISPLAY_NO_DATE", _display_no_date);
  server2.on("/BRIGHT=0", _bright_0); 
  server2.on("/BRIGHT=1", _bright_1); 
  server2.on("/BRIGHT=2", _bright_2); 
  server2.on("/BRIGHT=3", _bright_3); 
  server2.on("/BRIGHT=4", _bright_4); 
  server2.on("/BRIGHT=5", _bright_5); 
  server2.on("/BRIGHT=5", _bright_6); 
  server2.on("/BRIGHT=7", _bright_6); 
  server2.on("/BRIGHT=8", _bright_8); 
  server2.on("/BRIGHT=9", _bright_9); 
  server2.on("/BRIGHT=10", _bright_10); 
  server2.on("/BRIGHT=11", _bright_11); 
  server2.on("/BRIGHT=12", _bright_12); 
  server2.on("/BRIGHT=13", _bright_13); 
  server2.on("/BRIGHT=14", _bright_14); 
  server2.on("/BRIGHT=15", _bright_15); 
  server2.on("/HOME", _home);
  server2.on("/RESTART_1", _restart_1);  
  server2.on("/RESTART_2", _restart_2);  
  server2.on("/RESET_WIFI", _reset_wifi);
}
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////
void matrix_time() {
  if (show_seconds == true) {
    if (animated_time == true) {
      showAnimSecClock(); //With ROTATE 90 send 1 times + delay(4)
      delay(4);
      //showAnimSecClock(); //With ROTATE 0 send 4 times, without delay
      //showAnimSecClock();
      //showAnimSecClock();
    }else {
      showSecondsClock();
    }
  } else {
    if (animated_time == true) {
      showAnimClock();  //With ROTATE 90 send 1 times + delay(4)
      delay(4);
      //showAnimClock();  //With ROTATE 0 send 4 times, without delay
      //showAnimClock();
      //showAnimClock();
    }else {
      showSimpleClock();
    }
  }
}
//////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////
void matrix_crono() {
  dx=dy=0;
  clr();
  int col = 0;
  showDigit(Minuto/10,  col, dig4x8r);
  col+=5;
  showDigit(Minuto%10,  col, dig4x8r);
  col+=7;
  showDigit(Segundo/10, col, dig4x8r);
  col+=5;
  showDigit(Segundo%10, col, dig4x8r);
  col+=6;
  showDigit(Decima%10, col, dig4x8r);
  col+=5;
  showDigit(Centesima%10, col, dig4x8r);
  setCol(10,1 ? B00100100 : 0);
  setCol(22,1 ? B10000000 : 0);
  refreshAll();
}
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void showSecondsClock(){
  dx=dy=0;
  clr();
   if(h/10==0) {
    showDigit(10,  0, dig4x8r);
   }else {
    showDigit(h/10,  0, dig4x8r);
   }
  showDigit(h%10,  5, dig4x8r);
  showDigit(m/10, 11, dig4x8r);
  showDigit(m%10, 16, dig4x8r);
  showDigit(s/10, 23, dig4x8r);
  showDigit(s%10, 28, dig4x8r);
  setCol(9,1 ? B00100100 : 0);
  setCol(21,1 ? B00100100 : 0);
  refreshAll();
}
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void showSimpleClock(){
  dx=dy=0;
  clr();
   if(h/10==0) {
    showDigit(10,  0, dig6x8);
   }else {
    showDigit(h/10,  0, dig6x8);
   }
  showDigit(h%10,  8, dig6x8);
  showDigit(m/10, 17, dig6x8);
  showDigit(m%10, 25, dig6x8);
  showDigit(s/10, 34, dig6x8);
  showDigit(s%10, 42, dig6x8);
  setCol(15,dots ? B00100100 : 0);
  setCol(32,dots ? B00100100 : 0);
  refreshAll();
}
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void showAnimClock(){
  byte digPos[6]={0,8,17,25,34,42};
  int digHt = 12;
  int num = 6; 
  int i;
  if(del==0) {
    del = digHt;
    for(i=0; i<num; i++) digold[i] = dig[i];
    dig[0] = h/10 ? h/10 : 10;
    dig[1] = h%10;
    dig[2] = m/10;
    dig[3] = m%10;
    dig[4] = s/10;
    dig[5] = s%10;
    for(i=0; i<num; i++)  digtrans[i] = (dig[i]==digold[i]) ? 0 : digHt;
  } else
    del--;
  
  clr();
  for(i=0; i<num; i++) {
    if(digtrans[i]==0) {
      dy=0;
      showDigit(dig[i], digPos[i], dig6x8);
    } else {
      dy = digHt-digtrans[i];
      showDigit(digold[i], digPos[i], dig6x8);
      dy = -digtrans[i];
      showDigit(dig[i], digPos[i], dig6x8);
      digtrans[i]--;
    }
  }
  dy=0;
  setCol(15,dots ? B00100100 : 0);
  setCol(32,dots ? B00100100 : 0);
  refreshAll();
  server2.handleClient();
}
///////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Choose your time zone from: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv 
// See below for examples
// Or, choose a time server close to you, but in most cases it's best to use pool.ntp.org to find an NTP server
// then the NTP system decides e.g. 0.pool.ntp.org, 1.pool.ntp.org as the NTP syem tries to find  the closest available servers
// EU "0.europe.pool.ntp.org"
// US "0.north-america.pool.ntp.org"
// See: https://www.ntppool.org/en/                                                           
// UK normal time is GMT, so GMT Offset is 0, for US (-5Hrs) is typically -18000, AU is typically (+8hrs) 28800
// In the UK DST is +1hr or 3600-secs, other countries may use 2hrs 7200 or 30-mins 1800 or 5.5hrs 19800 Ahead of GMT use + offset behind - offset
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
boolean SetupTime() {
  char* Timezone;
  char* ntpServer;
  int gmtOffset_sec;
  int daylightOffset_sec;
  
  // Select Time Zone (Spain/Japan)
  if (T_Zone2 == false) {
    Timezone= "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00"; 
    ntpServer= "es.pool.ntp.org";
    gmtOffset_sec= 0;
    daylightOffset_sec= 7200;
  }else{
    Timezone= "UTC-9";  
    ntpServer= "ntp.nict.jp"; 
    gmtOffset_sec= 0;
    daylightOffset_sec= 0;
  }
  
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer, "time.nist.gov");  // configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  setenv("TZ", Timezone, 1);                                            // setenv()adds the "TZ" variable to the environment with a value TimeZone, only used if set to 1, 0 means no change
  tzset();                                                                    // Set the TZ environment variable
  delay(1000);
  bool TimeStatus = UpdateLocalTime();
  return TimeStatus;
}
//////////////////////////////////////////////////////////////////////////////
boolean UpdateLocalTime() {
  struct tm timeinfo;
  time_t now;
  time(&now);

  //See http://www.cplusplus.com/reference/ctime/strftime/
  // %w >>> Weekday as a decimal number with Sunday as 0 (0-6)
  String esWDay[7] = {"Domingo","Lunes","Martes","Miércoles","Jueves","Viernes","Sábado"};
  String esMonth[13] = {"Enero","Febrero","Marzo","Abril","Mayo","Junio","Julio","Agosto","Septiembre","Octubre","Noviembre","Diciembre"};
  String esDate;
  char output[30];
  
  if (T_Zone2 == false && display_EU == true) {
    strftime(output, 30, "%w", localtime(&now));
    mDay = esWDay[atoi(output)];
  }else {
    strftime(output, 30, "%A", localtime(&now));
    mDay = output;
  }
  strftime(output, 30, "%a. ", localtime(&now));
  nDay = output; 
  
  if (display_EU == true) {
    strftime(output, 30,"%d-%m", localtime(&now));
    CurrentDate = nDay + output;
    //%m  Month as a decimal number (01-12)
    if (T_Zone2 == false) {
      strftime(output, 30,", %d", localtime(&now));
      esDate = mDay + output;
      strftime(output, 30,"%m", localtime(&now));
      mDay = esDate + " de " + esMonth[atoi(output)-1];
      strftime(output, 30," de %Y", localtime(&now));
    }else {
      strftime(output, 30,", %d %B %Y", localtime(&now));
    }
    mDay = mDay + output;
    strftime(output, 30, "%H:%M:%S", localtime(&now));
    CurrentTime = output;
  }
  else { 
    strftime(output, 30, "%m-%d", localtime(&now));
    CurrentDate = nDay + output;
    strftime(output, 30, ", %B %d, %Y", localtime(&now));
    mDay = mDay + output;
    strftime(output, 30, "%r", localtime(&now));
    CurrentTime = output;
  }
  return true;
}
/////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
void display_flash(bool tst) {
  for (int i=0; i<8; i++) {
    if (tst==true)sendCmdAll(CMD_DISPLAYTEST, 1);
    sendCmdAll(CMD_SHUTDOWN,0);
    delay (50);
    sendCmdAll(CMD_DISPLAYTEST, 0);
    sendCmdAll(CMD_SHUTDOWN,1);
    delay (50);
  }
}
//////////////////////////////////////////////////////////////
void display_ip() {
  // Print the IP address MATRIX
  printStringWithShift((String("  http:// ")+ WiFi.localIP().toString()).c_str(), matrix_speed);
  display_flash(false);
}
//////////////////////////////////////////////////////////////
void display_AP_wifi() {
  // Print the IP address MATRIX
  printStringWithShift((String("  ESP32_AP: 192.168.4.1")).c_str(), matrix_speed);
  display_flash(false);
}
//////////////////////////////////////////////////////////////
void reset_ESP32() {
  sendCmdAll(CMD_SHUTDOWN,0);
  ESP.restart();
  delay(5000);
}
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void readConfig(){
  // Initialize EEPROM with predefined size
  EEPROM.begin(EEPROM_SIZE);

  // 0 - Display Status 
  display_EU = EEPROM.read(0);
  PRINT("\ndisplay_EU: ",display_EU);

  // 1 - Display Date  
  display_date = EEPROM.read(1);
  PRINT("\ndisplay_date: ",display_date);
  
  // 2 - Matrix Brightness  
  brightness = EEPROM.read(2);
  if (brightness < 0 || brightness > 15) {
    brightness = 5;
    EEPROM.write(2, brightness);
  }
  PRINT("\nbrightness: ",brightness);
  sendCmdAll(CMD_INTENSITY,brightness);
  
  // 3 - Animated Time  
  animated_time = EEPROM.read(3);
  PRINT("\nanimated_time: ",animated_time);

  // 4 - Show Seconds  
  show_seconds = EEPROM.read(4);
  PRINT("\nshow_seconds: ",show_seconds);

  // 5 - Speed Matrix (delay)  
  matrix_speed = EEPROM.read(5);
  if (matrix_speed < 10 || matrix_speed > 40) {
    matrix_speed = 25;
    EEPROM.write(5, matrix_speed);
  }
  PRINT("\nmatrix_speed: ",matrix_speed);

  // 6 - Init Display whit Time/Message  
  // 7 - Alarm #1 Hour  
  // 8 - Alarm #1 Minute  
  // 9 - Alarm #1 Repetitions  
  // 10 - Alarm #2 Hour  
  // 11 - Alarm #2 Minute  
  // 12 - Alarm #2 Repetitions  

  // 13 - Time Zone
  T_Zone2 = EEPROM.read(13);
  PRINT("\nT_Zone2: ",T_Zone2);
  PRINTS("\n");

  // Close EEPROM    
  EEPROM.commit();
  EEPROM.end();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
// A short method of adding the same web page header to some text
//////////////////////////////////////////////////////////////////////////////
void append_webpage_header() {
  // webpage is a global variable
  webpage = ""; // A blank string variable to hold the web page
  webpage += "<!DOCTYPE html><html>"; 
  webpage += "<meta charset=\"utf-8\">";
  webpage += "<style>html { font-family:tahoma; display:inline-block; margin:0px auto; text-align:center;}";
  
  webpage += "#mark      {border: 5px solid #316573 ; width: 1120px;} "; 
  webpage += "#header    {background-color:#C3E0E8; width:1100px; padding:10px; color:#13414E; font-size:36px;}";
  webpage += "#section   {background-color:#E6F5F9; width:1100px; padding:10px; color:#0D7693 ; font-size:14px;}";
  webpage += "#footer    {background-color:#C3E0E8; width:1100px; padding:10px; color:#13414E; font-size:24px; clear:both;}";

  webpage += ".content-select select::-ms-expand {display: none;}";
  webpage += ".content-input input,.content-select select{appearance: none;-webkit-appearance: none;-moz-appearance: none;}";
  webpage += ".content-select select{background-color:#C3E0E8; width:15%; padding:10px; color:#0D7693 ; font-size:48px ; border:6px solid rgba(0,0,0,0.2) ; border-radius: 24px;}";
   
  webpage += ".button {box-shadow: 0px 10px 14px -7px #276873; background:linear-gradient(to bottom, #599bb3 5%, #408c99 100%);";
  webpage += "background-color:#599bb3; border-radius:8px; color:white; padding:13px 32px; display:inline-block; cursor:pointer;";
  webpage += "text-decoration:none;text-shadow:0px 1px 0px #3d768a; font-size:50px; font-weight:bold; margin:2px;}";
  webpage += ".button:hover {background:linear-gradient(to bottom, #408c99 5%, #599bb3 100%); background-color:#408c99;}";
  webpage += ".button:active {position:relative; top:1px;}";
 
  webpage += ".button2 {box-shadow: 0px 10px 14px -7px #8a2a21; background:linear-gradient(to bottom, #f24437 5%, #c62d1f 100%);";
  webpage += "background-color:#f24437; text-shadow:0px 1px 0px #810e05; }";
  webpage += ".button2:hover {background:linear-gradient(to bottom, #c62d1f 5%, #f24437 100%); background-color:#f24437;}";
  
  webpage += ".line {border: 3px solid #666; border-radius: 300px/10px; height:0px; width:80%;}";
  
  webpage += "input[type=\"text\"] {font-size:42px; width:90%; text-align:left;}";
  
  webpage += "input[type=range]{height:61px; -webkit-appearance:none;  margin:10px 0; width:70%;}";
  webpage += "input[type=range]:focus {outline:none;}";
  webpage += "input[type=range]::-webkit-slider-runnable-track {width:70%; height:30px; cursor:pointer; animate:0.2s; box-shadow: 2px 2px 5px #000000; background:#C3E0E8;border-radius:10px; border:1px solid #000000;}";
  webpage += "input[type=range]::-webkit-slider-thumb {box-shadow:3px 3px 6px #000000; border:2px solid #FFFFFF; height:50px; width:50px; border-radius:15px; background:#316573; cursor:pointer; -webkit-appearance:none; margin-top:-11.5px;}";
  webpage += "input[type=range]:focus::-webkit-slider-runnable-track {background: #C3E0E8;}";
  webpage += "</style>";
 
  webpage += "<link rel=\"stylesheet\" href=\"https://cdnjs.cloudflare.com/ajax/libs/animate.css/4.1.1/animate.min.css\"/>";
  webpage += "<html><head><title>ESP32 Crono & Clock</title>";
  webpage += "</head>";

  webpage += "<script>";
  /////////////////////////////////
  webpage += "function SendBright()";
  webpage += "{";
  webpage += "  strLine = \"\";";
  webpage += "  var request = new XMLHttpRequest();";
  webpage += "  strLine = \"BRIGHT=\" + document.getElementById(\"bright_form\").Bright.value;";
  webpage += "  request.open(\"GET\", strLine, false);";
  webpage += "  request.send(null);";
  webpage += "}";
  /////////////////////////////////
  webpage += "</script>";
  webpage += "<div id=\"mark\">";
  webpage += "<div id=\"header\"><h1 class=\"animate__animated animate__flash\">NTP - Crono & Time " + HWversion + "</h1>";
}
//////////////////////////////////////////////////////////////////////////////
void button_Home() {
  webpage += "<p><a href=\"\\HOME\"><type=\"button\" class=\"button\">Refresh WEB</button></a></p>";
}
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void NTP_Clock_home_page() {
  append_webpage_header();
  
  // Muestra el modo: CRONO/TIME
  if (Mode_CRONO == true) {
    webpage += "<p><h5 class=\"animate__animated animate__fadeInLeft\"># CRONÓMETRO #";
    webpage += "</p>[Crono:";
    if (Crono == true) webpage += "ON "; else webpage += "OFF ";
  }else {
    webpage += "<p><h5 class=\"animate__animated animate__fadeInLeft\">RTC: ";
    if (T_Zone2 == false) webpage += zone1; else webpage += zone2;
    webpage += " - ";
    if (display_EU == true) webpage += "EU"; else webpage += "USA";
    webpage += " mode</p>[";
    if (animated_time == true) webpage += "Animated "; else webpage += "Normal ";
    if (show_seconds == true) webpage += "(hh:mm:ss) "; else webpage += "(HH:MM) ";
    if (display_date == true) webpage += "& Date ";
  }
  webpage += "- B:";     
  webpage += brightness;
  webpage += "]";
  webpage += "</h5></div>";

  webpage += "<div id=\"section\">";
  button_Home();

  // Muestra el modo: CRONO/TIME
  if (Mode_CRONO == true) {
    webpage += "<p><a href=\"\\TIME\"><type=\"button\" class=\"button\">TIME mode</button></a></p><p>";
    
    if (Crono == true) {
      webpage += "<a href=\"\\CRONO_STOP\"><type=\"button\" class=\"button\">STOP</button></a>";
    }else {
      webpage += "<a href=\"\\CRONO_START\"><type=\"button\" class=\"button\">START</button></a>";
    }
    webpage += "<a href=\"\\CRONO_INIT\"><type=\"button\" class=\"button\">RESET</button></a></p>";
  }else {
    webpage += "<p><a href=\"\\CRONO\"><type=\"button\" class=\"button\">CRONO mode</button></a></p>";

    if (display_EU == true) {
      webpage += "<p><a href=\"\\DISPLAY_MODE_USA\"><type=\"button\" class=\"button\">USA mode</button></a></p>";
    }else {
      webpage += "<p><a href=\"\\DISPLAY_MODE_EU\"><type=\"button\" class=\"button\">EU mode</button></a></p>";
    }
  
    if (animated_time == true) {
      webpage += "<p><a href=\"\\TIME_NORMAL\"><type=\"button\" class=\"button\">Normal Time</button></a></p>";
    }else {
      webpage += "<p><a href=\"\\TIME_ANIM\"><type=\"button\" class=\"button\">Animated Time</button></a></p>";
    }
  
    if (show_seconds == true){
      webpage += "<p><a href=\"\\TIME_MINUTE\"><type=\"button\" class=\"button\">HH:MM</button></a></p>";
    }else {
      webpage += "<p><a href=\"\\TIME_SECOND\"><type=\"button\" class=\"button\">hh:mm:ss</button></a></p>";
    }

    if (display_date == true) {
      webpage += "<p><a href=\"\\DISPLAY_NO_DATE\"><type=\"button\" class=\"button\">Only Time</button></a></p>";
    }else {
      webpage += "<p><a href=\"\\DISPLAY_DATE\"><type=\"button\" class=\"button\">Show Date</button></a></p>";
    }
  }
  
  webpage += "<form id=\"bright_form\">";
  webpage += "<a>Brightness<br>MIN(0)<input type=\"range\" name=\"Bright\" min=\"0\" max=\"15\" value=\"";
  webpage += brightness;
  webpage += "\">(15)MAX</a></form>";
  webpage += "<p><a href=\"\"><type=\"button\" onClick=\"SendBright()\" class=\"button\">Send Brightness</button></a></p>";
  webpage += "<hr class=\"line\">";

  // Muestra el modo: CRONO/TIME
  if (Mode_CRONO == true) {
    
  }else {
    webpage += "<p><a href=\"\\RESTART_1\"><type=\"button\" class=\"button\">RTC: ";
    webpage += zone1;
    webpage += "</button></a>";
    webpage += "<a href=\"\\RESTART_2\"><type=\"button\" class=\"button\">RTC: ";
    webpage += zone2;
    webpage += "</button></a></p>";
  }
  webpage += "<br><p><a href=\"\\RESET_WIFI\"><type=\"button\" class=\"button button2\">Reset WiFi</button></a></p>";
  webpage += "</div>";
  end_webpage();
}
//////////////////////////////////////////////////////////////////////////////
void reset_wifi() {
  append_webpage_header();
  webpage += "<p><h2>New WiFi Connection</h2></p></div>";
  webpage += "<div id=\"section\">";
  webpage += "<p>&#149; Connect WiFi to SSID: <b>ESP32_AP</b></p>";
  webpage += "<p>&#149; Next connect to: <b><a href=http://192.168.4.1/>http://192.168.4.1/</a></b></p>";
  webpage += "<p>&#149; Make the WiFi connection</p>";
  button_Home();
  webpage += "</div>";
  end_webpage();
  delay(1000);
  WiFiManager wifiManager;
  wifiManager.resetSettings();      // RESET WiFi in ESP32
  reset_ESP32();
}
//////////////////////////////////////////////////////////////
void web_reset_ESP32() {
  append_webpage_header();
  webpage += "<p><h2>Restarting ESP32...</h2></p></div>";
  webpage += "<div id=\"section\">";
  button_Home();
  webpage += "</div>";
  end_webpage();
  delay(1000);
  reset_ESP32();
}
//////////////////////////////////////////////////////////////
void end_webpage(){
  webpage += "<div id=\"footer\">Copyright &copy; J_RPM 2024</div></div></html>\r\n";
  server2.send(200, "text/html", webpage);
  PRINTS("\n>>> end_webpage() OK! ");
}
//////////////////////////////////////////////////////////////////////////////
