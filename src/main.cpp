#include <Arduino.h>

#include <SoftwareSerial.h>
#include <fpm.h>
#include <LowPower.h>
#include "pitches.h"

const byte interruptPin = 2;
int led_Rojo = 12, led_Verde = 8, buzzer = 5, base = 7; // Salidas de los led y buzzer
bool awakeFlag = true;
uint16_t fid, score = 0; // fingerID, confidence

SoftwareSerial fserial(4, 3); // Arduino #4 RX <==> Sensor TX; Arduino #3 TX <==> Sensor RX
FPM finger(&fserial);

/* for convenience */
#define PRINTF_BUF_SZ 60
char printfBuf[PRINTF_BUF_SZ];

// Functions
bool searchDatabase();
void wakeUp();
void fingerParams();
void melody(int);

void setup()
{
  Serial.begin(57600);
  fserial.begin(57600); // Inicializa comunicación con el sensor

  pinMode(led_Rojo, OUTPUT);
  pinMode(led_Verde, OUTPUT);
  pinMode(base, OUTPUT);
  pinMode(interruptPin, INPUT);

  Serial.println("Inicializando lector de huellas");
  delay(250); // Dejar al menos 200ms para inicializar

  if (finger.begin())
  {
    fingerParams();
    digitalWrite(led_Verde, HIGH);
    melody(0);
    digitalWrite(led_Verde, LOW);
  }
  else
  {
    Serial.println("Did not find fingerprint sensor :(");
    digitalWrite(led_Rojo, HIGH);
    melody(-1);
    while (1)
      yield();
  }

  attachInterrupt(0, wakeUp, FALLING); // Accion de interrupción 0
}

void loop()
{
  if (awakeFlag)
  {
    awakeFlag = false;
    // Rutina de apertura
    if (searchDatabase() && score > 100)
    {
      digitalWrite(led_Verde, HIGH);
      melody(0);
      digitalWrite(led_Verde, LOW);
      // Abrir puerta
      digitalWrite(base, HIGH);
      delay(3000);
      digitalWrite(base, LOW);
      score = 0;
    }
    else
    {
      digitalWrite(led_Rojo, HIGH);
      melody(-1);
      digitalWrite(led_Rojo, LOW);
    }
  }

  delay(200);                                          // Espera breve antes de entrar en reposo
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF); // Zzzzzzz.....
}

// Captura y búsqueda de huella ===============================================================
bool searchDatabase()
{
  FPMStatus status;
  uint8_t intentos = 0;

  /* Toma una imagen del dedo posicionado*/
  do
  {
    status = finger.getImage();
    intentos++;
    Serial.print("Intento de lectura de huella numero: "); // Máx. 5 intentos
    Serial.println(intentos);

    switch (status)
    {
    case FPMStatus::OK:
      Serial.println("Imagen adquirida.");
      break;

    case FPMStatus::NOFINGER:
      Serial.println(".");
      break;

    default:
      /* permite reintentos incluso cuando ocurre un error */
      snprintf(printfBuf, PRINTF_BUF_SZ, "getImage(): error 0x%X", static_cast<uint16_t>(status));
      Serial.println(printfBuf);
      break;
    }

    yield(); // Solo para ESP32
  } while (status != FPMStatus::OK && intentos < 5);

  /* Extraer las características de la huella digital */
  status = finger.image2Tz();

  switch (status)
  {
  case FPMStatus::OK:
    Serial.println("Imagen convertida.");
    break;

  default:
    snprintf(printfBuf, PRINTF_BUF_SZ, "image2Tz(): error 0x%X", static_cast<uint16_t>(status));
    Serial.println(printfBuf);
    return false;
  }

  /* Busca en la base de datos la imagen convertida */
  // uint16_t fid, score; // fingerID, confidence -> Se pasa a variable global
  status = finger.searchDatabase(&fid, &score);

  switch (status)
  {
  case FPMStatus::OK:
    snprintf(printfBuf, PRINTF_BUF_SZ, "Coincidencia con ID #%u y fiabilidad %u.", fid, score);
    Serial.println(printfBuf);
    break;

  case FPMStatus::NOTFOUND:
    Serial.println("No se han encontrado coincidencias.");
    return false;

  default:
    snprintf(printfBuf, PRINTF_BUF_SZ, "searchDatabase(): error 0x%X", static_cast<uint16_t>(status));
    Serial.println(printfBuf);
    return false;
  }

  /* Now wait for the finger to be removed, though not necessary.
     This was moved here after the Search operation because of the R503 sensor,
     whose searches oddly fail if they happen after the image buffer is cleared  */
  // Serial.println("Remove finger.");
  // delay(1000);
  // do
  // {
  //   status = finger.getImage();
  //   delay(200);
  // } while (status != FPMStatus::NOFINGER);

  return true;
}

// Función al despertar========================================================================
void wakeUp()
{
  awakeFlag = true;
}

// Inicializar sensor =========================================================================
void fingerParams()
{
  FPMSystemParams params;

  finger.readParams(&params);
  Serial.println("Found fingerprint sensor!");
  Serial.print("Status reg: ");
  Serial.println(params.statusReg);
  Serial.print("System ID: ");
  Serial.println(params.systemId);
  Serial.print("Capacity: ");
  Serial.println(params.capacity);
  Serial.print("Security level index: ");
  Serial.println(static_cast<uint16_t>(params.securityLevel));
  Serial.print("Device add: ");
  Serial.println(params.deviceAddr, HEX);
  Serial.print("Packet length: ");
  Serial.println(FPM::packetLengths[static_cast<uint16_t>(params.packetLen)]);
  Serial.print("Baud Rate index: ");
  Serial.println(static_cast<uint16_t>(params.baudRate));
}

// Melodías del buzzer ========================================================================
void melody(int melodia)
{
  if (melodia == 0)
  {
    // Melodía para estado correcto
    tone(buzzer, NOTE_C6);
    delay(75);
    noTone(buzzer);
    delay(37);
    tone(buzzer, NOTE_E6);
    delay(75);
    noTone(buzzer);
    delay(37);
    tone(buzzer, NOTE_G6);
    delay(75);
    noTone(buzzer);
    tone(buzzer, NOTE_C7);
    delay(200);
    noTone(buzzer);
  }
  else if (melodia == -1)
  {
    // Melodía para error
    tone(buzzer, NOTE_G6);
    delay(100);
    noTone(buzzer);
    delay(100);
    tone(buzzer, NOTE_C6);
    delay(300);
    noTone(buzzer);
  }
}
