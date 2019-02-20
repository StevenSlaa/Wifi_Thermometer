#include <Adafruit_NeoPixel.h> //Neopixel library
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include "LedControl.h" //8x8 matrix library

#define PIN            D5 //pin Din Neopixel
#define NUMPIXELS      16 //led in pins
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

LedControl lc = LedControl(D6, D8, D7, 1);

#define insideSensor D3 //inside temperature Sensor pin
#define outsideSensor D2 //outside temperature Sensor pin
#define ADC A0 //outside temperature Sensor pin
int tempBuiten, tempBinnen; //Temperature data as integer
unsigned long DeltaTime = 0; //Finite state machine time

const char *ssid = "TempMonitor";
const char *password = "its2hot";

ESP8266WebServer server(80);

void handleRoot() { //html page for webserver 
  String index = "<!DOCTYPE html>";
  index += "<html>";
  index += "<head>";
  index += "<title>Temperature Server</title>";
  index += "</head>";
  index += "<body>";
  index += "<p>Binnen temperatuur <span id='buiten'>" + String(tempBuiten, DEC) + "</span></p>";
  index += "<p>Buiten temperatuur <span id='binnen'>" + String(tempBinnen, DEC) + "</span></p>";
  index += "</body>";
  index += "</html> ";
  server.send(200, "text/html", index);
}

void initMatrix() { //setup 8x8 matrix display
  Serial.print("Initializing Matrix...");
  lc.shutdown(0, false);
  lc.setIntensity(0, 5);
  lc.clearDisplay(0);
  delay(200);
  Serial.println("DONE");
}

void matrixWrite(int num, bool outside) { //Write numbers too matrix display
  lc.clearDisplay(0);
  /* here is the data for the characters */
  byte degree = B10000000;
  byte minchar[] = {B00010000, B00010000, B00010000};
  //numbers
  byte number[10][3] = {
    {B01111100, B01000100, B01111100}, //zero
    {B00000000, B01111100, B00000000}, //one
    {B01011100, B01010100, B01110100}, //two
    {B01010100, B01010100, B01111100}, //three
    {B01110000, B00010000, B01111100}, //four
    {B01110100, B01010100, B01011100}, //five
    {B01111100, B01010100, B01011100}, //six
    {B01000000, B01000000, B01111100}, //seven
    {B01111100, B01010100, B01111100}, //eight
    {B01110100, B01010100, B01111100} //nine
  };
  if (num > 10) {
    //Twee digits
    int num1 = num / 10;
    int num2 = num - (num1 * 10);
    for (int i = 0; i <= 2; i++) {
      lc.setColumn(0, i, number[num1][i]);
    }
    for (int i = 0; i <= 2; i++) {
      lc.setColumn(0, i + 4, number[num2][i]);
    }
    lc.setColumn(0, 7, degree); //Graden Teken
  } else if (num < 10 && num > 0) {
    //Eén digit POSITIEF
    for (int i = 0; i <= 2; i++) {
      lc.setColumn(0, i + 2, number[num][i]);
    }
    lc.setColumn(0, 5, degree); //Graden Teken
  } else if (num < 0) {
    //Eén digit NEGATIEF
    for (int i = 0; i <= 2; i++) {
      lc.setColumn(0, i, minchar[i]);
    }
    for (int i = 0; i <= 2; i++) {
      lc.setColumn(0, i + 4, number[num * -1][i]);
    }
    lc.setColumn(0, 7, degree); //Graden Teken
  }
  if (outside == true) {
    lc.setRow(0, 7, B11000011);
  } else {
    lc.setRow(0, 7, B00111100);
  }
}

void initNeoRing() { //setup Neopixel ring
  Serial.print("Intintializing NeoPixels...");
  strip.begin();
  strip.setBrightness(255); //adjust brightness here
  strip.show(); // Initialize all pixels to 'off'
  delay(200);
  Serial.println("DONE");
}

void setNeoTemp(int CurrTemp) { //set Neopixel color within temperature range
  if (CurrTemp > 30) {
    colorFade(255, 0, 0, 10);
  } else if (CurrTemp <= 30 && CurrTemp > 25) {
    colorFade(255, 71, 0, 10);
  } else if (CurrTemp <= 25 && CurrTemp > 20) {
    colorFade(245, 126, 10, 10);
  } else if (CurrTemp <= 20 && CurrTemp > 15) {
    colorFade(198, 142, 57, 10);
  } else if (CurrTemp <= 15 && CurrTemp > 10) {
    colorFade(136, 156, 119, 10);
  } else if (CurrTemp <= 10 && CurrTemp > 5) {
    colorFade(73, 167, 182, 10);
  } else if (CurrTemp <= 5 && CurrTemp > 0) {
    colorFade(21, 176, 234, 10);
  } else if (CurrTemp < 0) { 
    colorFade(0, 180, 255, 10);
  }
}

void colorFade(uint8_t r, uint8_t g, uint8_t b, int wait) { //fade nicely from color to color funtion
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    uint8_t curr_r, curr_g, curr_b;
    uint32_t curr_col = strip.getPixelColor(i); // get the current colour
    curr_b = curr_col & 0xFF; curr_g = (curr_col >> 8) & 0xFF; curr_r = (curr_col >> 16) & 0xFF;  // separate into RGB components

    while ((curr_r != r) || (curr_g != g) || (curr_b != b)) { // while the curr color is not yet the target color
      if (curr_r < r) curr_r++; else if (curr_r > r) curr_r--;  // increment or decrement the old color values
      if (curr_g < g) curr_g++; else if (curr_g > g) curr_g--;
      if (curr_b < b) curr_b++; else if (curr_b > b) curr_b--;
      delay(wait);
      for (int j = 0; j < 17; j++) {
        strip.setPixelColor(j, curr_r, curr_g, curr_b);  // set the color
      }
      strip.show();
    }
  }
}

double getTemp(int sensor){ //get temperature from NTC and do calibration
  digitalWrite(sensor, HIGH);                 // turn on the selected sensor
  const double A = 0.0007464661219;           // constants A,B & C for the Steinhart-Hart equation
  const double B = 0.0002058397550;
  const double C = 0.0000001455340258;
  const int samples = 1000;                   // amount of samples we'll be taking (need multiple samples for accurate results)
  double temp = 0;                            // variable that will hold the measurements and eventually the temperature itself
  for (int measure = 0; measure < samples; measure++) {
    // read the value, translate it to resistance, and take the natural log (log is already e_log)
    temp += log( 10000.0 * ( (1023.0/analogRead(A0) - 1 ))); 
    // take log here with the measurements, it will take a little longer, but we'll gain accuracy 'cause log has rounding errors   
  }
  temp = temp / samples;                      // take the average of the measurements
  temp = 1 / (A + (B * temp) + (C * temp * temp * temp)) - 273.15;
  // T = 1 / (A + B * ln(R) + C * ln(R)^3)    //  subtract 273.15 for Celsius
  digitalWrite(sensor, LOW);                  // turn off the sensor after using it
  return temp;                                // return the requested value
}

void setup() {
  Serial.begin(115200);
  Serial.println();

  pinMode(ADC, INPUT);
  pinMode(insideSensor, OUTPUT);
  pinMode(outsideSensor, OUTPUT);

  //Setup application webserver
  Serial.print("Configuring access point...");
  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  server.on("/", handleRoot);
  server.begin();
  Serial.println("HTTP server started");
  
  initNeoRing(); //setup Neopixels
  initMatrix();  //setup 8x8 matrix

}

void loop() {
  unsigned long currentMillis = millis(); 
  server.handleClient();
   if (millis() - DeltaTime <= 5000) {                                               // 5 seconden op binnen
    matrixWrite(tempBinnen-5, false);
    setNeoTemp(tempBinnen);
  } else if (millis() - DeltaTime > 5000 and millis() - DeltaTime <= 10000) {          // 5 seconden op Buiten
    matrixWrite(tempBuiten, true);
    setNeoTemp(tempBuiten);
  }else if (millis() - DeltaTime > 10000) {                                 // reset DeltaTime naar millis
    DeltaTime = millis();
  }
  tempBinnen = (int)getTemp(insideSensor);  //set tempBinnen to accurate temperature
  tempBuiten = (int)getTemp(outsideSensor); //set tempBuiten to accurate temperature
}

