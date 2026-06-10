#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include "HX711.h"
#include "time.h"
#include "DHT.h"

// Cfg Wi-fi
const char* ssid = "WIFI"; // Altere aqui
const char* password = "WIFI_PASSWORD"; // Altere aqui
const char* botToken = "BOT_TOKEN"; // Altere aqui
const char* chatId = "CHAT_ID"; // Altere aqui

// Cfg Balanca
const int pinoDT_balanca = PINO_DT;  // Altere aqui
const int pinoSCK_balanca = PINO_SCK;  // Altere aqui
const float fatorCalibracao = FATOR_CALIBRACAO; // Altere aqui
const float pesoMinimo = PESO_MINIMO; // kg

// Cfg DHT
const int pinoDHT = PINO_DHT; // Altere aqui
#define tipoDHT DHT22 // Altere se necessário

// Cfg tempo / hora
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -10800; // GMT-3
const int   daylightOffset_sec = 0;  // Horário de verão

// Cfg tempo na caixa
unsigned long tempoEntrada = 0;
unsigned long tempoTotalDia = 0;
unsigned long tempoMaximo = 7200000; // 2 horas em ms

HX711 balanca; 
WiFiClientSecure client; 
UniversalTelegramBot bot(botToken, client); 
DHT dht(pinoDHT, tipoDHT); 

// Variáveis de controle de tempo e estado
int estadoAnteriorMinerva = 0; // Fora = 0; Dentro = 1
bool alertaDuasHorasEnviado = false; 
unsigned long tempoUltimaChecagemTelegram = 0;
const unsigned long intervaloTelegram = 2000; 

// Variáveis de controle de monitoramento
bool monitoramentoAtivo = true; // Controla se o sistema está medindo ou em pausa
int diaDoUltimoReset = -1;

void conectarWiFi() {
  //Serial.print("Conectando ao Wi-Fi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    //Serial.print(".");
  }
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer); 
  //Serial.println("\nWi-fi conectado");
}

String formataTempo(unsigned long tempo) { 
  int totalSegundos = tempo / 1000;
  int horas = totalSegundos / 3600;
  int minutos = (totalSegundos % 3600) / 60;
  int segundos = (totalSegundos % 3600) % 60; 
  
  String tempoFormatado;
  if (horas == 0) {
    tempoFormatado = String(minutos) + "m " + String(segundos) + "s";
  } else {
    tempoFormatado = String(horas) + "h " + String(minutos) + "m " + String(segundos) + "s";
  }
  return tempoFormatado;
}

void verificarMensagensTelegram() {
  int numNovasMensagens = bot.getUpdates(bot.last_message_received + 1);

  while (numNovasMensagens) {
    for (int i = 0; i < numNovasMensagens; i++) {
      String chat_id = String(bot.messages[i].chat_id);
      
      if (chat_id == chatId) {
        String text = bot.messages[i].text;

        if (text == "/status") {
          String resposta;
          
          if (!monitoramentoAtivo) {
            String naCaixa = estadoAnteriorMinerva==1? "está" : "não está";
            resposta = "O monitoramento está pausado para a noite.\nA Minerva " + naCaixa + " dormindo na caixa.";
          } else {
              if (estadoAnteriorMinerva == 1) { 
              float temperaturaStatus = dht.readTemperature(); 
              unsigned long tempoStatus = millis() - tempoEntrada;
              
              resposta = "A Minerva está há " + formataTempo(tempoStatus) + " dentro da caixa.\n";
              resposta += "Temperatura na caixa: " + String(temperaturaStatus, 1) + " °C\n";
              resposta += "Hoje ela já ficou " + formataTempo(tempoTotalDia + tempoStatus) + " dentro da caixa.";
            } else {
              resposta = "A Minerva está fora da caixa.\n";
              resposta += "Hoje ela já ficou " + formataTempo(tempoTotalDia) + " dentro da caixa.";
            }
          }
          bot.sendMessage(chatId, resposta, "");
        }
      }
    }
    numNovasMensagens = bot.getUpdates(bot.last_message_received + 1);
  }
}

void setup() {
  Serial.begin(115200);
  
  balanca.begin(pinoDT_balanca, pinoSCK_balanca); 
  balanca.read(); 
  balanca.set_scale(fatorCalibracao);
  
  dht.begin(); 
  
  //Serial.println("Zerando a balança (Tara)...");
  balanca.tare(); 
  //Serial.println("Tara concluída!");

  client.setInsecure();
  conectarWiFi();
  
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    diaDoUltimoReset = timeinfo.tm_mday;
    // Se ligar o ESP32 de noite (entre 20h e 04h), já inicia pausado
    if (timeinfo.tm_hour >= 20 || timeinfo.tm_hour < 4) {
      monitoramentoAtivo = false;
    }
  }

  bot.sendMessage(chatId, "Sistema de monitoramento da Minerva está online!", "");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    conectarWiFi();
  }


  struct tm timeinfo;
  getLocalTime(&timeinfo);
  int horaAtual = timeinfo.tm_hour;
  int diaAtual = timeinfo.tm_mday;

  // // === Monioramento de ruído em qualquer horário ===
  // if (digitalRead(pinoRuido) == LOW) {
  //   if (millis() - tempoUltimoAlertaRuido >= intervaloRuido) {
  //     bot.sendMessage(chatId, "Barulho detectado na caixa!", "");
  //     tempoUltimoAlertaRuido = millis(); // Atualiza a trava de tempo
  //   }
  // }

  // === Desativando o monitoramento e gerando um relatório final ===
  if (horaAtual >= 20 && monitoramentoAtivo) {
    monitoramentoAtivo = false;
    // Relatório de quanto tempo ela passou na caixa, se ela está na caixa e a temperatura
    String msgNoite;
    String naCaixa = estadoAnteriorMinerva==1? "Sim" : "Não";
    unsigned long tempoRel = millis() - tempoEntrada;
    msgNoite = "Período noturno inciado.\n------------\nRelatório do dia:\n";
    msgNoite += "\nTempo total de caixa: " + formataTempo(tempoTotalDia+tempoRel);
    msgNoite += "\nTemperatura da noite: " + String(dht.readTemperature()) + " °C";
    msgNoite += "\nMinerva está na caixa? " + naCaixa;
    msgNoite += "\n------------\nA partir de agora o monitoramento está pausado.";
    bot.sendMessage(chatId, msgNoite, "");
  }

  // === Ativação do sistema após 04h
  if (!monitoramentoAtivo && (horaAtual >= 4 && horaAtual < 20)) {
    
    if (balanca.is_ready()) {
      float pesoAtual = balanca.get_units(3)/1000;
      
      if (pesoAtual < pesoMinimo) { 
        delay(200);
        if (balanca.get_units(3)/1000 < pesoMinimo) {
          
          // Ativação das variáveis - Primeira saída
          monitoramentoAtivo = true;
          tempoTotalDia = 0;
          estadoAnteriorMinerva = 0;  
          alertaDuasHorasEnviado = false;
          diaDoUltimoReset = diaAtual;
          
          bot.sendMessage(chatId, "A Minerva acabou de sair da caixa. Monitoramento diário iniciado!", "");
        }
      }
    }
  }

  // === Monitoramento após a primeria saída ===
  if (monitoramentoAtivo) {
    
    if (balanca.is_ready()) {
      float pesoAtual = balanca.get_units(3)/1000; 
      ////Serial.print("\n" + String(pesoAtual));

      // Entrada
      if (pesoAtual >= pesoMinimo && estadoAnteriorMinerva == 0) {
        delay(200); 
        if (balanca.get_units(3)/1000 >= pesoMinimo) { 
          estadoAnteriorMinerva = 1;
          alertaDuasHorasEnviado = false; 
          tempoEntrada = millis();
          
          float temperaturaEntrada = dht.readTemperature();
          String msgEntrada = "A Minerva entrou na caixa!\nTemperatura atual: " + String(temperaturaEntrada, 1) + " °C";
          bot.sendMessage(chatId, msgEntrada, "");
        }
      }
      // Saída
      else if (pesoAtual < pesoMinimo && estadoAnteriorMinerva == 1) {
        delay(200); 
        if (balanca.get_units(3)/1000 < pesoMinimo) {
          estadoAnteriorMinerva = 0;
          unsigned long tempoEstadia = millis() - tempoEntrada;
          tempoTotalDia += tempoEstadia; 
          
          String msgSaida = "A Minerva saiu da caixa de transporte.\nTempo presa: " + formataTempo(tempoEstadia);
          bot.sendMessage(chatId, msgSaida, "");
        }
      }
    }

    // Regra do Alerta de 2 horas
    if (estadoAnteriorMinerva == 1 && !alertaDuasHorasEnviado) {
      if (millis() - tempoEntrada >= tempoMaximo) {
        bot.sendMessage(chatId, "⚠️ Atenção! A Minerva está na caixa há mais de 2 horas!", "");
        alertaDuasHorasEnviado = true; 
      }
    }
    
  }

  // === Verificação de mensagem no Telegram ===
  if (millis() - tempoUltimaChecagemTelegram > intervaloTelegram) {
    verificarMensagensTelegram();
    tempoUltimaChecagemTelegram = millis();
  }

  delay(100); 
}
