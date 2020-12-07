/*********
  Muehlgasse Umwälzpumpen Steuerung
  Es werden 3 Temperatursensoren verwendet. Die Wasserteperatur steuert das Pumpenrelais.
  Bei mehr als 55C wir die Pumpe eingeschaltet. Über den Webserver kann sie Pumpe manuell eingeschaltet werden.
  Jede Stunde schaltet die Pumpe für 30 sec ein, damit sie nicht durch zu lange Standzeiten festläuft.
*********/

// Import required libraries

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>
#include <iomanip>

 
// Data wire is connected to GPIO 15
#define ONE_WIRE_BUS 15

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);

// Replace with your network credentials
const char* ssid = "xxxx";
const char* password = "xxxx";

// gpio pin for relais
const int relaisPin1 = 23;


// timer werden initialisiert wenn relais eingeschaltet wird 
time_t lastDataRead = 0;    //zeitpunkt der letzten Sensor abfrage (alle 10 sec)
time_t relOnTime = 0;      // zeitpunkt wann das Relais eingeschaltet wurde
time_t now = 0;            // aktuelle Uhrzeit vor dem Einlesen der Sensordaten

// das relais soll automatisch jede Stunde für 30 sec eingeschaltet werden 
// um den Pumpenmotor nicht zulange ausgeschaltet zu lassen. dieses wird durch setzen 
// des flags signalisiert. 
boolean inKeepAlivePhase = false;
time_t nextKeepAlive = 0; // time(&now) + 3600;  // Zeitpunkt wann das Relais für 30 sec angeschaltet werden soll (in einer Stunde)


// SPACE FOR json DOCUMENT wird bei aufruf von readData() befüllt (alle 10 sec)
const size_t capacity = JSON_OBJECT_SIZE(5) + 60;
DynamicJsonDocument doc(capacity);

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

String readData() {
  // Call sensors.requestTemperatures() to issue a global temperature and Requests to all devices on the bus
  //serializeJson(doc, Serial);
  
  String data;
  serializeJson(doc, data);

  Serial.println(data);

  return data;
}

void loadData() {
  // Call sensors.requestTemperatures() to issue a global temperature and Requests to all devices on the bus
  
  sensors.requestTemperatures(); 
  
  for (int i=0; i < 3; i++){
    String sensor = "temp";
    char temp = i + '0';
    sensor = sensor + temp;

    
    float tempC = sensors.getTempCByIndex(i);
    if(tempC == -127.00) {
      Serial.print("Failed to read from DS18B20 sensor");
      Serial.println(i); 
      doc[sensor] = "00.00";
     
    } else {
        Serial.print("Temperature Celsius Sensor");
        Serial.print(i);
        Serial.print(": ");
        Serial.println(tempC); 
        doc[sensor] = floor( tempC * 10.f + 0.5f ) / 10.f; 
    }

  }
  Serial.print("Next KeepAlive in ");
  Serial.print(nextKeepAlive - now);
  Serial.println(" sec");
  String keepalive = "keepalive";
  doc[keepalive] = (nextKeepAlive - now) / 60;
  // Zeit merken wann zuletzt gelesen wurde
  time(&lastDataRead);
}

// funktion um relais einzuschalten
void relEin(){
  Serial.println("Set relay state to LOW (AN)");
    digitalWrite(relaisPin1, LOW);
    doc["relstate"] = "AN";
    time (&relOnTime); // setze zeitstempel wann eingeschaltet wurde
}

void relAus(){
  Serial.println("Set relay state to HIGH (AUS)");
  digitalWrite(relaisPin1, HIGH);
  doc["relstate"] = "AUS";
  relOnTime = 0;  // zeitstempel loeschen
  nextKeepAlive = time(NULL) + 3600; // in einer Stunde automatisch wieder für 30 sec einschalten
  inKeepAlivePhase = false; // KeepaliveFlag zurücksetzen
}

// relais automatisch einschalten, wenn noch nicht an und temp > 55 grad
boolean relAutoOn(){
  if (doc["relstate"] == "AUS" && doc["temp0"].as<float>() > 55){
    relEin();  // einschalten und anlassen
    return true;
  } else if (doc["relstate"] == "AN" && doc["temp0"].as<float>() > 55){
    return true; // anlassen
  }
  return false; // kann ausgeschaltet werden wenn der timer abgelaufen ist (900 sec nach manuellem einschalten)
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1", charset="utf-8">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <style>
    html {
     font-family: Arial;
     display: inline-block;
     margin: 0px auto;
     text-align: center;
    }
    h2 { font-size: 1.5rem; }
    p { font-size: 1.5rem; }
    .units { font-size: 1.2rem; }
    .btn {
        padding: 10px 10px; 
        cursor: pointer; 
    }
    .btn:hover {
        background-color: rgba(167, 169, 173, 0.185);
    }
  </style>
</head>
<body onload="requestData()">
  <h2 id=time>Datum und Uhrzeit</h2>
  <p>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i> 
    <span">Wasser</span> 
    <span id="temperature0">%TEMPERATURE0%</span>
    <sup class="units">&deg;C</sup>
  </p>
  <p>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i> 
    <span>Aussen</span>
    <span id="temperature1">%TEMPERATURE1%</span>
    <sup class="units">&deg;C</sup>
  </p>
  <p>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i> 
    <span>Innen</span>
    <span id="temperature2">%TEMPERATURE2%</span>
    <sup class="units">&deg;C</sup>
  </p>
  <p>
    <i id="relicon" class="btn fas fa-toggle-on" style="color:#059e8a;"  onClick="location.href='/relais_toggle'"></i>
    <span class="ds-labels">Pumpe</span>
    <span id="relstate">%RELSTATE%</span>
  </p>
  <p id="pKeep">
    <span class="ds-labels">Erhaltungslauf in</span>
    <span id="keepalive">%KEEPALIVE%</span>
    <span class="ds-labels">Min</span>
  </p>
</body>
<script>
setInterval(function () {requestData()}, 10000) ;

setInterval(function () {setDate()}, 1000) ;

function requestData(){
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      var data = JSON.parse(this.responseText);
      document.getElementById("temperature0").innerHTML = data.temp0;
      document.getElementById("temperature1").innerHTML = data.temp1;
      document.getElementById("temperature2").innerHTML = data.temp2;
      document.getElementById("relstate").innerHTML = data.relstate;
      document.getElementById("keepalive").innerHTML = data.keepalive;
      setRelIcon();
    }
  };
  xhttp.open("GET", "/data", true);
  xhttp.send();
}
function setRelIcon(){
        if (document.getElementById("relstate").innerHTML === "AN") {
        document.getElementById("relicon").className = "btn fas fa-toggle-on";
        document.getElementById("pKeep").style.visibility = "hidden";
      } else {
        document.getElementById("relicon").className = "btn fas fa-toggle-off";
        document.getElementById("pKeep").style.visibility = "visible";
      }
}
function setDate(){
    // get a new date (locale machine date time)
  var date = new Date();
  // get the date as a string
  var n = date.toLocaleString('de-DE');
  document.getElementById('time').innerHTML = n;
}
setDate();
setRelIcon();
</script>
</html>)rawliteral";

// Replaces placeholder with DHT values
String processor(const String& var){
  //Serial.println(var);
  if(var == "TEMPERATURE0"){
    return doc["temp0"];
  }
  else if(var == "TEMPERATURE1"){
    return doc["temp1"];
  }
  else if(var == "TEMPERATURE2"){
    return doc["temp2"];
  }
  else if(var == "RELSTATE"){
    return doc["relstate"];
  }
  else if(var == "KEEPALIVE"){
    return doc["keepalive"];
  }
  return String();
}

void setup(){
  // Serial port for debugging purposes
  Serial.begin(9600);
  //Serial.println();
  
  // Start up the DS18B20 library
  sensors.begin();
  
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  delay(1000);

  // setup gpio pin
  pinMode(relaisPin1, OUTPUT);

  Serial.println("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  
  // Print ESP Local IP Address
  Serial.println(WiFi.localIP());

  // beim hochfahren relais ausschalten
  relAus();
  // und sensordaten einlesen
  loadData();

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){

    request->send_P(200, "text/html", index_html, processor);
  });
  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", readData().c_str());
  });
  server.on("/temperature0", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", processor("TEMPERATURE0").c_str());
  });
  server.on("/temperature1", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", processor("TEMPERATURE1").c_str());
  });
  server.on("/temperature2", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", processor("TEMPERATURE2").c_str());
  });
  server.on("/relais_on", HTTP_GET, [](AsyncWebServerRequest *request){
    relEin();
    request->send_P(200, "text/plain", doc["relstate"]);  
  });
  server.on("/relais_off", HTTP_GET, [](AsyncWebServerRequest *request){
    relAus();
    request->send_P(200, "text/plain", doc["relstate"]);
  });
   server.on("/relais_state", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", doc["relstate"]);
  });
  server.on("/relais_toggle", HTTP_GET, [](AsyncWebServerRequest *request){
    if (doc["relstate"] == "AUS") {
      relEin();
    } else {
      relAus();
    }
    request->redirect("/"); 
  });
  // Start server
  server.begin();
}
// pruefe die WLan Verbindung 
void loop(){
    int wifi_retry = 0;
  while(WiFi.status() != WL_CONNECTED && wifi_retry < 5 ) {
      wifi_retry++;
      Serial.println("WiFi not connected. Try to reconnect");
      WiFi.disconnect();
      WiFi.mode(WIFI_OFF);
      WiFi.mode(WIFI_STA);
      WiFi.begin(ssid, password);
      delay(100);
  }
  if(wifi_retry >= 5) {
      Serial.println("\nReboot");
      ESP.restart();
  }

  // lese die sensoren alle 10 sec neu ein
  time(&now);
  if (lastDataRead > 0 && now - lastDataRead > 10 ){
    loadData();
  }
  // Keepalive starten wenn eine Stunde vergangen ist und das Relais ausgeschaltet ist
  if (relOnTime == 0 && now > nextKeepAlive) {
     relEin();
     inKeepAlivePhase = true;
     Serial.println("Keepalive phase started");
  }

  // pruefen wie lange das relais schon ON ist. Nach 60 min (in keepalive nach 30) OFF schalten, sofern es nicht automatisch on sein soll
 if (relAutoOn() == false){
    if (inKeepAlivePhase){
      if (relOnTime > 0 && now - relOnTime > 30 ){
        relAus();
        Serial.println("Keepalive phase stoped");
      }
    } else {
      if (relOnTime > 0 && now - relOnTime > 3600 ){
        relAus();
      } 
    }
 }

}
