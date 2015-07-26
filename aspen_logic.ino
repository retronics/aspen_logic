/*

ASPEN BY BRIARWOOD LOGIC

༼ノ◕ヮ◕༽ノ︵┻━┻  ---o

To-do:
-High score to EEPROM
-High score on gameOver()
-High score on attract()
-Debounce switch on going from attract to playerSetup
-Show score again at end of multiplayer

*/

//Libraries
//Library for the MAX7219 seven segment display board
#include "LedControl.h"


//CONFIGS
//GAME
//Maximum number of players
#define maxPlayers 4
//Balls per game
#define ballsPerGame 3
//Time in milliseconds to wait between start button press and start
#define startTimeToWait 3000
//Tilt warnings, tilt on last
#define maxTiltWarnings 3
//HARDWARE
//Time the ball return soleniod fires for
#define solenoidBReturnDelay 150
//Time the chime box solenoids fire for
#define solenoidChimeDelay 40
//Screen brightness 0 = low 15 = high
#define scoreBoardBrightness 7
//Test tone on start
#define testToneOn true

//VARIABLES
//Number of players
byte players = 1; //Default is one
//Current player
byte currentPlayer = 0; //0=P1 as is array index
//Player scores
long playerScore[maxPlayers];
//Players current ball
byte playerBall[maxPlayers];
//If each player has bonus lit
boolean bonusActivated[maxPlayers];
//Number of warnings the player has
byte tiltWarningCount;
//
boolean isTilted;

//INPUTS
// Define input names and pins
#define BRoll 23
#define BTrough 41
#define Tilt 50
#define Start 52
char* switchNames[]    =  {"LLSling", "LRSling", "LTarg", "CTarg", "RTart",
                          "LSpin", "CSpin", "RSpin", "TLSling", "TRSling", 
                          "LOut", "ROut", "BRoll", "BBump", "BTrough",
                          "Tilt", "Start"};
byte switchPins[]       =  {43,33,47,51,29,
                            49,31,37,25,39,
                            45,35,23,27,41,
                            50,52};
byte activePins[]      =   {1,1,1,1,1,
                            1,1,1,1,1,
                            1,1,1,1,1,
                            1,1};
int switchScore[]        = {10, 10, 100, 100, 100,
                            10, 10, 10, 10, 10,
                            100, 100, 100, 1000, 0,
                            0, 0};
//Bonus switch ID
byte bonusSwitchID = 12;
//Multiplier for bonus
byte bonusMultiplier = 10;
//Get the number of switches
#define numberOfSwitches sizeof(switchPins)
//Track if switches are currently pressed
volatile byte pressed[numberOfSwitches];

//OUTPUTS
LedControl scoreBoard=LedControl(22,26,24,1);
// pin 22 (blue) = DIN
// pin 26 (yellow) = CLK
// pin 24 (green)= CS
// 1 as we are only using 1 MAX7219
//Tilt relay
byte tiltRelay = 30;
//Ball return
byte ballReturn = 32;
//Internal Speaker
byte internalSpeaker = 34;
//Bonus lights
byte bonusLights = 42;
//Chimes
byte smallChime = 44;
byte midChime = 46;
byte bigChime = 48;

void setup(){
  
  //Initialise Input
  //Set up all input pins
  for (int i = 0; i < numberOfSwitches; i++){
    //Set pins to internal pull up
    pinMode(switchPins[i], INPUT_PULLUP);
  }
  
  //Initalise Output
  //Bonus Lights
  pinMode(bonusLights, OUTPUT);
  digitalWrite(bonusLights, LOW);
  //Ball Return
  pinMode(ballReturn, OUTPUT);
  digitalWrite(ballReturn, LOW);
  //Tilt Relay
  pinMode(tiltRelay, OUTPUT);
  digitalWrite(tiltRelay, LOW);
  //Chimes
  pinMode(smallChime, OUTPUT);
  digitalWrite(smallChime, LOW);
  pinMode(midChime, OUTPUT);
  digitalWrite(midChime, LOW);
  pinMode(bigChime, OUTPUT);
  digitalWrite(bigChime, LOW);
  //Initialise score board
  // the zero refers to the MAX7219 number, it is zero for 1 chip
  scoreBoard.shutdown(0,false);// turn off power saving, enables display
  scoreBoard.setIntensity(0,scoreBoardBrightness);// sets brightness (0~15 possible values)
  scoreBoard.clearDisplay(0);// clear screen
  //Set up serial port for debug
  Serial.begin(9600);
  //Test tone
  if (testToneOn) tone(internalSpeaker, 1600, 500);

  //Call attract mode
  attractMode();
  
  //Call player setup
  playerSetup();
}

void loop(){
    //Check score switches
    checkSwitches();
}

//When game is in active
void attractMode(){
  //Start button tracker
  boolean startButtonDown = false;
  //Turn on bonus lights
  digitalWrite(bonusLights, HIGH);
  //While start button not active loop a display
   while (digitalRead(Start)){
   //First message to display
   char attractMessage[3][8] = {{' ',' ','p','l','a','4',' ',' '},{' ',' ','a','5','p','e','n',' '},{'p','1','n','b','a','l','l',' '}};
   //Scroll first message
   for(int i=0; i<3; i++){
    for (int a=0; a<8; a++){ 
        scoreBoard.setChar(0,a,attractMessage[i][7-a],false);
        delay(100);
        //Check start button
        if (!startButtonDown && !digitalRead(Start)){
          startButtonDown = true;
        } else if(startButtonDown && digitalRead(Start)){
          scoreBoard.clearDisplay(0);//Clear the score board
          //Turn off bonus lights
          digitalWrite(bonusLights, LOW);
          //Clear score board
          scoreBoard.clearDisplay(0);
          //Wait for debounce
          delay(100);
          return;//Return to setup
        }
      }
     //Clear score board between words
     scoreBoard.clearDisplay(0); 
   }
 }
 //Turn off bonus lights
 digitalWrite(bonusLights, LOW);
}

//Setup players in game
void playerSetup(){
  //Initialise scores and balls
  //Player scores
  for (byte i = 0; i < maxPlayers; i++) playerScore[i] = 0;
  //Players current ball
  for (byte i = 0; i < maxPlayers; i++) playerBall[i] = 1; //First ball is 1, last ball is ballsPerGame
  //Set all bonuses to false
  for (byte i = 0; i < maxPlayers; i++) bonusActivated[i] = false;
  
  long timeToStart = millis()+startTimeToWait; //Time in which to call startBall
  boolean startButtonDown = false; //Saves state of the start button
  byte debounceDelay = 50;
  //Set up the score board
  char playerSelectMessage[8] = {'p','l','a','4','.',' ',' ',' '}; //Message to display in the seven leftmost segments
  //Score message
  for (int a=0; a<8 ; a++){
    scoreBoard.setChar(0,a,playerSelectMessage[7-a],false);
  }
  scoreBoard.setChar(0,0,players,false);//Set farthest right digit to player
  //While timeToStart is not met
  while (millis()<timeToStart){
    //Check for button presses here
    if (!digitalRead(Start)){
      timeToStart = millis()+startTimeToWait; //If the button is down change the time to start
      startButtonDown = true; //Start button is down
      delay(debounceDelay);//Wait as debounce
    }
    //If button is release
    if (digitalRead(Start) && startButtonDown){
      //Start button is no longer down
      startButtonDown = false;
        //Increment or wrap the player counr
        if (players < maxPlayers){
          players++; //Add a player
        } else if (players >= maxPlayers){
          players=1;//Wrap the player value
      } 
    }
    scoreBoard.setChar(0,0,players,false);//Update the player on display
  }
  //Number of players is selected
  currentPlayer = 0;//Ensure that the current player is the first
  //Call start game
  startBall();
}

//Start of a new ball
void startBall(){
  //Set up the score board
  char playerBallMessage[8] = {'b','a','l','l','.',' ',' ',' '}; //Message to display in the seven leftmost segments
  //Score message
  for (int a=0; a<8 ; a++){
    scoreBoard.setChar(0,a,playerBallMessage[7-a],false);
  }
  scoreBoard.setChar(0,0,playerBall[currentPlayer],false);//Set farthest right digit to the ball
  //Delay to show message
  delay(2000);
  //Reset tilt
  isTilted = false;
  //Reset tilt warnings
  tiltWarningCount = 0;
  //Activate flippers
  digitalWrite(tiltRelay, LOW);
  //Setup bonus lights
   if (bonusActivated[currentPlayer]){
     //Turn on bonus lights
     digitalWrite(bonusLights, HIGH);
   } else {
       //Turn off bonus lights
     digitalWrite(bonusLights, LOW); 
   }
   //Feed ball
   feedBall();
}

//Check all switch states
void checkSwitches(){
  //Variable store current and previous switch states
  static byte previousState[numberOfSwitches];
  static byte currentState[numberOfSwitches];
  //Cycle through buttons
  for (byte i = 0; i < numberOfSwitches; i++){
    //Read the buttons
    currentState[i]=digitalRead(switchPins[i]);
    //Check new and previous state match
    if (currentState[i] == previousState[i]){
     if (pressed[i] == 0 && currentState[i] == 0){
        //This is just pressed
        //Chime if has a score and not a spinner
        if (switchScore[i] > 0 && switchNames[i] != "LSpin" && switchNames[i] != "CSpin" && switchNames[i] !="RSpin") chime(switchScore[i]);
        //If ball through, call ballEnd()
        if (i==14) {
          ballEnd();
        }
        //If tilt call tilt
        if (i==15){
         tilt(); 
        }
        //Debug pressed
        Serial.print(switchNames[i]);
        Serial.print(" (ID: ");
        Serial.print(i);
        Serial.println(") pressed");
     }
    else if(pressed[i] == 1 && currentState[i] == 1){
      //This is just released
        Serial.print(switchNames[i]);
        Serial.print(" (ID: ");
        Serial.print(i);
        Serial.println(") released");
      switchUp(i);
    } 
    pressed[i] = !currentState[i]; //Pressed is the inverse of the current state
    }
    previousState[i] = currentState[i]; //Update previous state
  }
  
}

//Update the score board with the score
void updateScoreBoard (){
  //Display score
  //The ID of the segment to display on
  byte segmentPosition = -1;
  //Initialize the char array
  char scoreDigits[8] = {' ',' ',' ',' ',' ',' ',' ',' '};
  //Convert the score to a string
  String scoreString = String(playerScore[currentPlayer]);
  //Convert string to a character array
  scoreString.toCharArray(scoreDigits, 8);
  //Loop through array and output
    for(int a=8; a>=0; a--){
      //Replace the null terminator
      if (scoreDigits[a]=='\0') scoreDigits[a]=' ';
      //If score is a number
      if (scoreDigits[a]!=' '){
        //Write the score
        scoreBoard.setChar(0,segmentPosition,scoreDigits[a],false);
        segmentPosition++;
      }
    }
}

//When a switch is released from press
void switchUp (byte switchID){
  //If the switch scores and table not tilted then add the switch score
  if (switchScore[switchID]>0 && !isTilted){
    //Check to see if bonus is activated
    if (bonusActivated[currentPlayer] && switchScore[switchID]<1000){
      //Bonus activated scoring
      playerScore[currentPlayer] += (switchScore[switchID] * bonusMultiplier);
      //As we updated the score, update the score board
      updateScoreBoard();
    } else {
      //For non-bonus scoring
      playerScore[currentPlayer] += switchScore[switchID];
      //As we updated the score, update the score board
      updateScoreBoard();
    }
  }
  
  //If the switch is the bonus, bonus is not already activated and game not tilted
  if (switchID == bonusSwitchID && !bonusActivated[currentPlayer] && !isTilted){
    //Set the bonus to activated
    bonusActivated[currentPlayer] = true;
    //Update bonus lights
    digitalWrite(bonusLights, HIGH);
  }
}

//Feeds the ball to the plunger from the through
void feedBall(){
  //Wait for ball to settle
  delay(500); 
  //Fire ball return if ball return is pressed
  if (!digitalRead(switchPins[14])){
    //Turn of screen
    scoreBoard.shutdown(0,true); //Turn scoreboard of as it pulls down output voltage driving ball return
    delay(100); //Wait so voltage is back up
    //Fire the solenoid
    digitalWrite(ballReturn, HIGH);
    delay(solenoidBReturnDelay);
    digitalWrite(ballReturn, LOW);
    scoreBoard.shutdown(0,false); //Turn scoreboard back on
  }
  scoreBoard.clearDisplay(0);//clear screen
  updateScoreBoard();//Show the player's updated score
}

//Called when a ball ends
void ballEnd(){
  //If more than one player increment or wrap ball
  //If last ball of last player, then game over
  if (playerBall[currentPlayer]>=ballsPerGame && currentPlayer == (players-1)){
    gameOver();//Call the game over function
  }
  //if more than one player
  else if (players>1){
    playerBall[currentPlayer]++; //Increment current player's ball
    //Check for wrap then set player appropriately
    if (currentPlayer>=(players-1)){
      currentPlayer=0;//Wrap the player
    }else{
     currentPlayer++;//Increment player
    }
    //Message to display current player
    char playerNumberMessage[8] = {'p','1','a','4','.',' ',' ',' '}; //Message to display in the seven leftmost segments
    //Score message
    for (int a=0; a<8 ; a++){
      scoreBoard.setChar(0,a,playerNumberMessage[7-a],false);
    }
    scoreBoard.setChar(0,0,(currentPlayer+1),false);//Set farthest right digit to the current player
    delay(2000);//Wait 2 second
    startBall();
  }
  //Otherwise must be single player with ball(s) remaining
  else{
  playerBall[currentPlayer]++; //Increment ball
  startBall(); //Feed the ball
  }
}

//Called when tilt occurs
void tilt(){
  //Increment the warnings
  tiltWarningCount++;
  //If at max warnings, is a tilt
  if (tiltWarningCount>maxTiltWarnings){
    isTilted = true;
    digitalWrite(tiltRelay, HIGH); //Turn off flippers
    //Display --------
    for (int a=0; a<8; a++){
      scoreBoard.setChar(0,a,'-',false);//Set each display
    }
    //Low long tone
    tone(internalSpeaker, 50, 3000);
  }else{
  //Set tone
  tone(internalSpeaker, 100, 1000);
  //Display warning message
  for (int blinks = 3; blinks>0; blinks--){
    //Display --------
    for (int a=0; a<8; a++){
      scoreBoard.setChar(0,a,'-',false);//Set each display
    }
    delay(125);//Wait eight of a second
    scoreBoard.clearDisplay(0);// clear screen
    delay(125);//Wait eight of a second
  }
  delay(500);//Wait half a second
  updateScoreBoard();//Update to score
  }
}

//Fires a chime for solenoidChimeDelay dependent on score of switch
void chime(int switchScore){
  //Switch for incoming score
  switch(switchScore){
    case 1000: //Highest possible score
      digitalWrite(smallChime, HIGH);
      delay(solenoidChimeDelay);
      digitalWrite(smallChime, LOW);
      break;
    case 100: //Middle sized chime
      digitalWrite(midChime, HIGH);
      delay(solenoidChimeDelay);
      digitalWrite(midChime, LOW);
      break;
     case 10: //Small chime
      digitalWrite(bigChime, HIGH);
      delay(solenoidChimeDelay);
      digitalWrite(bigChime, LOW);
      break;
     default: //If unknown value, give an error beep
      tone(internalSpeaker, 1600, 500);
  }
}

//Called when all balls of all players are drained
void gameOver(){
  //Display message
  if (players>1){
    for (int i=0; i<players; i++){
    //Message to display current player
    char playerNumberMessage[8] = {'p','l','a','4','.',' ',' ',' '}; //Message to display in the seven leftmost segments
    //Score message
    for (int a=0; a<8 ; a++){
      scoreBoard.setChar(0,a,playerNumberMessage[7-a],false);
    }
    scoreBoard.setChar(0,0,i+1,false);//Set farthest right digit to the current player
    delay(1000);//Wait
    scoreBoard.clearDisplay(0);//clear screen
    currentPlayer = i;
    updateScoreBoard();//Displaye score
    delay(1000);//Wait
    }
    scoreBoard.clearDisplay(0);//clear screen
    //Blink the highest score player
    //Find highest scoring player
    int highestScorePlayer=0;//Starts with lowest index as highest score
    //Count through players, 
    for (int thisPlayer=1; thisPlayer<players; thisPlayer++){
      if(playerScore[thisPlayer]>playerScore[highestScorePlayer]) highestScorePlayer = thisPlayer;
    }
    Serial.print("Highest Scoring Player: ");
    Serial.print(highestScorePlayer);
    //Blink winning player
    for(int i=0; i<8; i++){
      delay(200);//Wait
      scoreBoard.clearDisplay(0);//clear screen
      delay(200);//Wait
      //Message to display current player
      char playerNumberMessage[8] = {'p','L','a','4','.',' ',' ',' '}; //Message to display in the seven leftmost segments
      //Score message
      for (int a=0; a<8 ; a++){
        scoreBoard.setChar(0,a,playerNumberMessage[7-a],false);
      }
      scoreBoard.setChar(0,0,highestScorePlayer+1,false);//Set farthest right digit to the highest scoring player
    }
    delay(2500);//Wait so player can see winning player
    scoreBoard.clearDisplay(0);//clear screen
    
  }else{
    //Blink score
    for(int i=0; i<8; i++){
      delay(200);//Wait
      scoreBoard.clearDisplay(0);//clear screen
      delay(200);//Wait
      updateScoreBoard();//Displaye score
    }
    delay(2500);//Wait so player can see score
    scoreBoard.clearDisplay(0);//clear screen
  }
  //
  //Call attract mode
  attractMode();
  //Call player setup
  playerSetup();
}
