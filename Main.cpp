#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <Preferences.h>
#include <SPI.h>
#define TFT_BL 21
#define TOUCH_CS 33
#define TOUCH_IRQ 36
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_CLK 25
#define HELTEC_TX 27
#define HELTEC_RX 22
#define SERIAL_BAUD 115200

TFT_eSPI tft=TFT_eSPI();
XPT2046_Touchscreen ts(TOUCH_CS,TOUCH_IRQ);
HardwareSerial MeshSerial(2);
Preferences prefs;
int16_t cal_x0=380,cal_x1=3800,cal_y0=300,cal_y1=3800;
bool calibrated=false;
String inputBuffer=""; String messageLog[14]; int logCount=0;
bool shift=false,symbols=false;
int currentChannel=0;
String channelNames[8]={"LongFast","ShortFast","LongSlow","VeryLong","MedSlow","ShortTurb","LongTurb","Private"};
int maxChannels=8;
struct Key{int x,y,w,h; String label; String out; uint16_t col;};
Key keys[60]; int keyCount=0;

void loadCal(){prefs.begin("cyd-mesh",true);if(prefs.isKey("cal")){cal_x0=prefs.getShort("x0",380);cal_x1=prefs.getShort("x1",3800);cal_y0=prefs.getShort("y0",300);cal_y1=prefs.getShort("y1",3800);calibrated=true;currentChannel=prefs.getShort("ch",0);}prefs.end();}
void saveCal(){prefs.begin("cyd-mesh",false);prefs.putShort("x0",cal_x0);prefs.putShort("x1",cal_x1);prefs.putShort("y0",cal_y0);prefs.putShort("y1",cal_y1);prefs.putBool("cal",true);prefs.end();}
void saveChannel(){prefs.begin("cyd-mesh",false);prefs.putShort("ch",currentChannel);prefs.end();}

void addLog(String m){
  if(m.length()==0) return;
  if(logCount<14){messageLog[logCount++]=m;}
  else{for(int i=0;i<13;i++) messageLog[i]=messageLog[i+1]; messageLog[13]=m;}
  tft.fillRect(0,28,240,92,TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  for(int i=0;i<logCount;i++){
    String s=messageLog[i];
    if(s.length()>36) s=s.substring(0,36);
    tft.setCursor(4,30+i*9);
    if(s.startsWith("YOU")) tft.setTextColor(TFT_CYAN);
    else if(s.startsWith("CH:")) tft.setTextColor(TFT_YELLOW);
    else tft.setTextColor(TFT_GREEN);
    tft.print(s);
  }
}

void drawStatusBar(){
  tft.fillRect(0,0,240,26,TFT_NAVY);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE,TFT_NAVY);
  tft.setCursor(4,4); tft.print("MESH");
  tft.fillRoundRect(42,2,68,18,4,TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE,TFT_DARKGREY);
  tft.setCursor(48,6); tft.print("CH"+String(currentChannel));
  tft.setCursor(114,4); tft.setTextColor(TFT_WHITE,TFT_NAVY);
  tft.print(channelNames[currentChannel].substring(0,8));
  tft.setCursor(180,4); tft.setTextColor(TFT_GREEN,TFT_NAVY);
  tft.print("LINK");
  tft.fillCircle(220,13,5,TFT_GREEN);
}

void drawInputArea(){
  tft.fillRect(0,120,240,28,TFT_DARKGREY);
  tft.drawRect(0,120,240,28,TFT_WHITE);
  tft.setCursor(6,128);
  tft.setTextColor(TFT_WHITE,TFT_DARKGREY);
  String disp="> "+inputBuffer;
  if(disp.length()>32) disp=disp.substring(disp.length()-32);
  tft.print(disp+"_");
}

void buildKeyboard(){
  keyCount=0;
  String rows[3];
  if(symbols){rows[0]="1234567890";rows[1]="!@#$%&*()";rows[2]="-_+=:;\"'";}
  else if(shift){rows[0]="QWERTYUIOP";rows[1]="ASDFGHJKL";rows[2]="ZXCVBNM<>";}
  else{rows[0]="qwertyuiop";rows[1]="asdfghjkl";rows[2]="zxcvbnm,.";}
  int startY=152;int rowH=36;
  for(int r=0;r<3;r++){
    String row=rows[r];
    int cols=row.length();
    int kw=(240-(cols+1)*2)/cols; if(kw>28) kw=28;
    int totalW=cols*kw+(cols-1)*2; int off=(240-totalW)/2;
    for(int c=0;c<cols;c++){
      String lbl=String(row[c]);
      keys[keyCount++]={off+c*(kw+2),startY+r*(rowH+2),kw,rowH,lbl,lbl,tft.color565(50,50,50)};
    }
  }
  int by=startY+3*(rowH+2)+2;
  keys[keyCount++]={2,by,34,32,symbols?"ABC":"123","",tft.color565(0,0,80)};
  keys[keyCount++]={38,by,38,32,"SHIFT","",shift?tft.color565(0,80,120):tft.color565(40,40,40)};
  keys[keyCount++]={78,by,72,32,"SPACE"," ",tft.color565(60,60,60)};
  keys[keyCount++]={152,by,32,32,"DEL","",tft.color565(120,20,20)};
  keys[keyCount++]={186,by,52,32,"SEND","",tft.color565(0,100,0)};
}

void drawKeyboard(){
  tft.fillRect(0,150,240,170,TFT_BLACK);
  for(int i=0;i<keyCount;i++){
    auto &k=keys[i];
    tft.fillRoundRect(k.x,k.y,k.w,k.h,5,k.col);
    tft.drawRoundRect(k.x,k.y,k.w,k.h,5,TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE,k.col);
    tft.drawString(k.label,k.x+k.w/2,k.y+k.h/2,1);
  }
}

void channelSelectScreen(){
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("SELECT CHANNEL",120,15,2);
  for(int i=0;i<maxChannels;i++){
    int y=35+i*32;
    uint16_t bg=(i==currentChannel)?TFT_GREEN:TFT_DARKGREY;
    tft.fillRoundRect(10,y,220,28,6,bg);
    tft.drawRoundRect(10,y,220,28,6,TFT_WHITE);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(TFT_WHITE,bg);
    tft.drawString(String(i)+" - "+channelNames[i],20,y+14,2);
  }
  tft.fillRoundRect(10,295,220,22,6,TFT_RED);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE,TFT_RED);
  tft.drawString("CANCEL",120,306,1);
  while(true){
    if(ts.touched()){
      TS_Point p=ts.getPoint();
      int sx=map(p.x,cal_x0,cal_x1,0,240);
      int sy=map(p.y,cal_y0,cal_y1,0,320);
      if(sy>295){break;}
      for(int i=0;i<maxChannels;i++){int y=35+i*32; if(sy>y&&sy<y+28){currentChannel=i;saveChannel();addLog("CH: -> "+String(i)+" "+channelNames[i]); break;}}
      delay(300); break;
    }
    delay(20);
  }
  tft.fillScreen(TFT_BLACK);
  drawStatusBar();
  addLog(""); // trigger redraw
  logCount--; // hack to not double add
  // redraw logs
  tft.fillRect(0,28,240,92,TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  for(int i=0;i<logCount;i++){tft.setCursor(4,30+i*9);tft.print(messageLog[i]);}
  drawInputArea(); buildKeyboard(); drawKeyboard();
}

void handleKey(int idx){
  String lbl=keys[idx].label;
  if(lbl=="SPACE"){inputBuffer+=" ";}
  else if(lbl=="DEL"){if(inputBuffer.length()>0) inputBuffer.remove(inputBuffer.length()-1);}
  else if(lbl=="SHIFT"){shift=!shift;buildKeyboard();drawKeyboard();return;}
  else if(lbl=="123"||lbl=="ABC"){symbols=!symbols;buildKeyboard();drawKeyboard();return;}
  else if(lbl=="SEND"){
    if(inputBuffer.length()==0) return;
    if(inputBuffer.startsWith("/ch")){int ch=inputBuffer.substring(3).toInt(); if(ch>=0&&ch<maxChannels){currentChannel=ch;saveChannel();addLog("CH: Switched to "+String(ch)+" "+channelNames[ch]);drawStatusBar();} inputBuffer="";}
    else{MeshSerial.println(inputBuffer);addLog("YOU ["+channelNames[currentChannel]+"]: "+inputBuffer);inputBuffer="";}
  } else {inputBuffer+=keys[idx].out; if(shift&&!symbols){shift=false;buildKeyboard();drawKeyboard();}}
  drawInputArea();
}

void splashScreen(){
  tft.fillScreen(TFT_BLACK);
  for(int i=0;i<80;i++){uint16_t c=tft.color565(0,i*2,i*3);tft.drawFastHLine(0,i,240,c);}
  tft.setTextDatum(TC_DATUM);tft.setTextColor(TFT_WHITE);tft.drawString("MESHTASTIC",120,10,4);tft.drawString("CYD KEYBOARD v2.1",120,35,2);tft.setTextColor(TFT_CYAN);tft.drawString("Heltec V3 + CH SWAP FIXED",120,58,1);
  tft.setTextDatum(TL_DATUM);tft.setTextColor(TFT_WHITE,TFT_BLACK);tft.setCursor(10,95);tft.println("MAC Build | ESP32-2432S028R");tft.setCursor(10,108);tft.println("Link: UART 115200 TX:27 RX:22");tft.setCursor(10,121);tft.println("Tap CH bar to switch channels");
  tft.drawRoundRect(10,140,220,18,4,TFT_DARKGREY);tft.fillRoundRect(12,142,30,14,3,TFT_GREEN);delay(400);
  for(int w=30;w<216;w+=8){tft.fillRoundRect(12+w,142,8,14,3,TFT_GREEN);delay(10);}
}

void calibrationScreen(){
  tft.fillScreen(TFT_BLACK);tft.setTextDatum(MC_DATUM);tft.drawString("TOUCH CALIBRATION",120,20,2);
  tft.setTextDatum(TL_DATUM);tft.setTextColor(TFT_YELLOW);tft.setCursor(20,40);tft.print("Tap crosshairs");
  int16_t xs[4],ys[4];int cx[4]={20,220,220,20};int cy[4]={20,20,300,300};
  for(int i=0;i<4;i++){
    tft.fillScreen(TFT_BLACK);tft.drawString("TOUCH CALIBRATION",120,20,2);
    tft.drawLine(cx[i]-15,cy[i],cx[i]+15,cy[i],TFT_WHITE);tft.drawLine(cx[i],cy[i]-15,cx[i],cy[i]+15,TFT_WHITE);
    tft.drawCircle(cx[i],cy[i],4,TFT_RED);tft.drawCircle(cx[i],cy[i],8,TFT_RED);
    while(!ts.touched()) delay(20);TS_Point p=ts.getPoint();xs[i]=p.x;ys[i]=p.y;
    tft.fillCircle(cx[i],cy[i],10,TFT_GREEN);delay(400);while(ts.touched())delay(20);delay(300);
  }
  cal_x0=(xs[0]+xs[3])/2;cal_x1=(xs[1]+xs[2])/2;cal_y0=(ys[0]+ys[1])/2;cal_y1=(ys[2]+ys[3])/2;
  saveCal();calibrated=true;
  tft.fillScreen(TFT_BLACK);tft.setTextDatum(MC_DATUM);tft.setTextColor(TFT_GREEN);tft.drawString("CALIBRATED!",120,150,4);delay(1000);
}

bool getTouch(int &sx,int &sy){
  if(!ts.touched()) return false;
  TS_Point p=ts.getPoint();
  sx=map(p.x,cal_x0,cal_x1,0,240);sy=map(p.y,cal_y0,cal_y1,0,320);
  if(sx<0)sx=0;if(sx>240)sx=240;if(sy<0)sy=0;if(sy>320)sy=320;
  return true;
}

void setup(){
  Serial.begin(115200);
  MeshSerial.begin(SERIAL_BAUD,SERIAL_8N1,HELTEC_RX,HELTEC_TX);
  pinMode(TFT_BL,OUTPUT);digitalWrite(TFT_BL,HIGH);
  tft.init();tft.setRotation(0);tft.fillScreen(TFT_BLACK);
  SPI.begin(TOUCH_CLK,TOUCH_MISO,TOUCH_MOSI,TOUCH_CS);
  ts.begin();ts.setRotation(0);
  loadCal();splashScreen();
  if(!calibrated){delay(500);calibrationScreen();}
  else{
    tft.setTextDatum(MC_DATUM);tft.setTextColor(TFT_YELLOW);tft.drawString("Hold top-left to recalibrate",120,190,1);
    unsigned long start=millis();bool want=false;
    while(millis()-start<2000){int sx,sy;if(getTouch(sx,sy)&&sy<40&&sx<60)want=true;delay(20);}
    if(want) calibrationScreen();
  }
  tft.fillScreen(TFT_BLACK);drawStatusBar();
  addLog("--- CYD v2.1 FIXED ---");addLog("Heltec V3 @ 6/7 115200");addLog("Tap CH bar to switch");addLog("Type /ch0-7 quick swap");
  if(calibrated) addLog("Touch OK");
  drawInputArea();buildKeyboard();drawKeyboard();
}

void loop(){
  if(MeshSerial.available()){String in=MeshSerial.readStringUntil('\n');in.trim();if(in.length()>0)addLog(in);}
  int sx,sy;
  if(getTouch(sx,sy)){
    if(sy<26&&sx>42&&sx<200){channelSelectScreen();delay(200);return;}
    for(int i=0;i<keyCount;i++){auto &k=keys[i];if(sx>k.x&&sx<k.x+k.w&&sy>k.y&&sy<k.y+k.h){tft.fillRoundRect(k.x,k.y,k.w,k.h,5,TFT_WHITE);delay(80);handleKey(i);drawKeyboard();break;}}
    delay(180);while(ts.touched())delay(10);
  }
  static unsigned long last=0;if(millis()-last>2000){drawStatusBar();last=millis();}
}
