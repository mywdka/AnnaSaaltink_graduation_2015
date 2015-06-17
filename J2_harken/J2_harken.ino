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
 
#define TEST_LED          13

#define BAUDRATE          19600

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
#define SEEN_THRESHOLD    35     /* TODO: Experiment!! */

#define JACKET_ID_1      0
#define JACKET_ID_2      1
#define JACKET_ID_3      2
#define JACKET_ID_4      3
#define JACKET_ID_LAST   JACKET_ID_4

#define MY_ID        JACKET_ID_2   /* change for jacket */

//#define DEBUG_WRITE /* HOOK RX from Radio module to GND */
//#define DEBUG_READ

unsigned long broadcastDelay;
unsigned long broadcastDelayStartMs;    /* delay between broadcasts */

unsigned long reactivationDelayStartMs[4];
boolean jacketSeen[4];

boolean easyRadioSendCommand(char *bfr)
{
  char rcv[32];
  uint8_t bsize, i=0, n=0;
  
  bsize = sizeof(bfr);
  
  /* we don't need the '\0' */
  if (bfr[bsize-1] == '\0') {
    bsize--;
  }

#ifdef DEBUG_WRITE
  Serial.println(bfr);
#else
  Serial.write(bfr, bsize);
#endif

  /* wait until we have enough data */
  while (Serial.available() < bsize)
    ;
  
  do {
    char c = Serial.readBytes(rcv, 1);
    
    if (c < 0) {
     if (++i > 100) {
       return false;
     }
     continue; 
    }
    
#ifdef DEBUG_READ
    Serial.print(c);
#endif
    if (bfr[n++] != c) {
      return false;
    }
      
  } while (i < bsize);
    
  return true;
}

void setup() {
  pinMode(TEST_LED, OUTPUT);
  
  Serial.begin(BAUDRATE);
  Serial.setTimeout(200);
  
  /* send command to tell the radio module to prepand an RSSI byte */
#if 0
  if (easyRadioSendCommand("ER_CMD#a01") == true) {
    /* All good, blink once */
    digitalWrite(TEST_LED, HIGH);
    delay(1000);
    digitalWrite(TEST_LED, LOW);    
  } else {
    /* ERROR, go into blink quickly 5 times */
    for (int i=0; i<5; i++) {
      digitalWrite(TEST_LED, HIGH);
      delay(100);
      digitalWrite(TEST_LED, LOW); 
      delay(50);
    }
  }
#else
  digitalWrite(TEST_LED, HIGH);
  delay(1000);
  digitalWrite(TEST_LED, LOW);
#endif
  
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
        Serial.print("Invalid jacket: ");
        Serial.println(packet[1], DEC);
#endif
      }
    }
  }
  
  currentMillis = millis();
  /* if it has been long enough te retrigger pretend we did not see the jacket with ID i [0..4] before */
  for (i=0; i<=JACKET_ID_LAST; i++) {
    if (reactivationDelayStartMs[i] > 0 && currentMillis - reactivationDelayStartMs[i] > REACTIVATE_DELAY) {
      reactivationDelayStartMs[i] = 0; /* set reactivationDelay back to 0 */
      jacketSeen[i] = false; /* indicate we have not seen this jacket before */
#ifdef DEBUG_READ
      Serial.print("reactivating jacket: ");
      Serial.println(i);
#endif
    }
  }

  /* What to do if there is a valid package */
  if (have_valid_packet) {
    digitalWrite(TEST_LED, HIGH);
    delay(50);
    digitalWrite(TEST_LED, LOW);
        
#ifdef DEBUG_READ
      Serial.print("JACKET: ");
      Serial.print(packet[1]);
      Serial.print(": ");
      Serial.println((unsigned long)packet[0], DEC);
#endif

    /* is the jacket who sent the message close enough? */
    if (packet[0] > SEEN_THRESHOLD) {
#ifdef DEBUG_READ
      Serial.print("Jacket ");
      Serial.print(packet[1]);
      Serial.println(" in range");
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
      Serial.print("Jacket ");
      Serial.print(packet[1]);
      Serial.println(" out of range");
#endif
    }
  }
  
  /* do the things if the jacket is close enough AND has not been reacted to yet 
     (reactivationDelay will have a value bigger than 0) */
  if (jacketSeen[JACKET_ID_1] && reactivationDelayStartMs[JACKET_ID_1] == 0) {
    /* do jacket stuff */
    reactivationDelayStartMs[JACKET_ID_1] = millis();
#ifdef DEBUG_READ
    Serial.println("JACKET_1 ACTIVE");
#endif
  }
  
  if (jacketSeen[JACKET_ID_2] && reactivationDelayStartMs[JACKET_ID_2] == 0) {
    /* do jacket stuff */
    reactivationDelayStartMs[JACKET_ID_2] = millis();
#ifdef DEBUG_READ
    Serial.println("JACKET_2 ACTIVE");
#endif
  }
  
  if (jacketSeen[JACKET_ID_3] && reactivationDelayStartMs[JACKET_ID_3] == 0) {
    /* do jacket stuff */
    reactivationDelayStartMs[JACKET_ID_3] = millis();
#ifdef DEBUG_READ
    Serial.println("JACKET_3 ACTIVE");
#endif
  }
  
  if (jacketSeen[JACKET_ID_4]) {
    if (reactivationDelayStartMs[JACKET_ID_4] == 0) {
      /* do jacket stuff */
      reactivationDelayStartMs[JACKET_ID_4] = millis();
#ifdef DEBUG_READ
      Serial.println("JACKET_4 ACTIVE"); 
#endif
      digitalWrite(TEST_LED, HIGH);
      delay(100);
      digitalWrite(TEST_LED, LOW);
      delay(100);
      digitalWrite(TEST_LED, HIGH);
      delay(100);
      digitalWrite(TEST_LED, LOW);
      delay(100);
      digitalWrite(TEST_LED, HIGH);
      delay(100);
      digitalWrite(TEST_LED, LOW);
      delay(100);
      digitalWrite(TEST_LED, HIGH);
      delay(100);
      digitalWrite(TEST_LED, LOW);
      delay(100);
      digitalWrite(TEST_LED, HIGH);
    }
  } else {
      digitalWrite(TEST_LED, LOW);
  }
}
