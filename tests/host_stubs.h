#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <vector>
using std::min;
using std::max;
class String : public std::string {
 public:
  using std::string::string;
  String() = default;
  String(const std::string& s):std::string(s) {}
  String(int n):std::string(std::to_string(n)) {}
  String(unsigned int n):std::string(std::to_string(n)) {}
  String(size_t n):std::string(std::to_string(n)) {}
  String(char c):std::string(1,c) {}
  void trim() { auto b=find_first_not_of(" \r\n\t"), e=find_last_not_of(" \r\n\t"); *this=b==npos?"":substr(b,e-b+1); }
  long toInt() const { return std::strtol(c_str(),nullptr,10); }
  void replace(const String& from,const String& to) { size_t p=0; while((p=find(from,p))!=npos) { std::string::replace(p,from.size(),to);p+=to.size(); } }
  String substring(size_t start,size_t end=std::string::npos) const{return substr(start,end==npos?npos:end-start);}
  void remove(size_t start){erase(start);}
};
struct Preferences {};
struct WebServer {
  WebServer(int) {}
  std::map<std::string,String> args;
  int status=0;
  String body;
  bool hasArg(const char* key) { return args.count(key); }
  String arg(const char* key) { auto found=args.find(key); return found==args.end()?String():found->second; }
  void send(int s,const char*,const String& b) {status=s;body=b;}
  void sendHeader(const char*,const char*) {}
  void handleClient(){}
};
enum wl_status_t { WL_IDLE_STATUS, WL_NO_SSID_AVAIL, WL_CONNECTED, WL_CONNECT_FAILED, WL_DISCONNECTED };
constexpr int WIFI_STA=1,WIFI_AP_STA=3;
constexpr int WIFI_SCAN_RUNNING=-1,WIFI_SCAN_FAILED=-2;
struct Ip { String toString() const {return "192.168.1.50";} };
struct FakeWifi {
  wl_status_t current=WL_DISCONNECTED;
  String ssid,startedSsid,startedPassword;
  bool scanWasAsync=false;
  int scanResult=WIFI_SCAN_RUNNING;
  std::vector<std::pair<String,int>> networks;
  wl_status_t status() {return current;}
  String SSID() {return ssid;}
  String SSID(int i) {return networks[i].first;}
  int RSSI() {return -55;}
  int RSSI(int i) {return networks[i].second;}
  int scanNetworks(bool async,bool) {scanWasAsync=async;return async?WIFI_SCAN_RUNNING:int(networks.size());}
  int scanComplete(){return scanResult;}
  void scanDelete(){}
  Ip localIP() {return {};}
  void disconnect(bool,bool) {current=WL_DISCONNECTED;}
  void mode(int) {}
  void begin(const char* s,const char* p) {startedSsid=s;startedPassword=p;}
} WiFi;
uint32_t clockMs=100;
uint32_t millis(){return clockMs;}
void delay(int n){clockMs+=n;}
struct FakeIr {
  FakeIr(int){}
  int sends=0;
  void send(){sends++;}
  void sendExtended(){sends++;}
  void sendOn(){sends++;}
  void sendOff(){sends++;}
  String toString(){return "IR";}
};
using IRsend=FakeIr; using IRSamsungAc=FakeIr; using IRMideaAC=FakeIr; using IRCoolixAC=FakeIr;
struct { void println(const String&){} } Serial;
constexpr int middle_center=0,top_left=1,top_right=2,middle_left=3;
struct FakeDisplay {
 template<class... A>void fillScreen(A...){}
 template<class... A>void fillRoundRect(A...){}
 template<class... A>void drawRoundRect(A...){}
 template<class... A>void setTextDatum(A...){}
 template<class... A>void setTextSize(A...){}
 template<class... A>void setTextColor(A...){}
 template<class... A>void drawString(A...){}
 template<class... A>void fillRect(A...){}
 template<class... A>void drawRect(A...){}
 template<class... A>void pushSprite(A...){}
};
struct FakeButton{bool clicked=false;bool wasHold(){return false;}bool wasClicked(){return clicked;}};
struct {FakeDisplay Display;FakeButton BtnA{true},BtnB{false};void update(){}} M5;
