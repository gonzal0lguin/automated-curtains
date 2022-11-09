/*Code by Gonzalo Olguin Moncada, Universidad de Chile
This code is intended to automate blinds so they open at a specific time
or open/close with sound signals like applauses.
We are using an ESP32 just to add a time input by BLE without the need 
of a manual adjust.


21->sda, 22 ->scl/sck (NodeMCU)
A4->sda, A5->scl (Arduino UNO)
*/


//#include <Servo_ESP32.h> //will be controlled by slave
//#include <DHT.h> //will be used by slave
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>

//BLYNK libraries
#include <BlynkSimpleEsp32_BLE.h>
#include <BLEDevice.h>
#include <BLEServer.h>

#include <AccelStepper.h>


#define BLYNK_PRINT Serial
#define BLYNK_USE_DIRECT_CONNECT

#define SLAVE_ADDR 9 //arduino UNO slave address

//#define DHT_pin 33
//#define DHTTYPE DHT11 

#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64 
#define OLED_RESET     -1

#define displayTransistorPin 

#define dirPinRight 12
#define stepPinRight 14
#define dirPinLeft 26
#define stepPinLeft 25

#define relayPin 27
#define IR_sensor_pin_der 35
#define IR_sensor_pin_izq 15
#define IR_sensor_transistor_pin 33
#define ledPin  19        //not adc
#define buttonPin 34

#define motorInterfaceType 1 

char auth[] = "coW4UkPEj-tHHvziMzlNxHTdb_Bramku"; //e-mail blykn auth token
   
const int menuVal = 200;
const int rightVal = 600;    
const int leftVal = 1450;    
const int sleepVal = 3500;          

int menuState = LOW;         
int menuBtn;             
int lastMenuBtn = HIGH;   
unsigned long lastTimeMenu = 0;  

int rightState = LOW;         
int rightBtn;             
int lastRightBtn = HIGH;   
unsigned long lastTimeRight = 0;  

int leftState = LOW;         
int leftBtn;             
int lastLeftBtn = HIGH;   
unsigned long lastTimeLeft = 0;  

int sleepState = LOW;         
int sleepBtn;             
int lastSleepBtn = HIGH;   

unsigned long lastTimeSleep = 0;  
unsigned long debounceTime = 50;

unsigned long previousMillis = 0;
unsigned long previousMillisDHT = 0;
unsigned long previousMillisAMP = 0;
unsigned long previousMillisDif = 0;

//esta es pa ver nomas dssps la borrai
unsigned long previous=0;

//Sound variables
const unsigned long difference = 100;
const unsigned long ampRefreshRate = 150;
const unsigned long dt = 700;

int searchIndex = 0;

//boolean variables for function to open/close curtains
bool positionRight = false;
bool positionLeft = false;

// speed and acceleration (can be changed by app)
float speed_stepper = 800;
float accel_stepper = 1700;

// declaration of sensors and actuators


AccelStepper stepperRight = AccelStepper(AccelStepper::DRIVER, stepPinRight, dirPinRight);
AccelStepper stepperLeft = AccelStepper(AccelStepper::DRIVER, stepPinLeft, dirPinLeft);


//DHT dht(DHT_pin, DHTTYPE);

RTC_DS3231 RTC;

//Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


//arrays to displays date in the display

String DaysWeek[7] = {"Domingo", "Lunes", "Martes", "Miercoles", "Jueves", "Viernes",
"Sábado"};

String Months[12] = { "Enero", "Febrero", "Marzo", "Abril", "Mayo",  "Junio", "Julio"
,"Agosto","Septiembre","Octubre","Noviembre","Diciembre" };


//arrays to store alarm times --> will be changed by Blynk or manually

int scheadule_hours[7] = {9, 9, 9, 9, 9, 10, 10};
int scheadule_minutes[7] = {00, 00, 15, 15, 00, 30, 30};


//---------------------AUXILLIARY FUNCTIONS--------------------

// Function to transfrom days from [sunday, (...), saturday] to [monday, (...), sunday].
int dayTransform(int day){
  if (day == 0) {return 7;}
  else {return day;}
  }


//Function to open/close curtains
void moveStepper(int pos, int speed, int accel){
  digitalWrite(relayPin, true);
  digitalWrite(ledPin, true);
  digitalWrite(IR_sensor_transistor_pin, true);
  
    stepperRight.setMaxSpeed(speed);
    stepperRight.setAcceleration(accel);
    stepperRight.moveTo(pos);

    stepperLeft.setMaxSpeed(speed);
    stepperLeft.setAcceleration(accel);
    stepperLeft.moveTo(pos);
  
}

//Function to change scheadule MANUALLY
void changeWakeTime(int hours, int minutes, int dayInput, int timeInputH, int timeInputM){
  scheadule_hours[dayInput] = timeInputH;
  scheadule_minutes[dayInput] = timeInputM;
  }
  

// void goToSleep(){
//   digitalWrite(displayTransistorPin, LOW);
//   }


//----------------BLYNK FUNCTIONS--------------------------------------

BlynkTimer timer;

int rowIndex1 = 0;
int rowIndex2 = 1;
int rowIndex3 = 2;
int rowIndex4 = 3;
int rowIndex5 = 4;
int rowIndex6 = 5;
int rowIndex7 = 6;


// Function to display scheadule in blynk app
void Table() {
  // adding rows to table
  Blynk.virtualWrite(V2, "clr"); //clear table
  Blynk.virtualWrite(V2, "add", rowIndex1, "Wakeup Lunes", (String)scheadule_hours[0] + ":" + (String)scheadule_minutes[0]);
  Blynk.virtualWrite(V2, "add", rowIndex2, "Wakeup Martes", (String)scheadule_hours[1] + ":" + (String)scheadule_minutes[1]);
  Blynk.virtualWrite(V2, "add", rowIndex3, "Wakeup Miercoles", (String)scheadule_hours[2] + ":" + (String)scheadule_minutes[2]);
  Blynk.virtualWrite(V2, "add", rowIndex4, "Wakeup Jueves", (String)scheadule_hours[3] + ":" + (String)scheadule_minutes[3]);
  Blynk.virtualWrite(V2, "add", rowIndex5, "Wakeup Viernes", (String)scheadule_hours[4] + ":" + (String)scheadule_minutes[4]);
  Blynk.virtualWrite(V2, "add", rowIndex6, "Wakeup Sábado", (String)scheadule_hours[5] + ":" + (String)scheadule_minutes[5]);
  Blynk.virtualWrite(V2, "add", rowIndex7, "Wakeup Domingo", (String)scheadule_hours[6] + ":" + (String)scheadule_minutes[6]);
  
  //highlighting latest added row in table
  //Blynk.virtualWrite(V2, "pick", rowIndex);

  //rowIndex++; // asi se sobreescriben y aparecen infinitas columnas
}

/*void TempHumGauge() {

  float hum = dht.readHumidity();
  float temp = dht.readTemperature();
  
  Blynk.virtualWrite(V3, temp);
  Blynk.virtualWrite(V6, hum);
  }*/


//Funtion to change scheadule through Blynk app

BLYNK_WRITE(V1){
  
  TimeInputParam t(param);

  if (t.hasStartTime()) {
    int start_hour = t.getStartHour();
    int start_minute = t.getStartMinute();

    for (int k = 1; k <= 7; k++){
      if (t.isWeekdaySelected(k)) {
        scheadule_hours[k-1] = start_hour;
        scheadule_minutes[k-1] = start_minute; 
        }
      }
    Table(); // instant refresh of display table
    }  
  }   

//Function to refresh screen data on blynk app
BLYNK_WRITE(V4){
  int refresh = param.asInt();
  if (refresh == true){
    Table();
    }
  }

BLYNK_WRITE(V5) {
  int test = param.asInt();
  if (test == true){
    moveStepper(2000, 1000, 1000);
    }
  }  

BLYNK_READ(V0){
  DateTime date = RTC.now();
  unsigned long second = date.second();
  unsigned long minute = date.minute();
  unsigned long hour = date.hour();
  Blynk.virtualWrite(V0, hour*10000 + minute*100 + second);
  }

BLYNK_WRITE(V7) {
  float dt = param.asFloat();
  speed_stepper = dt;
}

BLYNK_WRITE(V8) {
  float dt = param.asFloat();
  accel_stepper = dt;
}

//-----------------SETUP AND LOOP--------------------------------------

void setup() {
  
  //Serial.begin(115200);
  Serial.begin(9600);
  
    if (! RTC.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort();
  }

  if (RTC.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    RTC.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

  Blynk.begin(auth);
  Blynk.virtualWrite(V2, "clr"); //clear table
  timer.setInterval(100000L, Table);
  //timer.setInterval(4000L, TempHumGauge);
  delay(5);
  pinMode(buttonPin, INPUT);
  pinMode(IR_sensor_pin_der, INPUT);
  pinMode(IR_sensor_pin_izq, INPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(relayPin, OUTPUT);
  pinMode(IR_sensor_transistor_pin, OUTPUT);
  // pinMode(18, OUTPUT);
  // pinMode(17, OUTPUT);
  // pinMode(0, OUTPUT); 

  pinMode(stepPinRight,OUTPUT);
  pinMode(dirPinRight,OUTPUT);
  pinMode(stepPinLeft,OUTPUT);
  pinMode(dirPinLeft,OUTPUT);
}



void loop() {

timer.run();
Blynk.run();

DateTime date = RTC.now();

  unsigned long hour = date.hour();
  unsigned long minute = date.minute();
  unsigned long day = date.dayOfTheWeek();
  
  unsigned long current = millis();
  
  
  //float humidity = dht.readHumidity();
  //float temperature = dht.readTemperature();

  int menuRead = (int)analogRead(34)/menuVal;
  int rightRead = (int)analogRead(34)/rightVal;
  int leftRead = (int)analogRead(34)/leftVal;
  int sleepRead = (int)analogRead(34)/sleepVal;

  //Menu button:
  if (menuRead != lastMenuBtn) {
    lastTimeMenu = millis();
  }
  if ((millis() - lastTimeMenu) > debounceTime) {
    if (menuRead!= menuBtn) {
      menuBtn = menuRead;
      if (menuBtn == HIGH) {menuState = !menuState;}
      }}
      lastMenuBtn = menuRead;


  //Right button:
  if (rightRead != lastRightBtn) {
    lastTimeRight = millis();
  }
  if ((millis() - lastTimeRight) > debounceTime) {
    if (rightRead!= rightBtn) {
      rightBtn = rightRead;
      if (rightBtn == HIGH) {rightState = !rightState;}
      }}
      lastRightBtn = rightRead;


  //Left button:
  if (leftRead != lastLeftBtn) {
    lastTimeLeft = millis();
  }
  if ((millis() - lastTimeLeft) > debounceTime) {
    if (leftRead!= leftBtn) {
      leftBtn = leftRead;
      if (leftBtn == HIGH) {leftState = !leftState;}
      }}
      lastLeftBtn = leftRead;

  //Sleep button:
  if (sleepRead != lastSleepBtn) {
    lastTimeSleep = millis();
  }
  if ((millis() - lastTimeSleep) > debounceTime) {
    if (sleepRead!= sleepBtn) {
      sleepBtn = sleepRead;
      if (sleepBtn == HIGH) {sleepState = !sleepState;}
      }}
      lastSleepBtn = sleepRead;

  //hello

  int ir_der = digitalRead(IR_sensor_pin_der);
  int ir_izq = digitalRead(IR_sensor_pin_izq);


  while (leftState == 1) {
    int ir_izq = digitalRead(IR_sensor_pin_izq);
    moveStepper(2800*positionLeft, speed_stepper, accel_stepper); 
    stepperLeft.run();

    if (ir_izq == false){
      stepperLeft.stop();
      stepperLeft.setCurrentPosition(2800*positionLeft);
      positionLeft = !positionLeft;
      digitalWrite(relayPin, LOW);
      digitalWrite(ledPin, LOW);
      digitalWrite(IR_sensor_transistor_pin, false);
      leftState = 0;
      }

    else if (stepperLeft.distanceToGo()==0){
      positionLeft = !positionLeft;
      digitalWrite(relayPin, LOW);
      digitalWrite(ledPin, LOW);
      digitalWrite(IR_sensor_transistor_pin, false);
      leftState = 0;
      }
    }
    

  while (rightState == 1) {
    int ir_der = digitalRead(IR_sensor_pin_der);
    moveStepper(2800*positionRight, speed_stepper, accel_stepper); 
    stepperRight.run();
    
    if (ir_der == false){
      stepperRight.stop();
      stepperRight.setCurrentPosition(2800*positionRight);
      positionRight = !positionRight;
      digitalWrite(relayPin, LOW);
      digitalWrite(ledPin, LOW);
      digitalWrite(IR_sensor_transistor_pin, false);
      rightState = 0;
    }
    
    else if (stepperRight.distanceToGo()==0){
      positionRight = !positionRight;
      digitalWrite(relayPin, LOW);
      digitalWrite(ledPin, LOW);
      digitalWrite(IR_sensor_transistor_pin, false);
      rightState = 0;
      }
    }
  
  while (menuState == 1) {
    int ir_der = digitalRead(IR_sensor_pin_der);
    int ir_izq = digitalRead(IR_sensor_pin_izq);
    moveStepper(0, speed_stepper, accel_stepper);  
    stepperRight.run();
    stepperLeft.run();

    if (ir_der == false){
      stepperRight.stop();
      stepperRight.setCurrentPosition(0);
    }

    if (ir_izq == false){
      stepperLeft.stop();
      stepperLeft.setCurrentPosition(0);
    }
    if (stepperRight.distanceToGo()==0 && stepperLeft.distanceToGo() == 0){
      digitalWrite(relayPin, LOW);
      digitalWrite(ledPin, LOW);
      digitalWrite(IR_sensor_transistor_pin, false);
      menuState = 0;
      }
    }

  
  
  if (hour == scheadule_hours[dayTransform(day) - 1] and minute == scheadule_minutes[dayTransform(day) - 1]){
    moveStepper(0, 500, 200);
    digitalWrite(19, LOW);
    }
  
  // Serial.print(ir_izq);
  // Serial.print(" / ");
  // Serial.println(ir_der);

 // if (hour == 00 or digitalRead(sleepBtn == 1)){sleepVar = not sleepVar;} // MUST DEBOUNCE!!
  
  

 // Serial.println(scheadule_hours[6]);


}
