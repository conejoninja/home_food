#define ENCODERAPIN   12
#define ENCODERBPIN   13
#define MOTORAPIN     2
#define MOTORBPIN     3
#define BUTTONPIN 14

volatile long pos = 0;
volatile boolean CW = true;
boolean pressed = false;

void setup() { 
  Serial.begin (115200);
  Serial.println("Start calibration");
  Serial.println("Click on the button until the desired amount of food is dispensed");
  Serial.println("Use the POS value as MAXLONGROTATION in home_food");
  Serial.println("Repeat the process a few times and use an average value");
  
  pinMode(ENCODERAPIN, INPUT);
  pinMode(ENCODERBPIN, INPUT);
  attachInterrupt(ENCODERAPIN, doEncoder, RISING);  
  
  pinMode(MOTORAPIN, OUTPUT); 
  pinMode(MOTORBPIN, OUTPUT); 
  pinMode(BUTTONPIN, INPUT); 
} 

void loop() { 
  boolean p = digitalRead(BUTTONPIN);
  if(p && !pressed) {
    pressed = true;
    doMotor();
  }

  if(!p && pressed) {
    pressed = false;
  }

  Serial.print("POS:  ");
  Serial.print(pos);
  Serial.print("direction 0=CCW   1=CW:  ");  
  Serial.println(CW);

  delay(500);                      
} 


void doEncoder() {
  boolean a = digitalRead(ENCODERAPIN); 
  boolean b = digitalRead(ENCODERBPIN); 
  if(a) {
    CW = (a != b);
    if (CW)  {
      pos++;
    } else { 
      pos--;
    }
  }
}

void doMotor() {
  Serial.println("[DO MOTOR]");

  int n = 0;
  for(n=0;n<10;n++) {
    digitalWrite(MOTORAPIN, LOW);     
    digitalWrite(MOTORBPIN, HIGH);    
    delay (40);
    digitalWrite(MOTORAPIN, LOW);     
    digitalWrite(MOTORBPIN, LOW);    
    delay (8);
  }

  digitalWrite(MOTORAPIN, HIGH);
  digitalWrite(MOTORBPIN, LOW);    
  delay (140);
  for(n=0;n<6;n++) {
    digitalWrite(MOTORAPIN, LOW);     
    digitalWrite(MOTORBPIN, HIGH);    
    delay (40);
    digitalWrite(MOTORAPIN, LOW);     
    digitalWrite(MOTORBPIN, LOW);    
    delay (8);
  }

  digitalWrite(MOTORAPIN, HIGH);
  digitalWrite(MOTORBPIN, LOW);    
  delay (90);
  digitalWrite(MOTORAPIN, LOW);
  digitalWrite(MOTORBPIN, LOW);    
}




