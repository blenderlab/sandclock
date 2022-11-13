// Sandclock for ESP8266
// V1.6 -  October 2021
// Blenderlab (http://www.blenderlab.fr / http://www.asthrolab.fr)
// Fab4U (http://www.fab4U.de)
//
// Hardware needed :
// ESP8266
// 2 Led Matrix with MAX7219 controller
// Wires, powersupply ...;-D
//
//
// Connect the first matrix to the arduino (5 pins, as follow)
// Connect (chain) the second matrix to the first one : All same pins except (DOUT->DIN)
// Taht's it !
//
// Matrix pinout
// CS = D8
// CL = D5
// DI = D7
//
// SW350D : 12 & GND (Vibration sensor) - (We use internal pullup)

#ifndef STASSID
#define STASSID "LaCabane"
#define STAPSK  "123NousIronsAuBois"
#endif
#define MY_NTP_SERVER "fr.pool.ntp.org"
#define MY_TZ "CET-1CEST,M3.5.0,M10.5.0/3"
/* Globals */
time_t now;                         // this is the epoch
tm tm;                              // the structure tm holds time information in a more convient way

#define MAX_DEVICES 2

#define CLK_PIN D5 // or SCK
#define DATA_PIN D7 // or MOSI
#define CS_PIN D8 // or SS


#include <MD_MAX72xx.h> // Available in Arduino repo
#include <SPI.h>
#include "font_diagonal.h";
// Date and time functions using a DS1307 RTC connected via I2C and Wire lib
#include <ESP8266WiFi.h>
#include <time.h>                   // time() ctime()


#define HARDWARE_TYPE MD_MAX72XX::ICSTATION_HW //edit this as per your LED matrix hardware type
MD_MAX72XX matrix = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

#define NB_TRYWIFI        30    // Nbr de tentatives de connexion au réseau WiFi | Number of try to connect to WiFi network

#define HAUT 0
#define BAS 1
#define LEDON 1
#define LEDOFF 0
#define SHAKE 12 // Vibration sensor pin

#define MODE 2
// 1 = normal - random length loop
// 2 = 1 minute per loop.

int wait = 20; // In milliseconds
int ln = 5; // Filling level of the globe
// One second between each marble drop :
int htimeout = 1000;

// Timer for benchmarking
unsigned long startms = millis();

// Variable for the timing :
unsigned long timer = 0;

//  Timeout for a next-grain drop
int next_grain = 0;

//Array of pixels (2 matrices of 8x8)
int pixels[8][16];

// Emptying led array :
void cleanup_pixels() {
  for (int i = 0; i < 8 ; i++) {
    for (int j = 0; j < 16; j++) {
      pixels[i][j] = 0;
    }
  }
}

// Filling the Upper glass with some sand :
void fill_hourglass(int niv = 5, int globe = HAUT) {
  cleanup_pixels();
  byte minu = 0;
  niv = 0;

#if MODE==1

  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      if ((x + y) > niv) {
        set_pixel(x, y, globe, LEDON);
        //draw_hourglass();
        minu++;
        delay(10);
      }
    }
  }

# else
  while (minu <= 48) {
    int x = random(8);
    int y = random(8);
    if (get_pixel(x, y, HAUT) == LEDOFF) {
      set_pixel(x, y, HAUT, LEDON);
      minu++;
      draw_hourglass();
      delay(10);
    }
  }



# endif
  Serial.println("====");
  timer = millis();
}

// cleanupp the lower glass slowly  :
void empty_hourglass( int globe = BAS) {
  for (int i = 0; i <= 256; i++) {
    set_pixel(random(8), random(8), random(2), LEDOFF);
    animer(random(8), random(8) , 1);
    draw_hourglass();
    delay(1);
  }

}

// Routine to print the led array on the matrix
void draw_hourglass() {
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 16; j++) {
      matrix.setPoint(i, j, pixels[i][j]);
    }
  }
}

// Routine to move 1 pixel from X1/Y1 to X2/Y2 in the same globe :
int move_grain(int x1, int y1, int x2, int y2, int globe) {
  if (get_pixel(x2, y2, globe) == LEDOFF) {   // there's no pixel here !
    set_pixel(x1, y1,   globe, LEDOFF);
    set_pixel(x2, y2, globe, LEDON);
    //delay(1); // Wait a bit, it's so cute !
    return (1);
  }
  return (0);
}

// Routine to get the led's state :
int get_pixel(int x, int y, int globe) {
  if ((x > 7) || (y > 7)) {
    return (255); // Outcote scope... sorry !
  }

  if (globe == BAS) { // Adding ofset for the lower matrix :
    y += 8;
  }
  return ( pixels[x][y] );
}

// Setting led to selected state :
// X,Y,HAUT/BAS,ON/OFF
void set_pixel(int x, int y, int globe, const int allume) {
  if ((x > 7) || (y > 7)) {
    return;
  }
  if ((x < 0) || (y < 0)) {
    return;
  }
  if (globe == BAS) {
    y += 8; // Offset for the lower matrix
  }
  pixels[x][y] = allume;

}

// How pixels move (moving the pixel at X,Y from the HAUT/BAS globe)
int animer(int x, int y, int globe) {
  static int  cote = 0; // From the left ? To the right ...
  static int  gs = 0; // Number of grains that were NOT moved (fails)

  // trying not to move pixels away from the matrix :
  x = constrain(x, 0, 7);
  y = constrain(y, 0, 7);

  // If pixel is OFF : nothing to do & add a fail to counter :
  if (get_pixel(x, y, globe) == LEDOFF) {
    gs++;
    return (gs);
  }

  // If we managed to move the pixel by 1 pixel on X & Y( vertical move ) : reset fail counter :
  if ( move_grain(x, y, x + 1, y + 1, globe) ) {
    gs = 0;
    return (gs);
  }

  // If we didn't managed last move, trying to move on the side :
  if (cote) {
    cote ^= 1;    // change cote & trying to move :
    if ( move_grain(x, y, x + 1, y, globe) ) {
      gs = 0;   // reset failing counter
      return (gs);
    } // Other side :
    if ( move_grain(x, y, x, y + 1, globe) ) {
      gs = 0;   // reset failing counter
      return (gs);
    }
  }
  else {
    cote ^= 1;    // change cote
    if ( move_grain(x, y, x, y + 1, globe) ) {
      gs = 0;   // reset failing counter
      return (gs);
    }
    if ( move_grain(x, y, x + 1, y, globe) ) {
      gs = 0;   // reset failing counter
      return (gs);
    }
  }
  // If we try to move a pixel from the lower globe : failing again.
  if (globe == BAS) {
    gs++;
  }
  else {
    gs = 0;
  }
  // Setting the pixel to the newer position :
  set_pixel(x, y, globe, LEDON);

  // Drawing the result :
  draw_hourglass();
  return (gs);
}

// Removing the last pixel of the upper globe, to the first of the lower :
void drop() {
  if (get_pixel(7, 7, HAUT) == LEDOFF) {
    return;
  }
  if (get_pixel(0, 0, BAS) == LEDOFF) {
    set_pixel(7, 7, HAUT, LEDOFF);
    set_pixel(0, 0, BAS, LEDON);
  }
  draw_hourglass();
}

// Function to draw integers on the screen
void drawInt(int16_t x, int16_t y, int c,  byte globe) {
  for (int8_t i = 0 ; i < 8; i++ ) {
    uint8_t line;
    line = pgm_read_byte(font + (i + 8 * c));
    for (int8_t j = 0; j < 8; j++, line >>= 1 ) {
      if (line & 0x1) {
        set_pixel(x + j, y + 8 - i, globe, LEDON);
      }
    }
  }
}

// Function tha display current time
void displaytime() {
 time(&now);                       // read the current time
  localtime_r(&now, &tm);           // update the structure tm with the current time
  // Get current ime
  int ho = (tm.tm_hour);
  int mi = (tm.tm_min);
  // Moving from out the screen to the center :
  for (int k = 8; k >= 0; k--) {
    cleanup_pixels();
    drawInt(k, -k, ho / 10, HAUT);
    drawInt(k, -k, ho - (ho / 10) * 10, BAS);
    draw_hourglass();
    delay(100);
  }
  delay(1000);

  // Moving the hours out & let the minutes come in :
  for (int k = 0; k >= -8; k--) {
    cleanup_pixels();
    drawInt(k, -k, ho / 10, HAUT);
    drawInt(k, -k, ho - (ho / 10) * 10, BAS);
    drawInt(k + 8,  -k - 8, mi / 10, HAUT);
    drawInt(k + 8,  -k - 8, mi - (mi / 10) * 10, BAS);
    draw_hourglass();
    delay(100);
  }
  delay(1000);

  // Push the minutes out :
  for (int k = 0; k >= -8; k--) {
    cleanup_pixels();
    drawInt(k, -k, mi / 10, HAUT);
    drawInt(k, -k, mi - (mi / 10) * 10, BAS);
    draw_hourglass();
    delay(100);
  }

}

uint32_t sntp_startup_delay_MS_rfc_not_less_than_60000 () {
  randomSeed(A0);
  return random(5000);
}

// Setu p :What elese ?
void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(A0)); // Initialize random generator
  matrix.begin();
  matrix.control(MD_MAX72XX::INTENSITY, 0);

  int _try = 0;
  configTime(MY_TZ, MY_NTP_SERVER);


  // start network
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(STASSID, STAPSK);
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print ( "." );
  }
  Serial.println("\nWiFi connected");

  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  time(&now);                       // read the current time
  localtime_r(&now, &tm);           // update the structure tm with the current time
  fill_hourglass(ln, HAUT); // First refill
  startms = millis(); // init counter
}

void reset_hourglass() {
  empty_hourglass();
  ln = random(8) + 1;
  displaytime();
  fill_hourglass(ln);
  startms = millis();
  drop();
}

// Main loop :
void loop() {
  // TIme since poweron :
  long nowms = millis();

  // timeout reached ?  Dropping 1 marble :
  if ((nowms - startms) >= htimeout) {
    startms = nowms;
    drop();
  }
  if (digitalRead(12) == HIGH) { // If vibration recorded : reset !
    reset_hourglass();
  }

  // If we cannot manage to move 128 pixels, considering that the globe is empty.
  if (animer(random(8), random(8) , random(2) ) > 512) {
    reset_hourglass();
  }
}
