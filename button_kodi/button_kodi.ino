
/*
 * Codigo para la realizacion de un sistema de riego inteligente para la asignatura Sistemas Ubicuos.
 *
 * El dispositivo se conecta a un broker MQTT y a una red Wifi. Realizando reintentos
 * y reconexiones cuando sea necesario.
 *
 * Se utilizan las librerías ArduinoJSON, TimeLib, Ticker, PubSubClient y Esp8266Wifi
 * 
 * No existe ningún delay dentro del loop para poder relizar tareas criticas dentro del loop sin 
 * perjuicio de los delays.
 * 
 * Se han creado dos timers para la realización de las tareas periodicas (activacion de led de control y
 * la llamada al algoritmo de riego)
 *
 * Se utiliza una única callback para la recepcion de los mensajes MQTT que posteriormente es procesado.
 *
 * En la librería ArduinoJson se ha tenido que ampliar el tamaño max del mensaje MQTT_MAX_PACKET_SIZE
 * para la recepcion del mensaje de motas proximas.
 *
 */
//#define MQTT_MAX_PACKET_SIZE 512 
#include <ESP8266WiFi.h>
#include <Ticker.h>

#include <SPI.h> // From Kodi
#include <Ethernet.h> // from Kodi

#define WIFI_RECONNECT_TIME 30000 //Tiempo de intento de reconexion WIFI
#define WIFI_BEGIN_TIME 500 //Espera por comprobación de estado de la conexion WIFI
#define MAX_WIFI_BEGIN_TIME 500 * 30 //ESpera por ingraso en la WIFI

const char* ssid     = "RASUS"; //SSID de la WIFI a utilizar
const char* password = "TereBeaJosePablo49788290"; // Passwd de la WIFI a utilizar

Ticker Ticker1;
boolean led_state = false;
int pin=1; // correspondiente al GPIO5
int buttonState = 0;         // variable for reading the pushbutton status
const int buttonPin = 2;     // the number of the pushbutton pin

int lastWIFIStatus = 0;
int WifiConnect_state = 0;
long lastWIFIReconnectAttempt = 0;
long lastWIFIbeginAttempt = 0;
long firstWIFIbeginAttempt = 0;

WiFiClient espClient;

// arduino mac address
byte mac[] = {0xDE,0xAC,0xBF,0xEF,0xFE,0xAA};
// xbmc ip
byte xbmchost[] = {192,168,1,37};
//EthernetClient client;

void setup() {
  Serial.begin(115200);

  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  lastWIFIStatus = WiFi.status();

  lastWIFIReconnectAttempt = 0;

  Ticker1.attach(0.05, timer1);

  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  
  Serial.println("Setup done");
}



void timer1()
{        
    if(led_state == true){
      digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH
     led_state = false; 
    }else{
      digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on (Note that LOW is the voltage level
      led_state = true;
    }

}


void loop() {
  static boolean first = true;
  if(WiFi.status() != WL_CONNECTED && WifiConnect_state == 0){
      long now = millis();
      if ((now - lastWIFIReconnectAttempt > WIFI_RECONNECT_TIME) || first == true) {
        first=false;
        if(lastWIFIStatus == WL_CONNECTED){
          Serial.print("Perdida la conexion WIFI con ");
          Serial.println(ssid);
          Ticker1.attach(0.05, timer1);
          lastWIFIStatus = 0;
        }
        lastWIFIReconnectAttempt = now;
        // Attempt to reconnect
        
        WifiConnect_state = _WifiConnect(WifiConnect_state);
        if(WifiConnect_state == 3){
          lastWIFIReconnectAttempt = 0;
          lastWIFIStatus = WL_CONNECTED;
          Serial.println(WiFi.localIP());
          WifiConnect_state = 0;
        }else if(WifiConnect_state == -1){
          WifiConnect_state = 0;
          lastWIFIStatus = 0;
        }
      }
  }else if(WifiConnect_state == 1 || WifiConnect_state == 2 ){
    WifiConnect_state = _WifiConnect(WifiConnect_state);
    if(WifiConnect_state == 3){
      lastWIFIReconnectAttempt = 0;
      lastWIFIStatus = WL_CONNECTED;
      Serial.println(WiFi.localIP());
      WifiConnect_state = 0;
    }else if(WifiConnect_state == -1){
      WifiConnect_state = 0;
      lastWIFIStatus = 0;
    }
  }

  check_button();
}

void check_button() {
  
  buttonState = digitalRead(buttonPin);

  // check if the pushbutton is pressed.
  // if it is, the buttonState is HIGH:
  if (buttonState == LOW) {
    // turn LED on:
    digitalWrite(LED_BUILTIN, HIGH);

    Serial.println("Button pressed. Sending Action to KODI");
    xbmc("Player.GoTo","playerid\":1,\"to\":\"next\"");
    delay(2000);
  } else {
    // turn LED off:
    digitalWrite(LED_BUILTIN, LOW);
  }
}


int _WifiConnect(int state) {
  static int n = 0;
  int ret=-1;

    switch (state) {
      case 0:
      {
            Serial.print("Buscando Redes WIFI Para conectar a ");
            Serial.println(ssid);
            Ticker1.attach(0.05, timer1);
            n = WiFi.scanNetworks();
            if(n > 0)
              ret=1;
            else{
              Serial.println("No existen Redes WIFI proximas");
              ret=-1;
            }
        break;
      }
      case 1:
      {
            if(n != 0){
              if(strcmp(WiFi.SSID(n-1).c_str(), ssid) == 0){
                Serial.print(ssid);
                Serial.println(" Encontrada.");
                n = 0;
                WiFi.begin(ssid, password);
                firstWIFIbeginAttempt = millis();
                ret = 2;
              }else{
                --n;
                if(n > 0)
                  ret = 1;
                else{
                  ret = -1;
                  Serial.print(ssid);
                  Serial.println(" No Encontrada.");
                }
              }
            }
        break;
      }
      case 2:
      {
            long now = millis();
            if (now - lastWIFIbeginAttempt > WIFI_BEGIN_TIME) {
              lastWIFIbeginAttempt = now;
              // Attempt to check WIFI Status
              
              int status = WiFi.status();
              Serial.print("Wifi Status ");
              Serial.println(status);
              if(status == WL_CONNECTED){
                lastWIFIbeginAttempt = 0;
                firstWIFIbeginAttempt = 0;
                Ticker1.attach(1.0, timer1);
                ret = 3;
                Serial.print("Conectado a ");
                Serial.println(ssid);

              }else{
                if(now - firstWIFIbeginAttempt > MAX_WIFI_BEGIN_TIME){
                  lastWIFIbeginAttempt = 0;
                  firstWIFIbeginAttempt = 0;
                  WiFi.disconnect();
                  ret = -1;
                  Serial.print("Error al intental conectar a ");
                  Serial.println(ssid);
                }else{
                  ret = 2;
                }
              }
            }else{
              ret = 2;
            }
        break;    
      }
      default:
      {
            ret = -1;
      }
    }
    return ret;
}


/*
   XBMC/Kodi Arduino sketch by M.J. Meijer 2014
   control XBMC with an arduino through JSON
   disable password and username in xbmc
*/

void kodi_setup()
{
//  Serial.begin(9600);
//  Serial.print(F("Starting ethernet..."));
  //if(!Ethernet.begin(mac)) Serial.println("failed");
//  else Serial.println(Ethernet.localIP());

//  delay(5000);
//  Serial.println("Ready");
}

void kodi_loop()
{
  /********** media buttons **********/
  
  xbmc("Player.PlayPause","playerid\":1");
  delay(2000);
  xbmc("Player.Stop","playerid\":1");
  delay(2000);
  xbmc("Player.Seek","playerid\":1,\"value\":\"smallbackward\"");
  delay(2000);
  xbmc("Player.Seek","playerid\":1,\"value\":\"smallforward\"");
  delay(2000);
  xbmc("Player.Seek","playerid\":1,\"value\":\"bigbackward\"");
  delay(2000);
  xbmc("Player.Seek","playerid\":1,\"value\":\"bigforward\"");
  delay(2000);
  xbmc("Player.SetSpeed","playerid\":1,\"speed\":\"increment\"");
  delay(2000);
  xbmc("Player.SetSpeed","playerid\":1,\"speed\":\"decrement\"");
  delay(2000);
  
  /********** navigate **********/

  xbmc("Input.ExecuteAction","action\":\"up\"");
  delay(2000);
  xbmc("Input.ExecuteAction","action\":\"down\"");
  delay(2000);
  xbmc("Input.ExecuteAction","action\":\"left\"");
  delay(2000);
  xbmc("Input.ExecuteAction","action\":\"right\"");
  delay(2000);
  xbmc("Input.ExecuteAction","action\":\"select\"");
  delay(2000);
  xbmc("Input.ExecuteAction","action\":\"back\"");
  delay(2000);

  /********** fullscreen **********/

  xbmc("GUI.SetFullscreen","fullscreen\":\"toggle\"");
  delay(2000);

  /********** subtitles **********/

  xbmc("Player.SetSubtitle","playerid\":1,\"subtitle\":\"on\"");
  delay(2000);
  xbmc("Player.SetSubtitle","playerid\":1,\"subtitle\":\"off\"");
  delay(2000);
  xbmc("Player.SetSubtitle","playerid\":1,\"subtitle\":\"next\"");
  delay(2000);
  xbmc("Addons.ExecuteAddon","addonid\":\"script.xbmc.subtitles\"");
  delay(2000);
  
  /********** language **********/

  xbmc("Player.SetAudioStream","playerid\":1,\"stream\":\"next\"");
  delay(2000);

  /********** start addons **********/

  xbmc("Addons.ExecuteAddon","addonid\":\"script.tv.show.next.aired\"");
  delay(2000);
  xbmc("Addons.ExecuteAddon","addonid\":\"plugin.video.itunes_trailers\"");
  delay(2000);
  xbmc("Addons.ExecuteAddon","addonid\":\"plugin.video.youtube\"");
  delay(2000);
  xbmc("Addons.ExecuteAddon","addonid\":\"net.rieter.xot\"");
  delay(2000);
  xbmc("Addons.ExecuteAddon","addonid\":\"script.audio.spotimc\"");
  delay(2000);
  
  /********** enable/disable addons **********/
  
  xbmc("Addons.SetAddonEnabled","addonid\":\"script.xbmc.boblight\",\"enabled\":true");
  delay(5000);
  xbmc("Addons.SetAddonEnabled","addonid\":\"script.xbmc.boblight\",\"enabled\":false");
  delay(5000);

  /********** navigate menu's **********/

  xbmc("GUI.ActivateWindow","window\":\"home\"");
  delay(2000);
  xbmc("GUI.ActivateWindow","window\":\"favourites\"");
  delay(2000);
  xbmc("GUI.ActivateWindow","window\":\"weather\"");
  delay(2000);
  xbmc("GUI.ActivateWindow","window\":\"music\"");
  delay(2000);
  xbmc("GUI.ActivateWindow","window\":\"video\",\"parameters\":[\"MovieTitles\"]");
  delay(2000);
  xbmc("GUI.ActivateWindow","window\":\"video\",\"parameters\":[\"RecentlyAddedMovies\"]");
  delay(2000);
  xbmc("GUI.ActivateWindow","window\":\"video\",\"parameters\":[\"TVShowTitles\"]");
  delay(2000);
  xbmc("GUI.ActivateWindow","window\":\"video\",\"parameters\":[\"RecentlyAddedEpisodes\"]");
  delay(2000);

  /********** play music **********/

  xbmc("Player.Open","item\":{\"partymode\":\"music\"}");
  delay(2000);

  /********** select tv channel **********/

  xbmc("Player.Open","item\":{\"channelid\":41");
  delay(2000);
  xbmc("Player.Open","item\":{\"channelid\":42");
  delay(2000);
  xbmc("Player.Open","item\":{\"channelid\":43");
  delay(2000);
}

/*********** Send XBMC JSON **********/

void xbmc(char *method, char *params)
{
  if (espClient.connect(xbmchost,8080))
  {
    espClient.print("GET /jsonrpc?request={\"jsonrpc\":\"2.0\",\"method\":\"");
    espClient.print(method);
    espClient.print("\",\"params\":{\"");
    espClient.print(params);
    espClient.println("},\"id\":1}  HTTP/1.1");
    espClient.println("Host: XBMC");
    espClient.println("Connection: close");
    espClient.println();
    espClient.stop();
  }
  else
  {
    espClient.stop();
  }
}

