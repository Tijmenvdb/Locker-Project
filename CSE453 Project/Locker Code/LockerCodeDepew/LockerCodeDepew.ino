
/*
   Tijmen Van Der Beek
   3/3/2021
   DepewLockerCode
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

//other
#define init_num 25 //number is check to see if eeprom is initilized
#define LOCKER_TIME 30 //Time locker has till it turns off in sec
#define UNLOCK_TIME 10 //Time the locker will be unlocked for in sec

//----------------------------Functions----------------------------------//
void ReadSerial(String &ReadTagString);
void clearTags();
void loadTags();
bool writeTag(String data);
bool checkTag(String data);
void setLed (int rVal, int gVal, int bVal);
void ledBlink (int rVal, int gVal, int bVal, int halfCycle, int blinkCount);
void soundFeedback(int type);
void setBatteryLed();

//----------------------------------Variables---------------------------//
//RFID
String RFIDTAG = ""; //Holds the RFID Code read from a tag
String tag_list[32];
int len_address = 0;
int tags_address = 1;
int num_tags = 0;

//Locker States
bool add_tag = false;
bool clear_tags = false;
bool locked = true;

//Other
int timerCounter = 0;
int debounceTime = 0;

//---------------------------------Setup--------------------------------//
void setup() {
  // RFID reader SOUT pin connected to Serial RX pin at 2400bps
  RFIDReader.begin(RFIDSerialRate);

  // Set Enable pin as OUTPUT to connect it to the RFID /ENABLE pin
  pinMode(RFIDEnablePin, OUTPUT);
  pinMode(UnlockPin, OUTPUT);
  pinMode(Speaker, OUTPUT);
  pinMode(TurnOff, OUTPUT);
  
  pinMode(BatteryVoltage, INPUT);
  pinMode(BatteryLEDR, OUTPUT);
  pinMode(BatteryLEDG, OUTPUT);
  
  pinMode(clearButton, INPUT_PULLUP);

  // Activate the RFID reader
  // Setting the RFIDEnablePin HIGH will deactivate the reader
  // which could be usefull if you wanted to save battery life for
  // example.
  digitalWrite(RFIDEnablePin, LOW);
  digitalWrite(TurnOff, LOW);

  // sets baudrate for serial moniter
  Serial.begin(baudrate);

  //sets Interrupts
  attachInterrupt(digitalPinToInterrupt(clearButton), buttonISR, FALLING);

  interrupts();

  setBatteryLed(); //setsBatteryLED
  
  enableTimerInterrupt(); //sets timer interrupt for auto shut off
  
  //loads all registered tags
  loadTags();
  Serial.println("Setup done!");

  soundFeedback(startSound);

  setLed(255, 0, 0); //LED Red
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
    bool isTagReg = checkTag(RFIDTAG);

    //adds tag if in add tag mode
    //else uunlocks if tag is registered
    if (add_tag == true && isTagReg == false) {
      writeTag(RFIDTAG);
      add_tag = false;

      delay(tagDelay - 400);
      ledBlink(0, 0, 255, 200, 2); //blinks blue for registering tag

      digitalWrite(RFIDEnablePin, LOW);

    } else if (add_tag == true && isTagReg == true) { //if tag is already registered and in add tag mode
      add_tag = false;

      delay(tagDelay - 600);
      ledBlink(0, 0, 255, 200, 3); //blinks blue for registering tag

      digitalWrite(RFIDEnablePin, LOW);

    } else if (isTagReg == true) { //unlocks with registered tag

      delay(tagDelay - 400);
      soundFeedback(unlockSound);

      //unlock
      digitalWrite(UnlockPin, HIGH);
      timerCounter = UNLOCK_TIME; //sets timer to unlock_time

      ledBlink(0, 255, 0, 200, 2); //blinks green for unlocking tag
      locked = false;

    } else { //error

      delay(tagDelay - 600);
      soundFeedback(errorSound);
      ledBlink(255, 0, 0, 200, 3); //blinks red for wrong tag

      digitalWrite(RFIDEnablePin, LOW);
    }

    setLed(locked * 255, !locked * 255, 0); //sets led to locker state
  }

  //if in clear tags mode
  if (clear_tags == true) {
    clearTags();
    clear_tags = false;
    ledBlink(255, 0, 50, 200, 2); //blinks pink for clearing tags

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
   loadTags
   This function loads all the tags into the global tag_list
*/
void loadTags() {
  num_tags = EEPROM.read(len_address);
  String tempStr = "";

  for (int i = 0; i < num_tags; i++) {
    int strIndex = tags_address + (i * 10);
    int strEnd = strIndex + 10;
    for (strIndex; strIndex < strEnd; strIndex++) {
      tempStr += (char) EEPROM.read(strIndex);
    }
    tag_list[i] = tempStr;
    tempStr = "";
  }
}

/*
   writeTag
   This function adds a tag to memory and returns ture if succsesful and false if reached max tags
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
   bool isMatch
*/
bool checkTag(String data) {
  for (int i = 0; i < num_tags; i++) {
    if (data == tag_list[i]) {
      return true;
    }
  }
  return false;
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
   buttonISR
   sets clear_tags to true and sets led to pink
*/
void buttonISR() {
  if(millis() - debounceTime > 100){
    delay(5);
    if(digitalRead(clearButton) == 0){
      if(add_tag == true){
        clear_tags = true;
        add_tag = false;
        setLed(255, 0, 50);
      }else{
        add_tag = true;
        setLed(0, 0, 255);
      }
    }
  }   debounceTime = millis();
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
