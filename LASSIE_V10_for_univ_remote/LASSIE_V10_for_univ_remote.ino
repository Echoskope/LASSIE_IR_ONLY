/*
IMPORTANT: The ADAFRUIT PWM library has to be modified such that when the "off" value is 0 it actually writes 4096 to completely turn off the lights.

Key Commands:
0 = Enter
1 = Scene 1
2 = Scene 2
3 = Scene 3
4 = Scene 4
5 = Scene 5
6 = Scene 6
7 = RED Select
8 = GREEN Select
9 = BLUE Select
Volume Up = Incease color intensity (after pressing 7, 8, or 9)
Volume Down = Decrease color intensity (after pressing 7, 8, or 9)
Channel Up = Increase scene intensity
Channel Down = Decrease scene intensity
Power button = fade in from complete off to last scene or fade out from current scene to complete off

Operation:
Press 1 - 6 to activate the scene
  Current scene will stay in place while new scene fades in
To edit scene, press 7 - 9
  Use the Volume up/down to adjust the colors
To save scene, press 0
  Lights will blink once to signify a good save
  Press any key other than 7 - 9 or 0 to exit this mode
To turn the lights on to the last used scene
  Press the power button
To turn the lights off
  Press the power button
  
  
Scene fade mapping:
Fade Time / Current Color Value = Delay time per step
Delay time per step is an int so the decimal is truncated
Loop runs and delays 1 ms at the end
Colors wait for x,y,z ms to pass before decreasing/increasing their step
Colors set flag when they have all reached their projected value causing while loop to exit
(color flags used to account for the error in truncating the delay time per color)

Channel Fading:
  Channel UP:
    The code looks for which color has the greatest intensity 
    Then the code calculates the ratio of the greatest intensity color to the other colors
    Then it starts to increment the greatest intensity color and recalculate the other colors such that the color ratios don't change
    Once the greatest intensity color value reaches 4095 or goes above 4095 the code sets the colors to their last state and locks out the function
    
  Channel DOWN:
    The code looks for which color has the lowest intensity (with the exception of the colors that are already off)
    Then the code calculates the ratio of the lowest intensity color to the other colors
    Then it starts to decrement the lowest intensity color and recalculate the other colors such that the color ratios don't change
    Once the lowest intensity color value gets below 1 the code sets the colors to their last state and locks out the function
    

*/

#include <Wire.h>
#include <IRremote.h>
#include <EEPROM.h>
#include <Adafruit_PWMServoDriver.h>


//SparkFun Remote Codes

#define ONE 0x10EFF807      //Mapped to A
#define TWO 0x10EF7887      //Mapped to B
#define THREE 0x10EF58A7    //Mapped to C
#define FOUR 0x20DF28D7     //Not Used
#define FIVE 0x20DFA857     //Not Used
#define SIX 0x20DF6897      //Not Used
#define SEVEN 0x20DFE817    //Not Used
#define EIGHT 0x20DF18E7    //Not Used
#define NINE 0x20DF9867     //Not Used
#define ZERO 0x10EF20DF     //Mapped to CENTER
#define volUP 0x10EF807F    //Mapped to RIGHT
#define volDOWN 0x10EF10EF  //Mapped to LEFT
#define POWER 0x10EFD827    //Mapped to POWER
#define chanUP 0x10EFA05F   //Mapped to UP
#define chanDOWN 0x10EF00FF //Mapped to DOWN


#define REPEAT1 0xFFFF
#define RED 0
#define GREEN 1
#define BLUE 2

void fadeCALL(byte mode);
void fadeFUN(byte funCALL);

int RECV_PIN = 3, fade = 0, lastCOM = 0;
int irRECVcode = 0;
int lastSCENEred = 4095, lastSCENEgreen = 4095, lastSCENEblue = 4095;
int currentRED = 1000, currentBLUE = 1000, currentGREEN = 1000;
int sceneRED = 4095, sceneBLUE = 4095, sceneGREEN = 4095;
int fadeTIME = 7;
int lastCOMMAND = 0;
byte toggle = 0, exitFLAG = 0, scene = 0, currentCOLOR = 1;
byte lockOUTup = 0, lockOUTdown = 0;

IRrecv irrecv(RECV_PIN);
decode_results results;
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

void setup()
{
        Serial.begin(9600);
        pwm.begin();
        pwm.setPWMFreq(1600); //PWM driver refresh rate (1600 is the max)
        
        uint8_t twbrbackup = TWBR; //save current settings for I2C i/o speed
        TWBR = 12; //put I2C into fast+ mode
        
        pwm.setPWM(RED, 0, currentRED); //Turn on the lights after a POR
        pwm.setPWM(GREEN, 0, currentGREEN); //Turn on the lights after a POR
        pwm.setPWM(BLUE, 0, currentBLUE); //Turn on the lights after a POR
        
        
        /*****************************************************************/
        /***** The code checks EEPROM address 7 to see if this       *****/
        /***** arduino has been used before with the LASSIE          *****/
        /***** shield. If not it seeds scenes 1, 2, and 3 with       *****/
        /***** some values so you can use SparkFun's remote to       *****/
        /***** play with the string out of the box.                  *****/
        /*****************************************************************/
        
        byte checkFlag = EEPROM.read(7);
        if(checkFlag != 7)
        {
        EEPROM.write(10 , 0);
        EEPROM.write(11 , 255);
        EEPROM.write(12 , 0);
        EEPROM.write(13 , 0);
        EEPROM.write(14 , 0);
        EEPROM.write(15 , 0);
  
        EEPROM.write(20 , 0);
        EEPROM.write(21 , 0);
        EEPROM.write(22 , 0);
        EEPROM.write(23 , 255);
        EEPROM.write(24 , 0);
        EEPROM.write(25 , 0);

        EEPROM.write(30 , 0);
        EEPROM.write(31 , 0);
        EEPROM.write(32 , 0);
        EEPROM.write(33 , 0);
        EEPROM.write(34 , 0);
        EEPROM.write(35 , 255);

        EEPROM.write(7, 7);
        }
        
        
        irrecv.enableIRIn(); // Start the receiver
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void loop() 
{
        if (irrecv.decode(&results)) //if IR signal is detected
        {
                Serial.println(results.value, HEX); //print the command received to the serial port
                
                //results.value = (results.value & 0xFFFF);
   
                //Figure out which command was received and act on it
                if(results.value == REPEAT1)
                {
                        fadeFUN(15);
                        Serial.println("REPEAT"); 
                }
                if (results.value == POWER) 
                {
                        fadeFUN(10);
                        Serial.println("POWER");
                }
                if (results.value == volUP) 
                {
                        fadeFUN(11);
                        Serial.println("VOLUP");
                }
                if (results.value == volDOWN) 
                {
                        fadeFUN(12);
                        Serial.println("VOLDOWN");
                }
                if (results.value == chanUP)
                {
                        fadeFUN(13);
                        Serial.println("chanUP"); 
                }
                if (results.value == chanDOWN)
                {
                        fadeFUN(14);
                        Serial.println("chanDOWN");
                }
                if (results.value == ONE) 
                {
                        scene = 1;
                        fadeFUN(1);
                        Serial.println("ONE");
                }
                if (results.value == TWO) 
                {
                        scene = 2;
                        fadeFUN(2);
                        Serial.println("TWO");
                }
                if (results.value == THREE) 
                {
                        scene = 3;
                        fadeFUN(3);
                        Serial.println("THREE");
                }
                if (results.value == FOUR) 
                {
                        scene = 4;
                        fadeFUN(4);
                        Serial.println("FOUR");
                }
                if (results.value == FIVE) 
                {
                        scene = 5;
                        fadeFUN(5);
                        Serial.println("FIVE");
                }
                if (results.value == SIX) 
                {
                        scene = 6;
                        fadeFUN(6);
                        Serial.println("SIX");
                }
                if (results.value == SEVEN) 
                {
                        currentCOLOR = 1;
                        fadeFUN(7);
                        Serial.println("SEVEN");
                }
                if (results.value == EIGHT) 
                {
                        currentCOLOR = 2;
                        fadeFUN(8);
                        Serial.println("EIGHT");
                }
                if (results.value == NINE) 
                {
                        currentCOLOR = 3;
                        fadeFUN(9);
                        Serial.println("NINE");
                }
                if (results.value == ZERO) 
                {
                        fadeFUN(0);
                        Serial.println("ZERO");
                }
    
                irrecv.resume(); //resume the interrupt vector to accept more commands
        }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void fadeFUN(byte funCALL)
{
  
        int eepromADDR, redSTEP = 0, greenSTEP = 0, blueSTEP = 0;
        int previousRED, previousGREEN, previousBLUE;
        int bugFixRED, bugFixGREEN, bugFixBLUE;
        float redRATIO, greenRATIO, blueRATIO;
        byte maxCOLOR, minCOLOR;
        byte highBYTE = 0, lowBYTE = 0;
 
        if(funCALL != 15) //If the repeat command was NOT received
        {
                lastCOMMAND = funCALL; //save the received command 
        }
        else
        {
                funCALL = lastCOMMAND; //repeat was received; repeat the last command
        }
 
        if(funCALL == 0)
        {
                /**********************************************************************************
                Red Scene High Byte = Scene * 10 (i.e. scene 4 red high eeprom addr = 40)
                Red Scene Low Byte = Scene * 10 + 1 (i.e. scene 4 red low eeprom addr = 41)
                Green Scene High Byte = Scene * 10 + 2 (i.e. scene 4 green high eeprom addr = 42)
                Green Scene Low Byte = Scene *10 + 3 (i.e. scene 4 green low eeprom addr = 43)
                Blue Scene High Byte = Scene * 10 + 4 (i.e. scene 4 blue high eeprom addr = 44)
                Blue Scene Low Byte = Scene * 10 + 5 (i.e. scene 4 blue low eeprom addr = 45)
                ***********************************************************************************/
          
                eepromADDR = scene * 10;
                
                highBYTE = (currentRED >> 8) & 0x00FF; //mask upper byte and move into variable
                lowBYTE = (currentRED & 0x00FF); //mask lower byte and move into variable
                EEPROM.write(eepromADDR, highBYTE); //store the variable at the correct address
                eepromADDR++; //move the address pointer up by 1
                EEPROM.write(eepromADDR, lowBYTE);
                eepromADDR++;
                
                highBYTE = (currentGREEN >> 8) & 0x00FF;
                lowBYTE = (currentGREEN & 0x00FF);
                EEPROM.write(eepromADDR, highBYTE);
                eepromADDR++;
                EEPROM.write(eepromADDR, lowBYTE);
                eepromADDR++;
                
                highBYTE = (currentBLUE >> 8) & 0x00FF;
                lowBYTE = (currentBLUE & 0x00FF);
                EEPROM.write(eepromADDR, highBYTE);
                eepromADDR++;
                EEPROM.write(eepromADDR, lowBYTE);
                
                pwm.setPWM(RED, 0, 0); //turn off all colors to confirm the save
                pwm.setPWM(GREEN, 0, 0);
                pwm.setPWM(BLUE, 0, 0);
                
                delay(350);
                
                pwm.setPWM(RED, 0, currentRED); //turn the colors back on to the scene
                pwm.setPWM(GREEN, 0, currentGREEN);
                pwm.setPWM(BLUE, 0, currentBLUE);
                
                return;
        }
 
        if(funCALL == 11) //Volume UP
        {
  
                if(currentCOLOR == 1) // 7 or RED was the last color button press
                {
                        currentRED += 25; //step size of 5 so it doesn't take forever to go from 0 to 4095
                }
                if(currentCOLOR == 2) // 8 or GREEN was the last color button press
                {
                        currentGREEN += 25;
                }
                if(currentCOLOR == 3) // 9 or BLUE was the last color button press
                {
                        currentBLUE += 25;
                } 
        }
  
        if(funCALL == 12) //Volume Down
        {
                if(currentCOLOR == 1)
                {
                        currentRED -= 25;
                }
                if(currentCOLOR == 2)
                {
                        currentGREEN -= 25;
                }
                if(currentCOLOR == 3)
                {
                        currentBLUE -= 25;
                } 
        }
  
  
        //Check to see if there are any channels that are below 0 and if so set to 0
  
        if(currentRED < 0)
        {
                currentRED = 0;
        }
        if(currentRED > 4095)
        {
                currentRED = 4095; 
        }
        if(currentGREEN < 0)
        {
                currentGREEN = 0;
        }
        if(currentGREEN > 4095)
        {
                currentGREEN = 4095; 
        }
        if(currentBLUE < 0)
        {
                currentBLUE = 0;
        }
        if(currentBLUE > 4095)
        {
                currentBLUE = 4095; 
        }
  
        if((funCALL == 13) && (lockOUTup != 1)) //Channel UP
        {
                lockOUTdown = 0; //used with lockOUTup to prevent system trying to set fade below 0 or above 4095
                
                previousRED = currentRED;
                previousGREEN = currentGREEN;
                previousBLUE = currentBLUE;
                
                if(currentRED > currentGREEN) //figure out if RED or GREEN has the higher value
                {
                        maxCOLOR = 1; //if RED is greater
                }
                else
                {
                        maxCOLOR = 2;  //if green is greater
                }
                
                if(maxCOLOR == 1) //if RED was greater
                {
                        if(currentRED < currentBLUE) //see if blue is greater
                        {
                                maxCOLOR = 3; //if BLUE was greater RED
                        } 
                }
   
                if(maxCOLOR == 2) //if GREEN was greater
                {
                        if(currentGREEN < currentBLUE)
                        {
                                maxCOLOR = 3; //if BLUE was greater than GREEN
                        } 
                }

                if(maxCOLOR == 1) //if RED was the greatest value
                {
                        redRATIO = 1; //dummy value
                        greenRATIO = float(currentRED) / float(currentGREEN); //find the ratio between red and green
                        blueRATIO = float(currentRED) / float(currentBLUE); //find the ratio between red and blue
                        currentRED += 25; //increase RED
                        currentGREEN = (currentRED / greenRATIO) + 0.5; //recalculate green via the ratio and new red value
                        currentBLUE = (currentRED / blueRATIO) + 0.5; //recalculate blue via the ratio and new red value
                        Serial.print("maxCOLOR = RED    ");
                        Serial.print("greenRATIO = ");
                        Serial.print(greenRATIO, DEC);
                        Serial.print("     blueRATIO = ");
                        Serial.println(blueRATIO, DEC);
                }
                
                if(maxCOLOR == 2) //if GREEN was the greatest color
                {
                        redRATIO = float(currentGREEN) / float(currentRED);
                        greenRATIO = 1;
                        blueRATIO = float(currentGREEN) / float(currentBLUE);
                        currentGREEN += 25;
                        currentRED = (currentGREEN / redRATIO) + 0.5;
                        currentBLUE = (currentGREEN / blueRATIO) + 0.5;
                        Serial.print("maxCOLOR = GREEN    ");
                        Serial.print("redRATIO = ");
                        Serial.print(redRATIO, DEC);
                        Serial.print("     blueRATIO = ");
                        Serial.println(blueRATIO, DEC);
                }
                
                if(maxCOLOR == 3) //if BLUE was the greatest color
                {
                        redRATIO = float(currentBLUE) / float(currentRED);
                        greenRATIO = float(currentBLUE) / float(currentGREEN);
                        blueRATIO = 1;
                        currentBLUE += 25;
                        currentRED = (currentBLUE / redRATIO) + 0.5;
                        currentGREEN = (currentBLUE / greenRATIO) + 0.5;
                        Serial.print("maxCOLOR = BLUE    ");
                        Serial.print("redRATIO = ");
                        Serial.print(redRATIO, DEC);
                        Serial.print("     greenRATIO = ");
                        Serial.println(greenRATIO, DEC);
                        
                }
                
                if((currentRED >= 4095) || (currentGREEN >= 4095) || (currentBLUE >= 4095)) //if any of the colors are greater than max value
                {
                        lockOUTup = 1; //prevent the loop from running again until channel down is used
                        Serial.println("LockOutUp Active!");
                        if(currentRED > 4095)
                        {
                                currentRED = previousRED; 
                        }
                        if(currentGREEN > 4095)
                        {
                                currentGREEN = previousGREEN; 
                        }
                        if(currentBLUE > 4095)
                        {
                                currentBLUE = previousBLUE; 
                        }
                }
  
        }
        
        
        if((funCALL == 14) && (lockOUTdown != 1)) //Channel Down
        {
                lockOUTup = 0;
                
                previousRED = currentRED;
                previousGREEN = currentGREEN;
                previousBLUE = currentBLUE;
                
                bugFixRED = currentRED; //See below
                bugFixGREEN = currentGREEN;
                bugFixBLUE = currentBLUE;
                
                if(currentRED == 0)
                  bugFixRED = 5000; //This fixes a bug that when searching for lowest color value, colors that were not in the hue (off) would be selected
                                    //even though there were one or more other colors that were still greater than 0. If one (or more) of the colors aren't
                                    //apart of the hue (set to 0) it sets the color to something greater than a possible value just for finding the lowest
                                    //color value.
  
                if(currentGREEN == 0)
                  bugFixGREEN = 5000;
  
                if(currentBLUE == 0)
                  bugFixBLUE = 5000;                
                
                if(bugFixRED < bugFixGREEN) //figure out if RED or GREEN are the lowest value
                {
                        minCOLOR = 1; //RED was lower
                }
                else
                {
                        minCOLOR = 2; //GREEN was lower
                }
                
                if(minCOLOR == 1)
                {
                        if(bugFixRED > bugFixBLUE)
                        {
                                minCOLOR = 3; //BLUE was lower
                        } 
                }
   
                if(minCOLOR == 2)
                {
                        if(bugFixGREEN > bugFixBLUE)
                        {
                                minCOLOR = 3; //BLUE was lower
                        } 
                }

                if(minCOLOR == 1)
                {
                        redRATIO = 1;
                        greenRATIO = float(currentRED) / float(currentGREEN);
                        blueRATIO = float(currentRED) / float(currentBLUE);
                        currentRED -= 25;
                        currentGREEN = currentRED / greenRATIO;
                        currentBLUE = currentRED / blueRATIO;
                        Serial.print("minCOLOR = RED    ");
                        Serial.print("greenRATIO = ");
                        Serial.print(greenRATIO, DEC);
                        Serial.print("     blueRATIO = ");
                        Serial.println(blueRATIO, DEC);
                }
                
                if(minCOLOR == 2)
                {
                        redRATIO = float(currentGREEN) / float(currentRED);
                        greenRATIO = 1;
                        blueRATIO = float(currentGREEN) / float(currentBLUE);
                        currentGREEN -= 25;
                        currentRED = currentGREEN / redRATIO;
                        currentBLUE = currentGREEN / blueRATIO;
                        Serial.print("minCOLOR = GREEN    ");
                        Serial.print("redRATIO = ");
                        Serial.print(redRATIO, DEC);
                        Serial.print("     blueRATIO = ");
                        Serial.println(blueRATIO, DEC);
                }
                
                if(minCOLOR == 3)
                {
                        redRATIO = float(currentBLUE) / float(currentRED);
                        greenRATIO = float(currentBLUE) / float(currentGREEN);
                        blueRATIO = 1;
                        currentBLUE -= 25;
                        currentRED = currentBLUE / redRATIO;
                        currentGREEN = currentBLUE / greenRATIO;
                        Serial.print("minCOLOR = BLUE    ");
                        Serial.print("redRATIO = ");
                        Serial.print(redRATIO, DEC);
                        Serial.print("     greenRATIO = ");
                        Serial.println(greenRATIO, DEC);
                        
                }
                

                //Check to see if we have reached the lower intensity of the hue
                if(((currentRED < 1)&&(bugFixRED <= 4095)) || ((currentGREEN < 1)&&(bugFixGREEN <= 4095)) || ((currentBLUE < 1) && (bugFixBLUE <= 4095)))
                {
                        lockOUTdown = 1; //lock out the function until channel up is used!
                        
                        Serial.println("LockOutDown Active!");
                        if((currentRED < 1)&&(bugFixRED < 4095))
                        {
                                currentRED = previousRED;
                                Serial.println("currentRED LOW Limit"); 
                        }
                        if((currentGREEN < 1)&&(bugFixGREEN < 4095))
                        {
                                currentGREEN = previousGREEN;
                                Serial.println("currentGREEN LOW Limit"); 
                        }
                        if((currentBLUE < 1)&&(bugFixBLUE < 4095))
                        {
                                currentBLUE = previousBLUE;
                                Serial.println("currentBLUE LOW Limit"); 
                        }
                }
  
        }
  
                //Some debugging info...
                Serial.print("currentRED = ");
                Serial.println(currentRED);
                Serial.print("currentGREEN = ");
                Serial.println(currentGREEN);
                Serial.print("currentBLUE = ");
                Serial.println(currentBLUE);
 
        //Send the colors PWM info to the driver...
        pwm.setPWM(RED, 0, currentRED);
        pwm.setPWM(GREEN, 0, currentGREEN);
        pwm.setPWM(BLUE, 0, currentBLUE);
 
        if((funCALL <= 6) && (funCALL >= 1)) //One of the scene buttons
        {
                //Serial.println("In Loop");
                eepromADDR = funCALL * 10;
                
                highBYTE = EEPROM.read(eepromADDR); //pull in the stored scene values
                eepromADDR++; 
                lowBYTE = EEPROM.read(eepromADDR);
                sceneRED = highBYTE;
                sceneRED = (sceneRED << 8) | lowBYTE; //combine the low byte and high byte to 1 value
                eepromADDR++;
                
                highBYTE = EEPROM.read(eepromADDR);
                eepromADDR++; 
                lowBYTE = EEPROM.read(eepromADDR);
                sceneGREEN = highBYTE;
                sceneGREEN = (sceneGREEN << 8) | lowBYTE;
                eepromADDR++;
                
                highBYTE = EEPROM.read(eepromADDR);
                eepromADDR++; 
                lowBYTE = EEPROM.read(eepromADDR);
                sceneBLUE = highBYTE;
                sceneBLUE = (sceneBLUE << 8) | lowBYTE;
                eepromADDR++;
                
                
                fadeCALL(1); //fade to new scene

        }
 
        if(funCALL == 10) //Power Button
                fadeCALL(2); //fade on or off
  
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void fadeCALL(byte mode)
{
  
        int fadeDIFFred=0, fadeDIFFgreen=0, fadeDIFFblue=0, i=0;
        int fadeSTEPred=0, fadeSTEPgreen=0, fadeSTEPblue=0, redCOUNTER = 0, greenCOUNTER = 0, blueCOUNTER = 0;
        byte fadeFLAG = 0, redSIGN=0, greenSIGN=0, blueSIGN=0, redFLAG = 0, greenFLAG = 0, blueFLAG = 0, pwrONflag = 0, pwrOFFflag = 0;
        int stepREG = 1000;
 
        if(mode == 2) //power button was pressed
        {
                if((currentRED != 0) || (currentGREEN != 0) || (currentBLUE != 0)) //looking to see if any color is on
                {
                        //One or more colors were on, so we need to fade to all off
                        lastSCENEred = sceneRED;
                        sceneRED = 0;
                        lastSCENEgreen = sceneGREEN;
                        sceneGREEN = 0;
                        lastSCENEblue = sceneBLUE;
                        sceneBLUE = 0; 
                        pwrOFFflag = 1;
                }
                else
                {
                        //No colors were on so we need to fade to the last known scene
                        sceneRED = lastSCENEred;
                        sceneGREEN = lastSCENEgreen;
                        sceneBLUE = lastSCENEblue;
                        pwrONflag = 1; 
                }
        } 

        if((pwrONflag == 1) || (pwrOFFflag == 1))
        {
                stepREG = 2000; //fade time for power on or power off
        }
        else
        {
                stepREG = 1000; //fade time = ~ 1 second between scenes
        }
 
        //fade from current scene to new scene
        fadeDIFFred = sceneRED - currentRED; //calculate the difference between the new and old
        fadeDIFFgreen = sceneGREEN - currentGREEN;
        fadeDIFFblue = sceneBLUE - currentBLUE;
   
        if(fadeDIFFred != 0)
        {
                fadeSTEPred = stepREG / abs(fadeDIFFred); //calculate the step size needed to fade within the fade time
        }
   
        if(fadeDIFFgreen != 0)
        {
                fadeSTEPgreen = stepREG / abs(fadeDIFFgreen);
        }
   
        if(fadeDIFFblue != 0)
        {
                fadeSTEPblue = stepREG / abs(fadeDIFFblue);
        }
   
        redSIGN = (fadeDIFFred < 0) ? 1 : 2; //figure out if we need to fade up or fade down to the new value
        greenSIGN = (fadeDIFFgreen < 0) ? 1 : 2;
        blueSIGN = (fadeDIFFblue < 0) ? 1 : 2;
 

        while(!fadeFLAG)
        {
     
                if(currentRED - sceneRED == 0)
                        redFLAG = 1; //if RED is at its new value set the color flag
     
                if(redFLAG != 1) //if it hasn't reached its new value yet, keep working on it!
                {
                        if(redCOUNTER == fadeSTEPred) //check to see if enough "steps" have elapsed
                        {
                                if(redSIGN == 1) //we need to dim the color
                                {
                                        currentRED -= 1; 
                                }
                                else //we need to increase the color
                                {
                                        currentRED += 1; 
                                }
                                pwm.setPWM(RED, 0, currentRED); //update the hue
                                redCOUNTER = 0; //reset the counter
                        }
                        else
                        {
                                redCOUNTER++; //increase the steps if enough time hasn't elapsed yet...
                        }
                }     
     
                //Green color
                if(currentGREEN - sceneGREEN == 0)
                        greenFLAG = 1;
     
                if(greenFLAG != 1)
                {
                       if(greenCOUNTER == fadeSTEPgreen)
                        {
                                if(greenSIGN == 1)
                                {
                                        currentGREEN -= 1; 
                                }
                                else
                                {
                                        currentGREEN += 1; 
                                }
                 
                                pwm.setPWM(GREEN, 0, currentGREEN);
                                greenCOUNTER = 0;
                        }
                        else
                        {
                                greenCOUNTER++; 
                        }
                }
          
          
                //Blue color
                if(currentBLUE - sceneBLUE == 0)
                       blueFLAG = 1;
     
                if(blueFLAG != 1)
                {
                        if(blueCOUNTER == fadeSTEPblue)
                        {
                                if(blueSIGN == 1)
                                {
                                        currentBLUE -= 1; 
                                }
                                else
                                {
                                        currentBLUE += 1; 
                                }
                                pwm.setPWM(BLUE, 0, currentBLUE);
                        blueCOUNTER = 0;
                        }
                        else
                        {
                                blueCOUNTER++; 
                        }
                }     
     
                if((redFLAG+greenFLAG+blueFLAG) == 3) //see if all 3 colors have reached their new hue
                        fadeFLAG = 1; //if so set the flag to exit the loop!
     
                delay(1);
        }
 
        fadeFLAG = 0;
        redFLAG = 0;
        greenFLAG = 0;
        blueFLAG = 0;
        pwrONflag = 0;
        pwrOFFflag = 0;
   
}

