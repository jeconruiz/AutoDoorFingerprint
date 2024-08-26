#include <Arduino.h>

#include <SoftwareSerial.h>
#include <fpm.h>
#include <LowPower.h>
#include "pitches.h"

const byte interruptPin = 2;
int led_Rojo = 12, led_Verde = 8, buzzer = 5, base = 7; // Salidas de los led y buzzer
int fingerID;                                           // Almacenamiento de último ID de huella
bool awakeFlag = false;
SoftwareSerial fserial(4, 3); // Arduino #4 RX <==> Sensor TX; Arduino #3 TX <==> Sensor RX
FPM finger(&fserial);
/* for convenience */
#define PRINTF_BUF_SZ 60
char printfBuf[PRINTF_BUF_SZ];

// Functions
bool searchDatabase(void);
void wakeUp();
void fingerParams(void);
void melody(String);

void setup()
{
  Serial.println("SETUP");                                          // Debug
  Serial.println("=============================================="); // Debug

  Serial.begin(57600);
  fserial.begin(57600); // Inicializa comunicación con el sensor

  pinMode(led_Rojo, OUTPUT);
  pinMode(led_Verde, OUTPUT);
  pinMode(base, OUTPUT);
  pinMode(interruptPin, INPUT);

  Serial.println("Lector de huellas inicilizando");
  delay(250); // Dejar al menos 200ms para inicializar

  if (finger.begin())
  {
    fingerParams();
    digitalWrite(led_Verde, HIGH);
    melody("ok");
    digitalWrite(led_Verde, LOW);
  }
  else
  {
    Serial.println("Did not find fingerprint sensor :(");
    digitalWrite(led_Rojo, HIGH);
    melody("wrong");
    while (1)
      yield();
  }

  attachInterrupt(0, wakeUp, FALLING);                              // Accion de interrupción 0
  Serial.println("SETUP/INTERRUPT");                                // Debug
  Serial.println("=============================================="); // Debug
}

void loop()
{
  Serial.println("LOOP");                                           // Debug
  Serial.println("=============================================="); // Debug
  if (awakeFlag)
  {
    // Rutina de apertura
    if (searchDatabase())
    {
      Serial.println("LOOP/IF");                                        // Debug
      Serial.println("=============================================="); // Debug
      digitalWrite(led_Verde, HIGH);
      melody("ok");
      digitalWrite(led_Verde, LOW);
      // Abrir puerta
      Serial.print("Abriiendo... "); // Debug
      digitalWrite(base, HIGH);
      delay(1000);
      Serial.println("¡Abierto!"); // Debug
      digitalWrite(base, LOW);
      awakeFlag = false;
    }
    awakeFlag = false;
  }

  Serial.println("PRE-SLEEP");                                      // Debug
  Serial.println("=============================================="); // Debug
  delay(200);
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF); // Zzzzzzz.....

  /*
    Serial.println("\r\nSend any character to search for a print...");
    while (Serial.available() == 0)
      yield();

    searchDatabase();

    while (Serial.read() != -1)
      ;
      */
}

bool searchDatabase(void)
{
  Serial.println("SEARDATABASE");                                   // Debug
  Serial.println("=============================================="); // Debug
  FPMStatus status;

  /* Take a snapshot of the input finger */
  uint8_t tries = 0;
  do
  {
    status = finger.getImage();
    tries++;
    Serial.print("Lectura de huella. Intento: ");
    Serial.println(tries);

    switch (status)
    {
    case FPMStatus::OK:
      Serial.println("Image taken");
      break;

    case FPMStatus::NOFINGER:
      Serial.println(".");
      break;

    default:
      /* allow retries even when an error happens */
      snprintf(printfBuf, PRINTF_BUF_SZ, "getImage(): error 0x%X", static_cast<uint16_t>(status));
      Serial.println(printfBuf);
      break;
    }

    yield();
  } while (status != FPMStatus::OK || tries > 5);

  /* Extract the fingerprint features */
  status = finger.image2Tz();

  switch (status)
  {
  case FPMStatus::OK:
    Serial.println("Image converted");
    break;

  default:
    snprintf(printfBuf, PRINTF_BUF_SZ, "image2Tz(): error 0x%X", static_cast<uint16_t>(status));
    Serial.println(printfBuf);
    return false;
  }

  /* Search the database for the converted print */
  uint16_t fid, score;
  status = finger.searchDatabase(&fid, &score);

  switch (status)
  {
  case FPMStatus::OK:
    snprintf(printfBuf, PRINTF_BUF_SZ, "Found a match at ID #%u with confidence %u", fid, score);
    Serial.println(printfBuf);
    break;

  case FPMStatus::NOTFOUND:
    Serial.println("Did not find a match.");
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

// Función al despertar=========================================================================
void wakeUp()
{
  Serial.println("WAKEUP");                                         // Debug
  Serial.println("=============================================="); // Debug
  awakeFlag = true;
  Serial.println("Awake!"); // BORRAR: depuración

  /*
  fingerID = getFingerprintIDez();
  if (fingerID)
  { // Huella encontrada
    digitalWrite(led_Verde, HIGH);
    okSound();
    digitalWrite(led_Verde, LOW);
    verify_User();
  }
  else
  { // Lectura errónea o huella no coincide
    digitalWrite(led_Rojo, HIGH);
    wrongSound();
    digitalWrite(led_Rojo, LOW);
  }
  */
}
void fingerParams(void)
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

void melody(String melodia)
{
  Serial.println("MELODY");                                         // Debug
  Serial.println("=============================================="); // Debug
  Serial.print("Sonando ");
  if (melodia == "ok")
  {
    Serial.println(melodia);
    // tone(buzzer, NOTE_A7);
    tone(buzzer, NOTE_C6);
    delay(100);
    noTone(buzzer);
    delay(50);
    tone(buzzer, NOTE_G6);
    delay(100);
    noTone(buzzer);
  }
  else if (melodia == "wrong")
  {
    Serial.println(melodia);
    // tone(buzzer, NOTE_C7);
    tone(buzzer, NOTE_G6);
    delay(100);
    noTone(buzzer);
    delay(100);
    // tone(buzzer, NOTE_C7);
    tone(buzzer, NOTE_C6);
    delay(100);
    noTone(buzzer);
  }
}
