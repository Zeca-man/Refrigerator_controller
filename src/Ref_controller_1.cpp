/*
ENvio de email no powerup
Envio de email acima de 10C e abaixo de 0C a cada 1 hora
Gravando na EEProm com sucessoz 
*/

#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <Update.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "ESP_Mail_Client.h"
#include "Preferences.h"
#include "time.h" 

Preferences preferences;

 //Set your Static IP address
IPAddress local_IP(192, 168, 1, 184);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 0, 0);
IPAddress primaryDNS(8, 8, 8, 8);   // optional
IPAddress secondaryDNS(8, 8, 4, 4); // optional

//CONFIGURAÇÃO DA BIBLIOTECA DE HORAS

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -10800;
const int   daylightOffset_sec = 0;


// CONFIGURACAO EMAIL
#define AUTHOR_EMAIL    "esp32teste1234@gmail.com"
#define AUTHOR_PASSWORD "siqh lxfx fqvl xvdt"
#define EMAIL_DESTINO "jose.jadlog@gmail.com"
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT esp_mail_smtp_port_587

const char* ssid = "Boituva_2";
const char* password = "zecalindo2023";
const char* otaHostname = "geladeira-esp32";
const char* otaUsername = "admin";
const char* otaPassword = "admin";
 
// Define the SMTP Session object which used for SMTP transsport
SMTPSession smtp;
// Define the session config data which used to store the TCP session configuration
ESP_Mail_Session session;

/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status);
void envioemail(String mensagemEmail, String assuntoEmail);
void envioemailHtml(String mensagemEmail, String htmlEmail, String assuntoEmail);
const char rootCACert[] PROGMEM = "-----BEGIN CERTIFICATE-----\n"
                                  "-----END CERTIFICATE-----\n";

String nomeArquivoCompilado() {
  String arquivo = __FILE__;
  int posBarra = arquivo.lastIndexOf('/');
  int posContraBarra = arquivo.lastIndexOf('\\');
  int posSeparador = max(posBarra, posContraBarra);

  if (posSeparador >= 0) {
    return arquivo.substring(posSeparador + 1);
  }

  return arquivo;
}

// Default Threshold Temperature Value
String inputMessage = "6.0";
String lastTemperature;

// Definição de constantes e diversos
unsigned long previousMillis1 = 0;
long intervaloEnviarEmail = 3600000; //86400000 ms => 24hs
 

// HTML web page to handle the temperature control buttons
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html lang="pt-BR"><head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1">
  <title>Controle da Geladeira</title>
  <style>
    html, body { margin: 0; padding: 0; min-height: 100%; font-family: Arial, Helvetica, sans-serif; background: #FFFFFF; color: #111827; }
    body { display: flex; justify-content: center; align-items: center; padding: 16px; }
    .card { width: 100%; max-width: 460px; border-radius: 28px; background: #F8FAFC; box-shadow: 0 22px 44px rgba(15, 23, 42, 0.08); padding: 24px; box-sizing: border-box; }
    .header { text-align: center; margin-bottom: 22px; }
    .header h2 { margin: 0; font-size: 22px; letter-spacing: 0.18em; text-transform: uppercase; color: #0F172A; }
    .status { text-align: center; padding: 22px 0; }
    .status .label { font-size: 14px; color: #64748B; margin-bottom: 8px; }
    .status .temp { font-size: 62px; font-weight: 800; line-height: 1; color: #0B77FF; }
    .button-row { display: flex; gap: 12px; justify-content: space-between; margin-bottom: 20px; }
    .mode-btn { flex: 1; min-height: 64px; border: 1px solid transparent; border-radius: 18px; background: #FFFFFF; color: #0F172A; font-size: 15px; font-weight: 700; display: flex; align-items: center; justify-content: center; gap: 8px; cursor: pointer; transition: transform 0.14s ease, background 0.14s ease, border-color 0.14s ease; }
    .mode-btn.selected { background: #00A896; color: #FFFFFF; box-shadow: 0 12px 28px rgba(0, 168, 150, 0.16); }
    .mode-btn:active { transform: scale(0.98); }
    .goal { text-align: center; margin-bottom: 20px; }
    .goal .title { font-size: 13px; color: #64748B; text-transform: uppercase; letter-spacing: 0.14em; margin-bottom: 6px; }
    .goal .value { font-size: 32px; font-weight: 700; color: #111827; }
    .message { font-size: 13px; color: #475569; text-align: center; line-height: 1.5; margin-top: 12px; }
  </style>
</head><body>
  <div class="card">
    <div class="header">
      <h2>IOT_GELADEIRA</h2>
    </div>
    <div class="status">
      <div class="label">Temperatura atual</div>
      <div class="temp">%TEMPERATURE% &deg;C</div>
    </div>
    <div class="button-row">
      <button type="button" class="mode-btn" data-value="6">🌡️ Frio</button>
      <button type="button" class="mode-btn selected" data-value="4">❄️ Super Frio</button>
      <button type="button" class="mode-btn" data-value="2">🧊 Megafrio</button>
    </div>
    <div class="goal">
      <div class="title">Temperatura selecionada</div>
      <div class="value" id="goal">%THRESHOLD% &deg;C</div>
    </div>
    <div class="message">Toque em um modo para aplicar imediatamente a nova temperatura alvo. O valor será enviado ao dispositivo e mostrado na tela.</div>
  </div>
  <script>
    const buttons = document.querySelectorAll('.mode-btn');
    const goalValue = document.getElementById('goal');
    const currentThreshold = parseFloat('%THRESHOLD%');

    function setThreshold(value) {
      buttons.forEach(btn => btn.classList.toggle('selected', btn.dataset.value === value));
      goalValue.textContent = value + ' °C';
      fetch('/get?threshold_input=' + encodeURIComponent(value), { method: 'GET' });
    }

    buttons.forEach(btn => {
      btn.addEventListener('click', () => setThreshold(btn.dataset.value));
      if (parseFloat(btn.dataset.value).toFixed(1) === currentThreshold.toFixed(1)) {
        btn.classList.add('selected');
      }
    });

    setThreshold(currentThreshold.toFixed(1));
  </script>
</body></html>)rawliteral";

const char ota_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>OTA Update</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
</head><body>
  <h2>Atualizacao OTA1 - Geladeira</h2>
  <form method="POST" action="/update" enctype="multipart/form-data">
    <input type="file" name="update" required>
    <input type="submit" value="Atualizar">
  </form>
</body></html>)rawliteral";

WebServer server(80);

void notFound() {
  server.send(404, "text/plain", "Not found");
}

// Replaces placeholder with DS18B20 values
String processor(const String& var){
  //Serial.println(var);
  if(var == "TEMPERATURE"){
    return lastTemperature;
  }
  else if(var == "THRESHOLD"){
    return inputMessage;
  }
  return String();
}

String buildIndexPage() {
  String page = FPSTR(index_html);
  page.replace("%TEMPERATURE%", processor("TEMPERATURE"));
  page.replace("%THRESHOLD%", processor("THRESHOLD"));
  return page;
}

String buildPowerUpEmailHtml(String ip, String mac, String temperature) {
  String html = F("<!DOCTYPE HTML><html lang=\"pt-BR\"><head>"
                  "<meta charset=\"utf-8\">"
                  "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
                  "</head><body style=\"margin:0;padding:0;background:#eef2f7;color:#111827;font-family:Arial,Helvetica,sans-serif;\">"
                  "<div style=\"max-width:560px;margin:0 auto;padding:22px;\">"
                  "<div style=\"background:#ffffff;border-radius:16px;overflow:hidden;border:1px solid #dbe3ee;box-shadow:0 14px 34px rgba(15,23,42,0.10);\">"
                  "<div style=\"background:#0f766e;color:#ffffff;padding:18px 22px;\">"
                  "<div style=\"font-size:12px;letter-spacing:0.14em;text-transform:uppercase;opacity:0.86;\">PowerUp</div>"
                  "<h1 style=\"font-size:24px;line-height:1.2;margin:6px 0 0 0;\">Geladeira online</h1>"
                  "</div>"
                  "<div style=\"padding:22px;\">"
                  "<table role=\"presentation\" width=\"100%\" cellspacing=\"0\" cellpadding=\"0\" style=\"border-collapse:collapse;margin-bottom:20px;\">"
                  "<tr><td style=\"padding:9px 0;color:#64748b;font-size:13px;\">IP do aparelho</td><td style=\"padding:9px 0;text-align:right;font-size:14px;font-weight:700;\"><a href=\"http://");
  html += ip;
  html += F("\" style=\"color:#0b77ff;text-decoration:none;\">");
  html += ip;
  html += F("</a></td></tr>"
            "<tr><td style=\"padding:9px 0;color:#64748b;font-size:13px;border-top:1px solid #e5e7eb;\">MAC do ESP32</td><td style=\"padding:9px 0;text-align:right;font-size:14px;font-weight:700;border-top:1px solid #e5e7eb;\">");
  html += mac;
  html += F("</td></tr>"
            "</table>"
            "<div style=\"font-size:13px;color:#64748b;text-transform:uppercase;letter-spacing:0.12em;margin-bottom:10px;\">Tela exibida no IP</div>"
            "<div style=\"background:#f8fafc;border-radius:14px;border:1px solid #e2e8f0;padding:20px;text-align:center;\">"
            "<div style=\"font-size:20px;letter-spacing:0.16em;text-transform:uppercase;font-weight:700;color:#0f172a;margin-bottom:20px;\">IOT_GELADEIRA</div>"
            "<div style=\"font-size:13px;color:#64748b;margin-bottom:6px;\">Temperatura atual</div>"
            "<div style=\"font-size:54px;line-height:1;font-weight:800;color:#0b77ff;margin-bottom:18px;\">");
  html += temperature;
  html += F(" &deg;C</div>"
            "<table role=\"presentation\" width=\"100%\" cellspacing=\"0\" cellpadding=\"0\" style=\"border-collapse:separate;border-spacing:8px;margin-bottom:18px;\"><tr>"
            "<td style=\"background:#ffffff;border:1px solid #e2e8f0;border-radius:12px;padding:14px 8px;text-align:center;font-size:13px;font-weight:700;color:#0f172a;\">Frio</td>"
            "<td style=\"background:#00a896;border:1px solid #00a896;border-radius:12px;padding:14px 8px;text-align:center;font-size:13px;font-weight:700;color:#ffffff;\">Super Frio</td>"
            "<td style=\"background:#ffffff;border:1px solid #e2e8f0;border-radius:12px;padding:14px 8px;text-align:center;font-size:13px;font-weight:700;color:#0f172a;\">Megafrio</td>"
            "</tr></table>"
            "<div style=\"font-size:12px;color:#64748b;text-transform:uppercase;letter-spacing:0.12em;margin-bottom:6px;\">Temperatura selecionada</div>"
            "<div style=\"font-size:30px;font-weight:700;color:#111827;\">");
  html += inputMessage;
  html += F(" &deg;C</div>"
            "</div>"
            "</div></div></div></body></html>");
  return html;
}

// Flag variable to keep track if triggers was activated or not
bool triggerActive = true;
bool poweron = true;
bool flagenvioemailemerg = true;

const char* PARAM_INPUT_1 = "threshold_input";

unsigned long previousMillis = 0;     
const long interval = 5000;    // Interval between sensor readings.
const int vcc_ds1820 =  4;   // GPIO que alimenta o sensor de temperatura
const int output = 26;  // GPIO where the output is connected to
const int oneWireBus = 13;     // GPIO where the DS18B20 is connected to

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);
// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);

void setup() {
  Serial.begin(115200);
  Serial.println(__FILE__);
//      // Configures static IP address
//      if (!WiFi.config(local_IP, gateway, subnet)) {
//      Serial.println("STA Failed to configure");
//      }

// Desliga o sensor 1820
   pinMode(vcc_ds1820, OUTPUT);
   digitalWrite(vcc_ds1820, LOW);
   
// Leitura da EEprom para atualizar a variavel inoutMessage
  preferences.begin("MemEProm", false);
  String teste1 = preferences.getString("Temperatura");
  inputMessage = teste1;
  Serial.println("dado obtido da var InputMessage / EEPROM ->> " + inputMessage );
  preferences.end();

  //WiFi.config(local_IP, gateway,subnet , primaryDNS, secondaryDNS );           // Força o IP na conexão.
  WiFi.mode(WIFI_STA);WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.println("WiFi connected.");
  Serial.print("IP ");
  Serial.println(WiFi.localIP());
  Serial.println();

  if (!MDNS.begin(otaHostname)) {
    Serial.println("Erro ao iniciar mDNS para OTA");
  }
  else {
    Serial.print("mDNS OTA ativo em ");
    Serial.print(otaHostname);
    Serial.println(".local");
  }

  // Liga o sensor 1820
  pinMode(output, OUTPUT);
  digitalWrite(vcc_ds1820, HIGH);
  delay(500);
  // Start the DS18B20 sensor
  sensors.begin();


    
  // Send web page to client
  server.on("/", HTTP_GET, [](){
    server.send(200, "text/html", buildIndexPage());
  });

  // Receive an HTTP GET request at <ESP_IP>/get?threshold_input=<inputMessage>
  server.on("/get", HTTP_GET, []() {
    if (server.hasArg(PARAM_INPUT_1)) {
      inputMessage = server.arg(PARAM_INPUT_1);
    }
    Serial.println("Novo threshold: " + inputMessage);
    server.send(200, "text/plain", "OK");
  });

  server.on("/ota", HTTP_GET, []() {
    if (!server.authenticate(otaUsername, otaPassword)) {
      return server.requestAuthentication();
    }
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", FPSTR(ota_html));
  });

  server.on("/serverIndex", HTTP_GET, []() {
    if (!server.authenticate(otaUsername, otaPassword)) {
      return server.requestAuthentication();
    }
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", FPSTR(ota_html));
  });

  server.on("/update", HTTP_POST, []() {
    if (!server.authenticate(otaUsername, otaPassword)) {
      return server.requestAuthentication();
    }
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", Update.hasError() ? "FAIL" : "OK");
    delay(1000);
    ESP.restart();
  }, []() {
    if (!server.authenticate(otaUsername, otaPassword)) {
      return;
    }

    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        Update.printError(Serial);
      }
    }
    else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    }
    else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) {
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      }
      else {
        Update.printError(Serial);
      }
    }
  });

  server.onNotFound(notFound);
  server.begin();

    String ip = WiFi.localIP().toString();
    delay(2000);
    sensors.requestTemperatures();
    float temperature = sensors.getTempCByIndex(0);
    lastTemperature = String(temperature, 1);
    String mac = WiFi.macAddress();
    String textoparaemail = String("PowerUp da Geladeira\n\n") +
                            "IP do aparelho: " + ip + "\n" +
                            "MAC do ESP32: " + mac + "\n" +
                            "Temperatura atual: " + lastTemperature + " C\n" +
                            "Temperatura selecionada: " + inputMessage + " C\n\n" +
                            "Acesse: http://" + ip;
    envioemailHtml(textoparaemail, buildPowerUpEmailHtml(ip, mac, lastTemperature), "PowerUp da Geladeira");
}

void loop() {   
  server.handleClient();

   unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    sensors.requestTemperatures();
    float temperature = sensors.getTempCByIndex(0);
    Serial.print(temperature);
    Serial.println(" *C");
    
    // Grava a variavel inputMessage na EEProm
      preferences.begin("MemEProm");
      preferences.putString("Temperatura", inputMessage);
      Serial.println("Gravado -> " + inputMessage);
      preferences.end();
    
    lastTemperature = String(temperature);
    float TemperaturaCorrigidaMais = float (inputMessage.toFloat()+ 1);
    float TemperaturaCorrigidaMenos = float (inputMessage.toFloat() - 1);
    Serial.println(TemperaturaCorrigidaMais);
    Serial.println(TemperaturaCorrigidaMenos);

    // Check if temperature is above threshold and if it needs to trigger output
    if(temperature > TemperaturaCorrigidaMais && !triggerActive ){
     
      String message = String("Temperature above threshold. Current temperature: ") + 
                        String(temperature) + String("C");
      Serial.println(message);
      triggerActive = true;
      digitalWrite(output, LOW);
      Serial.println("Low");
    }

    // Check if temperature is below threshold and if it needs to trigger output
    else if((temperature < TemperaturaCorrigidaMenos) && triggerActive) {
      String message = String("Temperature below threshold. Current temperature: ") + 
      String(temperature) + String(" C");
      Serial.println(message);
      triggerActive = false;
      digitalWrite(output, HIGH);
      Serial.println("HIGH");
    }
  
    if((temperature > 10)  && flagenvioemailemerg) {
      String message = String("Temperatura acima de 10 graus - verificar geladeira - ") + String(temperature) + String(" C");
      Serial.println(message);
      flagenvioemailemerg = false;
      envioemail (message, message);
    }
      if((temperature < 0)  && flagenvioemailemerg) {
      String message = String("Temperatura abaixo de 0 graus - verificar geladeira - ") + String(temperature) + String(" C");
      Serial.println(message);
      flagenvioemailemerg = false;
      envioemail (message, message);
    }
  }
  unsigned long currentMillis1 = millis();
  if (currentMillis1 - previousMillis1 >= intervaloEnviarEmail) {
    previousMillis1 = currentMillis1;
    flagenvioemailemerg = true;
  }
}

/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status)
{
    /* Print the current status */
    Serial.println(status.info());
    /* Print the sending result */
    if (status.success())
    {
        // MailClient.printf used in the examples is for format printing via debug Serial port
        // that works for all supported Arduino platform SDKs e.g. SAMD, ESP32 and ESP8266.
        // In ESP8266 and ESP32, you can use Serial.printf directly.
        Serial.println("----------------");
        MailClient.printf("Message sent success: %d\n", status.completedCount());
        MailClient.printf("Message sent failed: %d\n", status.failedCount());
        Serial.println("----------------\n");
        for (size_t i = 0; i < smtp.sendingResult.size(); i++)
        {
            /* Get the result item */
            SMTP_Result result = smtp.sendingResult.getItem(i);
            // In case, ESP32, ESP8266 and SAMD device, the timestamp get from result.timestamp should be valid if
            // your device time was synched with NTP server.
            // Other devices may show invalid timestamp as the device time was not set i.e. it will show Jan 1, 1970.
            // You can call smtp.setSystemTime(xxx) to set device time manually. Where xxx is timestamp (seconds since Jan 1, 1970)
            MailClient.printf("Message No: %d\n", i + 1);
            MailClient.printf("Status: %s\n", result.completed ? "success" : "failed");
            MailClient.printf("Date/Time: %s\n", MailClient.Time.getDateTimeString(result.timestamp, "%B %d, %Y %H:%M:%S").c_str());
            MailClient.printf("Recipient: %s\n", result.recipients.c_str());
            MailClient.printf("Subject: %s\n", result.subject.c_str());
        }
        Serial.println("----------------\n");
        // You need to clear sending result as the memory usage will grow up.
        smtp.sendingResult.clear();
    }
}

void envioemail(String mensagemEmail , String assuntoEmail ) {
    envioemailHtml(mensagemEmail, String(), assuntoEmail);
}

void envioemailHtml(String mensagemEmail, String htmlEmail, String assuntoEmail) {

    /*  Set the network reconnection option */
    MailClient.networkReconnect(true);

    /** Enable the debug via Serial port
     * 0 for no debugging
     * 1 for basic level debugging
     *
     * Debug port can be changed via ESP_MAIL_DEFAULT_DEBUG_PORT in ESP_Mail_FS.h
     */
    smtp.debug(0);

    /* Set the callback function to get the sending results */
    smtp.callback(smtpCallback);
    /* Declare the Session_Config for user defined session credentials */
    Session_Config config;
    /* Set the session config */
    config.server.host_name = SMTP_HOST;
    config.server.port = SMTP_PORT;
    config.login.email = AUTHOR_EMAIL;
    config.login.password = AUTHOR_PASSWORD;

    config.login.user_domain = F("127.0.0.1");

    config.time.ntp_server = F("pool.ntp.org,time.nist.gov");
    config.time.gmt_offset = 3;
    config.time.day_light_offset = 0;

    /* Declare the message class */
    SMTP_Message message;

    /* Set the message headers */
    message.sender.name = F("Geladeira");
    message.sender.email = AUTHOR_EMAIL;
    message.subject = assuntoEmail;
    message.addRecipient(F("Someone"), EMAIL_DESTINO);
    message.text.flowed = true;
    message.text.content = mensagemEmail;
    message.text.charSet = F("us-ascii");
    message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
    if (htmlEmail.length() > 0) {
        message.html.content = htmlEmail;
        message.html.charSet = F("utf-8");
        message.html.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
    }
    message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_low;
    message.addHeader(F("Message-ID: <abcde.fghij@gmail.com>"));

    /* Connect to the server */
    if (!smtp.connect(&config))
    {
        MailClient.printf("Connection error, Status Code: %d, Error Code: %d, Reason: %s\n", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
        return;
    }
    if (!smtp.isLoggedIn())
    {
        Serial.println("Not yet logged in.");
    }
    else
    {
        if (smtp.isAuthenticated())
            Serial.println("Successfully logged in.");
        else
            Serial.println("Connected with no Auth.");
    }
    /* Start sending Email and close the session */
    if (!MailClient.sendMail(&smtp, &message))
        MailClient.printf("Error, Status Code: %d, Error Code: %d, Reason: %s\n", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
    // to clear sending result log
    // smtp.sendingResult.clear();

    MailClient.printf("Free Heap: %d\n", MailClient.getFreeHeap());
    }


    
