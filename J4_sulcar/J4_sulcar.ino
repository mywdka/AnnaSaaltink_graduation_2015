/**
 * @file: ERA_Proximity_detection.ino
 * @author: Simon de Bakker <s.j.de.bakker@hr.nl> or <simon@simbits.nl>
 *
 * An example of using the Easy Radio Advanced TRS modules as way to detect and identify
 * object proximity.
 *
 * The ERA modules can be configured to prepend an RSSI byte to the received data payload.
 * This information can be used to see if the transmitting object is within a certain range of
 * the receiver. This is not a definite measure as orientation, movement and objects wihtin the
 * line of sight of the transmitter and receiver can significantly alter the signal strength.
 *
 * This example will trigger an action based on the object ID in range. It will retrigger after
 * REACTIVATE_DELAY milliseconds, or after the object has been out of range first.
 *
 * The RSSI value will need to be determined by experimentation. An indication on the received
 * value and the signal strength can be found in the datasheet (see link below). The min/max value
 * depends on the transmition band. To change the trigger threshold change SEEN_THRESHOLD.
 *
 * @link: http://www.lprs.co.uk/assets/media/downloads/easyRadio%20Advanced%20Datasheet.pdf
 */
#include <Adafruit_NeoPixel.h>

#define NEOPIXEL_PIN        9
#define NUMPIXELS           16

#define TEST_LED            13

#define BAUDRATE            19600

#define BROADCAST_1_DELAY   1100   /* 1.00s */
#define BROADCAST_2_DELAY   1150   /* 1.15s */
#define BROADCAST_3_DELAY   1200   /* 1.20s */
#define BROADCAST_4_DELAY   1250   /* 1.25s */

/* This delay determines after how long the jacket will react again on another jacket */
#define REACTIVATE_DELAY    10000  /* 10s */

/* The Received Signal Strength Indication or RSSI value on which we base
   our "proximity" detection. With each packet the RSSI value is prepended to the payload.
   Important to realise is that this is an indicative value. It is not possible to use this value
   as an absolute measure of distance. It depends on a lot of factors like orientation with respect to the sender,
   objects in between, movement etc.). However, taking these limitations into account and by using a simple threshold
   we are able to use it as a crude indication of relative closeness to the receiver. */
#define SEEN_THRESHOLD    15     /* TODO: Experiment!! */

#define JACKET_ID_1      0
#define JACKET_ID_2      1
#define JACKET_ID_3      2
#define JACKET_ID_4      3
#define JACKET_ID_LAST   JACKET_ID_4

#define MY_ID        JACKET_ID_4   /* change for jacket */

#define NEO_RED     0x00ff0000
#define NEO_GREEN   0x0000ff00
#define NEO_BLUE    0x000000ff

typedef struct _WipeStc {
    int i;
    uint32_t color;
    unsigned long msdelay;
    unsigned long start;
} wipeStc;

typedef struct _colorFade {
    int i;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    unsigned long msdelay;
    unsigned long start;
} colorFadeStc;

typedef enum jacketLedStateEnum {
  STATE_NONE = 0,
  STATE_J1_LED_WIPE_START,
  STATE_J1_LED_WIPE_WORKING,
  STATE_J1_LOST,
  
  STATE_J2_LED_WIPE_START,
  STATE_J2_LED_WIPE_WORKING,
  STATE_J2_LOST,
  
  STATE_J3_LED_WIPE_START,
  STATE_J3_LED_WIPE_WORKING,
  STATE_J3_LOST,
  
  STATE_LED_DONE,
  STATE_END
} jacket_led_state;

jacket_led_state    currentLedState = STATE_NONE;
jacket_led_state    previousLedState;

colorFadeStc color_fade_stc;
wipeStc wipe_stc;

//#define DEBUG_WRITE /* HOOK RX from Radio module to GND */
#define DEBUG_READ

Adafruit_NeoPixel strip = Adafruit_NeoPixel(16, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
int delayval = 50; // delay for lightshow


unsigned long broadcastDelay;
unsigned long broadcastDelayStartMs;    /* delay between broadcasts */

unsigned long reactivationDelayStartMs[4];
boolean jacketSeen[4];

static void setNextLedState(uint8_t state)
{
  previousLedState = currentLedState;
  currentLedState = (jacket_led_state)state; 
}

static inline void broadcast() {//we put in a separate function to keep broadcasting during lightshows
  /* check if it is already time to broadcast our ID */
  if (millis() - broadcastDelayStartMs >= broadcastDelay) {
    /* The message we send is 2 bytes: and JACKET_ID and the newline: '\n' character */
    /* The message that will be received is 3!! bytes. The first byte is added by the radio module
       and is the signal strength indication of the received message */
    char bfr[2] = { MY_ID, '\n' };

#ifndef DEBUG_WRITE
    Serial.write(bfr, 2);
#else
    Serial.print("Sending: ");
    Serial.write(bfr, 2);
#endif

    broadcastDelayStartMs = millis();
  }

}

static void determineNextLedState(void)
{
  
  if (currentLedState == STATE_NONE) {
    if (jacketSeen[JACKET_ID_1]) {
      setNextLedState(STATE_J1_LED_WIPE_START);
    } else if (jacketSeen[JACKET_ID_2]) {
      setNextLedState(STATE_J2_LED_WIPE_START);
    } else if (jacketSeen[JACKET_ID_3]) {
      setNextLedState(STATE_J3_LED_WIPE_START);
    }
  }
  
  if (currentLedState == STATE_LED_DONE) {
    if (previousLedState == STATE_J1_LED_WIPE_WORKING) {
      if (jacketSeen[JACKET_ID_2]) {
        setNextLedState(STATE_J2_LED_WIPE_START);
      } else if (jacketSeen[JACKET_ID_3]) {
        setNextLedState(STATE_J3_LED_WIPE_START);
      } else if (jacketSeen[JACKET_ID_1]) {
        setNextLedState(STATE_J1_LED_WIPE_START);
      }
    } else if (previousLedState == STATE_J2_LED_WIPE_WORKING) {
      if (jacketSeen[JACKET_ID_3]) {
        setNextLedState(STATE_J3_LED_WIPE_START);
      } else if (jacketSeen[JACKET_ID_1]) {
        setNextLedState(STATE_J1_LED_WIPE_START);
      } else if (jacketSeen[JACKET_ID_2]) {
        setNextLedState(STATE_J2_LED_WIPE_START);
      }
    } else if (previousLedState == STATE_J3_LED_WIPE_WORKING) {
      if (jacketSeen[JACKET_ID_1]) {
        setNextLedState(STATE_J1_LED_WIPE_START);
      } else if (jacketSeen[JACKET_ID_2]) {
        setNextLedState(STATE_J2_LED_WIPE_START);
      } else if (jacketSeen[JACKET_ID_3]) {
        setNextLedState(STATE_J3_LED_WIPE_START);
      }
    } else {
      setNextLedState(STATE_J1_LOST);
    }
  }
}

void setup() {

  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

  pinMode(TEST_LED, OUTPUT);

  Serial.begin(BAUDRATE);
  Serial.setTimeout(200);

  digitalWrite(TEST_LED, HIGH);
  delay(1000);
  digitalWrite(TEST_LED, LOW);

  switch (MY_ID) {
    case JACKET_ID_1:
      broadcastDelay = BROADCAST_1_DELAY;
      break;
    case JACKET_ID_2:
      broadcastDelay = BROADCAST_2_DELAY;
      break;
    case JACKET_ID_3:
      broadcastDelay = BROADCAST_3_DELAY;
      break;
    case JACKET_ID_4:
    default:
      broadcastDelay = BROADCAST_4_DELAY;
  }
  broadcastDelayStartMs = millis();
}

void loop() {
  uint8_t packet[3] = {0, 0, 0}; // holds the received package if the have_valid_packet flags is true */
  boolean have_valid_packet = false; // if this flag is true we received a valid message from one of the jackets */
  unsigned long currentMillis = millis();
  int i;

  broadcast();

  /* Data arrived from the radio module */
  if (Serial.available() >= 3) {
    /* Read at most 3 bytes */
    uint8_t n = Serial.readBytes(packet, 3);

#ifdef DEBUG_READ
    Serial.print("GOT: ");
    Serial.print(packet[0], DEC);
    Serial.print(", ");
    Serial.print(packet[1], DEC);
    Serial.print(", ");
    Serial.println(packet[2], HEX);
    Serial.println("-");
#endif

    /* The message is exactly 3 bytes long, if the message arrives half we just discard it as invalid*/
    /* The 3d and last byte (index 2) needs to be the newline character: '\n' */
    if (n == 3 && packet[2] == '\n') {
      /* The jacket id is the 2nd byte (index 1), it must be a number between the first and last JACKET_ID */
      if (packet[1] >= 0 && packet[1] <= JACKET_ID_LAST) {
        /* mark we received a valid packet */
        have_valid_packet = true;
      } else {
#ifdef DEBUG_READ
        Serial.print("Invalid jacket: "); Serial.println(packet[1], DEC);
#endif
      }
    }
  }

  currentMillis = millis();
  /* if it has been long enough te retrigger pretend we did not see the jacket with ID i [0..4] before */
  for (i = 0; i <= JACKET_ID_LAST; i++) {
    if (reactivationDelayStartMs[i] > 0 && currentMillis - reactivationDelayStartMs[i] > REACTIVATE_DELAY) {
      reactivationDelayStartMs[i] = 0; /* set reactivationDelay back to 0 */
      jacketSeen[i] = false; /* indicate we have not seen this jacket before */
#ifdef DEBUG_READ
      Serial.print("reactivating jacket: "); Serial.println(i);
#endif
    }
  }

  /* What to do if there is a valid package */
  if (have_valid_packet) {
    digitalWrite(TEST_LED, HIGH);
    delay(50);
    digitalWrite(TEST_LED, LOW);

#ifdef DEBUG_READ
    Serial.print("JACKET: "); Serial.print(packet[1]); Serial.print(": "); Serial.println((unsigned long)packet[0], DEC);
#endif

    /* is the jacket who sent the message close enough? */
    if (packet[0] > SEEN_THRESHOLD) {
#ifdef DEBUG_READ
      Serial.print("Jacket "); Serial.print(packet[1]); Serial.println(" in range");
#endif
      /* did we see it already? */
      if ( ! jacketSeen[packet[1]]) {
        /* if the jacket who send this message is close enough indicate we saw it */
        jacketSeen[packet[1]]  = true;
      }
    } else {
      /* if the jacket who send this message is _not_ close enough */
      jacketSeen[packet[1]] = false;
      reactivationDelayStartMs[packet[1]] = 0;
#ifdef DEBUG_READ
      Serial.print("Jacket "); Serial.print(packet[1]); Serial.println(" out of range");
#endif
    }
  }
  
#ifdef DEBUG_READ
  for (int k=0; i<JACKET_ID_LAST; k++) {
    Serial.print("JACKET ");
    Serial.print(k+1);
    Serial.println((jacketSeen[k]) ? ": ACTIVE" : ": LOST");
  }
#endif

  determineNextLedState();

#if 0  
  /* do the things if the jacket is close enough AND has not been reacted to yet
     (reactivationDelay will have a value bigger than 0) */
  if (jacketSeen[JACKET_ID_1]) {
    if (reactivationDelayStartMs[JACKET_ID_1] == 0) {
      reactivationDelayStartMs[JACKET_ID_1] = millis();
  #ifdef DEBUG_READ
      Serial.println("JACKET_1 ACTIVE");
  #endif 
      setNextLedState(STATE_J1_LED_WIPE_START);
    }
  } else {
    if (currentLedState == STATE_LED_DONE) {
      wipe_stc.color = 0;
      wipe_stc.i = 0;
      setNextLedState(STATE_J1_LOST);
#ifdef DEBUG_READ
      Serial.println("JACKET_1 LOST");
#endif
    }
  }

  if (jacketSeen[JACKET_ID_2]) {
    if (reactivationDelayStartMs[JACKET_ID_2] == 0) {
      reactivationDelayStartMs[JACKET_ID_2] = millis();
  #ifdef DEBUG_READ
      Serial.println("JACKET_2 ACTIVE");
  #endif
      for (int teller = 0; teller < 255; teller++) {
        for (int i = 0; i < NUMPIXELS; i++) {
          // strip.Color takes RGB values, from 0,0,0 up to 255,255,255
          strip.setPixelColor(i, strip.Color(255 - teller, 0, teller));
          strip.show(); // This sends the updated pixel color to the hardware.
        }
        delay(10); // Delay for a period of time (in milliseconds).
        broadcast(); //keep broadcasting
      }
      for (int i = 0; i < NUMPIXELS; i++) {
        strip.setPixelColor(i, strip.Color(0, 0, 0)); // Moderately bright green color.
        strip.show(); // This sends the updated pixel color to the hardware.
      }
  
    } 
  } else {
    if (currentLedState == STATE_LED_DONE) {
      wipe_stc.color = 0;
      wipe_stc.i = 0;
      setNextLedState(STATE_J2_LOST);
#ifdef DEBUG_READ
      Serial.println("JACKET_2 LOST");
#endif
    }
  }

  if (jacketSeen[JACKET_ID_3]) {
    if (reactivationDelayStartMs[JACKET_ID_3] == 0) {
      reactivationDelayStartMs[JACKET_ID_3] = millis();
  #ifdef DEBUG_READ
      Serial.println("JACKET_3 ACTIVE");
  #endif
     for (int teller = 0; teller < 255; teller++) {
        for (int i = 0; i < NUMPIXELS; i++) {
          // strip.Color takes RGB values, from 0,0,0 up to 255,255,255
          strip.setPixelColor(i, strip.Color(0, 255 - teller, teller));
          strip.show(); // This sends the updated pixel color to the hardware.
        }
        delay(10); // Delay for a period of time (in milliseconds).
        broadcast(); //keep broadcasting
      }
        for (int i = 0; i < NUMPIXELS; i++) {
          strip.setPixelColor(i, strip.Color(0,0,0)); // Moderately bright green color.
          strip.show(); // This sends the updated pixel color to the hardware.
      }
    } 
  } else {
    if (currentLedState == STATE_LED_DONE) {
      wipe_stc.color = 0;
      wipe_stc.i = 0;
      setNextLedState(STATE_J3_LOST);
#ifdef DEBUG_READ
      Serial.println("JACKET_3 LOST");
#endif
    }
  }
#endif  
 
  switch (currentLedState) {
    case STATE_NONE:
        break;
    case STATE_J1_LED_WIPE_START:
        {
            wipe_stc.i = 0;
            wipe_stc.color = NEO_RED;
            wipe_stc.msdelay = 25;
            wipe_stc.start = millis();
            setNextLedState(STATE_J1_LED_WIPE_WORKING);
        }
    case STATE_J1_LED_WIPE_WORKING:
        {
            if (millis() - wipe_stc.start < wipe_stc.msdelay) {
                break;
            }

            strip.setPixelColor(wipe_stc.i++, wipe_stc.color);
            strip.show();
            
            if (wipe_stc.i >= strip.numPixels()) {
                if (wipe_stc.color == NEO_RED) {
                    wipe_stc.color = NEO_GREEN;
                    wipe_stc.i = 0;
                } else if (wipe_stc.color == NEO_GREEN) {
                    wipe_stc.color = NEO_BLUE;
                    wipe_stc.i = 0;
                } else {
                    setNextLedState(STATE_LED_DONE);
                }
            }

            wipe_stc.start = millis();
        }
        break;
      
    case STATE_J2_LED_WIPE_START:
        {
          color_fade_stc.i = 0; 
          color_fade_stc.r = 255;
          color_fade_stc.g = 0;
          color_fade_stc.b = 255;
          color_fade_stc.msdelay = 50;
          color_fade_stc.start = millis();
        }
    case STATE_J2_LED_WIPE_WORKING:
        {
            if (millis() - color_fade_stc.start < color_fade_stc.msdelay) {
                break;
            }

            strip.setPixelColor(color_fade_stc.i++, color_fade_stc.color);
            strip.show();
            
            if (co

            if (color_fade_stc.i >= strip.numPixels()) {
                if (wipe_stc.color == NEO_RED) {
                    wipe_stc.color = NEO_GREEN;
                    wipe_stc.i = 0;
                } else if (wipe_stc.color == NEO_GREEN) {
                    wipe_stc.color = NEO_BLUE;
                    wipe_stc.i = 0;
                } else {
                    setNextLedState(STATE_LED_DONE);
                }
            }

            color_fade_stc.start = millis();
        }
        break;      
    case STATE_J3_LED_WIPE_START:
    case STATE_J3_LED_WIPE_WORKING:
  
    case STATE_LED_DONE:
        break;
        
    case STATE_J1_LOST:
    case STATE_J2_LOST:
    case STATE_J3_LOST:
        {
            if (millis() - wipe_stc.start < wipe_stc.msdelay) {
                break;
            }

            strip.setPixelColor(wipe_stc.i++, wipe_stc.color);
            strip.show();

            if (wipe_stc.i >= strip.numPixels()) {
              setNextLedState(STATE_NONE);
            }

            wipe_stc.start = millis();        
        }
        break;

    case STATE_END:
    default:
        setNextLedState(STATE_NONE);
  } 
}

// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(wait);
    broadcast();
  }
}

void rainbow(uint8_t wait) {
  uint16_t i, j;

  for (j = 0; j < 256; j++) {
    for (i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel((i + j) & 255));
    }
    strip.show();
    delay(wait);
    broadcast();
  }
}

// Slightly different, this makes the rainbow equally distributed throughout
void rainbowCycle(uint8_t wait) {
  uint16_t i, j;

  for (j = 0; j < 256 * 5; j++) { // 5 cycles of all colors on wheel
    for (i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
    }
    strip.show();
    delay(wait);
    broadcast();
  }
}

//Theatre-style crawling lights.
void theaterChase(uint32_t c, uint8_t wait) {
  for (int j = 0; j < 10; j++) { //do 10 cycles of chasing
    for (int q = 0; q < 3; q++) {
      for (int i = 0; i < strip.numPixels(); i = i + 3) {
        strip.setPixelColor(i + q, c);  //turn every third pixel on
      }
      strip.show();

      delay(wait);
      broadcast();
      for (int i = 0; i < strip.numPixels(); i = i + 3) {
        strip.setPixelColor(i + q, 0);      //turn every third pixel off
      }
    }
  }
}

//Theatre-style crawling lights with rainbow effect
void theaterChaseRainbow(uint8_t wait) {
  for (int j = 0; j < 256; j++) {   // cycle all 256 colors in the wheel
    for (int q = 0; q < 3; q++) {
      for (int i = 0; i < strip.numPixels(); i = i + 3) {
        strip.setPixelColor(i + q, Wheel( (i + j) % 255)); //turn every third pixel on
      }
      strip.show();

      delay(wait);
      broadcast();
      for (int i = 0; i < strip.numPixels(); i = i + 3) {
        strip.setPixelColor(i + q, 0);      //turn every third pixel off
      }
    }
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else if (WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  } else {
    WheelPos -= 170;
    return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  }
}

