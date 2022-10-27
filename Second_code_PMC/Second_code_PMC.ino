#include "RPC.h"
#include "Wire.h"
#include "config.h"
#include "Arduino.h"
#include <cppQueue.h>
#include <ArduinoJson.h>
#include "ServoPortenta.h"
#include <Arduino_MachineControl.h>

using namespace rtos;
using namespace machinecontrol;

// --------------------------------> Relay values <--------------------------------
ServoPortenta servo(SERVO_PIN);
ServoPortenta servo2(SERVO2_PIN);

uint8_t position = 0; 
bool is_station_latch[no_stations];
uint32_t unlatch_station_time[no_stations]; 
const uint8_t stations[] = {TALL_BODY, TOWER_BODY, JITNEY_BODY, TALL_TAIL, TOWER_TAIL, JITNEY_TAIL};

const uint32_t times[] = {LATCH_TIME, LATCH_TIME * 2, LATCH_TIME * 3, LATCH_TIME, LATCH_TIME * 2, LATCH_TIME * 3 }; 

cppQueue tall_body_queue  (sizeof(uint8_t), stations[0], IMPLEMENTATION, OVERWRITE);
cppQueue tower_body_queue (sizeof(uint8_t), stations[1], IMPLEMENTATION, OVERWRITE);
cppQueue jitney_body_queue(sizeof(uint8_t), stations[2], IMPLEMENTATION, OVERWRITE);
cppQueue tall_tail_queue  (sizeof(uint8_t), stations[3], IMPLEMENTATION, OVERWRITE);
cppQueue tower_tail_queue (sizeof(uint8_t), stations[4], IMPLEMENTATION, OVERWRITE);
cppQueue jitney_tail_queue(sizeof(uint8_t), stations[5], IMPLEMENTATION, OVERWRITE);

void setServo1Angle(uint8_t angle_pos); 
void setServo2Angle(uint8_t angle_pos);
float dataSetToAngle(uint8_t data_index);               
uint8_t getCutPos(uint8_t station_index);
void latchStation(uint8_t index, bool value = false, uint16_t latch_time = 0);

void decodeResponse(std::string responseString){
  JsonObject JsonResponse = JsonObject();
  DynamicJsonDocument doc(1024);
  auto error = deserializeJson(doc, responseString);

  if (error) {
    // Serial.print(F("deserializeJson() failed with code "));
    // Serial.println(error.c_str());
    // Serial.println("Que fracasado dice");
  }

  JsonArray array = doc.as<JsonArray>();
  uint8_t output_values[9];
  uint8_t count = 0;
  for(JsonVariant v : array) {
    output_values[count] = v.as<int>();
    count++;
  }

  // if (output_values[RELAY1]) latchStation(TALL_BODY,   output_values[RELAY1], times[0]);
  // if (output_values[RELAY2]) latchStation(TOWER_BODY,  output_values[RELAY2], times[1]);
  // if (output_values[RELAY3]) latchStation(JITNEY_BODY, output_values[RELAY3], times[2]);
  // if (output_values[RELAY4]) latchStation(TALL_TAIL,   output_values[RELAY4], times[3]);
  // if (output_values[RELAY5]) latchStation(TOWER_TAIL,  output_values[RELAY5], times[4]);
  // if (output_values[RELAY6]) latchStation(JITNEY_TAIL, output_values[RELAY6], times[5]);

  if (output_values[RELAY1]) {
    const uint8_t tall_body_value = getCutPos(0);
    tall_body_queue.push  (&tall_body_value);  
  }
  else if (output_values[RELAY2]) {
    const uint8_t tower_body_value = getCutPos(1);
    tower_body_queue.push (&tower_body_value);  
  }
  else if (output_values[RELAY3]) {
    const uint8_t jitney_body_value = getCutPos(2);
    jitney_body_queue.push(&jitney_body_value); 
  }
  else if (output_values[RELAY4]) {
    const uint8_t tall_tail_value = getCutPos(3);
    tall_tail_queue.push  (&tall_tail_value); 
  }
  else if (output_values[RELAY5]) {
    const uint8_t tower_tail_value = getCutPos(4);
    tower_tail_queue.push (&tower_tail_value); 
  }
  else if (output_values[RELAY6]) {
    const uint8_t jitney_tail_value = getCutPos(5);
    jitney_tail_queue.push(&jitney_tail_value); 
  }
  
  setServo1Angle(output_values[SERVO]);
  setServo2Angle(output_values[SERVO2]);
}

void positionsHasChanged(uint8_t new_position){
  position = new_position;
}

void setup() {
  Wire.begin();
  RPC.begin();
  RPC.bind("decodeResponse", decodeResponse);
  RPC.bind("positionsHasChanged", positionsHasChanged); 

  for (uint8_t i = 0; i < no_stations; i++) {
    is_station_latch[i] = false;
    unlatch_station_time[i] = 0;
  }
 
  if (!digital_programmables.init()) Serial.println("GPIO expander initialization fail!!");
  digital_programmables.setLatch();  
  digital_outputs.setLatch();                        
  digital_outputs.setAll(0);                     
}

void loop() {
  for (uint8_t station_index = 0; station_index < no_stations; station_index++) {                    
    if (is_station_latch[station_index] && unlatch_station_time[station_index] < millis()) latchStation(station_index);
  }

  for (uint8_t i = 0; i < no_stations; i++) {
      bool not_empty = false;
      uint8_t cut_at_position; 
      bool should_cut = false;

      if     (i == 0) not_empty = tall_body_queue.peek  (&cut_at_position);  
      else if(i == 1) not_empty = tower_body_queue.peek (&cut_at_position);  
      else if(i == 2) not_empty = jitney_body_queue.peek(&cut_at_position); 
      else if(i == 3) not_empty = tall_tail_queue.peek  (&cut_at_position); 
      else if(i == 4) not_empty = tower_tail_queue.peek (&cut_at_position); 
      else if(i == 5) not_empty = jitney_tail_queue.peek(&cut_at_position); 
    
      if(not_empty) should_cut = cut_at_position == position;

      if(should_cut) { 
        if     (i == 0) tall_body_queue.pop   (&cut_at_position);  
        else if(i == 1) tower_body_queue.pop  (&cut_at_position);  
        else if(i == 2) jitney_body_queue.pop (&cut_at_position); 
        else if(i == 3) tall_tail_queue.pop   (&cut_at_position); 
        else if(i == 4) tower_tail_queue.pop  (&cut_at_position); 
        else if(i == 5) jitney_tail_queue.pop (&cut_at_position); 
        latchStation(i, true);
      }
    }
    
}

void latchStation(uint8_t index, bool value, uint16_t latch_time){
  if (value) unlatch_station_time[index] = millis() + times[index];
  is_station_latch[index] = value;
  digital_outputs.set(stations[index], value);
}

void setServo1Angle(uint8_t angle_pos){
  const float angle = dataSetToAngle(angle_pos);
  servo.moveTo(angle);
}

void setServo2Angle(uint8_t angle_pos){
  const float angle = dataSetToAngle(angle_pos);
  servo2.moveTo(angle);
}

float dataSetToAngle(uint8_t data_index){
  const uint8_t data_size = sizeof(positions)/4;
  const uint8_t index = data_index == HOME ? data_size - 1: data_index - OFFSET;
  return positions[index];
}

uint8_t getCutPos(uint8_t station_index){
  uint8_t cut_at_position = stations[station_index] + position; 
  if (cut_at_position > MAX_FLAG) cut_at_position -= MAX_FLAG;
  return cut_at_position;  
}

void setUp








// #include "RPC.h"
// #include "Wire.h"
// #include "config.h"
// #include "Arduino.h"
// #include <ArduinoJson.h>
// #include "ServoPortenta.h"
// #include <Arduino_MachineControl.h>

// using namespace rtos;
// using namespace machinecontrol;

// // --------------------------------> Relay values <--------------------------------
// ServoPortenta servo(SERVO_PIN);
// ServoPortenta servo2(SERVO2_PIN);

// boolean is_tall_body_latch = false;                  
// boolean is_tower_body_latch = false;                 
// boolean is_jitney_body_latch = false;                
// boolean is_tall_tail_latch = false;                  
// boolean is_tower_tail_latch = false;                 
// boolean is_jitney_tail_latch = false;                
// unsigned long unlatch_tall_body_time = 0;            
// unsigned long unlatch_tower_body_time = 0;           
// unsigned long unlatch_jitney_body_time = 0;          
// unsigned long unlatch_tall_tail_time = 0;            
// unsigned long unlatch_tower_tail_time = 0;           
// unsigned long unlatch_jitney_tail_time = 0;  


// void latchTallBody(boolean value);                   
// void latchTowerBody(boolean value);                  
// void latchJitneyBody(boolean value);                 
// void latchTallTail(boolean value);                   
// void latchTowerTail(boolean value);                  
// void latchJitneyTail(boolean value); 

// void setServo1Angle(uint8_t angle_pos); 
// void setServo2Angle(uint8_t angle_pos);
// float dataSetToAngle(uint8_t data_index);               

// void decodeResponse(std::string responseString){
//   JsonObject JsonResponse = JsonObject();
//   DynamicJsonDocument doc(1024);
//   auto error = deserializeJson(doc, responseString);

//   if (error) {
//     // Serial.print(F("deserializeJson() failed with code "));
//     // Serial.println(error.c_str());
//     // Serial.println("Que fracasado dice");
//   }

//   JsonArray array = doc.as<JsonArray>();
//   uint8_t output_values[9];
//   uint8_t count = 0;
//   for(JsonVariant v : array) {
//     output_values[count] = v.as<int>();
//     count++;
//   }

//   for (uint8_t station_index = 0; station_index < no_stations; station_index++) {
//     latchStation(station_index, LATCH_TIME, output_values[station_index]);
//   }  

  // latchTallBody(output_values[RELAY1]);
  // latchTowerBody(output_values[RELAY2]);
  // latchJitneyBody(output_values[RELAY3]);
  // latchTallTail(output_values[RELAY4]);
  // latchTowerTail(output_values[RELAY5]);
  // latchJitneyTail(output_values[RELAY6]);
  // setServo1Angle(output_values[SERVO]);
  // setServo2Angle(output_values[SERVO2]);
// }

// void setup() {
//   Wire.begin();
//   RPC.begin();
//   RPC.bind("decodeResponse", decodeResponse); 
 
//   if (!digital_programmables.init()) Serial.println("GPIO expander initialization fail!!");
//   digital_programmables.setLatch();  
//   digital_outputs.setLatch();                        
//   digital_outputs.setAll(0);                     
// }

// void loop() {

  // if(is_tall_body_latch && unlatch_tall_body_time < millis()) latchTallBody(false);         

  // if(is_tower_body_latch && unlatch_tower_body_time < millis()) latchTowerBody(false);      

  // if(is_jitney_body_latch && unlatch_jitney_body_time < millis()) latchJitneyBody(false);   

  // if(is_tall_tail_latch && unlatch_tall_tail_time < millis()) latchTallTail(false);         

  // if(is_tower_tail_latch && unlatch_tower_tail_time < millis()) latchTowerTail(false);      

  // if(is_jitney_tail_latch && unlatch_jitney_tail_time < millis()) latchJitneyTail(false);   
  
// }

// void latchTallBody(boolean value){
//   is_tall_body_latch = value;
//   unlatch_tall_body_time = millis() + LATCH_TIME;
//   digital_outputs.set(TALL1, value);
// }

// void latchTowerBody(boolean value){
//   is_tower_body_latch = value;
//   unlatch_tower_body_time = millis() + LATCH_TIME * 2;
//   digital_outputs.set(TOWER1, value);
// }

// void latchJitneyBody(boolean value){
//   is_jitney_body_latch = value;
//   unlatch_jitney_body_time = millis() + LATCH_TIME * 3;
//   digital_outputs.set(JITNEY1, value);
// }

// void latchTallTail(boolean value){
//  is_tall_tail_latch = value;
//   unlatch_tall_tail_time = millis() + LATCH_TIME;
//   digital_outputs.set(TALL2, value);
// }

// void latchTowerTail(boolean value){
//   is_tower_tail_latch = value;
//   unlatch_tower_tail_time = millis() + LATCH_TIME * 2;
//   digital_outputs.set(TOWER2, value);
// }

// void latchJitneyTail(boolean value){
//   is_jitney_tail_latch = value;
//   unlatch_jitney_tail_time = millis() + LATCH_TIME * 3;
//   digital_outputs.set(JITNEY2, value);
// }

// void setServo1Angle(uint8_t angle_pos){
//    const float angle = dataSetToAngle(angle_pos);
//   servo.moveTo(angle);
// }

// void setServo2Angle(uint8_t angle_pos){
//   const float angle = dataSetToAngle(angle_pos);
//   servo2.moveTo(angle);
// }

// float dataSetToAngle(uint8_t data_index){
//   const uint8_t data_size = sizeof(positions)/4;
//   const uint8_t index = data_index == HOME ? data_size - 1: data_index - OFFSET;
//   return positions[index];
// }

