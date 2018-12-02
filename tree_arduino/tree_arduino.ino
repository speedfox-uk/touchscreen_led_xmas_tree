//Stuff for nandling the nextion screen
#include <SoftwareSerial.h>

#include <NexButton.h>
#include <NexScrolltext.h>
#include <NexSlider.h>
#include <NexNumber.h>


//The only library needed for the LEDs
#include <Adafruit_NeoPixel.h>

//definitions for secren handling
#ifdef __AVR_ATtiny84__ || __AVR_ATtiny85__
  #define RX_PIN 3
  #define TX_PIN 4
#else
  #define RX_PIN 8
  #define TX_PIN 9
#endif

//Serial interface for the Nesxtion screen.
SoftwareSerial nextionSerial(RX_PIN, TX_PIN);

//UI components on the nextion screen
NexScrolltext  modeName = NexScrolltext(0, 4, "scrollMode");
NexButton prev = NexButton(0,6, "btnPrev");
NexButton next = NexButton(0, 7, "btnNext");
NexSlider speedSlider = NexSlider(0, 1, "scrollSpeed");
NexNumber speedDisp = NexNumber(0, 2, "numSpeed");

//List oc all of the UI elements that need to be checked. Needs to be NULL terminated
NexTouch *nex_listen_list[] = 
{
    &prev,
    &next,
    &speedSlider,
    NULL
};

//definitions for light control
#define NUM_MODES 4
#define NUM_LEDS 8
#ifdef __AVR_ATtiny84__ || __AVR_ATtiny85__
  #define LED_PIN 2 //Physical pin 7
#else
  #define LED_PIN 10
#endif
#define NUM_RB_COLS 7 //Number of colours in the rainbow
char* modeNames[] ={"Chase", "Rainbow", "Double Peaks", "OFF"}; //List of mode names.
unsigned int (* modeFns[NUM_MODES])(unsigned int);
//CRGB leds[NUM_LEDS];
uint8_t leds[NUM_LEDS];
// Parameter 1 = number of pixels in strip
// Parameter 2 = pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

byte mode = 0;
uint32_t twinkleSpeed = 5;
unsigned int lightPos = 0;
int itrPos=1;
long rainbow[] = {0xFF0000, 0x4b0082, 0xFFFF00, 0x00FF00, 0x0000FF, 0xFF4000, 0x70004a}; //Colours in the rainbow. Orange and Indigo are swapped;

//To be called after any update of teh speed value
inline void updateSpeed(){
  speedDisp.setValue(twinkleSpeed);
}

//To be called after any update of the mode
inline void updateMode(){
  modeName.setText(modeNames[mode]);
  lightPos = 0;
}

//==============================================
//These are the callbacks for the buttons and the slider

//Callback for the next button. Goes to the next mode. Need to make sure mode stays under NUM_MODES
void cbNext(void*)
{
  mode++;
  mode = mode % NUM_MODES;
  updateMode();
}


//Callback for the previous button. Goes to the previous mode. Need to make sure we don't go below 0
void cbPrev(void*)
{  
  if(0 == mode)
      mode = NUM_MODES -1;
  else
      mode--;
  updateMode();
}

//Responds to change in the speed slider
void cbSlider(void*){
    speedSlider.getValue(&twinkleSpeed);
    updateSpeed();
}

//The next functions set the lights to different modes

//This function is for the "chase pattern.
unsigned int setLedChase(unsigned int inPos){
  const unsigned int slowFactor = 5;
  unsigned int slowPos = inPos /slowFactor;
  int stepPos = NUM_LEDS - (slowPos % NUM_LEDS) -1;
  unsigned int rainbowPos = (slowPos / NUM_LEDS) % NUM_RB_COLS;
  
  boolean flipped = false;
  for(uint16_t i=0; i<NUM_LEDS; i++){
    uint8_t r,g,b;

    if(i > stepPos && !flipped){
      rainbowPos++;
      if(7 <= rainbowPos)
          rainbowPos =0;
      flipped = true;
    }
    
    g = (rainbow[rainbowPos] & 0xFF0000)>>16;
    r = (rainbow[rainbowPos] & 0x00FF00)>>8;
    b = rainbow[rainbowPos] &0xFF;
    strip.setPixelColor(i, r, g, b); 
  }

  inPos++;
  if(inPos == (NUM_LEDS * NUM_RB_COLS *slowFactor))
      inPos = 0;
  return inPos;
}

//This is the function for the rainbow pattern
unsigned int setLedsRainbow(unsigned int inPos)
{
    
    int howWhite = inPos %256;
    unsigned long changing = (inPos/(256)) % NUM_LEDS;
    unsigned long rainbowPos = (inPos/(256*NUM_LEDS)) % NUM_RB_COLS;
    for(uint16_t i=0; i<NUM_LEDS; i++){
      uint8_t r,g,b;
      //CRGB pix;
      if(i!=changing)
      {
        //red and green channels are swapped (WTF)
        g = (rainbow[rainbowPos] & 0xFF0000)>>16;
        r = (rainbow[rainbowPos] & 0x00FF00)>>8;
        b = rainbow[rainbowPos] &0xFF;
        rainbowPos++;
        if(rainbowPos >= NUM_RB_COLS)
            rainbowPos =0;
      }
      else{
        long nextCol = rainbow[(rainbowPos +1) % NUM_RB_COLS];
        int howColoured = 255-howWhite;
        g =  (((nextCol &0xFF0000) >>16  )*howColoured + 0xFF*howWhite)/255;
        r =  (((nextCol &0xFF00) >>8  )*howColoured + 0xFF*howWhite)/255;
        b =  (((nextCol &0xFF)  )*howColoured + 0xFF*howWhite)/255;
        
      }
      strip.setPixelColor(i, r, g, b);
      
    }
    
    inPos++;
    if((256*NUM_LEDS*sizeof(rainbow))==inPos){
        inPos =0;
    }
     
    return inPos;
}

//Function for the DoublePeak pattern
unsigned int setLedsDoublePeak(unsigned int inPos){

      unsigned short pairNum= inPos / 512;
      short pos = inPos%512;
      unsigned short adjust = 0;
      if(pos < 256)
          adjust = pos;
      else
          adjust = 511 - pos;
  
      for (uint16_t i=0; i<NUM_LEDS; i++)
      {
        short pair = (i/2) %2;
        unsigned short thisAdj = ( pair == pairNum) ? adjust :0;
        uint8_t g = (i%2==0)?  0xFF : thisAdj;
        uint8_t r = (i%2==1)?  0xFF : thisAdj;
        uint8_t b = thisAdj;
        strip.setPixelColor(i, r, g, b);
      }
      
      inPos++;
      
     if(1024 == inPos)
          inPos =0;

    return inPos;
}

//This is the function when the LEDS are to be just runed off
unsigned int setLedsOff(unsigned int inPos){
  for (uint16_t i=0; i<NUM_LEDS; i++)
  {    
    strip.setPixelColor(i, 0,0,0);
  }
  return 1;
}

//==============================================
//Core aruduino functions here

//The usual setup function
void setup() {
    nextionSerial.begin(9600);
    nexInit(&nextionSerial);
    
#if !defined(__AVR_ATtiny84__) && !defined(__AVR_ATtiny85__)
  Serial.begin(9600);
#endif
   //Attach the callbacks
   prev.attachPop(cbPrev);
   next.attachPop(cbNext);    
   speedSlider.attachPop(cbSlider);

  pinMode(LED_PIN, OUTPUT);
  strip.begin();
  strip.show();

  //Set teh function pointers for each mode. 
  //This needs to be in the same order as modeNames
  modeFns[0] = setLedChase;
  modeFns[1] = setLedsRainbow;
  modeFns[2] = setLedsDoublePeak;
  modeFns[3] = setLedsOff;
  
  updateMode();
  updateSpeed();
}

//The usual arduino loop function. 
void loop() {

  //Update the screen on every loop. 
  nexLoop(nex_listen_list);
#if !defined(__AVR_ATtiny84__) &&  !defined(__AVR_ATtiny85__)
  Serial.print("mode ");
  Serial.print(mode);
  Serial.print("\n");
#endif

  //If this is an itteration to do an update, do it.
  if((itrPos * 1000)  > (100000/(twinkleSpeed+1))) {
    lightPos = modeFns[mode](lightPos);    
    strip.show(); //Send the new light values to the lights.
    itrPos=0;
  }

  itrPos++;
  
  delay(1);//Wait a little bit
}

