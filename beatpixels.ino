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

/* various led dispatches */

void dispatchRGB(OSCMessage &msg)
{
  int red = msg.getInt(0);
  int green = msg.getInt(1);
  int blue = msg.getInt(2);
  LEDS.showColor(CRGB(red, green, blue));
  bundleOUT.add("/leds/rgb").add(red).add(green).add(blue);
}

void dispatchRainbow(OSCMessage &msg)
{
  static uint8_t startingHue = 0;
  startingHue -= 1;
  fill_rainbow(leds, NUM_LEDS, startingHue);
  bundleOUT.add("/leds/rainbow");
}
/*
/leds
  /strip/[0-9]
    /from/[0-9]/to/[0-9]
    /all
      /rgb
      /rainbow
      /hsv
        color1, color2, ...
      
/* main controller for led routes */
void routeLeds(OSCMessage &msg, int ledsOffset)
{
  bundleOUT.add("/routeLeds");

  if (msg.fullMatch("/rgb", ledsOffset)) {
    msg.dispatch("/rgb", dispatchRGB, ledsOffset);
  } else if (msg.fullMatch("/rainbow", ledsOffset)) {
    msg.dispatch("/rainbow", dispatchRainbow, ledsOffset);
  } else if (msg.fullMatch("/red", ledsOffset)) {
    LEDS.showColor(CRGB::Red);
    bundleOUT.add("/leds/red");
  } else if (msg.fullMatch("/green", ledsOffset)) {
    LEDS.showColor(CRGB::Green);
    bundleOUT.add("/leds/green");
  } else if (msg.fullMatch("/blue", ledsOffset)) {
    LEDS.showColor(CRGB::Blue);
    bundleOUT.add("/leds/blue");
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
