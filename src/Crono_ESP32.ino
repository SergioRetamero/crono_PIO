//  DIA 3 DE ENERO  OJO AL COMPILAR ESP32-S3,
// EN EL GESTOR DE PLACAS BOARD 3.0.5, AL ENCHUFAR PULSAR BOOT
// CAMBIAR NUMERO DE CRONO EN MENSAJES MATRIZ Y SERIAL
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include <MD_Parola.h>
#include <MD_MAX72XX.h>
#include <SPI.h>
#include <Bounce2.h>

const int numSlaves = 12;       // Número de esclavos

// Número de nodo = 0 receptor, 1 a numSlaves = transmisores
const uint8_t NODO = 0;
// Cambiar con la dirección MAC del receptor
uint8_t broadcastAddress[] = {0x3c, 0x8a, 0x1f, 0x5e, 0x32, 0xb0};

#define DEBUG

#ifdef DEBUG
#define D(x) x                  /// Shorthand for Debug
#define DP(x) Serial.println(x) /// Shorthand for Deug serial println
#define DF(...) {char text[100]; sprintf(text,__VA_ARGS__); Serial.print(text);} /// Shorthand for Debug serial printf

#else
#define D(x)
#define DP(x)
#define DF(x)
#endif

///////////////////////
// Definicion pines 
#define MINID1 1
#define DOITESP32 2
#define ESP32S3 3
#define MODULO MINID1 // Poner 

#if MODULO == MINID1
#define CLK_PIN 32 //16//  18//4  // Pin de reloj (GPIO18)
#define CS_PIN 25  //17 //  5 Pin de selección (GPIO5)
#define DATA_PIN 27 //18 //  23//6 // Pin de datos (GPIO23)

// Pines para los pulsadores y LEDs
#define START_BUTTON_PIN 16 //4  // 2 // Pulsador de inicio (GPIO19)
#define STOP_BUTTON_PIN 17 //5  // 0  // Pulsador de parada (GPIO21)
//#define GENSTART_BUTTON_PIN 8 //  Pulsador de inicio general
//#define GENSTOP_BUTTON_PIN 3 //  Pulsador de parada general

// Pines para el RS485
const int pinTX = 13;//17;
const int pinRX = 14;//18;
const int pinDE = 4;

#elif MODULO == DOITESP32
#define CLK_PIN 18 // Pin de reloj (GPIO18)
#define CS_PIN 5  // Pin de selección (GPIO5)
#define DATA_PIN 23 //18 //  23//6 // Pin de datos (GPIO23)
// Pines para los pulsadores y LEDs
#define START_BUTTON_PIN 26 //4  // 2 // Pulsador de inicio (GPIO19)
#define STOP_BUTTON_PIN 27 //5  // 0  // Pulsador de parada (GPIO21)
// Pines para el RS485
const int pinTX = 13;//17;
const int pinRX = 14;//18;
const int pinDE = 4;
#elif MODULO == ESP32S3
#define CLK_PIN 16 // Pin de reloj (GPIO18)
#define CS_PIN 17  // Pin de selección (GPIO5)
#define DATA_PIN 18 //18 //  23//6 // Pin de datos (GPIO23)
// Pines para los pulsadores y LEDs
#define START_BUTTON_PIN 4 //4  // 2 // Pulsador de inicio (GPIO19)
#define STOP_BUTTON_PIN 5 //5  // 0  // Pulsador de parada (GPIO21)
// Pines para el RS485
const int pinTX = 19;//17;
const int pinRX = 20;//18;
const int pinDE = 7;
#endif

// Definir el número total de matrices de 8x8 (4 matrices)
#define NUM_MATRICES 4

// Definir el tipo de hardware: FC16-HW
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
// Crear los objetos MD_Parola y MD_MAX72XX
MD_Parola mx = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, NUM_MATRICES);
MD_MAX72XX mxControl = MD_MAX72XX(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, NUM_MATRICES);


//////////////////////////////////////////
// Variables de comunicación ESP-NOW
const uint8_t IDSTARTCOUNT = 1;
const uint8_t IDSTOP = 98;
const uint8_t IDRESET = 97;
const uint8_t IDSTART = 96;

typedef struct
{
  uint8_t id;
  uint8_t msg;
} mensajeSimple;

typedef struct
{
  uint8_t id;
  uint8_t deciseconds;
  uint8_t seconds;
  uint8_t minutes;
  uint8_t estado;
  uint8_t hours; // Añadido para mostrar horas
  uint8_t mac_addr[6];
} datoTiempo;

datoTiempo datosRecibidos[numSlaves]; // Aquí se guardan los datos recibidos
// Interface con el receptor
esp_now_peer_info_t peerInfo;

uint8_t baseMac[6];

// Variables del cronómetro
volatile int deciseconds = 0;
volatile int seconds = 0;
volatile int minutes = 0;
volatile int hours = 0; // Añadido para mostrar horas
volatile bool running = false;
volatile bool countdownFinished = true;
uint32_t tiempoArranque = 0; // Tiempo de inicio del contador
const uint8_t cuentaAtrasNumeroInicial = 3; // Número inicial para la cuenta atrás

// Crear objetos Bounce para el debounce de los botones
Bounce startButton = Bounce();
Bounce stopButton = Bounce();

// Patrones de dígitos personalizados (8x5)
byte digits[numSlaves][5] = {
    {0x7E, 0x81, 0x81, 0x81, 0x7E},
    {0x00, 0x82, 0xFF, 0x80, 0x00},
    {0xC2, 0xA1, 0x91, 0x89, 0x86},
    {0x42, 0x81, 0x89, 0x89, 0x76},
    {0x18, 0x14, 0x12, 0xFF, 0x10},
    {0x4F, 0x89, 0x89, 0x89, 0x71},
    {0x7E, 0x89, 0x89, 0x89, 0x72},
    {0x01, 0x01, 0xF1, 0x09, 0x07},
    {0x76, 0x89, 0x89, 0x89, 0x76},
    {0x4E, 0x91, 0x91, 0x91, 0x7E}};

// Prototipos de funciones
void iniciaESPNow();
void mensajeBienvenida();
void drawDigit(int digit, int position);
void incrementTime();
void resetTimer();
void startCountdown();
void stopTimer();
void displayTime( bool force = false);
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len);
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
bool procesaMensaje(const uint8_t *mac_addr, const uint8_t *incomingData, int len);
bool registraReceptor();
bool registraEmisor();
void nombreDispositivo(char *nombre);
bool enviarDatosTiempo(uint8_t *mac_addr_tosend = nullptr);
bool enviarDatosTiempoATodos(uint8_t msg = 0);
bool enviarMensajeoATodos(uint8_t msg);
bool enviaPaquete(uint8_t *data, size_t len, uint8_t *mac_addr_tosend = nullptr);

void setup()
{
  // Inicializar puerto serie
  Serial.begin(115200);

  // Configuración de pines
  pinMode(START_BUTTON_PIN, INPUT_PULLUP);
  pinMode(STOP_BUTTON_PIN, INPUT_PULLUP);

  // Configurar botones con debounce
  startButton.attach(START_BUTTON_PIN);
  startButton.interval(25);
  stopButton.attach(STOP_BUTTON_PIN);
  stopButton.interval(25);

  DP("Iniciando Cronómetro ESP32...");

  // Inicializar MAX7219
  mx.begin();
  mxControl.begin();
  mx.setIntensity(8);
  mxControl.clear();
  DP("Iniciando pantalla...");

  iniciaESPNow();
  DP("Sistema ESP-NOW iniciado correctamente.");
  
  mensajeBienvenida();
}

/////////////////////////////////////////
void loop()
{
  incrementTime();


  // Prueba de envío cada 1 segundo
  // static uint32_t lastSeconds = 0;
  static uint32_t lastTime = 0;
  uint32_t now = millis();
  // if (seconds != lastSeconds && NODO > 0) { // Actualizar cada 1 segundo
  //   lastSeconds = seconds;
  if (now - lastTime >= 50 + random(5) && NODO > 0)
  { // Los satélites envían cada cierto tiempo los datos de tiempo
    lastTime = now;
    enviarDatosTiempo();
  }
  else if (now - lastTime >= 5000)
  { // El centro Manda la lista de resultados cada cierto tiempo
    lastTime = now;
    // Imprime lista tiempos recibidos
    Serial.println("----------------------------");
    D(Serial.printf("Centro Tiempo: %02d:%02d.%02d\n",
                    minutes, seconds, deciseconds);)
    for (int i = 0; i < numSlaves; i++)
    {
      if (datosRecibidos[i].id == 0)
        continue; //{

      Serial.printf("ID: %d\t", datosRecibidos[i].id);
      Serial.printf("Tiempo: %02d:%02d.%02d",
                    datosRecibidos[i].minutes,
                    datosRecibidos[i].seconds,
                    datosRecibidos[i].deciseconds);
      D(Serial.printf("\tMAC: %02x:%02x:%02x:%02x:%02x:%02x",
                      datosRecibidos[i].mac_addr[0], datosRecibidos[i].mac_addr[1],
                      datosRecibidos[i].mac_addr[2], datosRecibidos[i].mac_addr[3],
                      datosRecibidos[i].mac_addr[4], datosRecibidos[i].mac_addr[5]);)
      Serial.println("");
      //}
    }
    Serial.println("");
  }

  compruebaBotones();

  actualizaPantalla();

}

void compruebaBotones()
{
  startButton.update();
  stopButton.update();

  // Manejo de botones
  if (startButton.fell() && !running && countdownFinished)
  {
    DP("Boton inicio pulsado");
    resetTimer();
    startCountdown();
  }

  if (stopButton.fell()  && running)
  {
    DP("Boton parada pulsado");
    if (NODO == 0)
      enviarMensajeoATodos(IDSTOP);
    stopTimer();
  }

  // Se ha pulsado stop más de 2 segundos
  if (stopButton.rose() && stopButton.previousDuration() > 2000 && NODO == 0)
  {
    DP("Boton parada pulsado más de 2 segundos");
    if(NODO == 0) enviarMensajeoATodos(IDRESET);
    resetTimer();
    displayTime(true);
  }
}

void incrementTime()
{
static uint32_t lastIncrementTime = 0;
uint32_t currentTime = millis() - (NODO==0? 1:0);//+ 60*1000*59;

  if(lastIncrementTime == 0) lastIncrementTime = currentTime;
  if (!running) return; // No actualiza cuando está parado
  if(tiempoArranque == 0) tiempoArranque = currentTime/10;
  
  //if(currentTime - lastIncrementTime < 50) return; // Actualiza cada 50 ms
  //lastIncrementTime = currentTime;

  uint32_t elapsedTime = currentTime/10 - tiempoArranque; // Décimas de segundo (32 bits)
  deciseconds = (elapsedTime) % 100;
  seconds = (elapsedTime/100) % 60;
  minutes = (elapsedTime/6000) % 60;
  hours = (elapsedTime/360000) % 10;
}

void resetTimer()
{
  deciseconds = 0;
  seconds = 0;
  minutes = 0;
  hours = 0;
  mxControl.clear();
  // reseteaDatosReceptor();
}

uint8_t contAtras = 0;
uint32_t lastTime = 0;

void startCountdown()
{
  countdownFinished = false;
  contAtras = cuentaAtrasNumeroInicial;
  lastTime = millis();

  mxControl.clear();
  drawDigit(contAtras, 14);
  DP("Inicio cuenta atrás");

  if(NODO == 0) enviarMensajeoATodos(IDSTARTCOUNT);
}

void showCountdown()
{
  uint32_t now = millis();

  // Parpadeo boton inicio
  static bool lastState = HIGH;
  static uint32_t lastBlink = 0;
  if (now - lastBlink >= 250)
  {
    lastState = !lastState;
    lastBlink = now;
  }

  // Manda mensaje de inicio de cuenta atrás desde el centro
  /* if( NODO==0 && (lastBlink-now)%20==0) {
    enviarMensajeoATodos(IDSTARTCOUNT);
    } */

  // Cuenta atrás cada segundo
  if (now - lastTime >= 1000)
  {
    lastTime = now;
    if (contAtras > 1)
    {
      contAtras--;
      mxControl.clear();
      drawDigit(contAtras, 14);
    }
    else
    {
      // Fin cuenta atrás
      finishCountdown();
    }
  }
}

void startCrono()
{
  DF("Crono * %d * iniciado.\n", NODO);

  running = true;
  tiempoArranque = millis()/10;
}

void finishCountdown()
{
  countdownFinished = true;
  mxControl.clear();

  startCrono();
}

void stopTimer()
{
  if (NODO > 0)
  {
    // Enviar los datos del cronómetro por ESP-NOW
    enviarDatosTiempo();
  }

  running = false;
  countdownFinished = true;

  // Enviar los datos del cronómetro al puerto serie
  DF("Crono * %d * detenido.\n", NODO);
  DF("Tiempo: %02d:%02d:%02d.%02d\n", hours, minutes, seconds, deciseconds);

}


//////////////////////////////////
// Funciones matriz puntos

void displayTime(bool force)
{
  static int prevDeci = 0;
  static int cambioHora = 0;
  if (prevDeci == deciseconds/9 && !force)
    return;
  prevDeci = deciseconds/9;

  static uint32_t lastCycle = 0;
  uint32_t currentTime = millis();
  if( currentTime-lastCycle > 5000)
  {
    lastCycle = currentTime;
    mxControl.clear();
  }
  //mxControl.clear();

  int pos = 27;
  if (hours > 0)
  {
    if(cambioHora != hours)
    {
      cambioHora = hours;
      mxControl.clear();
    }
    drawDigit(hours % 10, pos); // Unidades de hora 27
    pos -= 1;
    mxControl.setColumn(pos, B00100100); // Dos puntos 19
    pos -= 6;
  }
  if(minutes>9 || hours > 0)  // muestra centesimas y no decenas deminutos
  {
    drawDigit((minutes % 60) / 10, pos); // Decenas de minutos 27
    pos -= 6;
  }
  drawDigit((minutes % 60) % 10, pos); // Unidades de minutos 21
  pos -= 2;
  mxControl.setColumn(pos, B00100100); // Dos puntos 19
  pos -= 6;
  drawDigit(seconds / 10, pos); // Decenas de segundos 13
  pos -= 6;
  drawDigit(seconds % 10, pos); // Unidades de segundos 7
  if ( hours > 0)
    return; // Mostrando horas, no se ven las décimas
  pos -= 7;
  drawDigit(deciseconds/10 , pos); // Décimas
  mxControl.setColumn(pos+5, B00100100); // Dos puntos
  if(minutes<10) // muestra centesimas
  {
    pos-= 6;
    drawDigit(deciseconds%10, pos); // Añadir cero a la izquierda si minutos < 10
  }
}

void drawDigit(int digit, int position)
{
  for (int i = 0; i < 5; i++)
  {
    mxControl.setColumn(position + 4 - i, digits[digit][i]);
  }
}

// Text parameters
#define CHAR_SPACING  1 // pixels between characters
// Print the text string to the LED matrix modules specified.
// Message area is padded with blank columns after printing.
void printText( const char *pMsg)
{
  uint8_t modStart= 0;
  uint8_t modEnd = NUM_MATRICES-1;
  uint8_t   state = 0;
  uint8_t   curLen;
  uint16_t  showLen;
  uint8_t   cBuf[8];
  int16_t   col = ((modEnd + 1) * COL_SIZE) - 1;

  mxControl.control(modStart, modEnd, MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);

  do     // finite state machine to print the characters in the space available
  {
    switch(state)
    {
      case 0: // Load the next character from the font table
        // if we reached end of message, reset the message pointer
        if (*pMsg == '\0')
        {
          showLen = col - (modEnd * COL_SIZE);  // padding characters
          state = 2;
          break;
        }

        // retrieve the next character form the font file
        showLen = mxControl.getChar(*pMsg++, sizeof(cBuf)/sizeof(cBuf[0]), cBuf);
        curLen = 0;
        state++;
        // !! deliberately fall through to next state to start displaying

      case 1: // display the next part of the character
      mxControl.setColumn(col--, cBuf[curLen++]);

        // done with font character, now display the space between chars
        if (curLen == showLen)
        {
          showLen = CHAR_SPACING;
          state = 2;
        }
        break;

      case 2: // initialize state for displaying empty columns
        curLen = 0;
        state++;
        // fall through

      case 3:	// display inter-character spacing or end of message padding (blank columns)
      mxControl.setColumn(col--, 0);
        curLen++;
        if (curLen == showLen)
          state = 0;
        break;

      default:
        col = -1;   // this definitely ends the do loop
    }
  } while (col >= (modStart * COL_SIZE));

  mxControl.control(modStart, modEnd, MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
}

void mensajeBienvenida()
{
  mx.displayText("AGUSTINOS GR", PA_CENTER, 40, 1200, PA_SCROLL_LEFT);
  while (!mx.displayAnimate())
  {
  }
  mx.displayClear();
  // mxControl.clear();

  // Mostrar el número 3 en espera
  // drawDigit(3, 14);
  // Muestra el nombre del dispositivo
  char nombre[20];
  nombreDispositivo(nombre);
  mx.displayText(nombre, PA_CENTER, 40, 1200, PA_SCROLL_UP);
  while (!mx.displayAnimate())
  {
  }

}

void actualizaPantalla()
{
  if (running && countdownFinished)
  {
    static uint32_t lastDeciSeconds = 0;
    if (deciseconds != lastDeciSeconds)
    { // Actualizar solo cuando sea necesario
      lastDeciSeconds = deciseconds;
      displayTime();
    }
    return;
  }

  if (!countdownFinished)
  { // Mostrar cuenta regresiva sin bloquear el micro
    showCountdown();
  }

  if(!running && countdownFinished)
  { // Mostrar mensaje de parada
    static uint32_t lastStopTime = 0;
    if (millis() - lastStopTime > 2000) // Actualizar cada segundos
    {
      lastStopTime = millis();
      mxControl.clear();
      /* char msj[40];
      sprintf(msj, "ESPERA NODOS:%d     ", contNodos);
      scrollText(msj, 80); */
      static bool alterna = true;
      if(alterna)
      {
        displayTime(true); // Muestra el tiempo final
        alterna = false;
      }
      else
      {
        printText("PAUSA");
        //mx.displayText("PAUSA", PA_CENTER, 40, 1200, PA_PRINT);
        alterna = true;
      }

    }
  }
}

char *cadenaEstado(uint8_t estado)
{
  static char estadoStr[20];
  switch (estado)
  {
  case IDSTARTCOUNT:
    strcpy(estadoStr, "Iniciar");
    break;
  case IDSTOP:
    strcpy(estadoStr, "Parado");
    break;
  case IDRESET:
    strcpy(estadoStr, "Reiniciar");
    break;
  case IDSTART:
    strcpy(estadoStr, "Corre");
    break;
  default:
    strcpy(estadoStr, "DESCONOCIDO");
    break;
  }
  return estadoStr;
}


///////////////////////////////////////////////
// ESP-NOW

void iniciaESPNow()
{
  // Inicializar ESP-NOW
  // Configurar el modo de Wi-Fi
  WiFi.mode(WIFI_STA);

  // Inicializar ESP-NOW
  if (esp_now_init() != ESP_OK)
  {
    DP("Error al inicializar ESP-NOW");
    return;
  } else 
    DP("ESP-NOW inicializado correctamente");


  esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
  if (ret == ESP_OK)
  {
    D(Serial.printf("MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
                    baseMac[0], baseMac[1], baseMac[2],
                    baseMac[3], baseMac[4], baseMac[5]));
  }
  else
  {
    DP("Failed to read MAC address");
  }

  // Registrar la función de callback
  // if(NODO == 0)
  registraEmisor();
  // else
  registraReceptor();

  // Inicializar estructuras de datos
  reseteaDatosReceptor();
}

// funcion que se ejecutara cuando se reciba un paquete
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len)
{
  // char macStr[18];
  // snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
  //          mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  // D(Serial.printf("Paquete recibido: %s tamaño %d\n", macStr, len);)

  if (len == sizeof(mensajeSimple))
  {
    if (NODO == 0)
      return; // El centro no procesa mensajes
    procesaMensaje(mac_addr, incomingData, len);
    return;
  }

  if (len != sizeof(datoTiempo)){
    DF("Error tamaño de datos recibido: %d, esperado: %d\n", len, sizeof(datoTiempo));
    return; // Error tamaño
  }  
  datoTiempo myData;
  memcpy(&myData, incomingData, sizeof(myData));
  if (myData.id < 1 || myData.id > numSlaves)
    return; // Error rango id

  // Si es centro, actualizar la estructura con los nuevos datos recibidos
  datosRecibidos[myData.id - 1].deciseconds = myData.deciseconds;
  datosRecibidos[myData.id - 1].seconds = myData.seconds;
  datosRecibidos[myData.id - 1].minutes = myData.minutes;
  datosRecibidos[myData.id - 1].hours = myData.hours; // Añadido para mostrar horas
  datosRecibidos[myData.id - 1].id = myData.id;
  datosRecibidos[myData.id - 1].estado = myData.estado;
  if (datosRecibidos[myData.id - 1].mac_addr[0] == 0 && datosRecibidos[myData.id - 1].mac_addr[1] == 0 && datosRecibidos[myData.id - 1].mac_addr[2] == 0)
  { // Nueva MAC recibida, la registramos
    // Register peer
    memcpy(peerInfo.peer_addr, myData.mac_addr, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    // Add peer
    if (esp_now_add_peer(&peerInfo) != ESP_OK)
    {
      D(Serial.printf("Failed to add peer %d MAC:%02x:%02x:%02x:%02x:%02x:%02x\n", myData.id,
                      myData.mac_addr[0], myData.mac_addr[1], myData.mac_addr[2],
                      myData.mac_addr[3], myData.mac_addr[4], myData.mac_addr[5]);)
    }
    else
    {
      D(Serial.printf("Receptor %d registrado con éxito MAC:%02x:%02x:%02x:%02x:%02x:%02x\n", myData.id,
                      myData.mac_addr[0], myData.mac_addr[1], myData.mac_addr[2],
                      myData.mac_addr[3], myData.mac_addr[4], myData.mac_addr[5]);)
    }
  }
  memcpy(datosRecibidos[myData.id - 1].mac_addr, myData.mac_addr, 6);
  /* D(Serial.printf("Tiempo: %02d:%02d.%02d\n",
    datosRecibidos[myData.id-1].minutes,
    datosRecibidos[myData.id-1].seconds,
    datosRecibidos[myData.id-1].deciseconds);) */
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  if (status != ESP_NOW_SEND_SUCCESS)
  {
    D(Serial.printf("Error al enviar los datos a: %02x:%02x:%02x:%02x:%02x:%02x\n",
                    mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);)
  }
}

bool procesaMensaje(const uint8_t *mac_addr, const uint8_t *incomingData, int len)
{
  if (len != sizeof(mensajeSimple))
    return false;
  mensajeSimple myMsg;
  memcpy(&myMsg, incomingData, len);
  if (NODO == 0)
    return false; // Centro no procesa mensajes

  D(Serial.printf("Mensaje %d/%d\n", myMsg.id, myMsg.msg);)
  switch (myMsg.msg)
  {
    case IDSTARTCOUNT:
      DP("Msg Start");
      resetTimer();
      startCountdown();
      break;
    case IDSTOP:
      DP("Msg Stop");
      stopTimer();
      break;
    case IDRESET:
      DP("Msg Reset");
      resetTimer();
      displayTime();
      break;
    case IDSTART:
      if( running) break; // Ya estaba en el modo
      resetTimer();
      finishCountdown();
      //DP("Contador iniciado");
      break;
    default:
      DP("Msg Desconocido");
      break;
  }

  return true;
}

// Pone todos los datos recibidos a 0
void reseteaDatosReceptor()
{
  for (int i = 0; i < numSlaves; i++)
  {
    datosRecibidos[i].id = 0;
    datosRecibidos[i].deciseconds = 0;
    datosRecibidos[i].seconds = 0;
    datosRecibidos[i].minutes = 0;
    datosRecibidos[i].hours = 0; // Añadido para mostrar horas
    datosRecibidos[i].estado = 0;
    datosRecibidos[i].mac_addr[0] = 0;
    datosRecibidos[i].mac_addr[1] = 0;
    datosRecibidos[i].mac_addr[2] = 0;
    datosRecibidos[i].mac_addr[3] = 0;
    datosRecibidos[i].mac_addr[4] = 0;
    datosRecibidos[i].mac_addr[5] = 0;
  }
}

bool registraReceptor()
{
  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  // Redistra la función de callback de envío
  esp_now_register_send_cb(OnDataSent);

  if (NODO == 0)
    return true; // El centro registra los peer a medida que llegan
  // Register peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  // Add peer
  if (esp_now_add_peer(&peerInfo) != ESP_OK)
  {
    DP("Failed to add peer");
    return false;
  }
  else
  {
    DP("Receptor registrado con éxito");
  }
  return true;
}

bool registraEmisor()
{
  // Registrar la función de callback de recepción
  esp_err_t result = esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
  if (result == ESP_OK)
  {
    DP("Recepción registrada con éxito");
    return true;
  }
  else
  {
    DP("Error al registrar para envío");
    return false;
  }
}

bool enviarDatosTiempo(uint8_t *mac_addr_tosend)
{
  if (!mac_addr_tosend) // && NODO!=0)
    mac_addr_tosend = broadcastAddress;

  datoTiempo myData;
  // if(NODO!=0) myData.id = NODO;
  // else if(running) myData.id = IDSTOP; // Manda mensaje de stop
  // else myData.id = IDSTART;

  myData.id = NODO;
  myData.deciseconds = deciseconds;
  myData.seconds = seconds;
  myData.minutes = minutes;
  myData.hours = hours; // Añadido para mostrar horas
  myData.estado = !countdownFinished ? IDSTARTCOUNT: (running ? IDSTART : IDSTOP); // Estado del cronómetro
  memcpy(myData.mac_addr, baseMac, 6);

  esp_err_t result = esp_now_send(mac_addr_tosend, (uint8_t *)&myData, sizeof(myData));
  if (result == ESP_OK)
  {
    // DP("Enviado con éxito");
    return true;
  }
  else
  {
    if (!mac_addr_tosend)
      DP("Error al enviar los datos a todos los receptores");
    else
      D(Serial.printf("Error al enviar los datos a: %02x:%02x:%02x:%02x:%02x:%02x\n",
                      mac_addr_tosend[0], mac_addr_tosend[1], mac_addr_tosend[2],
                      mac_addr_tosend[3], mac_addr_tosend[4], mac_addr_tosend[5]);)
    return false;
  }
  return true;
}

bool enviaMensaje(uint8_t msg, uint8_t *mac_addr_tosend)
{
  if (!mac_addr_tosend)
    mac_addr_tosend = broadcastAddress;

  mensajeSimple datoMsg;
  datoMsg.id = NODO;
  datoMsg.msg = msg;

  bool retVal = enviaPaquete((uint8_t *)&datoMsg, sizeof(datoMsg), mac_addr_tosend);
  if (!retVal)
  {
    D(Serial.printf("Error al enviar mensaje %d a: %02x:%02x:%02x:%02x:%02x:%02x\n", msg,
                    mac_addr_tosend[0], mac_addr_tosend[1], mac_addr_tosend[2],
                    mac_addr_tosend[3], mac_addr_tosend[4], mac_addr_tosend[5]);)
  }
  else
  {
    D(Serial.printf("Mensaje %d enviado a: %02x:%02x:%02x:%02x:%02x:%02x\n", msg,
                    mac_addr_tosend[0], mac_addr_tosend[1], mac_addr_tosend[2],
                    mac_addr_tosend[3], mac_addr_tosend[4], mac_addr_tosend[5]);)
  }
  return retVal;
}

bool enviarMensajeoATodos(uint8_t msg)
{
  if (NODO != 0)
  {
    DP("No se puede enviar datosa multiples puntos desde un receptor");
    return false;
  }

  for (int i = 0; i < numSlaves; i++)
  {
    if (datosRecibidos[i].id == 0)
      continue;
    D(Serial.printf("Enviando mensaje %d a %d\n", msg, datosRecibidos[i].id);)
    enviaMensaje(msg, datosRecibidos[i].mac_addr);
    delay(1);
  }
  return true;
}

bool enviarDatosTiempoATodos(uint8_t msg)
{
  if (NODO != 0)
  {
    DP("No se puede enviar datos a multiples puntos desde un receptor");
    return false;
  }

  // enviarDatosTiempo();
  for (int i = 0; i < numSlaves; i++)
  {
    if (datosRecibidos[i].id == 0)
      continue;
    datoTiempo myData;
    myData.id = msg;

    myData.deciseconds = deciseconds;
    myData.seconds = seconds;
    myData.minutes = minutes;
    myData.hours = hours;
    myData.estado = !countdownFinished ? IDSTARTCOUNT : (running ? IDSTART : IDSTOP); // Estado del cronómetro
    memcpy(myData.mac_addr, baseMac, 6);

    enviaPaquete((uint8_t *)&myData, sizeof(myData), datosRecibidos[i].mac_addr);

    delay(1);
  }
  return true;
}

bool enviaPaquete(uint8_t *data, size_t len, uint8_t *mac_addr_tosend)
{
  if (!mac_addr_tosend)
    mac_addr_tosend = broadcastAddress;

  esp_err_t result = esp_now_send(mac_addr_tosend, data, len);
  if (result == ESP_OK)
  {
    return true;
  }
  else
  {
    if (!mac_addr_tosend)
    {
      D(Serial.printf("Error al enviar los datos a todos los receptores\n");)
    }
    else
    {
      D(Serial.printf("Error al enviar los datos a: %02x:%02x:%02x:%02x:%02x:%02x\n",
                      mac_addr_tosend[0], mac_addr_tosend[1], mac_addr_tosend[2],
                      mac_addr_tosend[3], mac_addr_tosend[4], mac_addr_tosend[5]);)
    }
    return false;
  }
}

void nombreDispositivo(char *nombre)
{
  if (NODO == 0)
    sprintf(nombre, "Centro");
  else
    sprintf(nombre, "Nodo %d", NODO);
}

///////////////////////////////////////////////
