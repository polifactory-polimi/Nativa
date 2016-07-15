/* RGB web server with ESP8266-01
* only 2 GPIOs available: 0 and 2
* but RX and TX can also be used as: 3 and 1
* we use 0=red 2=green 3=blue
* analogWrite with values received from web page
*
* web server with captive portal works but better use fixed domain: http://rgb
* web page returns POST request with 3 RGB parameters
* web page inspired by https://github.com/dimsumlabs/nodemcu-httpd
* Serial Monitor for debugging but interferes with Blue channel GPIO3=RX
* switch off Serial for full RGB
*/

#include <MMA_7455.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>

#include "MovingAverage.h"

#define redPin D5
#define greenPin D6
#define bluePin D7


// Value of Z axis lower than this shuts down the light
#define Zmin 40

const char *ssid = "Nativa";
const char *password = "summerschool";
const uint8_t step_color = 4;

int16_t z10, r, g, b;

MovingAverage<int16_t, 32> moving_average;

/* Case 1: Accelerometer on the I2C bus (most common) */
MMA_7455 accel = MMA_7455(i2c_protocol);

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 1, 1);
DNSServer dnsServer;
ESP8266WebServer webServer(80);

String webpage = ""
"<!DOCTYPE html><html><head><title>RGB control</title><meta name='mobile-web-app-capable' content='yes' />"
"<meta name='viewport' content='width=device-width' /></head><body style='margin: 0px; padding: 0px;'>"
"<canvas id='colorspace'></canvas></body>"
"<script type='text/javascript'>"
"(function () {"
" var canvas = document.getElementById('colorspace');"
" var ctx = canvas.getContext('2d');"
" function drawCanvas() {"
" var colours = ctx.createLinearGradient(0, 0, window.innerWidth, 0);"
" for(var i=0; i <= 360; i+=10) {"
" colours.addColorStop(i/360, 'hsl(' + i + ', 100%, 50%)');"
" }"
" ctx.fillStyle = colours;"
" ctx.fillRect(0, 0, window.innerWidth, window.innerHeight);"
" var luminance = ctx.createLinearGradient(0, 0, 0, ctx.canvas.height);"
" luminance.addColorStop(0, '#ffffff');"
" luminance.addColorStop(0.05, '#ffffff');"
" luminance.addColorStop(0.5, 'rgba(0,0,0,0)');"
" luminance.addColorStop(0.95, '#000000');"
" luminance.addColorStop(1, '#000000');"
" ctx.fillStyle = luminance;"
" ctx.fillRect(0, 0, ctx.canvas.width, ctx.canvas.height);"
" }"
" var eventLocked = false;"
" function handleEvent(clientX, clientY) {"
" if(eventLocked) {"
" return;"
" }"
" function colourCorrect(v) {"
" return Math.round(1023-(v*v)/64);"
" }"
" var data = ctx.getImageData(clientX, clientY, 1, 1).data;"
" var params = ["
" 'r=' + colourCorrect(data[0]),"
" 'g=' + colourCorrect(data[1]),"
" 'b=' + colourCorrect(data[2])"
" ].join('&');"
" var req = new XMLHttpRequest();"
" req.open('POST', '?' + params, true);"
" req.send();"
" eventLocked = true;"
" req.onreadystatechange = function() {"
" if(req.readyState == 4) {"
" eventLocked = false;"
" }"
" }"
" }"
" canvas.addEventListener('click', function(event) {"
" handleEvent(event.clientX, event.clientY, true);"
" }, false);"
" canvas.addEventListener('touchmove', function(event){"
" handleEvent(event.touches[0].clientX, event.touches[0].clientY);"
"}, false);"
" function resizeCanvas() {"
" canvas.width = window.innerWidth;"
" canvas.height = window.innerHeight;"
" drawCanvas();"
" }"
" window.addEventListener('resize', resizeCanvas, false);"
" resizeCanvas();"
" drawCanvas();"
" document.ontouchmove = function(e) {e.preventDefault()};"
" })();"
"</script></html>";

//////////////////////////////////////////////////////////////////////////////////////////////////

int16_t fade_color(int16_t target, int16_t current)
{
  int16_t step_current = abs(target - current);

  if (step_current > step_color)
    step_current = step_color;
  
  if (target < current)
    return current - step_current;
  else
    return current + step_current;
}

void handleRoot() {
// Serial.println("handle root..");
String red = webServer.arg(0); // read RGB arguments
String green = webServer.arg(1);
String blue = webServer.arg(2);

r = red.toInt();
g = green.toInt();
b = blue.toInt();

// Serial.println(red.toInt()); // for TESTING
// Serial.println(green.toInt()); // for TESTING
// Serial.println(blue.toInt()); // for TESTING
webServer.send(200, "text/html", webpage);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {

pinMode(redPin, OUTPUT);
pinMode(greenPin, OUTPUT);
pinMode(bluePin, OUTPUT);

analogWrite(redPin, 1023);
analogWrite(greenPin, 1023);
analogWrite(bluePin, 1023);

//Accelerometer initialization
/* Set serial baud rate */
Serial.begin(9600);
/* Start accelerometer */
accel.begin();
/* Set accelerometer sensibility */
accel.setSensitivity(2);
/* Verify sensibility - optional */
if(accel.getSensitivity() != 2)   Serial.println("Sensitivity failure");
/* Set accelerometer mode */
accel.setMode(measure);
/* Verify accelerometer mode - optional */
if(accel.getMode() != measure)    Serial.println("Set mode failure");
/* Set axis offsets */
/* Note: the offset is hardware specific
 * and defined thanks to the auto-calibration example. */
accel.setAxisOffset(0, 0, 0);

delay(1000);

WiFi.mode(WIFI_AP);
WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
WiFi.softAP(ssid, password);

// if DNSServer is started with "*" for domain name, it will reply with provided IP to all DNS request
dnsServer.start(DNS_PORT, "rgb", apIP);

webServer.on("/", handleRoot);

webServer.begin();

}

//////////////////////////////////////////////////////////////////////////////////////////////////

void loop() {
  static int16_t r_cur = 1023;
  static int16_t g_cur = 1023;
  static int16_t b_cur = 1023;
  
  int16_t r_tar = 0;
  int16_t g_tar = 0;
  int16_t b_tar = 0;
  
  dnsServer.processNextRequest();
  webServer.handleClient();
  
  z10 = moving_average.filter(abs(accel.readAxis10('z')));
  Serial.print("\tZ: ");  Serial.println(z10, DEC);
  if(z10 < Zmin){
    r_tar = 1023;
    g_tar = 1023;
    b_tar = 1023;
  }
  else {
    r_tar = r;
    g_tar = g;
    b_tar = b;
  }
  
  r_cur = fade_color(r_tar, r_cur);
  g_cur = fade_color(g_tar, g_cur);
  b_cur = fade_color(b_tar, b_cur);
  
  analogWrite(redPin, r_cur);
  analogWrite(greenPin, g_cur);
  analogWrite(bluePin, b_cur);
  
  delay(4);
}
