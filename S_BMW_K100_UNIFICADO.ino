/*
  BMW K100 - Sketch unificado para Arduino Nano

  Integra:
  - Indicador de marchas
  - Testigo aceite / temperatura
  - Señal de velocidad
  - Señal RPM
  - Indicador gasolina

  Notas importantes:
  - Se ha cambiado la entrada de temperatura de D13 a A1 porque D13 ya estaba usado
    como salida de 4ª marcha.
  - En Arduino Nano clásico, A6 y A7 son entradas analógicas solamente. Por eso los
    sensores de gasolina se leen con analogRead(). Necesitan pull-up externo o una
    señal que quede alta en reposo y baja al activarse.
  - La entrada RPM usa interrupción en D3 para no perder pulsos.
  - La señal RPM está pensada como activa a masa: pulso válido cuando la BMW tira
    la línea a GND.
  - IMPORTANTE: ninguna entrada del Arduino Nano debe recibir 12V. La señal RPM debe
    llegar acondicionada/protegida a 0-5V.
  - Incluye modo banco de pruebas por USB/Serial para forzar salidas desde una interfaz.
  - Todas las tareas se ejecutan sin delay() largo para que ninguna bloquee a las demás.
*/

// ============================================================
// CONFIGURACIÓN DE PINES
// ============================================================

// -------- MARCHAS: entradas --------
const byte gearPinA = 6;
const byte gearPinB = 7;
const byte gearPinC = 8;

// -------- MARCHAS: salidas activas en LOW --------
const byte gearOutN = 9;
const byte gearOut1 = 10;
const byte gearOut2 = 11;
const byte gearOut3 = 12;
const byte gearOut4 = 13;
const byte gearOut5 = A0;

// -------- ACEITE / TEMPERATURA --------
const byte oilPin = A2;
const byte tempPin = A1;          // Antes era D13; cambiado por conflicto con 4ª marcha
const byte oilTempOutPin = A3;

// -------- VELOCIDAD --------
const byte speedInPin = 2;        // Entrada desde módulo BMW
const byte speedOutPin = 4;       // Salida al cuadro

// -------- RPM --------
const byte rpmInPin = 3;          // Entrada RPM activa a masa
const byte rpmOutPin = 5;         // Salida RPM

// -------- GASOLINA --------
const byte fuel4LPin = A6;        // Solo analógico en Nano clásico
const byte fuel8LPin = A7;        // Solo analógico en Nano clásico
const byte fuelFullOutPin = A5;
const byte fuelMidOutPin = A4;

// ============================================================
// CONFIGURACIÓN DE TIEMPOS Y FILTROS
// ============================================================

const byte requiredStableGearReads = 5;
const unsigned long gearReadIntervalMs = 5;

const unsigned long tempBlinkIntervalMs = 500;

const unsigned int speedDebounceUs = 100;
const unsigned int speedOutputPulseUs = 50;

const unsigned int rpmDebounceUs = 500;
const unsigned int rpmOutputPulseUs = 50;

const unsigned long fuelReadIntervalMs = 50;
const int fuelActiveThreshold = 512;   // Menor que esto = sensor activo / LOW

const unsigned long serialBaudRate = 115200;

// ============================================================
// ESTADO INTERNO
// ============================================================

// Marchas
int stableGear = -1;
int candidateGear = -1;
byte stableGearCount = 0;
unsigned long lastGearReadMs = 0;

// Aceite / temperatura
unsigned long lastTempBlinkMs = 0;
bool tempBlinkState = false;

// Velocidad
bool lastSpeedState = HIGH;
unsigned long lastSpeedPulseTimeUs = 0;
bool speedPulseActive = false;
unsigned long speedPulseStartUs = 0;

// RPM
volatile bool rpmPulsePending = false;
volatile unsigned long lastRpmPulseTimeUs = 0;
bool rpmPulseActive = false;
unsigned long rpmPulseStartUs = 0;

// Gasolina
unsigned long lastFuelReadMs = 0;

// Banco de pruebas por Serial
bool testMode = false;
int testGear = -1;
bool testOil = false;
bool testTemp = false;
byte testFuelMode = 0; // 0 = vacío, 1 = medio, 2 = lleno

unsigned int testSpeedHz = 0;
unsigned int testRpmHz = 0;
bool testSpeedPulseActive = false;
bool testRpmPulseActive = false;
unsigned long testLastSpeedPulseUs = 0;
unsigned long testLastRpmPulseUs = 0;
unsigned long testSpeedPulseStartUs = 0;
unsigned long testRpmPulseStartUs = 0;

char serialBuffer[48];
byte serialBufferIndex = 0;

// ============================================================
// FUNCIONES AUXILIARES
// ============================================================

int readGearRaw()
{
  int A = digitalRead(gearPinA);
  int B = digitalRead(gearPinB);
  int C = digitalRead(gearPinC);

  if (A == LOW  && B == LOW  && C == LOW)  return 0; // N
  if (A == LOW  && B == LOW  && C == HIGH) return 1;
  if (A == LOW  && B == HIGH && C == LOW)  return 2;
  if (A == LOW  && B == HIGH && C == HIGH) return 3;
  if (A == HIGH && B == LOW  && C == LOW)  return 4;
  if (A == HIGH && B == LOW  && C == HIGH) return 5;

  return -1;
}

void setAllGearOutputsOff()
{
  digitalWrite(gearOutN, HIGH);
  digitalWrite(gearOut1, HIGH);
  digitalWrite(gearOut2, HIGH);
  digitalWrite(gearOut3, HIGH);
  digitalWrite(gearOut4, HIGH);
  digitalWrite(gearOut5, HIGH);
}

void setGearOutputDirect(int gear)
{
  setAllGearOutputsOff();

  if (gear == 0) digitalWrite(gearOutN, LOW);
  if (gear == 1) digitalWrite(gearOut1, LOW);
  if (gear == 2) digitalWrite(gearOut2, LOW);
  if (gear == 3) digitalWrite(gearOut3, LOW);
  if (gear == 4) digitalWrite(gearOut4, LOW);
  if (gear == 5) digitalWrite(gearOut5, LOW);
}

bool readFuelSensorActive(byte analogPin)
{
  return analogRead(analogPin) < fuelActiveThreshold;
}

void setFuelOutputDirect(byte fuelMode)
{
  // 2 = lleno, 1 = medio, 0 = vacío
  if (fuelMode == 2) {
    digitalWrite(fuelFullOutPin, HIGH);
    digitalWrite(fuelMidOutPin, LOW);
  } else if (fuelMode == 1) {
    digitalWrite(fuelFullOutPin, LOW);
    digitalWrite(fuelMidOutPin, HIGH);
  } else {
    digitalWrite(fuelFullOutPin, LOW);
    digitalWrite(fuelMidOutPin, LOW);
  }
}

void printStatus()
{
  Serial.print(F("MODE="));
  Serial.print(testMode ? F("TEST") : F("RUN"));
  Serial.print(F(";GEAR="));
  Serial.print(testGear);
  Serial.print(F(";OIL="));
  Serial.print(testOil ? 1 : 0);
  Serial.print(F(";TEMP="));
  Serial.print(testTemp ? 1 : 0);
  Serial.print(F(";FUEL="));
  Serial.print(testFuelMode);
  Serial.print(F(";SPD="));
  Serial.print(testSpeedHz);
  Serial.print(F(";RPM="));
  Serial.println(testRpmHz);
}

// ============================================================
// INTERRUPCIONES
// ============================================================

void rpmInputISR()
{
  unsigned long nowUs = micros();

  // Filtro anti ruido: ignorar flancos demasiado juntos.
  if (nowUs - lastRpmPulseTimeUs > rpmDebounceUs) {
    rpmPulsePending = true;
    lastRpmPulseTimeUs = nowUs;
  }
}

// ============================================================
// TAREAS
// ============================================================

void taskGear()
{
  unsigned long nowMs = millis();
  if (nowMs - lastGearReadMs < gearReadIntervalMs) return;
  lastGearReadMs = nowMs;

  int currentReading = readGearRaw();

  if (currentReading == candidateGear) {
    if (stableGearCount < 255) stableGearCount++;
  } else {
    candidateGear = currentReading;
    stableGearCount = 0;
  }

  if (stableGearCount >= requiredStableGearReads &&
      candidateGear != stableGear &&
      candidateGear != -1) {
    stableGear = candidateGear;
  }

  setGearOutputDirect(stableGear);
}

void taskOilTemp()
{
  bool oil = (digitalRead(oilPin) == LOW);
  bool temp = (digitalRead(tempPin) == LOW);

  // Prioridad aceite: fijo encendido.
  if (oil) {
    digitalWrite(oilTempOutPin, LOW);
    return;
  }

  // Temperatura: parpadeo.
  if (temp) {
    unsigned long nowMs = millis();
    if (nowMs - lastTempBlinkMs >= tempBlinkIntervalMs) {
      lastTempBlinkMs = nowMs;
      tempBlinkState = !tempBlinkState;
    }
    digitalWrite(oilTempOutPin, tempBlinkState ? LOW : HIGH);
    return;
  }

  digitalWrite(oilTempOutPin, HIGH);
}

void taskSpeed()
{
  unsigned long nowUs = micros();
  bool currentState = digitalRead(speedInPin);

  // Apagar el pulso limpio cuando haya durado lo configurado.
  if (speedPulseActive && nowUs - speedPulseStartUs >= speedOutputPulseUs) {
    digitalWrite(speedOutPin, LOW);
    speedPulseActive = false;
  }

  // Detectar flanco de bajada real.
  if (currentState == LOW && lastSpeedState == HIGH) {
    if (nowUs - lastSpeedPulseTimeUs > speedDebounceUs) {
      digitalWrite(speedOutPin, HIGH);
      speedPulseStartUs = nowUs;
      speedPulseActive = true;
      lastSpeedPulseTimeUs = nowUs;
    }
  }

  lastSpeedState = currentState;
}

void taskRpm()
{
  unsigned long nowUs = micros();

  // Apagar el pulso limpio cuando haya durado lo configurado.
  if (rpmPulseActive && nowUs - rpmPulseStartUs >= rpmOutputPulseUs) {
    digitalWrite(rpmOutPin, LOW);
    rpmPulseActive = false;
  }

  // Si la interrupción ha detectado un pulso válido, generar un pulso limpio en D5.
  if (rpmPulsePending && !rpmPulseActive) {
    noInterrupts();
    rpmPulsePending = false;
    interrupts();

    digitalWrite(rpmOutPin, HIGH);
    rpmPulseStartUs = nowUs;
    rpmPulseActive = true;
  }
}

void taskFuel()
{
  unsigned long nowMs = millis();
  if (nowMs - lastFuelReadMs < fuelReadIntervalMs) return;
  lastFuelReadMs = nowMs;

  bool s4L = readFuelSensorActive(fuel4LPin);
  bool s8L = readFuelSensorActive(fuel8LPin);

  // LLENO
  if (!s8L && !s4L) {
    setFuelOutputDirect(2);
  }
  // MEDIO
  else if (s8L && !s4L) {
    setFuelOutputDirect(1);
  }
  // VACÍO
  else if (s8L && s4L) {
    setFuelOutputDirect(0);
  }
  // Estado raro: 4L activo y 8L no activo. Lo tratamos como vacío por seguridad.
  else {
    setFuelOutputDirect(0);
  }
}

void taskTestOilTemp()
{
  // Misma lógica que en real: aceite tiene prioridad sobre temperatura.
  if (testOil) {
    digitalWrite(oilTempOutPin, LOW);
    return;
  }

  if (testTemp) {
    unsigned long nowMs = millis();
    if (nowMs - lastTempBlinkMs >= tempBlinkIntervalMs) {
      lastTempBlinkMs = nowMs;
      tempBlinkState = !tempBlinkState;
    }
    digitalWrite(oilTempOutPin, tempBlinkState ? LOW : HIGH);
    return;
  }

  digitalWrite(oilTempOutPin, HIGH);
}

void taskTestPulseGenerator(byte outputPin,
                            unsigned int hz,
                            unsigned int pulseWidthUs,
                            bool &pulseActive,
                            unsigned long &lastPulseUs,
                            unsigned long &pulseStartUs)
{
  unsigned long nowUs = micros();

  if (pulseActive && nowUs - pulseStartUs >= pulseWidthUs) {
    digitalWrite(outputPin, LOW);
    pulseActive = false;
  }

  if (hz == 0) return;

  unsigned long intervalUs = 1000000UL / hz;
  if (!pulseActive && nowUs - lastPulseUs >= intervalUs) {
    digitalWrite(outputPin, HIGH);
    pulseStartUs = nowUs;
    lastPulseUs = nowUs;
    pulseActive = true;
  }
}

void taskTestMode()
{
  setGearOutputDirect(testGear);
  taskTestOilTemp();
  setFuelOutputDirect(testFuelMode);

  taskTestPulseGenerator(speedOutPin,
                         testSpeedHz,
                         speedOutputPulseUs,
                         testSpeedPulseActive,
                         testLastSpeedPulseUs,
                         testSpeedPulseStartUs);

  taskTestPulseGenerator(rpmOutPin,
                         testRpmHz,
                         rpmOutputPulseUs,
                         testRpmPulseActive,
                         testLastRpmPulseUs,
                         testRpmPulseStartUs);
}

void enterRunMode()
{
  testMode = false;
  testSpeedHz = 0;
  testRpmHz = 0;
  testSpeedPulseActive = false;
  testRpmPulseActive = false;
  digitalWrite(speedOutPin, LOW);
  digitalWrite(rpmOutPin, LOW);
}

void enterTestMode()
{
  testMode = true;
  testGear = -1;
  testOil = false;
  testTemp = false;
  testFuelMode = 0;
  testSpeedHz = 0;
  testRpmHz = 0;
  testSpeedPulseActive = false;
  testRpmPulseActive = false;
  setGearOutputDirect(testGear);
  digitalWrite(oilTempOutPin, HIGH);
  setFuelOutputDirect(testFuelMode);
  digitalWrite(speedOutPin, LOW);
  digitalWrite(rpmOutPin, LOW);
}

void processSerialCommand(char *cmd)
{
  // Pasar a mayúsculas.
  for (byte i = 0; cmd[i] != '\0'; i++) {
    if (cmd[i] >= 'a' && cmd[i] <= 'z') cmd[i] = cmd[i] - 32;
  }

  if (strcmp(cmd, "TEST") == 0) {
    enterTestMode();
    Serial.println(F("OK TEST"));
  } else if (strcmp(cmd, "RUN") == 0) {
    enterRunMode();
    Serial.println(F("OK RUN"));
  } else if (strncmp(cmd, "GEAR ", 5) == 0) {
    testGear = atoi(cmd + 5);
    if (testGear < -1) testGear = -1;
    if (testGear > 5) testGear = 5;
    testMode = true;
    Serial.println(F("OK GEAR"));
  } else if (strncmp(cmd, "OIL ", 4) == 0) {
    testOil = atoi(cmd + 4) != 0;
    testMode = true;
    Serial.println(F("OK OIL"));
  } else if (strncmp(cmd, "TEMP ", 5) == 0) {
    testTemp = atoi(cmd + 5) != 0;
    testMode = true;
    Serial.println(F("OK TEMP"));
  } else if (strncmp(cmd, "FUEL ", 5) == 0) {
    if (strcmp(cmd + 5, "FULL") == 0) testFuelMode = 2;
    else if (strcmp(cmd + 5, "MID") == 0) testFuelMode = 1;
    else testFuelMode = 0;
    testMode = true;
    Serial.println(F("OK FUEL"));
  } else if (strncmp(cmd, "SPD ", 4) == 0) {
    testSpeedHz = atoi(cmd + 4);
    testMode = true;
    Serial.println(F("OK SPD"));
  } else if (strncmp(cmd, "RPM ", 4) == 0) {
    testRpmHz = atoi(cmd + 4);
    testMode = true;
    Serial.println(F("OK RPM"));
  } else if (strcmp(cmd, "STATUS") == 0) {
    printStatus();
  } else {
    Serial.println(F("ERR"));
  }
}

void taskSerial()
{
  while (Serial.available() > 0) {
    char c = Serial.read();

    if (c == '\r') continue;

    if (c == '\n') {
      serialBuffer[serialBufferIndex] = '\0';
      if (serialBufferIndex > 0) processSerialCommand(serialBuffer);
      serialBufferIndex = 0;
      continue;
    }

    if (serialBufferIndex < sizeof(serialBuffer) - 1) {
      serialBuffer[serialBufferIndex++] = c;
    }
  }
}

// ============================================================
// SETUP / LOOP
// ============================================================

void setup()
{
  Serial.begin(serialBaudRate);

  // Marchas
  pinMode(gearPinA, INPUT);
  pinMode(gearPinB, INPUT);
  pinMode(gearPinC, INPUT);

  pinMode(gearOutN, OUTPUT);
  pinMode(gearOut1, OUTPUT);
  pinMode(gearOut2, OUTPUT);
  pinMode(gearOut3, OUTPUT);
  pinMode(gearOut4, OUTPUT);
  pinMode(gearOut5, OUTPUT);
  setAllGearOutputsOff();

  // Aceite / temperatura
  pinMode(oilPin, INPUT_PULLUP);
  pinMode(tempPin, INPUT_PULLUP);
  pinMode(oilTempOutPin, OUTPUT);
  digitalWrite(oilTempOutPin, HIGH);

  // Velocidad
  pinMode(speedInPin, INPUT_PULLUP);
  pinMode(speedOutPin, OUTPUT);
  digitalWrite(speedOutPin, LOW);

  // RPM
  pinMode(rpmInPin, INPUT);
  pinMode(rpmOutPin, OUTPUT);
  digitalWrite(rpmOutPin, LOW);
  attachInterrupt(digitalPinToInterrupt(rpmInPin), rpmInputISR, FALLING);

  // Gasolina
  pinMode(fuelFullOutPin, OUTPUT);
  pinMode(fuelMidOutPin, OUTPUT);
  digitalWrite(fuelFullOutPin, LOW);
  digitalWrite(fuelMidOutPin, LOW);
}

void loop()
{
  taskSerial();

  if (testMode) {
    taskTestMode();
  } else {
    taskGear();
    taskOilTemp();
    taskSpeed();
    taskRpm();
    taskFuel();
  }
}
