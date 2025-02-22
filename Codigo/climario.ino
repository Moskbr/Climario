#include <DHT.h>
#include <Adafruit_BMP085.h>
#include <LiquidCrystal.h>
#include <time.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// Fita LED RGB
#define LED_PIN     15
#define NUM_LEDS    16
#define BRIGHTNESS  64
#define LED_TYPE    WS2812
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];
#define UPDATES_PER_SECOND 100

// Pinos de saida
#define PIN_CHUVA 14
#define PIN_NEBUL 19

// API Open-meteo retorna JSON com precipitacao e chuva (mm), velocidade (km/h) do vento e, condicao das nuvens.
// As variaveis apresentadas podem ser escolhidas no site da API: https://open-meteo.com/
String API_URL = "https://api.open-meteo.com/v1/forecast?latitude=52.52&longitude=13.41&current=precipitation,rain,weather_code,cloud_cover";

// Sensores
DHT dht(5, DHT11);
Adafruit_BMP085 bmp;
String modoAtivo = ""; // guarda o modo atual = {ENSOL, CHUVA, TEMPS, NEBLN, LOCAL}

// WiFi
const char ssid[] = "nome-da-rede";
const char password[] = "senha-da-rede";
const char* hostname = "Climario";
// Servidor WEB
AsyncWebServer server(80);
// NTP
long timezone = -3;
byte daysavetime = 1;
struct tm timeinfo;

// --------------------------------------------------------------------------------------------- VARIAVEIS


// Variaveis para controle de tempo
// Intervalo para as medidas (2 minutos = 120000ms)
const unsigned long INTERVALO = 120000;
// Intervalo para calculo da media (20 minutos = 10 requisicoes)
const int NUM_MEDIDAS = 10;
float temperatura[NUM_MEDIDAS];
float pressao[NUM_MEDIDAS];
float humidade[NUM_MEDIDAS];
int indiceFila = 0;
int totalMedidas = 0;
unsigned long ultimaMedida = 0;

// Variaveis do modo LOCAL
int setupLOCAL = 1; // para quando o modo for trocado, atualiza o clima com valores imediatos
float mediaTemp = 0.0;
float mediaHum = 0.0;
float mediaPress = 0.0;

// Variaveis da requisicao POST
String Mode; // armazena comando que vem do cliente
int houveTroca = 0; // mudara para 1 se houver requisicao POST
unsigned long ultimaTrocaModo = 0;
const int ESPERA_TROCA = 2*60000; // 2min x 60.000ms

// Variáveis a definir
String date, hour;
int cloud_cover, weather_code;
float wind_speed, rain;
// --------------------------------------------------------------------------------------------- HORA + API + MEDIAS
void atualizaDataHora()
{
  date = "dd/mm/aaaa";
  hour = "hh:mm";
  timeinfo.tm_year = 0; // since 1900; tm_mon=0 (January)
  if (!getLocalTime(&timeinfo)) {;
    Serial.println("Connection Err");
  } else {
    String date = (String(timeinfo.tm_mday) + "/" +  String((timeinfo.tm_mon) + 1) + "/" + ((timeinfo.tm_year) + 1900));
    String hour = (String(timeinfo.tm_hour) + ":" + String(timeinfo.tm_min));
  }
  Serial.println("Data: " + date + " - Hora: " + hour);
}

void leDadosAPI()
{
  HTTPClient http;
  http.begin(API_URL);
  int code = http.GET();
  if(code > 0){
    String JSON_Data = http.getString();
    // Obtendo o objeto JSON
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, JSON_Data);
    JsonObject obj = doc.as<JsonObject>();

    // Acessando as informacoes do objeto
    cloud_cover = obj["current"]["cloud_cover"].as<int>();         // %
    wind_speed = obj["current"]["wind_speed_10m"].as<float>();   // km/h
    rain = obj["current"]["rain"].as<float>();                   // mm
    weather_code = obj["current"]["weather_code"].as<int>();
    // teste
    Serial.println("Nuvens: "+String(cloud_cover)+"% Vento: "+String(wind_speed)+"km/h Chuva: "+String(rain)+"mm "+"WC:"+String(weather_code));
  } else {// caso requisicao falha
    Serial.println("Error: " + String(code));
  }
}

void calcularMedias() {
  float somaTemp = 0.0;
  float somaPress = 0.0;
  
  // Calcula a soma de todos os valores no array
  for (int i = 0; i < NUM_MEDIDAS; i++) {
    somaTemp += temperatura[i];
    somaPress += pressao[i];
  }
  
  // Calcula medias
  mediaTemp = somaTemp / NUM_MEDIDAS;
  mediaPress = somaPress / NUM_MEDIDAS;
  
  // Exibe informacoes no Serial
  Serial.println("\n---- MEDIAS DOS ULTIMOS 20 MINUTOS ----");
  Serial.println("Temperatura media: " + String(mediaTemp, 1) + " C");
  Serial.println("Pressao media: " + String(mediaPress, 1) + " mbar");
  Serial.println("---------------------------------------\n");
}

// --------------------------------------------------------------------------------------------- ANIMACOES LEDS

// animacoes dos LEDs

void sunnyMode() {// gradiente de amarelo ate laranjado
  fill_gradient_RGB(leds, 0, CRGB(255,165,0), NUM_LEDS/2, CRGB(255,255,0));
  fill_gradient_RGB(leds, (NUM_LEDS/2+1), CRGB(255,255,0), NUM_LEDS-1, CRGB(255,165,0));
  modoAtivo = "ENSOL";
  delay(200);
  FastLED.show();
}
// animacao que simula trovoes
void thunderMode(){
  int blink_time = 100;
  int wait_time = random(1,3);
  int count_leds = random(2,4);
  int random_index[count_leds];

  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  for(int i=0; i<5; i++){

    for(int j=0; j<count_leds; j++){
      random_index[j] = random(j*4+1, j*4+4);
    }

    for(int j=0; j<count_leds; j++){
      leds[random_index[j]] = CRGB::Blue;
    }
    FastLED.show();
    delay(blink_time);
    for(int j=0; j<count_leds; j++){
      leds[random_index[j]] = CRGB::Black;
    }
    FastLED.show();
    delay(blink_time);
    for(int j=0; j<count_leds; j++){
      leds[random_index[j]] = CRGB::Blue;
    }
    FastLED.show();
    delay(blink_time);
    for(int j=0; j<count_leds; j++){
      leds[random_index[j]] = CRGB::Black;
    }
    FastLED.show();
    delay(wait_time*1000);
  }
  delay(1000);
  fill_solid(leds, NUM_LEDS, CRGB::Blue);
  FastLED.show();
  delay(blink_time);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
  delay(blink_time);
  fill_solid(leds, NUM_LEDS, CRGB::Blue);
  FastLED.show();
  delay(blink_time);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
}
void foggyMode(){
  fill_solid(leds, NUM_LEDS, CRGB::DarkGray);
  FastLED.show();
}
void cloudyMode(){
  fill_solid(leds, NUM_LEDS, CRGB::SkyBlue);
  FastLED.show();
}


// Executa a animacao escolhida
void AnimaLEDs(String clima)
{
  if(clima == "e"){
      sunnyMode();
  }
  else if(clima == "t"){
    thunderMode();
  }
  else if(clima == "n"){
    foggyMode();
  }
  else if(clima == "c"){
    cloudyMode();
  }
  else if(clima == "d"){
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
  }
}
// --------------------------------------------------------------------------------------------- FUNÇÕES MODOS

// Funcao dos modos

int bombaLigada = 0;
void ligarBombaDagua(){
  digitalWrite(PIN_CHUVA, HIGH);
  bombaLigada = 1;
}
void desligarBombaDagua(){
  digitalWrite(PIN_CHUVA, LOW);
  bombaLigada = 0;
}
void executaAnimacaoLocal()
{
  // Verificaoees para animacao
  if(weather_code >= 95 && weather_code <= 99) AnimaLEDs("t");
  else if(rain >= 50) AnimaLEDs("n");
  else if(cloud_cover >= 70) AnimaLEDs("c");
  else AnimaLEDs("e");
  if(rain >= 50.0 && bombaLigada==0) ligarBombaDagua();
  else if(rain < 50.0 && bombaLigada==1) desligarBombaDagua();
}
// Modo LOCAL:
// a cada 2 minutos capta uma medida dos sensores
// ao final de 20 minutos, faz a media das medidas e cruza informacoes com API
// Com base nisso, recria o clima no terrario
void modoLOCAL()
{
  // Verifica se eh a primeira execucao do modo
  if(setupLOCAL == 1){
    setupLOCAL = 0;
    // atualiza servidor
    leDadosAPI();
    mediaTemp = dht.readTemperature(false);
    mediaHum = dht.readHumidity();
    mediaPress = bmp.readPressure()/100;
    executaAnimacaoLocal();
  }
  unsigned long tempoAtual = millis();
  
  // Verifica se eh hora de fazer uma nova medicao
  if (tempoAtual - ultimaMedida >= INTERVALO || ultimaMedida == 0)
  {
    temperatura[indiceFila] = dht.readTemperature(false); // false -> Celsius
    humidade[indiceFila] = dht.readHumidity();
    pressao[indiceFila] = bmp.readPressure()/100;
    // Atualiza indice da fila e contador de requisicoes
    indiceFila = (indiceFila + 1) % NUM_MEDIDAS;
    totalMedidas++;
    
    // Atualiza o timestamp da ultima requisicao
    ultimaMedida = tempoAtual;
    
    // Verifica se eh hora de calcular medias (a cada 10 requisicoes = 20 minutos)
    if (totalMedidas % NUM_MEDIDAS == 0){
      calcularMedias();
      leDadosAPI();
      executaAnimacaoLocal();
    }
  }
}

// Se o modo atual nao for Local, ainda eh necessario atualizar o servidor web
// por isso, essa funcao eh chamada pelos outros modos
void atualizaSensores()
{
  unsigned long tempoAtual = millis();
  
  // Verifica se eh hora de fazer uma nova medicao
  if (tempoAtual - ultimaMedida >= INTERVALO || ultimaMedida == 0)
  {
    ultimaMedida = tempoAtual;
    mediaTemp = dht.readTemperature(false);
    mediaHum = dht.readHumidity();
    mediaPress = bmp.readPressure()/100;
  }
}

// outros modos
void modoENSOL(){
  AnimaLEDs("e");
  atualizaSensores();
}
void modoCHUVA(){
  AnimaLEDs("n");
  if(bombaLigada==0) ligarBombaDagua();// desligar na troca de modos
  atualizaSensores();
}
void modoTEMPS(){
  AnimaLEDs("t");
  if(bombaLigada==0) ligarBombaDagua();// desligar na troca de modos
  atualizaSensores();
}

// no modo NEBLN, apenas ligar nebulizador a cada 10 minutos
const int INTERVALO_NEBUL = 6000000; // 10min x 60s x 1000 = 600000ms
int ultimoAcionamento = 0;

void modoNEBLN(){
  AnimaLEDs("n");
  atualizaSensores();
  unsigned long tempoAtual = millis();
  
  if (tempoAtual - ultimoAcionamento >= INTERVALO_NEBUL || ultimoAcionamento == 0)
  {
    ultimoAcionamento = tempoAtual;
    digitalWrite(PIN_NEBUL, HIGH);
    delay(2000); // mantem 2 segundos ligado
    digitalWrite(PIN_NEBUL, LOW);
  }
}

//------------------------------------------------------------------------------------------------ SERVIDOR: HTML + PROCESSOR

// abaixo o HTML da pagina WEB

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <style>
    html {
     font-family: Arial;
     display: inline-block;
     margin: 0px auto;
     text-align: center;
    }
    h2 { font-size: 3.0rem; }
    p { font-size: 3.0rem; }
    .units { font-size: 1.2rem; }
    .dht-labels{
      font-size: 1.5rem;
      vertical-align:middle;
      padding-bottom: 15px;
    }
  </style>
</head>
<body>
  <h2>Projeto Climario</h2>
  <p>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i> 
    <span class="dht-labels">Sensacao</span> 
    <span id="temperature">%TEMPERATURE%</span>
    <sup class="units">&deg;C</sup>
  </p>
  <p>
    <i class="fas fa-tint" style="color:#00add6;"></i> 
    <span class="dht-labels">Humidade</span>
    <span id="humidity">%HUMIDITY%</span>
    <sup class="units">&percnt;</sup>
  </p>
  <p>
    <span class="dht-labels">Pressao</span>
    <span id="pressure">%PRESSURE%</span>
    <sup class="units">mbar</sup>
  </p>
  <p>
    <span class="dht-labels">Chuva</span>
    <span id="rain">%RAIN%</span>
    <sup class="units">mm</sup>
  </p>
  <p>
    <span class="dht-labels">Nuvens</span>
    <span id="cloud">%CLOUD%</span>
    <sup class="units">&percnt;</sup>
  </p>
  <p>
    <span class="dht-labels">Modo Atual</span>
    <span id="mode">%MODE%</span>
    <sup class="units"></sup>
  </p>
  <h3>Troca de Modo (Delay de 2 minutos)</h3>
  <form id="trocaModo">
      <select name="modos" id="modos">
        <option value="ENSOL">Ensolarado</option>
        <option value="CHUVA">Chuvoso</option>
        <option value="TEMPS">Tempestuoso</option>
        <option value="LOCAL">Local</option>
      </select>
      <br>
      <br>
      <button type="submit" >Enviar</button>
</form>
<p id="response"></p>
</body>
<script>
  document.getElementById("trocaModo").addEventListener("submit", function(event) {
     event.preventDefault();
            
     const selectedOption = document.getElementById("modos").value;
     const serverIp = "http://192.168.100.126"; // Altere para o IP do seu ESP32
            
     fetch(serverIp, {
        method: "POST",
        headers: {
          "Content-Type": "application/json"
        },
          body: JSON.stringify({ opcao: selectedOption })
      })
      .then(response => {
         if (response.ok) {
            document.getElementById("response").innerText = "Comando enviado com sucesso!";
       } else {
            document.getElementById("response").innerText = "Erro ao enviar comando.";
        }
      })
      .catch(error => {
         console.error("Erro ao enviar requisicao:", error);
         document.getElementById("response").innerText = "Erro ao conectar ao servidor";
     });
  });
  setInterval(function ( ) {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
        document.getElementById("temperature").innerHTML = this.responseText;
      }
    };
    xhttp.open("GET", "/temperature", true);
    xhttp.send();
  }, 10000 ) ;

  setInterval(function ( ) {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
        document.getElementById("humidity").innerHTML = this.responseText;
      }
    };
    xhttp.open("GET", "/humidity", true);
    xhttp.send();
  }, 10000 ) ;

  setInterval(function ( ) {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
        document.getElementById("pressure").innerHTML = this.responseText;
      }
    };
    xhttp.open("GET", "/pressure", true);
    xhttp.send();
  }, 10000 ) ;

  setInterval(function ( ) {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
        document.getElementById("mode").innerHTML = this.responseText;
      }
    };
    xhttp.open("GET", "/mode", true);
    xhttp.send();
  }, 10000 ) ;

  setInterval(function ( ) {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
        document.getElementById("rain").innerHTML = this.responseText;
      }
    };
    xhttp.open("GET", "/rain", true);
    xhttp.send();
  }, 10000 ) ;

  setInterval(function ( ) {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
        document.getElementById("cloud").innerHTML = this.responseText;
      }
    };
    xhttp.open("GET", "/cloud", true);
    xhttp.send();
  }, 10000 ) ;
</script>
</html>)rawliteral";

// Substitui os valores nos placeholder correspondentes no HTML
String processor(const String& var){
  if(var == "TEMPERATURE"){
    return String(mediaTemp);
  }
  else if(var == "HUMIDITY"){
    return String(mediaHum);
  }
  else if(var == "PRESSURE"){
    return String(mediaPress);
  }
  else if(var == "MODE"){
    return modoAtivo;
  }
  else if(var == "RAIN"){
    return String(rain);
  }
  else if(var == "CLOUD"){
    return String(cloud_cover);
  }
  return String();
}

// -------------------------------------------------------------------------------------------------- SETUP + LOOP
void setup() {
  delay(3000); // delay de seguranca (FastLED)
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  // Inicia serial
  Serial.begin(115200);


  // Conexao WiFi
  WiFi.mode(WIFI_STA);
  Serial.print("Conectando no WiFi..");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }
  Serial.println("\nConectado!\n");
  Serial.print("Iniciado STA:\t");
  Serial.println(ssid);
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());
  // NTP
  Serial.println("Contactando servidor NTP..");
  configTime(3600 * timezone, daysavetime * 3600, "time.nist.gov", "0.pool.ntp.org", "1.pool.ntp.org");


  // Inicializando Sensores
  dht.begin();
  if (!bmp.begin(0x76)) {
    Serial.println("Could not find a valid BMP085/BMP180 sensor, check wiring!");
    while (1) {}
  }
  delay(1100); // delay de seguranca (dht11)
  pinMode(PIN_CHUVA, OUTPUT);
  pinMode(PIN_NEBUL, OUTPUT);
  // Dados iniciais para serem mostrados no servidor web
  leDadosAPI();
  mediaTemp = dht.readTemperature(false);
  mediaHum = dht.readHumidity();
  mediaPress = bmp.readPressure()/100; // Pa -> mbar
  modoAtivo = "ENSOL";
  ultimaTrocaModo = millis(); // trocar modo apenas depois de 5 minutos
  ultimaMedida = millis(); // nova leitura de sensores depois de 2 minutos

  // Iniciando Servidor
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });
  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(mediaTemp).c_str());
  });
  server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(mediaHum).c_str());
  });
  server.on("/pressure", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(mediaPress).c_str());
  });
  server.on("/mode", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", modoAtivo.c_str());
  });
  server.on("/rain", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(rain).c_str());
  });
  server.on("/cloud", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(cloud_cover).c_str());
  });
  server.on("/",HTTP_POST,[](AsyncWebServerRequest * request){},NULL,
    [](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {
      String receivedData = "";
      for (size_t i = 0; i < len; i++) {
        receivedData += (char)data[i]; // Concatena os dados recebidos na string
      }
      // Parse JSON
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, receivedData);
            
      if (!error) {
        Mode = doc["opcao"].as<String>();
        Serial.print("Opcao recebida: ");
        Serial.println(Mode);
        houveTroca = 1;
        if(Mode != modoAtivo) setupLOCAL = 1;
      } else {
        Serial.println("Erro ao parsear JSON");
      }
      request->send(200);
  });
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(400, "text/plain", "Not found");
  });
  server.begin();
  Serial.println("Server HTTP iniciado");
}


void loop() {
  // Obtem data e hora atuais
  atualizaDataHora();
  
  // Verificar se ha troca de MODO. Se sim, desligar tudo (bomba, nebul, leds) e
  // obter atraves de entrada do usuario pelo servidor web
  if(houveTroca){
    unsigned int tempoAtual = millis();
    if(tempoAtual - ultimaTrocaModo >= ESPERA_TROCA){
      modoAtivo = String(Mode);
      ultimaTrocaModo = tempoAtual;
      houveTroca = 0;
    } else Serial.println("ainda em espera");
  }

  // codigo para apresentacao sem WiFi

  // String input_modo = modoAtivo;
  // Serial.println("Modo?");
  // if(Serial.available()>0){
  //   input_modo = Serial.readString(); // exceto modo LOCAL: ENSOL, CHUVA, TEMPS
  //   modoAtivo = input_modo;
  // }
  
  Serial.println("Modo Ativo: " + modoAtivo);
  
  if(modoAtivo == "LOCAL") modoLOCAL();
  else if(modoAtivo == "ENSOL") modoENSOL();
  else if(modoAtivo == "CHUVA") modoCHUVA();
  else if(modoAtivo == "TEMPS") modoTEMPS();
  //else if(modoAtivo == "NEBLN") modoNEBLN();

  FastLED.delay(1000 / UPDATES_PER_SECOND);
  delay(1100);
}
