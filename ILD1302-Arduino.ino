// ===============================================================
//  ILD1302 AUTOMATICO  -- Plato rotatorio para calibracion
//  2 modos: AUTONOMO (al encender)  +  MANUAL (Monitor Serie)
//
//  Flujo modo AUTONOMO:
//    1) Espera TIEMPO_ESPERA_INICIAL_MS (sincronizacion del sensor)
//    2) Por cada ciclo:
//         a) Avanza PASO_ANGULO_DEG (con compensacion acumulativa)
//         b) Espera TIEMPO_ESPERA_POSICION_MS quieto
//         c) Repite hasta completar 360 deg (POSICIONES posiciones)
//    3) Hace CICLOS vueltas en SENTIDO_PRIMERO
//    4) Si MEDIR_AMBOS_SENTIDOS=true:
//         - Espera ESPERA_ENTRE_SENTIDOS_MS
//         - Repite CICLOS vueltas en sentido opuesto
//
//  Flujo modo MANUAL:
//    Envia cualquier tecla en los primeros DETECT_MS al encender
//    Comandos:  H  A  +N  -N  E  S  ?
//
//  TODO ES CONFIGURABLE desde el bloque CONFIGURACION abajo.
// ===============================================================

#define ENABLE  8
#define Y_DIR   6
#define Y_STEP  3

// ╔══════════════════════════════════════════════════════════════╗
// ║                      CONFIGURACION                            ║
// ║              (edita SOLO este bloque)                         ║
// ╚══════════════════════════════════════════════════════════════╝

// ── HARDWARE: motor + reductora + driver ──────────────────────
const long PASOS_MOTOR_REV      = 200;   // pasos/rev del motor (NEMA 1.8 deg = 200, 0.9 deg = 400)
const long RELACION_ENGRANAJES  = 40;    // engranaje 1:N  (actual = 40)
const long MICROSTEP            = 1;     // driver: 1, 2, 4, 8, 16, 32

// ── GEOMETRIA DEL ENSAYO ──────────────────────────────────────
const long PASO_ANGULO_DEG      = 30;    // grados por posicion (30, 45, 60, 90...)
const int  CICLOS               = 5;     // vueltas completas por sentido

// ── SENTIDO DE GIRO ───────────────────────────────────────────
const bool SENTIDO_PRIMERO_ES_HORARIO = true;  // true=horario, false=antihorario
const bool MEDIR_AMBOS_SENTIDOS       = true;  // true=mide horario Y antihorario

// ── TIEMPOS (en milisegundos) ─────────────────────────────────
const long TIEMPO_ESPERA_INICIAL_MS   = 8000;  // pausa inicial (sensor sincroniza)
const long TIEMPO_ESPERA_POSICION_MS  = 3000;  // pausa por cada posicion (estable)
const long ESPERA_ENTRE_CICLOS_MS     = 0;     // pausa adicional entre vueltas (0 = sin pausa)
const long ESPERA_ENTRE_SENTIDOS_MS   = 3000;  // pausa al cambiar de sentido

// ── VELOCIDAD DEL MOTOR ───────────────────────────────────────
const int  VEL_US               = 800;   // microsegundos entre pulsos (menor=mas rapido)
                                          //  800us ≈ 3.2s para 90 deg

// ── MODO MANUAL ───────────────────────────────────────────────
const long DETECT_MS            = 5000;  // ventana al encender para entrar en modo MANUAL

// ╔══════════════════════════════════════════════════════════════╗
// ║   No modificar debajo de esta linea (calculado automatico)    ║
// ╚══════════════════════════════════════════════════════════════╝

const long PASOS_VUELTA = PASOS_MOTOR_REV * RELACION_ENGRANAJES * MICROSTEP;
const int  POSICIONES   = 360 / PASO_ANGULO_DEG;

#define MODO_AUTONOMO 0
#define MODO_MANUAL   1

int  modo       = MODO_AUTONOMO;
int  direccion  = HIGH;        // HIGH = horario, LOW = antihorario
bool motorOn    = true;
int  pasoActual = 0;           // 0..POSICIONES-1

// -- Buffer serial -----------------------------------------------
static String _lineBuf = "";

bool leerLinea(String &out) {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (_lineBuf.length() > 0) {
        out = _lineBuf; out.trim(); _lineBuf = "";
        return true;
      }
    } else if (c >= 32 && c < 127) {
      _lineBuf += c;
    }
  }
  return false;
}

// -- Compensacion acumulativa ------------------------------------
// Devuelve cuantos pasos hay que dar para pasar de la posicion i a i+1.
// La suma de los POSICIONES tramos da exactamente PASOS_VUELTA.
long pasosTramo(int i) {
  long a = (PASOS_VUELTA * (long)(i + 1)) / POSICIONES;
  long b = (PASOS_VUELTA * (long)i)       / POSICIONES;
  return a - b;
}

// -- Motor --------------------------------------------------------
void mover(long pasos, int dir) {
  digitalWrite(Y_DIR, !dir);
  delayMicroseconds(10);
  for (long i = 0; i < pasos; i++) {
    digitalWrite(Y_STEP, HIGH);
    delayMicroseconds(VEL_US);
    digitalWrite(Y_STEP, LOW);
    delayMicroseconds(VEL_US);
  }
}

void moverUnPaso(int dir) {
  long pasos = pasosTramo(pasoActual);
  mover(pasos, dir);
  if (dir == HIGH) pasoActual = (pasoActual + 1) % POSICIONES;
  else             pasoActual = (pasoActual - 1 + POSICIONES) % POSICIONES;
}

// -- Modo autonomo ------------------------------------------------
void ciclosDir(int dir, const String &nombre) {
  Serial.println(">>> " + nombre + " <<<");
  for (int c = 1; c <= CICLOS; c++) {
    Serial.print("  Ciclo "); Serial.print(c);
    Serial.print("/"); Serial.println(CICLOS);
    pasoActual = 0;
    for (int p = 1; p <= POSICIONES; p++) {
      moverUnPaso(dir);
      Serial.print("    "); Serial.print((long)p * PASO_ANGULO_DEG);
      Serial.print(String(char(176)));
      Serial.print(" midiendo...");
      delay(TIEMPO_ESPERA_POSICION_MS);
      Serial.println(" OK");
    }
    if (c < CICLOS && ESPERA_ENTRE_CICLOS_MS > 0) {
      delay(ESPERA_ENTRE_CICLOS_MS);
    }
    Serial.println();
  }
}

void ejecutarAutonomo() {
  int dir1 = SENTIDO_PRIMERO_ES_HORARIO ? HIGH : LOW;
  int dir2 = SENTIDO_PRIMERO_ES_HORARIO ? LOW  : HIGH;
  String nom1 = SENTIDO_PRIMERO_ES_HORARIO ? "HORARIO"     : "ANTIHORARIO";
  String nom2 = SENTIDO_PRIMERO_ES_HORARIO ? "ANTIHORARIO" : "HORARIO";

  Serial.print("[CONFIG] Paso="); Serial.print(PASO_ANGULO_DEG);
  Serial.print((char)176);
  Serial.print(" | Pos="); Serial.print(POSICIONES);
  Serial.print(" | PasosVuelta="); Serial.println(PASOS_VUELTA);

  Serial.print("[INICIO] Espera inicial ");
  Serial.print(TIEMPO_ESPERA_INICIAL_MS / 1000.0); Serial.println(" s...");
  delay(TIEMPO_ESPERA_INICIAL_MS);
  Serial.println("[INICIO] Comenzando.\n");

  ciclosDir(dir1, nom1);

  if (MEDIR_AMBOS_SENTIDOS) {
    if (ESPERA_ENTRE_SENTIDOS_MS > 0) {
      Serial.print("Cambio de sentido. Espera ");
      Serial.print(ESPERA_ENTRE_SENTIDOS_MS / 1000.0); Serial.println(" s...");
      delay(ESPERA_ENTRE_SENTIDOS_MS);
    }
    ciclosDir(dir2, nom2);
  }

  Serial.println("=== Secuencia completada ===");
  Serial.println("Puedes detener la grabacion del sensor.");
  digitalWrite(ENABLE, HIGH);
}

// -- Modo manual ---------------------------------------------------
void procesarManual(const String &cmd) {
  if (cmd == "H") {
    Serial.print("Moviendo "); Serial.print(PASO_ANGULO_DEG);
    Serial.println(" deg horario...");
    moverUnPaso(HIGH); direccion = HIGH;
    Serial.println("OK");

  } else if (cmd == "A") {
    Serial.print("Moviendo "); Serial.print(PASO_ANGULO_DEG);
    Serial.println(" deg antihorario...");
    moverUnPaso(LOW); direccion = LOW;
    Serial.println("OK");

  } else if (cmd.length() >= 2 && (cmd[0] == '+' || cmd[0] == '-')) {
    bool hor = (cmd[0] == '+');
    long pasos = cmd.substring(1).toInt();
    if (pasos > 0 && pasos <= 80000) {
      Serial.print("Moviendo "); Serial.print(pasos);
      Serial.println(hor ? " pasos horario..." : " pasos antihorario...");
      mover(pasos, hor ? HIGH : LOW);
      Serial.println("OK");
    } else {
      Serial.println("ERROR: valor fuera de rango (1-80000)");
    }

  } else if (cmd == "E") {
    digitalWrite(ENABLE, LOW);  motorOn = true;  Serial.println("Motor ON");
  } else if (cmd == "S") {
    digitalWrite(ENABLE, HIGH); motorOn = false; Serial.println("Motor OFF");

  } else if (cmd == "?") {
    Serial.println("=== Estado ===");
    Serial.print("Paso: "); Serial.print(PASO_ANGULO_DEG); Serial.println(" deg");
    Serial.print("Posiciones/vuelta: "); Serial.println(POSICIONES);
    Serial.print("Pasos/vuelta: "); Serial.println(PASOS_VUELTA);
    Serial.print("Ciclos config: "); Serial.println(CICLOS);
    Serial.print("Espera inicial: "); Serial.print(TIEMPO_ESPERA_INICIAL_MS); Serial.println(" ms");
    Serial.print("Espera posicion: "); Serial.print(TIEMPO_ESPERA_POSICION_MS); Serial.println(" ms");
    Serial.print("Sentido primero: ");
    Serial.println(SENTIDO_PRIMERO_ES_HORARIO ? "HORARIO" : "ANTIHORARIO");
    Serial.print("Ambos sentidos: "); Serial.println(MEDIR_AMBOS_SENTIDOS ? "SI" : "NO");
    Serial.print("Motor: "); Serial.println(motorOn ? "ON" : "OFF");
    Serial.print("Pos actual: "); Serial.println(pasoActual);
    Serial.println("Cmds: H A +N -N E S ?");
  } else {
    Serial.println("? Cmds: H=hor A=anti +N/-N=pasos E=on S=off ?=info");
  }
}

// ===============================================================
//  SETUP
// ===============================================================
void setup() {
  pinMode(ENABLE, OUTPUT);
  pinMode(Y_DIR,  OUTPUT);
  pinMode(Y_STEP, OUTPUT);
  digitalWrite(ENABLE, LOW);

  Serial.begin(9600);
  delay(100);

  Serial.println("=========================================");
  Serial.println("  ILD1302 AUTOMATICO  |  Plato rotatorio");
  Serial.println("=========================================");
  Serial.print("  Paso="); Serial.print(PASO_ANGULO_DEG); Serial.print(" deg | ");
  Serial.print("Pos="); Serial.print(POSICIONES); Serial.print(" | ");
  Serial.print("PasosVuelta="); Serial.println(PASOS_VUELTA);
  Serial.println("Esperando 5s...");
  Serial.println("  -> Cualquier tecla = modo MANUAL");
  Serial.println("  -> Sin respuesta   = modo AUTONOMO");

  unsigned long t0 = millis();
  bool detectado = false;
  while (millis() - t0 < DETECT_MS) {
    String cmd;
    if (leerLinea(cmd)) {
      modo = MODO_MANUAL;
      Serial.println("=========================================");
      Serial.println("  MODO MANUAL activo");
      Serial.println("  H=hor  A=anti  +N/-N=pasos");
      Serial.println("  E=on  S=off  ?=info");
      Serial.println("=========================================");
      detectado = true;
      break;
    }
    delay(50);
  }

  if (!detectado) {
    modo = MODO_AUTONOMO;
    Serial.println("=========================================");
    Serial.println("  MODO AUTONOMO");
    Serial.println("=========================================");
    ejecutarAutonomo();
    modo = MODO_MANUAL;
    Serial.println("Autonomo completado. Modo manual activo.");
  }
}

// ===============================================================
//  LOOP
// ===============================================================
void loop() {
  String cmd;
  if (!leerLinea(cmd)) return;
  if (cmd.length() == 0) return;
  procesarManual(cmd);
}
