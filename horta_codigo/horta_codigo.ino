/***************************************************

SISTEMA — BLYNK

***************************************************/

// Definições do template Blynk
#define BLYNK_TEMPLATE_ID "TMPL2575u97VR"
#define BLYNK_TEMPLATE_NAME "Monitoramento da Horta"
#define BLYNK_AUTH_TOKEN "uYJoTFU9e1kDJVbN8aBj21dqdq0EUHyR"

// Bibliotecas necessárias
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#define BLYNK_PRINT Serial

// --------------- CONFIGURAÇÃO DE REDE ---------------
char ssid[] = "celularRafael";  // Nome da rede WiFi
char pass[] = "123456780"; // Senha da rede WiFi

// --------------- DEFINIÇÃO DE PINOS ---------------
// Relés/pinos (D5 e D1)
const int RELAY_MOTOR_FECHAR = D5;   // Motor 1 -> FECHAR sombrite
const int RELAY_MOTOR_ABRIR  = D1;   // Motor 2 -> ABRIR sombrite

// true  -> relé ativo em LOW  (LIGA com digitalWrite(pin, LOW))
// false -> relé ativo em HIGH (LIGA com digitalWrite(pin, HIGH))
const bool RELAY_ACTIVE_LOW = true;

// Sensores
const byte PIN_FLUXO = D2;  // YF-S201B (saída de pulsos)
const int PIN_LDR = A0;     // Sensor de luminosidade (LDR)

// --------------- VARIÁVEIS DO SENSOR DE FLUXO ---------------
volatile unsigned long pulseCount = 0;
const float K_FACTOR = 7.5f;
float litrosTotal = 0.0f;

// --------------- VARIÁVEIS DO SISTEMA ---------------
bool sistemaHabilitado = false;   // Sistema só opera após primeira interação do usuário
bool modoAutomatico = false;      // Controle por LDR (V0 define modo: 0=Manual, 1=Automático)

// --------------- CONTROLE DE MOTOR (NÃO-BLOQUEANTE) ---------------
bool motorAtivo = false;           // Indica se algum motor está em operação
unsigned long motorDesligaEm = 0;  // Timestamp para desligar motor
int releMotorAtivo = -1;           // Qual pino do relé está ativo (-1 = nenhum)

// --------------- ESTADO DO SOMBRITE ---------------
int estadoSombrite = 0;  // 0=indefinido, 1=aberto, 2=fechado

// --------------- VARIÁVEIS PARA CONTROLE DE ALERTAS ---------------
bool semFluxoEnviado = false;
unsigned long semFluxoTimerStart = 0;

bool fluxoBaixoEnviado = false;
unsigned long fluxoBaixoTimerStart = 0;
unsigned long ultimoFluxoBaixoEnvio = 0;
const unsigned long REPEAT_FLUXO_BAIXO_MS = 60000UL;  // Reenvia alerta a cada 60s

// --------------- LIMIARES PARA ALERTAS ---------------
const float LIMITE_SEM_FLUXO = 0.1f;   // Abaixo deste valor = sem fluxo (L/min)
const int TEMPO_SEM_FLUXO_S = 5;       // Tempo mínimo sem fluxo para gerar alerta (segundos)
const float LIMITE_FLUXO_BAIXO = 0.7f; // Abaixo deste valor = fluxo baixo (L/min)
const int TEMPO_FLUXO_BAIXO_S = 5;     // Tempo mínimo com fluxo baixo para gerar alerta (segundos)

// --------------- LIMIARES PARA CONTROLE AUTOMÁTICO (LDR) ---------------
const int LIMITE_FECHAR = 400;  // Acima deste valor (muita luz) → FECHAR sombrite
const int LIMITE_ABRIR  = 70;   // Abaixo deste valor (pouca luz) → ABRIR sombrite
unsigned long ultimaAcaoAuto = 0;  // Última ação automática registrada
const unsigned long COOLDOWN_AUTO_MS = 10000UL;  // Tempo mínimo entre ações automáticas (10s)

// --------------- OBJETOS BLYNK ---------------
BlynkTimer timer;

// --------------- FLAGS PARA EVITAR CALLBACKS INFINITOS ---------------
volatile bool ignoreV4Callback = false;
volatile bool ignoreV5Callback = false;

// --------------- TEMPOS DE ACIONAMENTO DO MOTOR ----------------
const unsigned long TEMPO_ABRIR_MS  = 500UL; // Abrir sombrite
const unsigned long TEMPO_FECHAR_MS = 1000UL;  // Fechar sombrite

// ----------------- FUNÇÕES DO SISTEMA -----------------

// Função de interrupção: conta pulsos do sensor de fluxo
void ICACHE_RAM_ATTR pulseCounter() {
  pulseCount++;
}

// Controle abstrato do relé
void setRelay(int pin, bool on) {
  if (RELAY_ACTIVE_LOW) {
    digitalWrite(pin, on ? LOW : HIGH);
  } else {
    digitalWrite(pin, on ? HIGH : LOW);
  }
}

// Desliga ambos os relés (segurança)
void relaysAllOff() {
  setRelay(RELAY_MOTOR_FECHAR, false);
  setRelay(RELAY_MOTOR_ABRIR, false);
}

// Inicia motor por tempo determinado (3 segundos) de forma não-bloqueante
void startMotorNonBlocking(int relePin, unsigned long tempoMs) {
  relaysAllOff();
  setRelay(relePin, true);

  releMotorAtivo = relePin;
  motorAtivo = true;
  motorDesligaEm = millis() + tempoMs;

  Serial.print("Motor ligado no rele (pin): ");
  Serial.print(relePin);
  Serial.print(" | Tempo (ms): ");
  Serial.println(tempoMs);
}

// Verifica se o tempo do motor acabou e desliga se necessário
void checkMotorTimeout() {
  if (motorAtivo && millis() >= motorDesligaEm) {
    setRelay(releMotorAtivo, false);
    Serial.println("Motor desligado (timeout).");
    motorAtivo = false;
    releMotorAtivo = -1;
  }
}

// Aciona motor com verificações de segurança (habilitação e exclusão mútua)
void acionarMotorSeguro(int relePin) {
  // Verifica se sistema está habilitado (após primeira interação)
  if (!sistemaHabilitado) {
    Serial.println("Ignorado: sistema nao habilitado (aguardando interacao).");
    return;
  }
  
  // Verifica se outro motor já está ativo
  if (motorAtivo) {
    Serial.println("Ignorado: outro motor ativo.");
    return;
  }
  
  // Inicia o motor
  if (relePin == RELAY_MOTOR_FECHAR) {
    startMotorNonBlocking(relePin, TEMPO_FECHAR_MS);
    estadoSombrite = 2;
    Blynk.logEvent("sombrite_fechou", "O sombrite foi FECHADO.");
  }
  else if (relePin == RELAY_MOTOR_ABRIR) {
    startMotorNonBlocking(relePin, TEMPO_ABRIR_MS);
    estadoSombrite = 1;
    Blynk.logEvent("sombrite_abriu", "O sombrite foi ABERTO.");
  }
  
  // Atualiza estado do sombrite e envia alerta correspondente
  if (relePin == RELAY_MOTOR_FECHAR) {
    estadoSombrite = 2;  // Fechado
    Blynk.logEvent("sombrite_fechou", "O sombrite foi FECHADO.");
  } else if (relePin == RELAY_MOTOR_ABRIR) {
    estadoSombrite = 1;  // Aberto
    Blynk.logEvent("sombrite_abriu", "O sombrite foi ABERTO.");
  }
  
  // Atualiza botão V4 no app para refletir estado atual (sem disparar callback)
  ignoreV4Callback = true;
  Blynk.virtualWrite(V4, (estadoSombrite == 1) ? 1 : 0);
  ignoreV4Callback = false;
}

// ---------------- CALLBACKS BLYNK ----------------

// Callback do botão manual V4 (0 = fechar, 1 = abrir)
BLYNK_WRITE(V4) {
  if (ignoreV4Callback) {
    // Atualização gerada pelo código interno → ignorar
    return;
  }
  
  int valor = param.asInt();
  Serial.print("V4 usuário recebeu: ");
  Serial.println(valor);
  
  // Habilita sistema na primeira interação do usuário
  if (!sistemaHabilitado) {
    sistemaHabilitado = true;
    Serial.println("Sistema habilitado (primeira interacao V4).");
  }
  
  // Ignora comandos manuais se estiver em modo automático
  if (modoAutomatico) {
    Serial.println("Modo automático ativo — botão manual ignorado.");
    // Atualiza botão para refletir estado real (evita confusão visual)
    ignoreV4Callback = true;
    Blynk.virtualWrite(V4, (estadoSombrite == 1) ? 1 : 0);
    ignoreV4Callback = false;
    return;
  }
  
  // Processa comando manual
  if (valor == 0) {
    acionarMotorSeguro(RELAY_MOTOR_FECHAR);
  } else {
    acionarMotorSeguro(RELAY_MOTOR_ABRIR);
  }
}

// Callback do seletor de modo V0 (0 = Manual, 1 = Automático)
BLYNK_WRITE(V0) {
  if (ignoreV5Callback) {
    return;
  }
  
  int valor = param.asInt();
  Serial.print("V0 usuário recebeu: ");
  Serial.println(valor);
  
  modoAutomatico = (valor != 0);
  
  // Ao alternar modos, atualiza V4 para refletir estado atual (sem disparar callback)
  ignoreV4Callback = true;
  Blynk.virtualWrite(V4, (estadoSombrite == 1) ? 1 : 0);
  ignoreV4Callback = false;
}

// ---------------- TAREFA EXECUTADA A CADA 1 SEGUNDO ----------------
void task_1s() {
  // --------------- LEITURA DO SENSOR DE FLUXO ---------------
  // Lê e zera contador de pulsos de forma segura (com interrupções desabilitadas)
  noInterrupts();
  unsigned long pulses = pulseCount;
  pulseCount = 0;
  interrupts();
  
  // Converte pulsos para vazão (L/min) e acumula volume total
  float fluxoLmin = (float)pulses / K_FACTOR;
  litrosTotal += fluxoLmin / 60.0f;
  
  // Envia vazão para Blynk (stream V1)
  Blynk.virtualWrite(V1, fluxoLmin);
  
  Serial.print("Fluxo (L/min): ");
  Serial.print(fluxoLmin);
  Serial.print("  Pulses: ");
  Serial.println(pulses);
  
  // --------------- ALERTA: SEM FLUXO (CRÍTICO) ---------------
  if (fluxoLmin <= LIMITE_SEM_FLUXO) {
    if (semFluxoTimerStart == 0) {
      semFluxoTimerStart = millis();
    }
    unsigned long diffS = (millis() - semFluxoTimerStart) / 1000UL;
    
    if (!semFluxoEnviado && diffS >= (unsigned long)TEMPO_SEM_FLUXO_S) {
      Serial.println("Enviando evento Blynk: sem_fluxo");
      Blynk.logEvent("sem_fluxo", "SEM FLUXO DE ÁGUA! Bomba sem circulacao. Verificar entupimento/energia/tubulacao.");
      semFluxoEnviado = true;
    }
  } else {
    semFluxoTimerStart = 0;
    if (semFluxoEnviado) {
      Serial.println("Fluxo normalizado -> reset alerta sem_fluxo");
      semFluxoEnviado = false;
    }
  }
  
  // --------------- ALERTA: FLUXO BAIXO (ADVERTÊNCIA) ---------------
  if (fluxoLmin > LIMITE_SEM_FLUXO && fluxoLmin < LIMITE_FLUXO_BAIXO) {
    if (fluxoBaixoTimerStart == 0) {
      fluxoBaixoTimerStart = millis();
    }
    unsigned long diffS = (millis() - fluxoBaixoTimerStart) / 1000UL;
    
    if (diffS >= (unsigned long)TEMPO_FLUXO_BAIXO_S) {
      if (!fluxoBaixoEnviado || (millis() - ultimoFluxoBaixoEnvio) >= REPEAT_FLUXO_BAIXO_MS) {
        Serial.println("Enviando evento Blynk: fluxo_baixo");
        Blynk.logEvent("fluxo_baixo", "Fluxo de agua abaixo do normal - possivel obstrucao.");
        fluxoBaixoEnviado = true;
        ultimoFluxoBaixoEnvio = millis();
      }
    }
  } else {
    fluxoBaixoTimerStart = 0;
    if (fluxoBaixoEnviado && fluxoLmin >= LIMITE_FLUXO_BAIXO) {
      fluxoBaixoEnviado = false;
    }
  }
  
  // --------------- LEITURA DO SENSOR LDR ---------------
  int valorLDR = analogRead(PIN_LDR);
  Blynk.virtualWrite(V2, valorLDR);  // Envia para stream V2
  Serial.print("LDR: ");
  Serial.println(valorLDR);
  
  // --------------- CONTROLE AUTOMÁTICO DO SOMBRITE ---------------
  if (modoAutomatico && sistemaHabilitado) {
    if (!motorAtivo && (millis() - ultimaAcaoAuto >= COOLDOWN_AUTO_MS)) {
      if (valorLDR > LIMITE_FECHAR && estadoSombrite != 2) {
        Serial.println("Auto: luz ALTA e sombrite ABERTO -> FECHAR");
        acionarMotorSeguro(RELAY_MOTOR_FECHAR);
        ultimaAcaoAuto = millis();
      }
      else if (valorLDR < LIMITE_ABRIR && estadoSombrite != 1) {
        Serial.println("Auto: luz BAIXA e sombrite FECHADO -> ABRIR");
        acionarMotorSeguro(RELAY_MOTOR_ABRIR);
        ultimaAcaoAuto = millis();
      }
    }
  }
}

// ---------------- SETUP (EXECUTADO UMA VEZ) ----------------
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\nInicializando sistema...");
  
  // Configura pinos dos relés
  pinMode(RELAY_MOTOR_FECHAR, OUTPUT);
  pinMode(RELAY_MOTOR_ABRIR, OUTPUT);
  relaysAllOff();  // Garante que relés começam desligados
  
  // Configura sensor de fluxo com interrupção
  pinMode(PIN_FLUXO, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_FLUXO), pulseCounter, FALLING);
  
  // Conecta ao Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  
  // Configura timer para executar task_1s a cada segundo
  timer.setInterval(1000L, task_1s);
  
  Serial.println("Sistema iniciado (versao corrigida).");
  Serial.print("Reles active LOW? ");
  Serial.println(RELAY_ACTIVE_LOW ? "SIM" : "NAO");
  Serial.println("Aguardando interacao do usuario...");
}

// ---------------- LOOP PRINCIPAL (EXECUTADO CONTINUAMENTE) ----------------
void loop() {
  Blynk.run();      // Processa comunicação com Blynk
  timer.run();      // Executa tarefas agendadas
  
  // Verifica timeout do motor (desliga após 3 segundos)
  checkMotorTimeout();
}