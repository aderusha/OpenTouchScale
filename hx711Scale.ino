////////////////////////////////////////////////////////////////////////////////////////////////////
// hx711Scale.ino

// This allows us to interface with the Pro Micro's USB HID controller to send USB keyboard
// presses to the host PC
#include <Keyboard.h>

// EEPROM access to save settings to device
#include <EEPROM.h>

// This is the "Queuetue HX711 Library" by Scott Russel for interfacing with the HX711 amplifier
// and which handles the data collection and smoothing.
#include <Q2Balance.h>
#include <Q2HX711.h>

// Define the data and clock pins for your setup
const byte hx711_data_pin = 2;
const byte hx711_clock_pin = 3;
Q2HX711 hx711(hx711_data_pin, hx711_clock_pin);
Q2Balance balance = Q2Balance();

// Create a struct for our saved settings
BalanceCalibrationStruct balanceCalibration;

////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup function
void setup() {
  Serial.begin(19200);  // opens serial port, sets data rate to 19200 bps
  Serial1.begin(9600);  // Open pro micro hardware UART to talk to Nextion display

  balance.SAMPLE_COUNT = 20;
  // Read saved configuration values from EEPROM
  EEPROM.get(0, balanceCalibration);
  balance.setCalibration(balanceCalibration);

  // Set zero point at power on
  balance.tare(1000, tareCallback);
  balance.measure(hx711.read());
  balance.tick();
  balance.calibrateZero(1000, calibrateZeroCallback);
  delay(1000);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Main execution loop
void loop() {
  // Check for an incoming command over serial.
  if (Serial.available() > 0) {
    processSerialInput();
  }
  if (Serial1.available() > 0) {
    processNextionInput();
  }

  // Update the current scale reading
  balance.measure(hx711.read());
  balance.tick();

  Serial1.print("n0.val=");
  Serial1.print(int(balance.adjustedValue(0) + 0.5));
  byte nextionSuffix[] = {0xFF, 0xFF, 0xFF };
  Serial1.write(nextionSuffix, sizeof(nextionSuffix));

  //  Serial.print("adjustedValue: ");
  //  Serial.println(balance.adjustedValue(0));
  //  Serial.print("tared: ");
  //  Serial.println(balance.tared());
}

void calibrateCallback() {
  //  Serial.println("Calibration complete:");
  EEPROM.put(0, balance.getCalibration());
  balance.printCalibrations();
}
void calibrateZeroCallback() {
  //  Serial.println("Zero set complete:");
}
void tareCallback() {
  //  Serial.println("Tare set complete:");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle incoming commands over serial
void processSerialInput() {
  // read the incoming byte:
  int incomingByte = Serial.read();

  // Kern scales expect a single character ("w", "s", or "t") with no CRLF.
  // Acaia scales expect a command starting with BT<something> followed by CRLF.
  // We will grab the first byte (because that's all we'll get for a Kern command), see if it's
  // "w", "s", "t", or "B", then react accordingly.
  // For debug purposes I've implemented "z" to zero and "c" to calibrate
  //
  //////////////////////////////////////////
  // Handle Kern scale commands
  //
  // Artisan has implemented a rough approximation of the Kern scale RS232 format.
  // Check page 56 here: https://www.vagar.com/pub_docs/files/Kern_manual_b%C3%A4nkv%C3%A5g_NDE.pdf
  // Artisan sends out a "w" command to request a reading and will accept a response as follows:
  // <int>g<anything>CRLF
  // example: 100g0
  // That's 100grams, the trailing zero can be any string, is required, and is ignored.
  // No provision is made for reading past the decimal, only integer responses in grams
  // will be accepted.
  //
  // Check for incoming ASCII "w" (dec 119)
  // Kern weigh command (stable or unstable reading)
  if (incomingByte == 'w') {
    Serial.print(int(balance.adjustedValue(0) + 0.5));
    Serial.println("g0");
  }

  // Check for incoming ASCII "t" (dec 116)
  // Kern tare command, tare the scale and don't return a response
  // Not implemented by Artisan as near as I know, included here for completeness sake
  if (incomingByte == 't') {
    //    Serial.println("Setting tare");
    balance.tare(1000, tareCallback);
  }

  // Check for incoming ASCII "s" (dec 115)
  // Kern stable weigh command (stable reading)
  // Not implemented by Artisan as near as I know, included here for completeness sake
  if (incomingByte == 's') {
    Serial.print(int(balance.adjustedValue(0) + 0.5));
    Serial.println("g0");
  }

  //
  //////////////////////////////////////////
  // Handle Acaia scale commands
  //
  // Someone developing Artisan must have gotten access to the Acaia SDK
  // which means they're better than me as my own requests for access went
  // ignored.  Given that, I have no idea what the actual Acaia spec
  // looks like and it's not published anywhere I can find.  This is
  // entirely put together based on reading the Artisan source code.
  // I'd be shocked if this worked for anything other than Artisan.
  //
  // Check for incoming ASCII "B" (dec 66)
  // Artisan will send "BTST\r\n" to check if it's connected to an Acaia scale
  if (incomingByte == 66) {
    // Read the rest of the line until the final LF
    String incomingCommand = Serial.readStringUntil('\n');
    if (incomingCommand.substring(0, 3) == "TST") {
      // Artisan will accept any value here EXCEPT "status=DISCONNECTED"
      // I have no idea what the "correct" value should be, this is just a guess
      Serial.println("status=CONNECTED");
    }
  }

  // Check for incoming ASCII "G" (dec 71)
  // Artisan will send "GWT1,1,1\r\n" to read the value from an Acaia scale
  if (incomingByte == 'G') {
    String incomingCommand = Serial.readStringUntil('\n');
    if (incomingCommand.substring(0, 7) == "WT1,1,1") {
      // Artisan ignores the first line
      Serial.println("");
      Serial.print(balance.adjustedValue(0));
      Serial.println(" g");
    }
  }

  // Check for incoming ASCII "c" (dec 99)
  // Not a Kern/Acaia command!
  // Place weight of mass scaleCalWeight then run this command to calibrate
  // Not implemented by Artisan
  //  if (incomingByte == 'c') {
  //    Serial.println("Calibrating 10g");
  //    balance.calibrate(0, 10, 1000, calibrateCallback);
  //  }
  //
  //  if (incomingByte == 'v') {
  //    Serial.println("Calibrating 50g");
  //    balance.calibrate(1, 50, 1000, calibrateCallback);
  //  }
  //
  //  if (incomingByte == 'b') {
  //    Serial.println("Calibrating 100g");
  //    balance.calibrate(2, 100, 1000, calibrateCallback);
  //  }
  //
  //  if (incomingByte == 'n') {
  //    Serial.println("Calibrating 500g");
  //    balance.calibrate(3, 500, 1000, calibrateCallback);
  //  }
  //
  //  if (incomingByte == 'm') {
  //    Serial.println("Calibrating 1000g");
  //    balance.calibrate(4, 1000, 1000, calibrateCallback);
  //  }

  // Check for incoming ASCII "z" (dec 122)
  // Not a Kern/Acaia command!
  // Remove weight from scale and run this command to zero
  // Not implemented by Artisan
  //  if (incomingByte == 'z') {
  //    Serial.println("Setting zero");
  //    balance.calibrateZero(1000, calibrateZeroCallback);
  //  }

  // Check for incoming ASCII "d" (dec 100)
  // Not a Kern/Acaia command!
  // Dump debug data over serial
  // Not implemented by Artisan
  //  if (incomingByte == 'd') {
  //    Serial.println(":::::[DEBUG]:::::");
  //    Serial.print("balance.adjustedValue(0): ");
  //    Serial.println(balance.adjustedValue(0));
  //    Serial.print("balance.adjustedRawValue(0): ");
  //    Serial.println(balance.adjustedRawValue(0));
  //    Serial.print("balance.calibrating(): ");
  //    Serial.println(balance.calibrating());
  //    Serial.print("balance.jitter(): ");
  //    Serial.println(balance.jitter());
  //    Serial.print("balance.rawValue(): ");
  //    Serial.println(balance.rawValue());
  //    Serial.print("balance.settling(): ");
  //    Serial.println(balance.settling());
  //    Serial.print("balance.smoothValue(): ");
  //    Serial.println(balance.smoothValue());
  //    Serial.print("balance.taring(): ");
  //    Serial.println(balance.taring());
  //    Serial.print("balance.tared(): ");
  //    Serial.println(balance.tared());
  //    Serial.print("balance.printCalibrations(): ");
  //    balance.printCalibrations();
  //  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle incoming commands from the Nextion device
// Command reference: https://www.itead.cc/wiki/Nextion_Instruction_Set#Format_of_Device_Return_Data
// tl;dr, command byte, command data, 0xFF 0xFF 0xFF
// Create a buffer for our command, waiting for those 0xFFs, then process the command once we get
// three in a row
void processNextionInput() {
  byte nextionBuffer[20];  // bin to hold our incoming command
  int nextionCommandByteCnt = 0;  // Counter for incoming command buffer
  bool nextionCommandIncoming = true; // we'll flip this to false when we receive 3 consecutive 0xFFs
  int nextionCommandTimeout = 100; // timeout for receiving termination string in milliseconds
  int nextionTermByteCnt = 0;
  unsigned long nextionCommandTimer = millis(); // record current time for our timeout

  // Load the nextionBuffer until we receive a termination command or we hit our timeout
  while (nextionCommandIncoming && ((millis() - nextionCommandTimer) < nextionCommandTimeout)) {
    byte nextionCommandByte = Serial1.read();

    // check to see if we have one of 3 consecutive 0xFF which indicates the end of a command
    if (nextionCommandByte == 0xFF) {
      nextionTermByteCnt++;
      if (nextionTermByteCnt >= 3) {
        nextionCommandIncoming = false;
      }
    }
    else {
      nextionTermByteCnt = 0;  // reset counter if a non-term byte was encountered
    }
    nextionBuffer[nextionCommandByteCnt] = nextionCommandByte;
    nextionCommandByteCnt++;
  }

  // Handle incoming touch command
  // 0X65+Page ID+Component ID+TouchEvent+End
  // Return this data when the touch event created by the user is pressed.
  // Definition of TouchEvent: Press Event 0x01, Release Event 0X00
  // Example: 0X65 0X00 0X02 0X01 0XFF 0XFF 0XFF
  // Meaning: Touch Event, Page 0, Button 2, Press
  if (nextionBuffer[0] == 0x65) {
    byte nextionPage = nextionBuffer[1];
    byte nextionButtonID = nextionBuffer[2];
    byte nextionButtonAction = nextionBuffer[3];

    // Handle Tare button
    if (nextionPage == 0x00 && nextionButtonID == 0x04 && nextionButtonAction == 0x01) {
//      Serial.println("Setting tare");
      balance.tare(1000, tareCallback);
    }
    // Handle Calibrate button
    if (nextionPage == 0x00 && nextionButtonID == 0x05 && nextionButtonAction == 0x01) {
//      Serial.println("Calibrating 100g");
      balance.calibrate(2, 100, 1000, calibrateCallback);
    }
    // Handle Green button, sends "i" key on keyboard to trigger roast weight in in Artisan
    if (nextionPage == 0x00 && nextionButtonID == 0x02 && nextionButtonAction == 0x01) {
      //      Serial.println("Sending keystroke \"i\"");
      Keyboard.write('i');
    }
    // Handle Roasted button, sends "o" key on keyboard to trigger roast weight out in Artisan
    if (nextionPage == 0x00 && nextionButtonID == 0x03 && nextionButtonAction == 0x01) {
      //      Serial.println("Sending keystroke \"o\"");
      Keyboard.write('o');
    }
  }
}
