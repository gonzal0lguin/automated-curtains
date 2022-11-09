#include <Servo.h>
#include <AccelStepper.h>
#include <Wire.h>

#define SLAVE_ADDR 9
#define master_interrupt_pin 13

#define step_pin_rg 6
#define dir_pin_rg 7
#define en_pin_rg 5

#define step_pin_lf 8
#define dir_pin_lf 9
#define en_pin_lf 10

#define servo_pin_rg 11
#define servo_pin_lf 12


int sv_read;
int sv_var = 0;

unsigned long previous_millis = 0;
unsigned long current_millis;

int minAngle = 25;
int midAngle = 105;
int maxAngle = 180;

Servo servoRg;
Servo servoLf;

AccelStepper nemaRg(AccelStepper::DRIVER, step_pin_rg, dir_pin_rg);
AccelStepper nemaLf(AccelStepper::DRIVER, step_pin_lf, dir_pin_lf);

/*  DIGIT ACTION
  rd = 1 -> OPEN both sides
  rd = 2 -> CLOSE both sides
  rd = 3 -> OPEN RG
  rd = 4 -> CLOSE RG
  rd = 5 -> OPEN LF
  rd = 6 -> CLOSE LF

  default is rd = 0, ie.- dont do anything 
*/
void recieveEvent(){
  sv_read = Wire.read();
  //add variable manipulation
}

void open(Servo servo, AccelStepper stepper, int servo_pin){
  stepper.setMaxSpeed(1500);
  stepper.setAcceleration(1000);


  stepper.moveTo(1000);
  while (stepper.distanceToGo()>0){
    stepper.run();
    }

  stepper.moveTo(0);
  while (abs(stepper.distanceToGo())>0){
    stepper.run();
    }
  servo.attach(servo_pin);
  servo.write(180);

  
  stepper.moveTo(4200);
  while (stepper.distanceToGo()>0){
    stepper.run();
    }


  stepper.moveTo(0);
  while (abs(stepper.distanceToGo())>0){
    stepper.run();

  servo.write(90);
  }}

void close(Servo servo, AccelStepper stepper, int servo_pin){
  stepper.setMaxSpeed(1500);
  stepper.setAcceleration(1000);
  
  stepper.moveTo(4750);
  while (stepper.distanceToGo() > 0){
    stepper.run();
    }
  servo.attach(servo_pin);
  servo.write(0);

  

  stepper.moveTo(0);
  while (abs(stepper.distanceToGo())>0){
    stepper.run();
    }

  servo.write(105);
  }

void setup() {
  Serial.begin(9600);
  pinMode(step_pin_rg, OUTPUT);
  pinMode(step_pin_lf, OUTPUT);
  pinMode(dir_pin_rg, OUTPUT);
  pinMode(dir_pin_lf, OUTPUT);
  pinMode(servo_pin_rg, OUTPUT);
  pinMode(servo_pin_lf, OUTPUT);
  pinMode(master_interrupt_pin, OUTPUT);

  Wire.begin();
  Wire.onReceive(recieveEvent);
}

void loop() {
  if (rd == 1){
    open(servolf, AccelStepper stepper, int servo_pin)          
  }

}
