# Monitoramento de Caixa de Transporte de pet  (Caso Minerva)

Este projeto consiste em um sistema de Internet das Coisas (IoT) desenvolvido para monitorar o tempo de permanência e o conforto térmico de um pet (Minerva) dentro de sua caixa de transporte. O sistema automatiza a coleta de dados e envia updates e alertas críticos diretamente para o tutor através de um bot no Telegram.

---

## Funcionalidades

* **Detecção de Presença por Peso:** Identifica automaticamente quando o pet entra ou sai da caixa através de uma balança (4 sensores de carga).
* **Telemetria Térmica:** Monitoramento da temperatura interna do ambiente para prevenir estresse térmico.
* **Alertas de Segurança:** Envio de notificação imediata caso o pet permaneça confinado por mais de 2 horas seguidas.
* **Bloqueio Noturno:** O monitoramento entra em pausa automática entre 20h e 04h para respeitar o sono do pet, gerando um relatório consolidado do dia.
* **Interatividade (Comandos):** Permite enviar o comando `/status` no Telegram a qualquer momento para receber os dados atuais de tempo e temperatura.

---

## Hardware Utilizado

* **Microcontrolador:** ESP32 DevKit v1 (Wi-Fi nativo e sincronização de horário via protocolo NTP).
* **Sensor de Peso:** Célula de Carga acoplada ao conversor A/D HX711.
* **Sensor de Temperatura:** DHT22 (Leitura digital de alta precisão com resolução decimal).

---

## Bibliotecas Utilizadas no Código

* `WiFi.h` & `WiFiClientSecure.h` (Conexão de rede e criptografia SSL para a API do Telegram)
* `UniversalTelegramBot.h` (Interface de comunicação com o app de mensagens)
* `ArduinoJson.h` (Tratamento dos dados recebidos via API)
* `HX711.h` (Gerenciamento e calibração da célula de carga)
* `DHT.h` (Leitura do sensor de temperatura)
* `time.h` (Sincronização de relógio via servidores NTP)

---

## Como o Sistema Funciona (Lógica)

1. Ao ser iniciado, o ESP32 realiza a tara automática da balança e se conecta ao Wi-Fi para buscar a hora exata na internet.
2. O sistema monitora ativamente o peso. Se o valor superar o limite configurado, o cronômetro inicia.
3. Se o pet passar do limite de tempo seguro, um alerta com o símbolo ⚠️ é disparado no Telegram.
4. Às 20h, o sistema encerra o monitoramento, envia um resumo completo do dia (Tempo total acumulado, temperatura da noite e status atual) e entra em modo de repouso, reativando-se a partir da primeira vez que o pet sai no dia seguinte.
