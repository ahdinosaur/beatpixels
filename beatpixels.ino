#include <FastSPI_LED2.h>

#include <OSCBundle.h>
#include <OSCBoards.h>

#ifdef BOARD_HAS_USB_SERIAL
#include <SLIPEncodedUSBSerial.h>
SLIPEncodedUSBSerial SLIPSerial( thisBoardsSerialUSB );
#else
#include <SLIPEncodedSerial.h>
 SLIPEncodedSerial SLIPSerial(Serial);
#endif

//#define NUM_STRIPS 2
//#define LEDS_PER_STRIP 16
//#define NUM_LEDS (NUM_STRIPS * LEDS_PER_STRIP)
#define NUM_LEDS 32

CRGB leds[NUM_LEDS];

OSCBundle bundleOUT;

/* strip definition */
typedef struct {
  int start;
  int end;
} Strip;

Strip strips [2] = {
  { 0, 15 },
  { 16, 31 }
};

/* RGB utilities */

typedef struct {
  CRGB * vals;
  int len;
} CRGBS;

/* set RGB colors */
void setRGB(int stripNum, CRGBS &colors)
{
  int pxStart = strips[stripNum].start;
  int pxEnd = strips[stripNum].end;
  for (int px = pxStart; px < pxEnd; px++)
  {
    CRGB color = colors.vals[px % colors.len];
    leds[px] = colors.vals[px % colors.len];
  }
}

void getRGB(OSCMessage &msg, CRGBS &colors)
{
  /* get rgb colors */
  int msgSize = msg.size();
  if (msgSize && ((msgSize % 3) != 0))
  {
    bundleOUT.add("/error/num/rgb/colors");
    return;
  }
  int numColors = msgSize / 3;
  bundleOUT.add("/numColors").add(numColors);
  CRGB * vals = (CRGB *) malloc(numColors * sizeof(CRGB));
  for (int i = 0; i < numColors; i++)
  {
    int red = msg.getInt(i * 3);
    int green = msg.getInt(i * 3 + 1);
    int blue = msg.getInt(i * 3 + 2);
    bundleOUT.add("/rgb").add(red).add(green).add(blue);
    vals[i] = CRGB(red, green, blue);
  }
  colors.len = numColors;
  colors.vals = vals;
}

void setHSV(int stripNum, CHSV color[])
{
  
}

/* various led dispatches */

void dispatchRainbow(OSCMessage &msg)
{
  static uint8_t startingHue = 0;
  startingHue -= 1;
  fill_rainbow(leds, NUM_LEDS, startingHue);
  bundleOUT.add("/leds/rainbow");
}
      
/* main controller for led routes 

/leds
  /strip/[0-9]
    /one/[0-9]
    /many/[0-9]/[0-9]/.../endmany
    /from/[0-9]/to/[0-9]
    /all
      /rgb
      /rainbow
      /hsv
        color1, color2, ...

*/
void routeLeds(OSCMessage &msg, int ledsOffset)
{
  char address [30];
  int addressLen = msg.getAddress(address, ledsOffset);
  bundleOUT.add("/leds").add(address).add(addressLen);
  
  /* get strip number */
  int startStrip = msg.match("/strip", ledsOffset);
  int stripOffset = msg.match("/strip/*", ledsOffset);
  int stripCharsLen =  stripOffset - startStrip;
  
  char stripChars [stripCharsLen];
  strncpy(stripChars, address + startStrip + 1, stripCharsLen);
  stripChars[stripCharsLen] = '\0';
  
  int stripNum = atoi(stripChars);
  
  bundleOUT.add("/strip").add(stripNum);
  
  /* get whether all, range, or single pixels */
  
  int offset = ledsOffset + stripOffset;
  
  int allOffset = msg.match("/all", offset);
  if (allOffset)
  {
    offset += allOffset;
    
    if (msg.fullMatch("/rgb", offset))
    { 
      CRGBS colors;
      getRGB(msg, colors);
      setRGB(stripNum, colors);
      free(colors.vals);
    } else if (msg.fullMatch("/rainbow", offset)) {
      msg.dispatch("/rainbow", dispatchRainbow, offset);
    } else if (msg.fullMatch("/red", offset)) {
      CRGB vals [] = { CRGB::Red };
      CRGBS colors = { vals, 1 };
      setRGB(stripNum, colors);
      bundleOUT.add("/red");
    } else if (msg.fullMatch("/green", offset)) {
      CRGB vals [] = { CRGB::Green };
      CRGBS colors = { vals, 1 };
      setRGB(stripNum, colors);
      bundleOUT.add("/green");
    } else if (msg.fullMatch("/blue", offset)) {
      CRGB vals [] = { CRGB::Blue };
      CRGBS colors = { vals, 1 };
      setRGB(stripNum, colors);
      bundleOUT.add("/blue");
    }
  }

}

void setup() {
  // sanity check delay - allows reprogramming if accidently blowing power w/leds
  delay(2000);

  // set led configuration
  // add each individual led strip in use
  LEDS.addLeds<WS2801,RGB>(leds, NUM_LEDS);
  
  // clear leds
  LEDS.showColor(CRGB::Black);
  
  // set serial configuration
  // use baud rate as high as you can reliably run on your platform
  SLIPSerial.begin(9600);
#if ARDUINO >= 100
  while(!Serial)
    ;   // Leonardo bug
#endif
}

void loop() {
  // reads and routes the incoming message
  OSCBundle bundleIN;
  int size;

  while (!SLIPSerial.endofPacket()) {
    if ((size =SLIPSerial.available()) > 0)
    {
      while(size--) {
        bundleIN.fill(SLIPSerial.read());
      }
    }
  }

  if(!bundleIN.hasError()) {
    bundleOUT.add("/noerror/size").add(bundleIN.size());
    if (bundleIN.size() > 0) {
      bundleIN.route("/leds", routeLeds);
      LEDS.show();
    }
  }
  bundleIN.empty();
  
  // send the outgoing message
  SLIPSerial.beginPacket();
  bundleOUT.send(SLIPSerial);
  SLIPSerial.endPacket();
  bundleOUT.empty();
}
