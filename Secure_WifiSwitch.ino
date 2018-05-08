#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>

//This is our login html page, contains user and pass fields that are posted to /login
const String loginPage = "<!DOCTYPE html><html><head><title>Login</title></head><body> <div id=\"login\"> <form action='/login' method='POST'> <center> <h1>Login </h1><p><input type='text' name='user' placeholder='User name'></p><p><input type='password' name='pass' placeholder='Password'></p><br><button type='submit' name='submit'>login</button></center> </form></body></html>";
//Our page if user succesfully loged in, includes timeout timer refresh and logout href
const String loginok = "<!DOCTYPE html><html><head><title>Login</title></head><body> <div> <form action='/' method='POST'> <center> <a href=\"/refresh\">Refresh</a><br><a href=\"/logoff\">Logoff</a></center> </form></body></html>";

const char* ssid     = "NetworkName";
const char* password = "NetworkPassword";


const int pwr = 2;  //GPIO2
//const int channel = 3; //what is this?
const int buttonPin = 0;
const int switchType = 1;    //Change this to 0 for on/off switch, or 1 for momentary type if using gpio0 for a switch

int state = HIGH;      // the current state of the output pin
int reading;           // the current reading from the input pin
int previous = LOW;    // the previous reading from the input pin

// the follow variables are long's because the time, measured in miliseconds,
// will quickly become a bigger number than can be stored in an int.
long period = 0;         // the last time the output pin was toggled
long debounce = 200;   // the debounce time, increase if the output flickers

int buttonState = 0;         // current state of the button
int lastButtonState = 0;     // previous state of the button



bool lock = false; //This bool is used to control device lockout

String anchars = "abcdefghijklmnopqrstuvwxyz0123456789", username = "admin", loginPassword = "password"; //anchars will be explained below. username will be compared with 'user' and loginPassword with 'pass' when login is done

unsigned long logincld = millis(), reqmillis = millis(), tempign = millis(); //First 2 timers are for lockout and last one is inactivity timer

uint8_t i, trycount = 0; // i is used for for index, trycount will be our buffer for remembering how many false entries there were

ESP8266WebServer server(80);

String sessioncookie; //this is cookie buffer

void setup(void) {
  Serial.begin(115200);
  delay(10);

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  gencookie(); //generate new cookie on device start

  server.on("/", handleRoot);

  server.onNotFound(handleNotFound);

  server.on("/login", handleLogin);
  server.on("/refresh", refresh);
  server.on("/logoff", logoff);

  //URLs available to query
  server.on ( "/", handleRoot);
  server.on ( "/on", turnON );
  server.on ( "/off", turnOFF );

  pinMode(buttonPin, INPUT);
  pinMode(pwr, OUTPUT);
  digitalWrite ( pwr, HIGH );

  const char * headerkeys[] = {"User-Agent", "Cookie"} ;
  size_t headerkeyssize = sizeof(headerkeys) / sizeof(char*);
  server.collectHeaders(headerkeys, headerkeyssize ); //These 3 lines tell esp to collect User-Agent and Cookie in http header when request is made

  server.begin();
}


bool is_authentified() { //This function checks for Cookie header in request, it compares variable c to sessioncookie and returns true if they are the same
  if (server.hasHeader("Cookie")) {
    String cookie = server.header("Cookie"), authk = "c=" + sessioncookie;
    if (cookie.indexOf(authk) != -1) return true;
  }
  return false;
}

void handleRoot() {

  String header;
  if (!is_authentified()) { //This here checks if your cookie is valid in header and it redirects you to /login if not, if ok send loginok html file
    String header = "HTTP/1.1 301 OK\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n";
    server.sendContent(header);
    return;
  }
  if (is_authentified()) {
    wifiSwitch();
  }
  //server.send(200, "text/html", loginok);
  tempign = millis(); //reset the inactivity timer if someone logs in
}

void handleLogin() {

  String msg; //this is our buffer that we will add to the login html page when headers are wrong or device is locked

  if (server.hasHeader("Cookie")) {
    String cookie = server.header("Cookie"); //Copy the Cookie header to this buffer
  }

  if (server.hasArg("user") && server.hasArg("pass")) { //if user posted with these arguments
    if (server.arg("user") == username &&  server.arg("pass") == loginPassword && !lock) { //check if login details are good and dont allow it if device is in lockdown
      String header = "HTTP/1.1 301 OK\r\nSet-Cookie: c=" + sessioncookie + "\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n"; //if above values are good, send 'Cookie' header with variable c, with format 'c=sessioncookie'
      server.sendContent(header);
      trycount = 0; //With good headers in mind, reset the trycount buffer
      return;
    }

    //  String msg; //this is our buffer that we will add to the login html page when headers are wrong or device is locked
    msg = "<center><br>";
    if (trycount != 10 && !lock)trycount++; //If system is not locked up the trycount buffer
    if (trycount < 10 && !lock) { //We go here if systems isn't locked out, we give user 10 times to make a mistake after we lock down the system, thus making brute force attack almost imposible
      msg += "Wrong username/password<p></p>";
      msg += "You have ";
      msg += (10 - trycount);
      msg += " tries before system temporarily locks out.";
      logincld = millis(); //Reset the logincld timer, since we still have available tries
    }

    if (trycount == 10) { //If too much bad tries
      if (lock) {
        msg += "Too much invalid login requests, you can use this device in ";
        msg += 5 - ((millis() - logincld) / 60000); //Display lock time remaining in minutes
        msg += " minutes.";
      }
      else {
        logincld = millis();
        lock = true;
        msg += "Too much invalid login requests, you can use this device in 5 minutes."; //This happens when your device first locks down
      }
    }
  }
  String content = loginPage;
  content +=  msg + "</center>";
  server.send(200, "text/html", content); //merge loginPage and msg and send it
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

void gencookie() {
  sessioncookie = "";
  for ( i = 0; i < 32; i++) sessioncookie += anchars[random(0, anchars.length())]; //Using randomchar from anchars string generate 32-bit cookie
}

void loop(void) {
  server.handleClient();

  if (lock && abs(millis() - logincld) > 300000) {
    lock = false;
    trycount = 0;
    logincld = millis(); //After 5 minutes is passed unlock the system
  }

  if (!lock && abs(millis() - logincld) > 60000) {
    trycount = 0;
    logincld = millis();
    //After minute is passed without bad entries, reset trycount
  }


  if (abs(millis() - tempign) > 120000) {
    gencookie();
    tempign = millis();
    //if there is no activity from loged on user, change the generate a new cookie. This is more secure than adding expiry to the cookie header
  }
  extSwitch();
}

void logoff() {
  String header = "HTTP/1.1 301 OK\r\nSet-Cookie: c=0\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n"; //Set 'c=0', it users header, effectively deleting it's header
  server.sendContent(header);
}

void refresh() {
  if (is_authentified()) { //this is for reseting the inactivity timer, it covers everything explained above
    tempign = millis();
    String header = "HTTP/1.1 301 OK\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n";
    server.sendContent(header);
  }
  else {
    String header = "HTTP/1.1 301 OK\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n";
    server.sendContent(header);
  }
}

void wifiSwitch() {

 if (is_authentified()) {
  
  int size = 1000;
  char temp[size];

  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  snprintf ( temp, size,

             "<html>\
  <head>\
  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"/>\
    <title>Wifi Switch</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <center>\
    <h3>You are connected to Wifi Switch</h3>\
    <p>Uptime: %02d:%02d:%02d</p>\
    <p></p>\
    <p><a href=\"/on\"><button style=\"height:100px;width:200px\"><h1>Turn On</h1></button></a></p>\
    <p></p>\
    <p><a href=\"/off\"><button style=\"height:100px;width:200px\"><h1>Turn Off</h1></button></a></p>\
    <p><a href=\"/logoff\">Logoff</a></p>\
    <\center>\
  </body>\
</html>",

             hr, min % 60, sec % 60
           );
  server.send ( 200, "text/html", temp );
}
else {
  server.send ( 200, "text/html", loginPage );
}
}

void turnON() {

 if (is_authentified()) {

  digitalWrite ( pwr, LOW );

  int size = 2000;
  char temp[size];

  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  snprintf ( temp, size,

             "<html>\
  <head>\
<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"/>\
    <title>Wifi Switch</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <center>\
    <h3>You are connected to Wifi Switch</h3>\
    <p>Uptime: %02d:%02d:%02d</p>\
    <p></p>\
    <p><a href=\"/on\"><button style=\"height:100px;width:200px\"><h1>Turn On</h1></button></a></p>\
    <p></p>\
    <p><a href=\"/off\"><button style=\"height:100px;width:200px\"><h1>Turn Off</h1></button></a></p>\
    <p><a href=\"/logoff\">Logoff</a></p>\
    <\center>\
  </body>\
</html>",

             hr, min % 60, sec % 60
           );

  server.send ( 200, "text/html", temp);
 }
else {
  server.send ( 200, "text/html", loginPage );
}
}

void turnOFF() {

  if (is_authentified()) {

    digitalWrite ( pwr, HIGH );

    int size = 1000;
    char temp[size];

    int sec = millis() / 1000;
    int min = sec / 60;
    int hr = min / 60;

    snprintf ( temp, size,

               "<html>\
  <head>\
<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"/>\
    <title>Wifi Switch</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <center>\
    <h3>You are connected to Wifi Switch</h3>\
    <p>Uptime: %02d:%02d:%02d</p>\
    <p></p>\
    <p><a href=\"/on\"><button style=\"height:100px;width:200px\"><h1>Turn On</h1></button></a></p>\
    <p></p>\
    <p><a href=\"/off\"><button style=\"height:100px;width:200px\"><h1>Turn Off</h1></button></a></p>\
    <p><a href=\"/logoff\">Logoff</a></p>\
    <\center>\
  </body>\
</html>",

               hr, min % 60, sec % 60
             );

    server.send ( 200, "text/html", temp);
  }
else{
  server.send ( 200, "text/html", loginPage );
}
}

void extSwitch() {

  if (switchType == 0) {
    buttonState = digitalRead(buttonPin);  

    // compare the buttonState to its previous state
    if (buttonState != lastButtonState) {

      if (buttonState == HIGH) {
        digitalWrite(pwr, HIGH);
      } else {
        digitalWrite(pwr, LOW);

      }
      // Delay a little bit to avoid bouncing
      delay(50);
    }
    // save the current state as the last state, for next time through the loop
    lastButtonState = buttonState;
  }
  if (switchType == 1) {

    buttonState = digitalRead(buttonPin);  //uncomment this code for a momentary switch

    if (buttonState != lastButtonState) {

      if (buttonState == HIGH && lastButtonState == LOW && millis() - period > debounce) {
        if (state == HIGH)
        {
          state = LOW;
        }
        else
        {
          state = HIGH;
        }
        period = millis();
      }
      digitalWrite(pwr, state);

      lastButtonState = buttonState;
    }
  }
}
