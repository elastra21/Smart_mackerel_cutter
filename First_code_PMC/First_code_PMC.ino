#include "Arduino.h"
#include "RPC.h"
#include "Wire.h"
#include "config.h"
#include <Ethernet.h>
#include <ArduinoJson.h>
#include <EthernetUdp.h>
#include <PortentaEthernet.h>
#include <Arduino_MachineControl.h>

using namespace rtos;
using namespace machinecontrol;

EthernetUDP Udp;
boolean flag = false;
IPAddress ip(192, 168, 1, 177);

void UDPLoop();
void printFlag();
void printUltra(float fish_size);
void sendMessage(const char* data);
void decodeResponse(std::string responseString);

void setup() {
  RPC.begin();
  Wire.begin();
  Ethernet.begin(ip);
  Serial.begin(115200);
  analogReadResolution(16);
  analog_in.set0_10V();

  

  while (!Serial);
  Serial.println("Ready");
  
  if (!digital_inputs.init()) Serial.println("Digital input GPIO expander initialization fail!!");
  
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
    while (true) delay(1);
  }

  if (Ethernet.linkStatus() == LinkOFF) Serial.println("Ethernet cable is not connected.");
   
  Udp.begin(PORT);
}

void loop() {

  UDPLoop();

  if (digital_inputs.read(DIN_READ_CH_PIN_07) == HIGH ) flag = true;

  if (digital_inputs.read(DIN_READ_CH_PIN_07) == LOW && flag){
    flag = false;
    printFlag();
    const float raw_voltage_ch1 = analog_in.read(1);
    printUltra(raw_voltage_ch1/100);
  }

}

void UDPLoop(){
  const uint8_t packetSize = Udp.parsePacket();
  if (packetSize > 0) {
    Serial.println("algo esta");
    char packetBuffer[packetSize];
    Udp.read(packetBuffer, packetSize);
    String serial_buffer = String(packetBuffer);
    Serial.println("Contents:"); Serial.println(serial_buffer);
    if (serial_buffer == HANDSHAKE) return;
    RPC.call("decodeResponse", serial_buffer.c_str());
  }
}

void printFlag(){
  const String msg = "{\"act\":"+String(FLAG)+"}";  
  sendMessage(msg.c_str());
}

void printUltra(float fish_size){
  const String message = "{\"act\":"+String(ULTRA)+",\"sz\":"+ String(fish_size) +"}";
  sendMessage(message.c_str());
}

void sendMessage(const char* data){
  Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
  Udp.write(data);
  Udp.endPacket();
}



