/**
 * The MySensors Arduino library handles the wireless radio link and protocol
 * between your home built sensors/actuators and HA controller of choice.
 * The sensors forms a self healing radio network with optional repeaters. Each
 * repeater and gateway builds a routing tables in EEPROM which keeps track of the
 * network topology allowing messages to be routed to nodes.
 *
 * Created by Henrik Ekblad <henrik.ekblad@mysensors.org>
 * Copyright (C) 2013-2015 Sensnology AB
 * Full contributor list: https://github.com/mysensors/Arduino/graphs/contributors
 *
 * Documentation: http://www.mysensors.org
 * Support Forum: http://forum.mysensors.org
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 *******************************
 *
 * REVISION HISTORY
 * Version 1.0 - Henrik Ekblad
 * 
 * DESCRIPTION
 * Example sketch showing how to control physical relays. 
 * This example will remember relay state after power failure.
 * http://www.mysensors.org/build/relay
 */ 

#define MY_DEBUG 
#define MY_RADIO_NRF24

#define MY_NODE_ID 44
#define MY_RF24_CE_PIN 6
#define MY_RF24_CS_PIN 7
#define MY_RF24_DATARATE RF24_250KBPS



// Enable repeater functionality for this node
//#define MY_REPEATER_FEATURE


#include "cat-feeder.h"
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

// Set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27, 16, 2);
#include <SPI.h>
#include <MySensors.h>
#include <Time.h>  


#define RELAY_1  2  // Arduino Digital I/O pin number for first relay (second on pin+1 etc)
#define NUMBER_OF_RELAYS 1 // Total number of attached relays
#define RELAY_ON 1  // GPIO value to write to turn on attached relay
#define RELAY_OFF 0 // GPIO value to write to turn off attached relay

// NRFRF24L01 radio driver (set low transmit power by default) 

//MyTransportNRF24 radio(RF24_CE_PIN, RF24_CS_PIN, RF24_PA_LEVEL_GW);  
//MyTransportRFM69 radio;
// Message signing driver (none default)
//MySigningNone signer;
// Select AtMega328 hardware profile
//MyHwATMega328 hw;
// Construct MySensors library
//MySensor gw(radio, hw);

//unsigned long epoch;






unsigned long next_meal_time = 0;
unsigned long next_destick_time = 0;
unsigned long dispense_stop_time = 0;
MEAL_STATE next_meal_state = meal_waiting;

void receiveTime(unsigned long controllerTime) {
  // Ok, set incoming time 
  //Serial.print("Time value received: ");
  //Serial.println(controllerTime);
  setTime(controllerTime);
  //setTime(1463918390); // 11:59:55 am
  //epoch = controllerTime;
  displayTime();
}

void time_hms(char * buf, time_t t){
  sprintf(buf, "%02d:%02d:%02d", hour(t), minute(t), second(t));
}


DISPLAY_PAGE display_page_current = display_time;
time_t next_display_page = 0;


void setDisplayPage(enum DISPLAY_PAGE page){
  display_page_current = page;
  next_display_page = now() + DISPLAY_PAGE_DURATION;
  lcd.clear();
}

void displayDispense(){
  if(next_meal_state == meal_waiting){
    lcd.setCursor(0,0);
    lcd.print("Next feed @ ");
    lcd.print(hour(next_meal_time),DEC);
    lcd.setCursor(3,1);
    lcd.print("in ");
    char buf[16] = "";
    time_hms(buf, next_meal_time - now());
    lcd.print(buf);
  }else if(next_meal_state == meal_dispensing){
    lcd.setCursor(0,0);
    lcd.print("Dispensing food ");
    lcd.setCursor(0,1);
    unsigned long tmp = dispense_stop_time - now();
    for(byte i = 0; i < 16; i++){
      lcd.print((i < tmp & 0xFF ? "." : " "));
    }
  }
  
}

void displayTime(){
  char timestr[16] = "";
  time_hms(timestr, now());
  
  //lcd.clear();
  lcd.setCursor(3,0);
  lcd.print("Time ");
  lcd.print(timestr);

  lcd.setCursor(3,1);
  for(byte i=0;i< MAX_FEEDINGS;i++){
    if(feeding_times[i] == -1){
      lcd.print(" -");
    }else{
      lcd.print(feeding_times[i],DEC);
    }
    lcd.print(" ");
  }
}

void setup()  
{ 
  Serial.begin(115200);
  Serial.println("Init");
  // initialize the LCD
  lcd.begin();
  // Turn on the blacklight and print a message.
  lcd.backlight();
  lcd.print("Initialising");

  
  // Initialize library and add callback for incoming messages
  //gw.begin(incomingMessage, AUTO, true);
  lcd.clear();
  lcd.print("CatbutFud v0.1");
  // Send the sketch version information to the gateway and Controller
  sendSketchInfo("CatbutFud", "0.1");
  
  // Fetch relay status
  for (int sensor=1, pin=RELAY_1; sensor<=NUMBER_OF_RELAYS;sensor++, pin++) {
    // Register all sensors to gw (they will be created as child devices)
    present(sensor, S_LIGHT);
    // Then set relay pins in output mode
    pinMode(pin, OUTPUT);   
    // Set relay to last known state (using eeprom storage) 
    digitalWrite(pin, loadState(sensor)?RELAY_ON:RELAY_OFF);
  }

  // get the time

  lcd.clear();
  lcd.print("Requesting time");
  timesync();
  // wait up to fifteen seconds
  unsigned long timeout = millis() + 15000UL;
  while(now() < 100000UL && millis() > timeout){
    //gw.process();
  }
  if(now() <  100000UL){
    lcd.clear();
    lcd.print("No time sync!");
    lcd.setCursor(0,1);
    lcd.print("check gateway");
  }
  
  setDisplayPage(display_time);

}

// start of day for specified timestamp
#define SECONDS_IN_DAY (24UL * 60UL * 60UL)
unsigned long start_of_day(time_t t){
  unsigned long tmp = t / SECONDS_IN_DAY;
  return tmp * SECONDS_IN_DAY;
}

// sync time every 60 minutes
#define TIME_SYNC_INTERVAL_MS 5*60000UL
unsigned long next_time_sync = 0;
void timesync(){
  if(millis() < next_time_sync) return;
  requestTime();  
  next_time_sync = (next_time_sync == 0 ? millis() : next_time_sync) + TIME_SYNC_INTERVAL_MS;
}

// refresh display every 500ms
#define REFRESH_INTERVAL_MS 500
unsigned long next_display_refresh = 0;
void screen(){
  if(millis() < next_display_refresh) return;
  
  
  if(now() >= next_display_page){
    if(display_page_current == display_dispense){
      setDisplayPage(display_time);
    }else{
      setDisplayPage(display_dispense);
    }
  }

  if (display_page_current == display_dispense || next_meal_state == meal_dispensing){
    displayDispense();
  }else if (display_page_current == display_time){
    displayTime();
  }
  next_display_refresh = (next_display_refresh == 0 ? millis() : next_display_refresh) + REFRESH_INTERVAL_MS;
}

void update_next_meal(){
  // get the next feeding time
  unsigned long t = now();
  short latest_feeding = -1;
  short next_feeding = -1;
  for(byte i=0;i<MAX_FEEDINGS;i++){
    if(feeding_times[i] == -1) continue;
    // this is the 'latest' one
    latest_feeding = feeding_times[i];
    // check if this is the next feeding time
    if(feeding_times[i] > hour(t) && next_feeding == -1){
      next_feeding = feeding_times[i];
    }
  }
  unsigned long tmp;
  //Serial.println(next_feeding, DEC);
  if(next_feeding != -1){
    //Serial.println("today");
    // next feeding is today
    tmp = next_feeding;
    // calculate hour of day as ms after start of day
    tmp = tmp * 60UL * 60UL;
    // add the offset to start of today
    next_meal_time = start_of_day(t) + tmp;
  }else{
    // next feeding is tomorrow
    //Serial.println("tomorrow");
    tmp = feeding_times[0];
    // calculate hour of day as ms after start of day
    tmp = tmp * 60UL * 60UL;
    // return next feeding
    next_meal_time = start_of_day(t+SECONDS_IN_DAY) + tmp;
  }
  next_meal_state = meal_waiting;
}

void dispense_next_meal(){
  next_meal_state = meal_dispensing;
  dispense_stop_time = now() + DISPENSER_RUN_SECONDS;
  digitalWrite(RELAY_1, 1);
  
  MyMessage msg(1, V_LIGHT);
  send(msg.set(RELAY_ON));
  
}

void finish_dispensing(){
  digitalWrite(RELAY_1,0);
  MyMessage msg(1, V_LIGHT);
  send(msg.set(RELAY_OFF)); 
  
  next_meal_state = meal_dispensed;
}

void destick(){
  digitalWrite(RELAY_1,0);
  delay(DESTICK_MS);
  digitalWrite(RELAY_1,1);
  next_destick_time = now() + DESTICK_INTERVAL;
}

void dispense(){
  if(next_meal_state == meal_waiting && now() >= next_meal_time){
    dispense_next_meal();
    destick();
  }
  if(next_meal_state == meal_dispensing){
    if(now() > next_destick_time) destick();
    if(now() > dispense_stop_time) finish_dispensing();
  }
  if(next_meal_state == meal_dispensed){
    update_next_meal();
  }
}


void loop() 
{
  // Alway process incoming messages whenever possible
  //process();
  
  dispense();
  screen();
  timesync();
  
}

void receive(const MyMessage &message) {
  // We only expect one type of message from controller. But we better check anyway.
  if (message.type==V_LIGHT) {
    // initiate feeding
    if(message.getBool()){
      next_meal_time = now();
    }
    
     // Change relay state
     digitalWrite(message.sensor-1+RELAY_1, message.getBool()?RELAY_ON:RELAY_OFF);
     // Store state in eeprom
     saveState(message.sensor, message.getBool());
     // Write some debug info
     //Serial.print("Incoming change for sensor:");
     //Serial.print(message.sensor);
     //Serial.print(", New status: ");
     //Serial.println(message.getBool());
   } 
}

