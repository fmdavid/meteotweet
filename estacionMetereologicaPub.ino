#include <PCD8544.h>
#include <SPI.h>
#include <Wire.h>
#include "SoftwareSerial.h"
#include <Adafruit_Sensor.h> // https://github.com/adafruit/Adafruit_Sensor
#include <Adafruit_BMP280.h>
#include "DHT.h"
#include "RTClib.h"
#include "WiFiEsp.h"

char ssid[] = "TU_RED_WIFI";            // SSID (Nombre de la red WiFi)
char pass[] = "TU_PASSWORD";        // Contraseña
int status = WL_IDLE_STATUS;     // Estado del ESP. No tocar

// Configuración ThingSpeak
char thingSpeakAddress[] = "api.thingspeak.com";
String thingtweetAPIKey = "TU_API_KEY";

// Botón para envío manual de tweet
const int pinBoton = 8;
int value = 0;

// Configuración del DHT
#define pinDHT11 2
#define DHTTYPE DHT11

// Configuración del ESP
#define PIN_RX 11
#define PIN_TX 12

// Definimos el caracter º para indicar grados
static const byte GRADO_CHAR = 1;
static const byte grado_glyph[] = { 0x00, 0x07, 0x05, 0x07, 0x00 };

// Control del tiempo transcurrido
long marcaTiempoAnterior = 0;
long intervalo = 900000; // quince minutos (0.25*60*60*1000)

// Pantalla
static PCD8544 lcd;

// Sensor presión - temperatura
Adafruit_BMP280 bme;

// Sensor temperatura - humedad
DHT dht(pinDHT11, DHTTYPE);

// Reloj
RTC_DS3231 rtc;

// Wifi
WiFiEspClient client;  //Iniciar el objeto para cliente 
SoftwareSerial esp8266(PIN_RX, PIN_TX);

void setup()   {
  iniciarPantalla();
  
  Serial.begin(9600); // Monitor serie
  esp8266.begin(9600); //ESP01

  pinMode(pinBoton, INPUT); //botón

  bme.begin(); // inicio sensor temperatura - presión

  rtc.begin(); // inicio reloj
  if (rtc.lostPower()) { // si se queda sin batería
      // Fijar a fecha y hora de compilacion
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  iniciarWifi();
}


void loop() {
  // obtenemos la marca de tiempo actual
  unsigned long marcaTiempoActual = millis();

  // obtenemos los valores de los sensores
  float h = dht.readHumidity();
  float t = bme.readTemperature();
  float p = bme.readPressure() / 100;
  DateTime ahora = rtc.now();

  // tiempo que falta para el próximo tweet
  int minutos = (int) ((intervalo - marcaTiempoActual + marcaTiempoAnterior) / 1000) / 60;
  
  mostrarInformacionPantalla(t, p, (int) h, ahora, minutos);

  value = digitalRead(pinBoton);  //lectura digital de pin

  if(marcaTiempoActual - marcaTiempoAnterior > intervalo || value == HIGH) {
    marcaTiempoAnterior = marcaTiempoActual;  

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("METEOTWEET 1.0");
    lcd.setCursor(0, 1);
    lcd.print("Enviando tweet");

    String mensaje = obtenerTextoTweet(ahora, t, (int) h, p);
    Serial.println(mensaje);
    enviarTweet(mensaje);
    lcd.setCursor(0, 1);
    lcd.print("Tweet enviado!");
    delay(1000);
  }
  
  delay(2000);
}

String obtenerTextoTweet(DateTime tiempo, float t, int h, float p){
     String mensaje = "[";
     mensaje.concat(tiempo.hour());
     mensaje.concat(":");
     mensaje.concat(tiempo.minute());
     mensaje.concat("][T=");
     mensaje.concat(t);
     mensaje.concat("][H=");
     mensaje.concat(h);
     mensaje.concat("][P=");
     mensaje.concat(p);
     mensaje.concat("]");

     return mensaje;
}

void enviarTweet(String texto)
{
  if (client.connect(thingSpeakAddress, 80))
  { 
    Serial.println("Conectado al servidor");
    // Creamos la petición HTTP POST
    texto = "api_key="+thingtweetAPIKey+"&status="+texto;

    client.print("POST /apps/thingtweet/1/statuses/update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(texto.length());
    client.print("\n\n");

    client.print(texto);

    while (client.available()) {
      char c = client.read();
      Serial.write(c);
    
    }
  
    //Desconexion
    if (client.connected()) {
      Serial.println();
      Serial.println("Desconectando del servidor...");
      client.flush();
      client.stop();
    }

  }
  else
  {
    Serial.println("Ha fallado la conexión con ThingSpeak");   
  }
}

void iniciarWifi(){
  WiFi.init(&esp8266); // inicio Wifi

  //intentar iniciar el modulo ESP
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("Modulo no presente. Reinicie el Arduino y el ESP01 (Quite el cable que va de CH_PD a 3.3V y vuelvalo a colocar)");
    //Loop infinito
    while (true);
  }

  //Intentar conectar a la red wifi
  while ( status != WL_CONNECTED) {
    Serial.print("Intentando conectar a la red WiFi: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
  }
}

void iniciarPantalla(){
  lcd.begin(84, 48);

  // damos de alta el caracter personalizado de grado
  lcd.createChar(GRADO_CHAR, grado_glyph);

  // Mensaje de inicio por pantalla
  lcd.setCursor(0, 0);
  lcd.print("METEOTWEET 1.0");
  lcd.setCursor(0, 1);
  lcd.print("Iniciando  ...");  
}

void mostrarInformacionPantalla(float t, float p, int h, DateTime ahora, int minutos){
  lcd.setCursor(0, 0);
  lcd.print("METEOTWEET 1.0");
  lcd.setCursor(0, 1);
  lcd.print("Temp. ");
  lcd.print(t);
  lcd.print("\001C ");
  lcd.setCursor(0, 2);
  lcd.print("P. ");
  lcd.print(p);
  lcd.print(" hPa");
  lcd.setCursor(0, 3);
  lcd.print("H. ");
  lcd.print(h);
  lcd.print(" %");
  lcd.setCursor(0, 4);
  lcd.print("Env. en ");
  lcd.print(minutos);
  lcd.print(" min");
  lcd.setCursor(0, 5);
  if(ahora.day()< 10) lcd.print("0");
  lcd.print(ahora.day(), DEC);
  lcd.print('/');
  if(ahora.month() < 10) lcd.print("0");
  lcd.print(ahora.month(), DEC);
  lcd.print('/');
  lcd.print(ahora.year() % 1000);
  lcd.print(' ');
  lcd.print(ahora.hour(), DEC);
  lcd.print(':');
  lcd.print(ahora.minute(), DEC);
}

