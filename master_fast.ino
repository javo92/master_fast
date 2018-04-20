
/*
 * 
 * Slow TCP connection: https://github.com/esp8266/Arduino/issues/1430
 * 
 * posible mejora: client.setNoDelay(true);

    SPI Master Demo Sketch
    Connect the SPI Master device to the following pins on the esp8266:

    GPIO      Name  |   STM
   ===================================
     12       MISO  |   PA7
     13       MOSI  |   PA6
     14       SCK   |   PA5
     15       SS    |   PA4

    Note: If the ESP is booting at a moment when the SPI Master has the Select line HIGH (deselected)
    the ESP8266 WILL FAIL to boot!


*/
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

// Funciones prototipo
uint8_t read_command(uint8_t command);
void FloatArrayFromSTM(float F_array [], int N_elementos);
float floatFromSTM(void);
uint8_t String2int (String Array);
void Float2TCP (float Float_data, WiFiClient client);

char ssid[] = "Oregano_X"; //  your network SSID (name)
char pass[] = "MikoKlein25=";    // your network password (use for WPA, or use as key for WEP)

IPAddress local_IP(192,168,4,22);
IPAddress gateway(192,168,4,9);
IPAddress subnet(255,255,255,0);
IPAddress remote_IP(192,168,4,121);

// Create an instance of the server
// specify the port to listen on as an argument
WiFiServer server(80);
WiFiUDP Udp;

unsigned int localPort = 2390;      // local port to listen on


// Definición de los pines
//      Función   GPIO#
#define HSPI_CS   15
#define HSPI_CLK  14 
#define HSPI_MISO 12 
#define HSPI_MOSI 13 
#define DRDY_N    4 
#define START     5 
#define LED       2 

#define hotspot false  // hotspot = true
                    // wifi    = false
#define debug true

int i=1;

// la rutina de setup corre una vez o cuando se presiona reset
void setup() {
  Serial.begin(115200);                
  SPI.begin();
  
  SPI.setClockDivider(SPI_CLOCK_DIV2); //Divides 16MHz clock by 2 to set CLK speed to 4MHz
  SPI.setDataMode(SPI_MODE1);  //clock polarity = 0; clock phase = 1 (pg. 8)
  SPI.setBitOrder(MSBFIRST);  //data format is MSB (pg. 25)  

  pinMode(HSPI_CS,OUTPUT);
  pinMode(HSPI_CLK,SPECIAL);
  pinMode(HSPI_MISO,SPECIAL);
  pinMode(HSPI_MOSI,SPECIAL);
  pinMode(DRDY_N,INPUT);
  pinMode(START,OUTPUT);
  pinMode(LED,OUTPUT);

  Serial.println();

  #if (hotspot == false)

  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
#else

  Serial.print("Setting soft-AP configuration ... ");
  Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");

  Serial.print("Setting soft-AP ... ");
  Serial.println(WiFi.softAP("ESPBCI_WIFI", "Password_01", false) ? "Ready" : "Failed!");

  Serial.print("Soft-AP IP address = ");
  Serial.println(WiFi.softAPIP());

#endif

  // Start the server
  Serial.println("Starting servers");
  server.begin();
  Udp.begin(localPort);
  Serial.println("Server started");
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());

}

// la rutina loop corre constantemente
void loop() {
    uint8_t command2STM = 0x02;
    uint8_t response = 0x00;
    float F_data [8];
    float F_data_array[250];
    int N_elementos = 250;
    int t1 = 0;
    int offset_configurar = 0x00;
    

  //########### Server handler #################
  // Check if a client has connected
  WiFiClient client = server.available();
  if (!client) {
    //Serial.println("No client available");
    return;
  }
  
  // Wait until the client sends some data
  Serial.println("new client");
  t1 = millis();
  while(!client.available()){
    delay(1);
    Serial.print(".");
    ESP.wdtFeed();    // Evita reinicios por watchdog
    if (millis()-t1 >= 100000){
      Serial.println("Timeout");
      return;
    }
  }
  Serial.println();
  
  // Read the first line of the request
  String req = client.readStringUntil('\r\n');
      
  #if (debug == true)    
    Serial.println(req);
  #endif

  
  // Match the request

  if (req.indexOf("Configurar") != -1)
  {
    command2STM = 0xF0;
    Serial.println("Configurar");
  }
  else if (req.indexOf("Single float") != -1)
   {
    command2STM = 0x01;
    Serial.println("Single float");
  }
  else if (req.indexOf("Float array") != -1)
    {
    command2STM = 0x02;
    Serial.println("Float array");
  }
  else{
    Serial.println("invalid request");
    client.print("invalid request\r\n");
    delay(1);
    client.flush();
    //client.stop();
    return;
  }
  

  // Prepara la respuesta para LabView
  String s = "";
  //client.setNoDelay(true);

  while (response == 0x00) // Espera a que el STM se sincronice con el ESP
  {
    response = read_command(command2STM);
    ESP.wdtFeed();    // Evita reinicios por watchdog
    Serial.println(response,HEX);
  }
   
    #if (debug == true)
      Serial.print("Comando recibido: ");
      Serial.println(response,HEX);
    #endif
    
    switch(command2STM){
      
      case 0xF0:
        #if (debug == true)
          Serial.println("CASO 0: Configurar");
        #endif
        offset_configurar = req.indexOf("Configurar");
        read_command(String2int(req.substring(offset_configurar+11,offset_configurar+19))); //Config 1
        read_command(String2int(req.substring(offset_configurar+20,offset_configurar+28))); //Config 2
        read_command(String2int(req.substring(offset_configurar+29,offset_configurar+37))); //Config 3
        read_command(String2int(req.substring(offset_configurar+38,offset_configurar+46))); //Config 4

        read_command(String2int(req.substring(offset_configurar+47,offset_configurar+55))); //Channel 1
        read_command(String2int(req.substring(offset_configurar+56,offset_configurar+64))); //Channel 2
        read_command(String2int(req.substring(offset_configurar+65,offset_configurar+73))); //Channel 3
        read_command(String2int(req.substring(offset_configurar+74,offset_configurar+82))); //Channel 4
        read_command(String2int(req.substring(offset_configurar+83,offset_configurar+91))); //Channel 5
        read_command(String2int(req.substring(offset_configurar+92,offset_configurar+100))); //Channel 6
        read_command(String2int(req.substring(offset_configurar+101,offset_configurar+109))); //Channel 7
        read_command(String2int(req.substring(offset_configurar+110,offset_configurar+118))); //Channel 8
        
        #if (debug == true)
          Serial.println("CASO 0: END");
        #endif
        
      break;
      case 0x01:    // El STM va a transmitir un dato de tipo float
        #if (debug == true)
          Serial.println("CASO 1: Single Float");
        #endif
        for (int i = 0; i<8; i++)
        {
        F_data[i] = floatFromSTM();
        Serial.println(F_data[i],7);
        }
        for (int i = 0; i<8; i++)
        {
          Float2TCP(F_data[i], client);
        }
        client.write("\r\n");   //cierra la conexión con Labview
        
//        s += String(F_data,7);
//        s += "\r\n"; 
//        client.print(s);  // Envía la respuesta a LabView
//        client.print(F_data);  // Envía la respuesta a LabView
//        Serial.println(F_data,HEX);
        
//        Udp.beginPacket(remote_IP, 60000); 
//        Udp.write(0x1A); // Envía la respuesta a LabView
//        Udp.endPacket();
        
        #if (debug == true)
          Serial.println("CASO 1: END");
        #endif        
      break;
      case 0x02:
        #if (debug == true)
          Serial.println("CASO 2: ARRAY");
        #endif
        FloatArrayFromSTM(F_data_array, N_elementos);

        for (i=0; i<250; i++)
        {
          Float2TCP(F_data_array[i], client);
          
         //Serial.println(F_data_array[i],7);
//         s += String(F_data_array[i],7);
//         s += "\r"; 
        }
        client.write("\r\n");   //cierra la conexión con Labview
//        s += "\n";
//        client.print(s);  // Envía la respuesta a LabView
        Serial.println("END - CASO 2: ARRAY");
        
        #if (debug == true)
          Serial.println("CASO 2: END");
        #endif        
        
      break;
      default:
        #if (debug == true)
          Serial.println("Default");
        #endif
        ESP.wdtFeed();    // Evita reinicios por watchdog

        client.write("\r\n");   //cierra la conexión con Labview
        
        #if (debug == true)
          Serial.println("Default: END");
        #endif  
        
      break;
    }

  Serial.println("Client disconnected");

  // The client will actually be disconnected 
  // when the function returns and 'client' object is detroyed
  
  client.flush();
  
}





uint8_t read_command(uint8_t command)
{ 
while (digitalRead(DRDY_N)!=LOW)
{
  ESP.wdtFeed();    // Evita reinicios por watchdog
  //Serial.println("Waiting for DRDY");
}

  digitalWrite(HSPI_CS, LOW);
  command = SPI.transfer(command);
  digitalWrite(HSPI_CS, HIGH);
  while(digitalRead(DRDY_N)==LOW){}  // Bloquea hasta que el ESP considera terminada la transmisión

  return command;
}

void FloatArrayFromSTM(float F_array [], int N_elementos)
{
 union miDato{
  struct
  {
    byte  b[4];     // Array de bytes de tamaño igual al tamaño de la primera variable: int = 2 bytes, float = 4 bytes
    }split;
    float fval;
  } F_recibido; 
  F_recibido.fval = 8765.4321;
  int i = 0;

while (digitalRead(DRDY_N)!=LOW)
{
  ESP.wdtFeed();    // Evita reinicios por watchdog
  //Serial.println("Waiting for DRDY");
}
     digitalWrite(HSPI_CS, LOW);
     for (i = 0; i<N_elementos; i++)
        {
           F_recibido.split.b[0] = SPI.transfer(0b01000000);
           F_recibido.split.b[1] = SPI.transfer(0b01000000);
           F_recibido.split.b[2] = SPI.transfer(0b01000000);
           F_recibido.split.b[3] = SPI.transfer(0b01000000);
           
           F_array[i]=F_recibido.fval;
        }
     digitalWrite(HSPI_CS, HIGH);
     
while (digitalRead(DRDY_N)==LOW)
{
  ESP.wdtFeed();    // Evita reinicios por watchdog
} 
 // Espera a que DRDY levante, significa que se ha acabado el bucle.
 // Bloquea el micro pero asegura que no entra más de una vez
      
     if (digitalRead(LED)==1)
       digitalWrite(LED,LOW);
     else
       digitalWrite(LED,HIGH);     
}

float floatFromSTM(void)
{
  union miDato{
    struct
    {
      byte  b[4];     // Array de bytes de tamaño igual al tamaño de la primera variable: int = 2 bytes, float = 4 bytes
    }split;
    float fval;
  } F_recibido;
   
  F_recibido.fval = 8765.4321;
  int i = 0;

  digitalWrite(HSPI_CS, LOW);
  while (digitalRead(DRDY_N)!=LOW)
  {
    ESP.wdtFeed();    // Evita reinicios por watchdog
    //Serial.println("Waiting for DRDY");
  }
  F_recibido.split.b[0] = SPI.transfer(0b01000000);
  F_recibido.split.b[1] = SPI.transfer(0b01000000);
  F_recibido.split.b[2] = SPI.transfer(0b01000000);
  F_recibido.split.b[3] = SPI.transfer(0b01000000);
  
  digitalWrite(HSPI_CS, HIGH);
       
  while (digitalRead(DRDY_N)==LOW)
  {
    ESP.wdtFeed();    // Evita reinicios por watchdog
  } 
  // Espera a que DRDY levante, significa que se ha acabado el bucle.
  // Bloquea el micro pero asegura que no entra más de una vez
      
  if (digitalRead(LED)==1)
   digitalWrite(LED,LOW);
  else
   digitalWrite(LED,HIGH);   

  return F_recibido.fval;
}

uint8_t String2int (String Array)
{
  uint8_t output = 0x00;
  for (int i = 0; i < 8; i++)
    bitWrite(output, i, bitRead(Array[7-i], 0));
  
  return output;
}

void Float2TCP (float Float_data, WiFiClient client)
{
  union miDato{
  struct
  {
    byte  b[4];     // Array de bytes de tamaño igual al tamaño de la primera variable: int = 2 bytes, float = 4 bytes
  }split;
  float fval;
} F_recibido;

  F_recibido.fval = Float_data;
 
  client.write(F_recibido.split.b[3]); // Envía la respuesta a LabView
  client.write(F_recibido.split.b[2]); // Envía la respuesta a LabView
  client.write(F_recibido.split.b[1]); // Envía la respuesta a LabView
  client.write(F_recibido.split.b[0]); // Envía la respuesta a LabView*/
}

