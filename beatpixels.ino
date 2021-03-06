#include <math.h>

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

#define NUM_STRIPS 15
#define LEDS_PER_STRIP 32
#define NUM_LEDS (NUM_STRIPS * LEDS_PER_STRIP)

CRGB leds[NUM_LEDS];

OSCBundle bundleOUT;

/* strip definition */
typedef struct {
  int start;
  int length;
} Strip;

Strip strips [1] = {
  { 0, 480 }
};

/* RGB utilities */

typedef struct {
  CRGB * vals;
  int len;
} CRGBS;

/* set RGB colors */
void setRGB(int stripNum, CRGB &color, int px)
{
  int i = ((px % strips[stripNum].length) + strips[stripNum].start);
  leds[i] = color;
}

void setRGB(int stripNum, CRGBS &colors)
{
  for (int px = strips[stripNum].start;
       px < (strips[stripNum].start + strips[stripNum].length);
       px++)
  {
    setRGB(stripNum, colors.vals[px % colors.len], px);
  }
}

void setRGB(int stripNum, CRGBS &colors, int from, int to)
{
  if ((to - from) > strips[stripNum].length)
  {
     setRGB(stripNum, colors);
     return;
  }
  for (int px = strips[stripNum].start + from;
       px < (strips[stripNum].start + to);
       px++)
  {
    setRGB(stripNum, colors.vals[px % colors.len], px);
  }
}

void getRGB(OSCMessage &msg, CRGBS &colors)
{
  /* get rgb colors */
  int msgSize = msg.size();
  if (msgSize && ((msgSize % 3) != 0))
  {
    //bundleOUT.add("/error/num/rgb/colors");
    return;
  }
  colors.len = msgSize / 3;
  //bundleOUT.add("/numColors").add(colors.len);
  CRGB * vals = (CRGB *) malloc(colors.len * sizeof(CRGB));
  for (int i = 0; i < colors.len; i++)
  {
    //int red = msg.getInt(i * 3);
    //int green = msg.getInt(i * 3 + 1);
    //int blue = msg.getInt(i * 3 + 2);
    //bundleOUT.add("/rgb").add(red).add(green).add(blue);
    //vals[i] = CHSV(red, green, blue);
    vals[i] = CRGB(msg.getInt(i * 3),
                   msg.getInt(i * 3 + 1),
                   msg.getInt(i * 3 + 2));
  }
  colors.vals = vals;
}

/* HSV utilities */

typedef struct {
  CHSV * vals;
  int len;
} CHSVS;

void setHSV(int stripNum, CHSV &color, int px)
{
  int i = ((px % strips[stripNum].length) + strips[stripNum].start);
  leds[i] = color;
}

/* set HSV colors */
void setHSV(int stripNum, CHSVS &colors)
{
  for (int px = strips[stripNum].start;
       px < (strips[stripNum].start + strips[stripNum].length);
       px++)
  {
    setHSV(stripNum, colors.vals[px % colors.len], px);
  }
}

void setHSV(int stripNum, CHSVS &colors, int from, int to)
{
  if ((to - from) > strips[stripNum].length)
  {
     setHSV(stripNum, colors);
     return;
  }
  for (int px = strips[stripNum].start + from;
       px < (strips[stripNum].start + to);
       px++)
  {
    setHSV(stripNum, colors.vals[px % colors.len], px);
  }
}

void getHSV(OSCMessage &msg, CHSVS &colors)
{
  /* get hsv colors */
  int msgSize = msg.size();
  if (msgSize && ((msgSize % 3) != 0))
  {
    //bundleOUT.add("/error/num/hsv/colors");
    return;
  }
  colors.len = msgSize / 3;
  //bundleOUT.add("/numColors").add(colors.len);
  CHSV * vals = (CHSV *) malloc(colors.len * sizeof(CHSV));
  for (int i = 0; i < colors.len; i++)
  {
    //int hue = msg.getInt(i * 3);
    //int sat = msg.getInt(i * 3 + 1);
    //int val = msg.getInt(i * 3 + 2);
    //bundleOUT.add("/hsv").add(hue).add(sat).add(val);
    //vals[i] = CHSV(hue, sat, val);
    vals[i] = CHSV(msg.getInt(i * 3),
                   msg.getInt(i * 3 + 1),
                   msg.getInt(i * 3 + 2));
  }
  colors.vals = vals;
}

/* various led dispatches */

void setRainbow(int stripNum, OSCMessage &msg)
{
  
  // default HSV
  double hue = 0;
  int sat = 255;
  int val = 255;

  int msgSize = msg.size();
  switch (msgSize) {
    case 3:
      val = msg.getInt(2);
    case 2:
      sat = msg.getInt(1);
    case 1:
      hue = fmod(msg.getInt(0), 255); break;
  } 
  
  //bundleOUT.add("/leds/rainbow").add(hue).add(sat).add(val);
  
  int stripStart = strips[stripNum].start;
  int stripLength = strips[stripNum].length;
  int stripEnd = stripStart + stripLength;
  double hueStep = 255.0 / stripLength;
  for (int px = stripStart; px < stripEnd; px++)
  {
    leds[px] = CHSV(round(hue), sat, val);
    hue = fmod(hue + hueStep, 255);
  }
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
  byte addressLen = msg.getAddress(address, ledsOffset);
  //bundleOUT.add("/leds").add(address).add(addressLen);
  
  /* get strip number */
  byte startStrip = msg.match("/strip", ledsOffset);
  byte stripOffset = msg.match("/strip/*", ledsOffset);
  byte stripCharsLen =  stripOffset - startStrip;
  
  char stripChars [stripCharsLen];
  strncpy(stripChars, address + startStrip + 1, stripCharsLen);
  stripChars[stripCharsLen] = '\0';
  
  int stripNum = atoi(stripChars);
  
  //bundleOUT.add("/strip").add(stripNum);
  
  /* get whether all, range, or single pixels */
  
  byte offset = ledsOffset + stripOffset;
  
  if (msg.match("/all", offset))
  {
    byte allOffset = msg.match("/all", offset);
    offset += allOffset;
    
    if (msg.fullMatch("/rgb", offset))
    { 
      CRGBS colors = { NULL, 0 };
      getRGB(msg, colors);
      setRGB(stripNum, colors);
      free(colors.vals);
    }
    else if (msg.fullMatch("/hsv", offset))
    {
      // WTF!?!?!?
      if (msg.getInt(2) == 9058) {
        return;
      }
      CHSVS colors = { NULL, 0 };
      getHSV(msg, colors);
      setHSV(stripNum, colors);
      free(colors.vals);
    }
    else if (msg.fullMatch("/rainbow", offset))
    {
      setRainbow(stripNum, msg);
    }
    else if (msg.fullMatch("/red", offset))
    {
      CRGB vals [] = { CRGB::Red };
      CRGBS colors = { vals, 1 };
      setRGB(stripNum, colors);
      //bundleOUT.add("/red");
    }
    else if (msg.fullMatch("/green", offset))
    {
      CRGB vals [] = { CRGB::Green };
      CRGBS colors = { vals, 1 };
      setRGB(stripNum, colors);
      //bundleOUT.add("/green");
    }
    else if (msg.fullMatch("/blue", offset))
    {
      CRGB vals [] = { CRGB::Blue };
      CRGBS colors = { vals, 1 };
      setRGB(stripNum, colors);
      //bundleOUT.add("/blue");
    }
    return;
  }

  if (msg.match("/one", offset))
  {
    byte startOne = msg.match("/one", offset);
    byte oneOffset = msg.match("/one/*", offset);
    
    byte oneCharsLen =  oneOffset - startOne - 1;
    
    char oneChars [oneCharsLen];
    strncpy(oneChars, address + stripOffset + startOne + 1, oneCharsLen);
    oneChars[oneCharsLen] = '\0';

    int oneIndex = atoi(oneChars);
    
    //bundleOUT.add("/one").add(oneChars).add(oneIndex);
    
    offset += oneOffset;
    
    if (msg.fullMatch("/rgb", offset))
    { 
      CRGBS colors = { NULL, 0 };
      getRGB(msg, colors);
      CRGB color = colors.vals[0];
      setRGB(stripNum, color, oneIndex);
      free(colors.vals);
    }
    else if (msg.fullMatch("/hsv", offset))
    {
      // WTF!?!?!?
      if (msg.getInt(2) == 9058) {
        return;
      }
      CHSVS colors = { NULL, 0 };
      getHSV(msg, colors);
      CHSV color = colors.vals[0];
      setHSV(stripNum, color, oneIndex);
      free(colors.vals);
    }
    return;
  }

  if (msg.match("/from", offset))
  {
    byte startFrom = msg.match("/from", offset);
    byte fromOffset = msg.match("/from/*", offset);
    
    byte fromCharsLen =  fromOffset - startFrom - 1;
    
    char fromChars [fromCharsLen];
    strncpy(fromChars, address + stripOffset + startFrom + 1, fromCharsLen);
    fromChars[fromCharsLen] = '\0';

    int fromIndex = atoi(fromChars);
    
    offset += fromOffset;

    byte startTo = msg.match("/to", offset);
    byte toOffset = msg.match("/to/*", offset);
    
    byte toCharsLen =  toOffset - startTo - 1;
    
    char toChars [toCharsLen];
    strncpy(toChars, address + stripOffset + fromOffset + startTo + 1, toCharsLen);
    toChars[toCharsLen] = '\0';

    int toIndex = atoi(toChars);

    offset += toOffset;
    
    if (msg.fullMatch("/rgb", offset))
    { 
      CRGBS colors = { NULL, 0 };
      getRGB(msg, colors);
      setRGB(stripNum, colors, fromIndex, toIndex);
      free(colors.vals);
    }
    else if (msg.fullMatch("/hsv", offset))
    {
      // WTF!?!?!?
      if (msg.getInt(2) == 9058) {
        return;
      }
      CHSVS colors = { NULL, 0 };
      getHSV(msg, colors);
      setHSV(stripNum, colors, fromIndex, toIndex);
      free(colors.vals);
    }
    return;
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
  SLIPSerial.begin(115200);

  bundleOUT.add("/setup");
  // send the outgoing message
  SLIPSerial.beginPacket();
  bundleOUT.send(SLIPSerial);
  SLIPSerial.endPacket();
  bundleOUT.empty();
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
    if (bundleIN.size() > 0) {
      bundleIN.route("/leds", routeLeds);
      LEDS.show();
    }
    bundleOUT.add("/loop");
  }
  bundleIN.empty();
  
  // send the outgoing message
  SLIPSerial.beginPacket();
  bundleOUT.send(SLIPSerial);
  SLIPSerial.endPacket();
  bundleOUT.empty();
}

/* */
