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

// Crear los objetos MD_Parola y MD_MAX72XX
MD_Parola mx = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, NUM_MATRICES);
MD_MAX72XX mxControl = MD_MAX72XX(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, NUM_MATRICES);

// Variables del cronómetro
volatile int deciseconds = 0;
volatile int seconds = 0;
volatile int minutes = 0;
volatile bool running = false;
volatile bool countdownFinished = true;

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
  mxControl.clear();

  digitalWrite(LED_START_PIN, LOW);
  digitalWrite(LED_STOP_PIN, HIGH);

  // Mostrar el número 3 en espera
  drawDigit(3, 14);

  // Configurar Ticker
  ticker.attach_ms(10, incrementTime);

  // Inicializar puerto serie
  Serial.begin(115200);
  Serial.println("Sistema iniciado correctamente.");
}

void loop() {
  startButton.update();
  stopButton.update();
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

  Serial.println("Crono * 01 * iniciado.");
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
  Serial.println("Crono * 01 * detenido.");
 // Enviar los datos del cronómetro al puerto serie
      Serial.print("Tiempo: ");
      Serial.print(minutes);
      Serial.print(":");
      Serial.print(seconds);
      Serial.print(":");
      Serial.println(deciseconds);
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
