
/*
   Tijmen Van Der Beek
   3/3/2021
   AldenLockerCode
*/
#include <SoftwareSerial.h>
#include <EEPROM.h>

//---------------------------------RFID--------------------------------//
#define RFIDEnablePin 10     //Pin that enables reading. Set as OUTPUT and LOW to read an RFID tag
#define RxPin 11             //Pin to read data from Reader 
#define TxPin 9             //Pin to write data to the Reader NOTE: The reader doesn't get written to, don't connect this line.
#define RFIDSerialRate 2400 //Parallax RFID Reader Serial Port Speed
SoftwareSerial RFIDReader(RxPin, TxPin);

//--------------------------------LEDS---------------------------------//
#define LEDPinR 3
#define LEDPinG 5
#define LEDPinB 6

#define BatteryVoltage A5
#define BatteryLEDR 4
#define BatteryLEDG A3

//LED colors
//Pink: 255, 0, 50
//Orange: 200, 10, 0
//Yellow: 200, 100, 0
//White: 255, 100, 240
//Purple: 60, 0, 200

//-----------------------------OtherOutputs------------------------------//
#define UnlockPin 13
#define Speaker 7
#define TurnOff A4
#define clearButton 2

//--------------------------------Static_Variables-----------------------------//
#define baudrate 9600       //baudrate for the serial moniter
#define tagDelay 1000       //delay between each tag read in ms
#define ERRORBlinkRate 200

//SoundTypes
#define startSound 0
#define unlockSound 1
#define errorSound 2

//address
#define init_address 0
#define master_tag_address 1
#define reg_tag_address 11
#define clear_tag_address 21
#define len_address 31
#define tags_address 32

//tagMatches
#define noMatch -1
#define studentTag 0
#define regTag 1
#define clearTag 2
#define masterTag 3

//other
#define init_num 25 //number is check to see if eeprom is initilized
#define LOCKER_TIME 30 //Time locker has till it turns off in sec
#define UNLOCK_TIME 10 //Time the locker will be unlocked for in sec

//----------------------------Functions----------------------------------//
void ReadSerial(String &ReadTagString);
void clearTags();
void loadTags();
bool writeTag(String data);
int checkTag(String data);
void setLed (int rVal, int gVal, int bVal);
void ledBlink (int rVal, int gVal, int bVal, int halfCycle, int blinkCount);
void soundFeedback(int type);
void initLocker();
void setBatteryLed();
void enableTimerInterrupt();
void resetTimer();

//----------------------------------Variables---------------------------//
String RFIDTAG = ""; //Holds the RFID Code read from a tag

String master_tag = "";
String reg_tag = "";
String clear_tag = "";
String tag_list[32]; //list of student tags

int num_tags = 0;

bool reset = false;
bool locked = true;

int timerCounter = 0;

//---------------------------------Setup--------------------------------//
void setup() {
  // RFID reader SOUT pin connected to Serial RX pin at 2400bps
  RFIDReader.begin(RFIDSerialRate);

  // Set Enable pin as OUTPUT to connect it to the RFID /ENABLE pin
  pinMode(RFIDEnablePin, OUTPUT);
  pinMode(UnlockPin, OUTPUT);
  pinMode(Speaker, OUTPUT);
  pinMode(TurnOff, OUTPUT);
  pinMode(clearButton, INPUT_PULLUP);

  pinMode(BatteryVoltage, INPUT);
  pinMode(BatteryLEDR, OUTPUT);
  pinMode(BatteryLEDG, OUTPUT);

  // Activate the RFID reader
  // Setting the RFIDEnablePin HIGH will deactivate the reader
  // which could be usefull if you wanted to save battery life for
  // example.
  digitalWrite(RFIDEnablePin, LOW);
  digitalWrite(TurnOff, LOW);

  // sets baudrate for serial moniter
  Serial.begin(baudrate);

  //sets Interrupts
  attachInterrupt(digitalPinToInterrupt(clearButton), resetISR, FALLING);

  interrupts();

  setBatteryLed(); //setsBatteryLED
  
  enableTimerInterrupt(); //sets timer interrupt for auto shut off
  
  //loads all registered tags
  loadTags();
  Serial.println("Setup done!");

  soundFeedback(startSound);
  
  setLed(255, 0, 0);//LED Red
}


//---------------------------------Loop--------------------------------//
void loop() {

  //If tag is being read
  if (RFIDReader.available() > 0) // If data available from reader
  {
    setLed(255, 80, 0); //LED Yellow

    digitalWrite(RFIDEnablePin, HIGH);
    ReadSerial(RFIDTAG);  //Read the tag number from the reader. Should return a 10 digit serial number

    //checks if tag is in list
    int tagMatch = checkTag(RFIDTAG);

    //adds tag if in add tag mode
    //else unlocks if tag is registered
    if (tagMatch == regTag) {
      setLed(0, 0, 255); //sets Led to blue
      
      digitalWrite(RFIDEnablePin, LOW);
      while(RFIDReader.available() <= 0){}
      digitalWrite(RFIDEnablePin, HIGH);
      ReadSerial(RFIDTAG);

      tagMatch = checkTag(RFIDTAG);
      
      if(tagMatch == noMatch || tagMatch == studentTag){
        writeTag(RFIDTAG);
        delay(tagDelay - 400);
        ledBlink(0, 0, 255, ERRORBlinkRate, 3 + tagMatch); //blinks blue for registering tag. (blinks 3 times if already registered 2 if not registered yet)
      }else{
        delay(tagDelay - 400);
        ledBlink(255, 0, 0, ERRORBlinkRate, 2); //blinks red for already registered tag
        switch(tagMatch){
          case regTag:
            ledBlink(0, 0, 255, ERRORBlinkRate, 1);
            break;
          case clearTag:
            ledBlink(255, 0, 50, ERRORBlinkRate, 1);
            break;
          case masterTag:
            ledBlink(255, 100, 240, ERRORBlinkRate, 1);
            break;
        }
      }
      digitalWrite(RFIDEnablePin, LOW);

    } else if (tagMatch == clearTag) {
      clearTags();
      delay(tagDelay - 400);
      ledBlink(255, 0, 50, ERRORBlinkRate, 2); //blinks pink for clear
      
      digitalWrite(RFIDEnablePin, LOW);
      
    } else if (tagMatch == studentTag || tagMatch == masterTag) { //unlocks locker
      delay(tagDelay - 400);
      soundFeedback(unlockSound);

      //unlock
      digitalWrite(UnlockPin, HIGH);
      timerCounter = UNLOCK_TIME; //sets timer to unlock_time

      ledBlink(0, 255, 0, ERRORBlinkRate, 2); //blinks green for unlocking tag
      locked = false;

    } else{
      delay(tagDelay - 600);
      soundFeedback(errorSound);
      ledBlink(255, 0, 0, ERRORBlinkRate, 3); //blinks red for wrong tag

      digitalWrite(RFIDEnablePin, LOW);
    }

    setLed(locked * 255, !locked * 255, 0); //sets led to locker state
  }

  //if reset button was pushed
  if (reset == true) {
    ledBlink(60, 0, 200, ERRORBlinkRate, 2); //blinks purple for reset
    resetLocker();
    reset = false;

    setLed(locked * 255, !locked * 255, 0); //sets led to locker state
  }
}

//---------------------------------Functions--------------------------------//
/*
   ReadSerial
    This function reads the tag infront of the RFID sensor and returns the string in the through the passed in string
    Inputs:
    String RFIDTAG
*/
void ReadSerial(String &ReadTagString)
{
  resetTimer();//resets timer when ever readSerial is run
  
  int bytesread = 0;
  int  val = 0;
  char code[10];
  String TagCode = "";

  if (RFIDReader.available() > 0) {         // If data available from reader
    if ((val = RFIDReader.read()) == 10) {  // Check for header
      bytesread = 0;
      while (bytesread < 10) {              // Read 10 digit code
        if ( RFIDReader.available() > 0) {
          val = RFIDReader.read();
          if ((val == 10) || (val == 13)) { // If header or stop bytes before the 10 digit reading
            break;                         // Stop reading
          }
          code[bytesread] = val;           // Add the digit
          bytesread++;                     // Ready to read next digit
        }
      }
      if (bytesread == 10) {               // If 10 digit read is complete

        for (int x = 0; x < 10; x++)       //Copy the Chars to a String
        {
          TagCode += code[x];
        }
        ReadTagString = TagCode;          //Update the caller

        delay(500);                       //adds delay for double scan
        while (RFIDReader.available() > 0) //Burn off any characters still in the buffer
        {
          RFIDReader.read();
        }

      }
      bytesread = 0;
      TagCode = "";
    }
  }
}

/*
   clearTags
   This function clears all the tags in memory
*/
void clearTags() {
  EEPROM.write(len_address, 0);
  num_tags = 0;
}

/*
  resetLocker
  This function resets the locker
*/
void resetLocker(){
  EEPROM.write(init_address, 0);
  digitalWrite(TurnOff, HIGH);
}

/*
   loadTags
   This function loads all the tags. student tags go into the global tag_list. Init num is a number to check if the program has been initilized before
*/
void loadTags() {
  //checks init num
  if(EEPROM.read(init_address) != init_num){
    initLocker();
  }
  
  String tempStr = "";
  int strIndex;
  int strEnd;
  
  //loads master tag
  strIndex = master_tag_address;
  strEnd = strIndex + 10;
  for (strIndex; strIndex < strEnd; strIndex++) {
    tempStr += (char) EEPROM.read(strIndex);
  }
  master_tag = tempStr;
  tempStr = "";

  //loads register tag
  strIndex = reg_tag_address;
  strEnd = strIndex + 10;
  for (strIndex; strIndex < strEnd; strIndex++) {
    tempStr += (char) EEPROM.read(strIndex);
  }
  reg_tag = tempStr;
  tempStr = "";

  //loads clear tag
  strIndex = clear_tag_address;
  strEnd = strIndex + 10;
  for (strIndex; strIndex < strEnd; strIndex++) {
    tempStr += (char) EEPROM.read(strIndex);
  }
  clear_tag = tempStr;
  tempStr = "";

  //loads student tags
  num_tags = EEPROM.read(len_address);

  for (int i = 0; i < num_tags; i++) {
    strIndex = tags_address + (i * 10);
    strEnd = strIndex + 10;
    for (strIndex; strIndex < strEnd; strIndex++) {
      tempStr += (char) EEPROM.read(strIndex);
    }
    tag_list[i] = tempStr;
    tempStr = "";
  }
}

/*
  initLocker
  This function puts the locker in initilize mode mode if it hasn't been initilized before.
  In initilize mode the locker will blink white for when it wants a master tag to be registered, blue for the register tag and pink for the clear tag. Once 3 tags have been initlized it will exit and run as normal.
*/
void initLocker() {
  //register master tag
  setLed(255, 100, 240);
   while(master_tag == ""){
    if (RFIDReader.available() > 0) // If data available from reader
    {
      setLed(255, 80, 0); //LED Yellow
      digitalWrite(RFIDEnablePin, HIGH);
      ReadSerial(RFIDTAG);  //Read the tag number from the reader. Should return a 10 digit serial number
  
      //checks if tag is in list
      int isTagReg = checkTag(RFIDTAG);
      Serial.println(isTagReg);
  
      //adds tag if in add tag mode
      //else uunlocks if tag is registered
      if (isTagReg == -1) {
          master_tag = RFIDTAG;
          
          int start_address = master_tag_address;
          int end_address = master_tag_address + 10;
          for (int i = start_address; i < end_address; i++) {
            EEPROM.write(i, RFIDTAG[i - start_address]);
          }
          
          soundFeedback(unlockSound);
          ledBlink(0, 255, 0, ERRORBlinkRate, 2);
          digitalWrite(RFIDEnablePin, LOW);
      }else {
          soundFeedback(errorSound);
          ledBlink(255, 0, 0, ERRORBlinkRate, 2);
          setLed(255, 100, 240);
          digitalWrite(RFIDEnablePin, LOW);
      }
    }
  }

  //register register tag
  setLed(0, 0, 255);
  while(reg_tag == ""){
    if (RFIDReader.available() > 0) // If data available from reader
    {
      setLed(255, 80, 0); //LED Yellow
      digitalWrite(RFIDEnablePin, HIGH);
      ReadSerial(RFIDTAG);  //Read the tag number from the reader. Should return a 10 digit serial number
  
      //checks if tag is in list
      int isTagReg = checkTag(RFIDTAG);
      Serial.println(isTagReg);
  
      //adds tag if in add tag mode
      //else uunlocks if tag is registered
      if (isTagReg == -1) {
          reg_tag = RFIDTAG;
          
          int start_address = reg_tag_address;
          int end_address = reg_tag_address + 10;
          for (int i = start_address; i < end_address; i++) {
            EEPROM.write(i, RFIDTAG[i - start_address]);
          }
          
          soundFeedback(unlockSound);
          ledBlink(0, 255, 0, ERRORBlinkRate, 2);
          digitalWrite(RFIDEnablePin, LOW);
      }else {
          soundFeedback(errorSound);
          ledBlink(255, 0, 0, ERRORBlinkRate, 2);
          setLed(0, 0, 255);
          digitalWrite(RFIDEnablePin, LOW);
      }
    }
  }

  //register clear tag
  setLed(255, 0, 50);
  while(clear_tag == ""){
    if (RFIDReader.available() > 0) // If data available from reader
    {
      setLed(255, 80, 0); //LED Yellow
      digitalWrite(RFIDEnablePin, HIGH);
      ReadSerial(RFIDTAG);  //Read the tag number from the reader. Should return a 10 digit serial number
  
      //checks if tag is in list
      int isTagReg = checkTag(RFIDTAG);
      Serial.println(isTagReg);
  
      //adds tag if in add tag mode
      //else uunlocks if tag is registered
      if (isTagReg == -1) {
          clear_tag = RFIDTAG;
          
          int start_address = clear_tag_address;
          int end_address = clear_tag_address + 10;
          for (int i = start_address; i < end_address; i++) {
            EEPROM.write(i, RFIDTAG[i - start_address]);
          }
          
          soundFeedback(unlockSound);
          ledBlink(0, 255, 0, ERRORBlinkRate, 2);
          digitalWrite(RFIDEnablePin, LOW);
      }else {
          soundFeedback(errorSound);
          ledBlink(255, 0, 0, ERRORBlinkRate, 2);
          setLed(255, 0, 50);
          digitalWrite(RFIDEnablePin, LOW);
      }
    }
  }
  //initilizes number of student tags
  EEPROM.write(len_address, 0);

  //sets init_num in eepron
  EEPROM.write(init_address, init_num);
}

/*
   writeTag
   This function adds a tag to memory and returns true if succsesful and false if reached max tags
   Inputs:
   String tag
   Returns:
   bool sucsess
*/
bool writeTag(String data) {
  if (num_tags == 32) {
    return false;
  }
  int start_address = (num_tags * 10) + tags_address;
  int end_address = start_address + 10;
  for (int i = start_address; i < end_address; i++) {
    EEPROM.write(i, data[i - start_address]);
  }

  tag_list[num_tags] = data;
  num_tags++;
  EEPROM.write(len_address, num_tags);

  return true;
}

/*
   checkTag
   checks the given tag with the registered tags if any of them match it returns true.
   Inputs:
   String tag
   Returns:
   int -1 no match
   int 0 student tag
   int 1 reg_tag
   int 2 clear_Tag
   int 4 master tag
*/
int checkTag(String data) {
  if(data == master_tag){
    return masterTag;
  }else if(data == reg_tag){
    return regTag;
  }else if(data == clear_tag){
    return clearTag;
  }
  for (int i = 0; i < num_tags; i++) {
    if (data == tag_list[i]) {
      return studentTag;
    }
  }
  return noMatch;
}

/*
   setLed
   sets the rgb led color
   Input:
   int red value (0-255)
   int green value (0-255)
   int blue value (0-255)
*/
void setLed (int rVal, int gVal, int bVal) {
  analogWrite (LEDPinR, rVal);
  analogWrite (LEDPinG, gVal);
  analogWrite (LEDPinB, bVal);
}

/*
   ledBlink
   Blink the RGB LED given the provided RGB value, the time for each half cycle (time off, time on, in ms) and the number of blinks
   Input:
   int red value (0-255)
   int green value (0-255)
   int blue value (0-255)
   int halfCycle (ms)
   int blinkCount
*/
void ledBlink (int rVal, int gVal, int bVal, int halfCycle, int blinkCount) {
  for (int i = 0; i < blinkCount; i++) {
    setLed(0, 0, 0);
    delay(halfCycle);
    setLed(rVal, gVal, bVal);
    delay(halfCycle);
  }
}

/*
   soundFeedback
   adds sound effects to the locker for better feed back
   Input:
   int type (0 start, 1 unlock, 2 wrong tag)
*/
void soundFeedback(int type) {
  switch (type) {
    case 0:
      tone(Speaker, 100, 200);
      delay(200);
      tone(Speaker, 300, 200);
      delay(200);
      tone(Speaker, 400, 200);
      delay(200);
      break;
    case 1:
      tone(Speaker, 300, 200);
      delay(200);
      tone(Speaker, 400, 200);
      delay(200);
      break;
    case 2:
      tone(Speaker, 100, 200);
      delay(200);
      tone(Speaker, 50, 200);
      delay(200);
      break;
  }
}

/*
  setBatteryLed
  sets the battery led color based on the voltage of the battery
 */
void setBatteryLed(){
  int batteryVoltage = analogRead(BatteryVoltage);
  Serial.print("BatteryVoltage:"); 
  Serial.println(batteryVoltage);
  
  if(batteryVoltage >= 690){
    analogWrite(BatteryLEDR, 0);
    analogWrite(BatteryLEDG, 255);
    
  }else if(batteryVoltage >= 630){
    analogWrite(BatteryLEDR, 255);
    analogWrite(BatteryLEDG, 0);
    
  }else{
    analogWrite(BatteryLEDR, 255);
    analogWrite(BatteryLEDG, 0);
    ledBlink(255, 0, 0, ERRORBlinkRate, 3);
    digitalWrite(TurnOff, HIGH);
  }
}

/*
 * Sets the timer interrupt to interrupt every second
 */
 void enableTimerInterrupt(){
    cli();//disables interrupts
    timerCounter = LOCKER_TIME;
    
    
    //setting Timer1 register to 0
    TCCR1A = 0;
    TCCR1B = 0;
    //initilizing the counter to 0
    TCNT1  = 0;
    //setting cmp match number
    OCR1A = 15624; // = (16*10^6) / (1*1024) - 1 (must be <65536)
    //turn on CTC mode
    TCCR1B |= (1 << WGM12);
    //setting prescaler
    TCCR1B |= (1 << CS12) | (1 << CS10);
    //enable timer compare interrupt
    TIMSK1 |= (1 << OCIE1A);
    
    sei();//enable interrupts
 }

/*
 * resets the timer back to Locker_Time when ever called
 */
void resetTimer(){
    cli();//disables interrupts
    TCNT1 = 0; //reset counter to 0
    timerCounter = LOCKER_TIME;
    sei();//enable interrupts
}

//---------------------------------Interrupts--------------------------------//
/*
   resetISR
   resets entire locker
*/
void resetISR() {
  delay(5);
  if(digitalRead(clearButton) == 0){
    reset = true;
    setLed(70, 0, 200);
  }
}

/*
 * Turns off locker after timer interrupt activates
 */
 ISR(TIMER1_COMPA_vect){
  if(timerCounter == 0){
    digitalWrite(TurnOff, HIGH);
  }
  timerCounter--;
  Serial.print("Timer: ");
  Serial.println(timerCounter);
 }
 
