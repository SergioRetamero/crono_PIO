//  DIA 3 DE ENERO  OJO AL COMPILAR ESP32-S3, 
// EN EL GESTOR DE PLACAS BOARD 3.0.5, AL ENCHUFAR PULSAR BOOT
// CAMBIAR NUMERO DE CRONO EN MENSAJES MATRIZ Y SERIAL
#include <esp_now.h>
#include <WiFi.h>
#include <MD_Parola.h>
#include <MD_MAX72XX.h>
#include <SPI.h>
#include <Bounce2.h>
#include <Ticker.h>

#define DATA_PIN    11 // Pin de datos (GPIO23)
#define CLK_PIN     12  // Pin de reloj (GPIO18)
#define CS_PIN      10   // Pin de selección (GPIO5)

// Pines para los pulsadores y LEDs
#define START_BUTTON_PIN  19  // Pulsador de inicio (GPIO19)
#define STOP_BUTTON_PIN   21  // Pulsador de parada (GPIO21)
#define LED_START_PIN     4   // LED de inicio (GPIO4)
#define LED_STOP_PIN      2   // LED de parada (GPIO2)

// Definir el número total de matrices de 8x8 (4 matrices)
#define NUM_MATRICES 4

// Definir el tipo de hardware: FC16-HW
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW

//////////////////////////////////////////
// Variables de comunicación ESP-NOW
const uint8_t NODO = 2; // Número de nodo = = receptor, 1 a 10 = transmisores

typedef struct{
  uint8_t id;
  uint8_t deciseconds;
  uint8_t seconds;
  uint8_t minutes;
} datoTiempo;

datoTiempo datosRecibidos[10]; // Aquí se guardan los datos recibidos
// Interface con el receptor
esp_now_peer_info_t peerInfo;

// Cambiar con la dirección MAC del receptor
uint8_t broadcastAddress[] = {0x3c, 0x84, 0x27, 0xf2, 0x7d, 0xcc};

// Variables del cronómetro
volatile int deciseconds = 0;
volatile int seconds = 0;
volatile int minutes = 0;
volatile bool running = false;
volatile bool countdownFinished = true;

// Crear los objetos MD_Parola y MD_MAX72XX
MD_Parola mx = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, NUM_MATRICES);
MD_MAX72XX mxControl = MD_MAX72XX(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, NUM_MATRICES);

// Crear objetos Bounce para el debounce de los botones
Bounce startButton = Bounce();
Bounce stopButton = Bounce();

// Crear un objeto Ticker
Ticker ticker;

// Patrones de dígitos personalizados (8x5)
byte digits[10][5] = {
  {0x7E, 0x81, 0x81, 0x81, 0x7E},
  {0x00, 0x82, 0xFF, 0x80, 0x00},
  {0xC2, 0xA1, 0x91, 0x89, 0x86},
  {0x42, 0x81, 0x89, 0x89, 0x76},
  {0x18, 0x14, 0x12, 0xFF, 0x10},
  {0x4F, 0x89, 0x89, 0x89, 0x71},
  {0x7E, 0x89, 0x89, 0x89, 0x72},
  {0x01, 0x01, 0xF1, 0x09, 0x07},
  {0x76, 0x89, 0x89, 0x89, 0x76},
  {0x4E, 0x91, 0x91, 0x91, 0x7E}
};

// Prototipos de funciones
void drawDigit(int digit, int position);
void incrementTime();
void resetTimer();
void startCountdown();
void stopTimer();
void displayTime();
void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len);
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
bool registraReceptor();
bool registraEmisor();
void nombreDispositivo(char *nombre);

void setup() {
  // Configuración de pines
  pinMode(START_BUTTON_PIN, INPUT_PULLUP);
  pinMode(STOP_BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_START_PIN, OUTPUT);
  pinMode(LED_STOP_PIN, OUTPUT);

  // Inicializar LEDs
  digitalWrite(LED_START_PIN, HIGH);
  digitalWrite(LED_STOP_PIN, HIGH);

  // Configurar botones con debounce
  startButton.attach(START_BUTTON_PIN);
  startButton.interval(25);
  stopButton.attach(STOP_BUTTON_PIN);
  stopButton.interval(25);

  // Inicializar MAX7219
  mx.begin();
  mxControl.begin();
  mx.setIntensity(8);
  mxControl.clear();

  // Configurar Ticker
  ticker.attach_ms(10, incrementTime);

  // Inicializar puerto serie
  Serial.begin(115200);

  // Inicializar ESP-NOW
  // Configurar el modo de Wi-Fi
  WiFi.mode(WIFI_STA);
  // Inicializar ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error al inicializar ESP-NOW");
    return;
  }
  // Registrar la función de callback
  if(NODO == 0)
    registraEmisor();
  else
    registraReceptor();

  // Inicializar estructuras de datos
  reseteaDatosReceptor();

  Serial.println("Sistema iniciado correctamente.");

  // Mensaje inicial
  //mx.displayText("CRONO_01", PA_CENTER, 40, 500, PA_SCROLL_LEFT);
  //while (!mx.displayAnimate()) {}
  //mxControl.clear();
//  *****  BORRADO ENTRE MENSAJES
 //mx.displayText("        ", PA_CENTER, 40, 90, PA_SCROLL_LEFT);
  //while (!mx.displayAnimate()) {}
  //mxControl.clear();

  mx.displayText("AGUSTINOS GR", PA_CENTER, 40, 1200, PA_SCROLL_LEFT);
  while (!mx.displayAnimate()) {}
  mx.displayClear();
  //mxControl.clear();

  digitalWrite(LED_START_PIN, LOW);
  digitalWrite(LED_STOP_PIN, HIGH);

  // Mostrar el número 3 en espera
  //drawDigit(3, 14);
  // Muestra el nombre del dispositivo
  char nombre[20];
  nombreDispositivo(nombre);
  mx.displayText(nombre, PA_CENTER, 40, 1200, PA_SCROLL_UP);
  while (!mx.displayAnimate()) {}
}

/////////////////////////////////////////
void loop() {
  startButton.update();
  stopButton.update();

  // Prueba de envío cada 1 segundo
  //static uint32_t lastSeconds = 0;
  static uint32_t lastTime = 0;
  uint32_t now = millis();
  //if (seconds != lastSeconds && NODO > 0) { // Actualizar cada 1 segundo
  //  lastSeconds = seconds;
  if(now - lastTime >= 5000 && NODO > 0) {
      lastTime = now;
      enviarDatosTiempo();
  }

/**/
  // Manejo de botones
  if (startButton.fell() && !running && countdownFinished) {
    resetTimer();
    startCountdown();
  }

  if (stopButton.fell()) {
    if(running){
      stopTimer();
    }
    else if(countdownFinished) {
      resetTimer();
      displayTime();
    }
  }

  if (running) {
    static uint32_t lastDeciSeconds = 0; 
    if (deciseconds != lastDeciSeconds) { // Actualizar solo cuando sea necesario
      lastDeciSeconds = deciseconds;
      displayTime();
    }
  }

  if(!countdownFinished) { // Mostrar cuenta regresiva sin bloquear el micro
      showCountdown();
  }
}

void resetTimer() {
  deciseconds = 0;
  seconds = 0;
  minutes = 0;
  mxControl.clear();
  reseteaDatosReceptor();
}


uint8_t count = 0;
uint32_t lastTime = 0;
void startCountdown() {
  countdownFinished = false;
  count = 3;
  lastTime = millis();
  digitalWrite(LED_START_PIN, HIGH);
  digitalWrite(LED_STOP_PIN, LOW);

  mxControl.clear();
  drawDigit(3, 14);
  Serial.println("Inicio cuenta atrás");
}


void showCountdown() {
  uint32_t now = millis();

  // Parpadeo boton inicio
  static bool lastState = HIGH;
  static uint32_t lastBlink = 0;
  if(now - lastBlink >= 250) {
    lastState = !lastState;
    lastBlink = now;
    digitalWrite(LED_START_PIN, lastState);
  }

  // Cuenta atrás cada segundo
  if (now - lastTime >= 1000) {
    lastTime = now;
    if (count > 1) {
      count--;
      mxControl.clear();
      drawDigit(count, 14);
    } else {
      // Fin cuenta atrás
      finishCountdown();
      digitalWrite(LED_START_PIN, HIGH);
    }
  }
}

void finishCountdown() {
  countdownFinished = true;
  mxControl.clear();

  Serial.printf("Crono * %d * iniciado.\n", NODO);
  running = true;
  // Se debe encender el LED de inicio y apagar el de parada
  digitalWrite(LED_START_PIN, LOW);
  digitalWrite(LED_STOP_PIN, LOW);
  //digitalWrite(LED_START_PIN, LOW);
  //digitalWrite(LED_STOP_PIN, HIGH);
}

void stopTimer() {
  running = false;
  countdownFinished = true;
  digitalWrite(LED_START_PIN, LOW);
  digitalWrite(LED_STOP_PIN, HIGH);

  if(NODO > 0) {
    // Enviar los datos del cronómetro por ESP-NOW
    enviarDatosTiempo(); 

    // Enviar los datos del cronómetro al puerto serie
    Serial.printf("Crono * %d * detenido.\n", NODO);
    Serial.print("Tiempo: ");
    Serial.print(minutes);
    Serial.print(":");
    Serial.print(seconds);
    Serial.print(":");
    Serial.println(deciseconds);
  }
}

void incrementTime() {
  if (running) {
    deciseconds++;
    if (deciseconds >= 100) {
      deciseconds = 0;
      seconds++;
    }
    if (seconds >= 60) {
      seconds = 0;
      minutes++;
    }
    if (minutes >= 99) {
      minutes = 0;
    }
  }
}
// DISTRIBUIR LOS DIGITOS 8x5 EN MATTRIZ
// MAX7219 4 MODULOS 8x8 LED
// LAS FILAS SE CUENTAN DESDE 0 AL 31
// FOMRMATO: MINUTOS XX XX :  SEPARADOR DOS PUNTOS 
// SEGUNDOS XX XX DECIMAS X
// LAS CENTESIMAS SE USAN PARA CONTAR PERO NO SE DIBUJAN

void displayTime() {
  drawDigit(minutes / 10, 27); // Decenas de minutos
  drawDigit(minutes % 10, 21); // Unidades de minutos
  mxControl.setColumn(19, B00100100); // Dos puntos
  drawDigit(seconds / 10, 13);   // Decenas de segundos
  drawDigit(seconds % 10, 7);   // Unidades de segundos
  drawDigit(deciseconds / 10, 0); // Décimas
  // mxControl.setColumn(9, B00100100); // Dos puntos 
}

void drawDigit(int digit, int position) {
  for (int i = 0; i < 5; i++) {
    mxControl.setColumn(position + 4 - i, digits[digit][i]);
  }
}

///////////////////////////////////////////////
// ESP-NOW
// funcion que se ejecutara cuando se reciba un paquete
void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.printf("Paquete recibido: %s tamaño %d\n", macStr, len);
  if(len == sizeof(datoTiempo)) {
    datoTiempo myData;
    memcpy(&myData, incomingData, sizeof(myData));
    Serial.printf("ID de la placa %u\t", myData.id);
    // Actualizar la estructura con los nuevos datos recibidos
    datosRecibidos[myData.id-1].deciseconds = myData.deciseconds;
    datosRecibidos[myData.id-1].seconds = myData.seconds;
    datosRecibidos[myData.id-1].minutes = myData.minutes;
    Serial.printf("Tiempo: %02d:%02d.%02d\n", 
      datosRecibidos[myData.id-1].minutes, 
      datosRecibidos[myData.id-1].seconds, 
      datosRecibidos[myData.id-1].deciseconds);
  }
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Estado ultimo paquete:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Envio Correcto" : "Envio fallido");
}

// Pone todos los datos recibidos a 0
void reseteaDatosReceptor() {
  for (int i = 0; i < 10; i++) {
    datosRecibidos[i].id = 0;
    datosRecibidos[i].deciseconds = 0;
    datosRecibidos[i].seconds = 0;
    datosRecibidos[i].minutes = 0;
  }
}

bool registraReceptor(){
  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  // Redistra la función de callback de envío
  esp_now_register_send_cb(OnDataSent);
  
  // Register peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return false;
  } else {
    Serial.println("Receptor registrado con éxito");
  }
  return true;
}

bool registraEmisor(){
  // Registrar la función de callback de recepción
  esp_err_t result = esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
  if (result == ESP_OK) {
    Serial.println("Recepción registrada con éxito");
    return true;
  }
  else {
    Serial.println("Error al registrar para envío");
    return false;
  }
}

bool enviarDatosTiempo() {
  datoTiempo myData;
  myData.id = NODO;
  myData.deciseconds = deciseconds;
  myData.seconds = seconds;
  myData.minutes = minutes;
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
  if (result == ESP_OK) {
    //Serial.println("Enviado con éxito");
    return true;
  }
  else {
    Serial.println("Error al enviar los datos");
    return false;
  }
}


void nombreDispositivo(char *nombre){
  if(NODO == 0)
    sprintf(nombre, "Centro");
  else
    sprintf(nombre, "Nodo %d", NODO);
}