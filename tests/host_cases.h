#pragma once
int failures=0;
void check(bool condition,const char* name){std::cout<<(condition?"PASS ":"FAIL ")<<name<<"\n";if(!condition)failures++;}
void reset(){wifiConnecting=false;wifiConnectPending=false;wifiEditingSavedIndex=-1;savedNetworkCount=1;std::fill(std::begin(savedNetworks),std::end(savedNetworks),SavedNetwork{});savedNetworks[0].ssid="home";savedNetworks[0].password="correct-password";WiFi=FakeWifi{};webServer.args.clear();webServer.body="";webServer.status=0;activeTv=0;activeAc=0;emittedDevice=-1;acStateSends=0;samsungAc.sends=0;mideaAc.sends=0;for(auto& ac:airConditioners)ac.state=AcState{};wifiPendingSsid="";wifiPendingPassword="";screen=Screen::MAIN;selected=0;scannedNetworkCount=0;wifiScanForMenu=false;wifiScanForAuto=false;webUiMode=WebUiMode::OFF;clockMs=100;
#ifdef HAS_ASYNC_SCAN
 wifiScanRunning=false;wifiKeyboardActive=false;wifiAutoCandidatesReady=false;wifiAutoCandidateCursor=0;wifiScanHasResults=false;
#endif
}
#include "mouse_audio_cases.h"
int main(){
 reset(); beginWifiConnection("first","first-pass",WifiConnectSource::PHYSICAL);beginWifiConnection("second","second-pass",WifiConnectSource::WEB_SETUP);
 check(wifiPendingSsid=="first","a pending Wi-Fi request cannot be overwritten");
 reset(); beginWifiConnection("first","first-pass",WifiConnectSource::PHYSICAL);processWifiConnection();beginWifiConnection("second","second-pass",WifiConnectSource::WEB_SETUP);WiFi.ssid="first";WiFi.current=WL_CONNECTED;processWifiConnection();
 check(findSavedNetwork("first")>=0 && findSavedNetwork("second")<0,"connection success saves credentials of the actual attempt");
 reset();wifiConnecting=true;handleWebApiStatus();check(webServer.body.find("\"connecting\":true")!=String::npos,"status API reports an in-progress connection");
 reset();webServer.args={{"device","2"},{"cmd","0"}};handleWebApiTv();check(activeTv==0 && emittedDevice==2,"web TV command preserves physical selection");
 reset();webServer.args={{"device","1"},{"action","0"}};handleWebApiAc();check(activeAc==0 && emittedDevice==1,"web AC command preserves physical selection");
 reset();webServer.args={{"device","garbage"},{"cmd","0"}};handleWebApiTv();check(webServer.status==400,"invalid device text never becomes a power command to device zero");
 reset();webServer.args={{"index","garbage"}};handleWebApiDeleteSaved();check(savedNetworkCount==1,"invalid index never deletes the first saved network");
 reset();wifiConnecting=true;webServer.args={{"index","0"},{"ssid","changed"},{"password","new-password"},{"test","1"}};handleWebApiSaveNetwork();check(wifiEditingSavedIndex==-1 && savedNetworks[0].ssid=="home" && webServer.status==400,"editing while connecting cannot change the active transaction");
 reset();airConditioners[0].state.power=false;samsungAc.sends=0;toggleAcPower();check(samsungAc.sends==1,"Samsung power-on sends one logical state command");
 reset();airConditioners[0].state.power=true;samsungAc.sends=0;toggleAcPower();check(samsungAc.sends==1,"Samsung power-off sends one logical state command");
 check(jsonEscape("a\rb\tc")=="a\\rb\\tc","SSID control characters produce valid JSON");
 reset();airConditioners[0].state.temp=16;executeAcAction(0);check(airConditioners[0].state.temp==16,"AC decrement preserves the minimum supported temperature");
 reset();airConditioners[1].state.temp=30;activeAc=1;executeAcAction(1);check(airConditioners[1].state.temp==30,"AC increment preserves the maximum supported temperature");
 reset();WiFi.networks={{"weak",-70},{"strong",-30}};scanNetworksNow();check(WiFi.scanWasAsync,"manual Wi-Fi scan returns without blocking the UI");
 reset();savedNetworkCount=2;savedNetworks[0].ssid="weak";savedNetworks[1].ssid="strong";WiFi.networks={{"weak",-70},{"strong",-30}};tryAutoConnectStrongest();
#ifdef HAS_ASYNC_SCAN
 WiFi.scanResult=2;processWifiScan();
#endif
 check(wifiPendingSsid=="strong","automatic connection chooses the strongest visible saved network");
 reset();WiFi.startedSsid="";beginWifiConnection("home","correct-password",WifiConnectSource::PHYSICAL);bool cancelled=false;wifiKeyboard("Password","",true,cancelled);check(WiFi.startedSsid=="home","keyboard continues processing a pending Wi-Fi connection");
 reset();wifiAutoCandidatesReady=true;beginWifiConnection("home","correct-password",WifiConnectSource::AUTO_RECONNECT);processWifiConnection();WiFi.current=WL_CONNECTED;WiFi.ssid="home";processWifiConnection();check(!wifiAutoCandidatesReady,"a successful connection invalidates the scan used for the old retry cycle");
 reset();savedNetworkCount=3;savedNetworks[1].ssid="B";savedNetworks[2].ssid="C";screen=Screen::WIFI_DELETE_CONFIRM;wifiSelectedSavedIndex=1;deleteSavedNetwork(0);check(screen==Screen::WIFI_SAVED_LIST && wifiSelectedSavedIndex==-1,"web deletion cancels a physical confirmation whose index became stale");
 reset();savedNetworkCount=2;savedNetworks[1].ssid="other";wifiEditingSavedIndex=0;beginWifiConnection("other","password",WifiConnectSource::EDIT_VERIFY);check(!wifiConnectPending,"physical SSID edit cannot create a duplicate saved network");
 reset();activeAc=1;webServer.args={{"device","0"},{"power","on"}};handleWebApiAc();handleWebApiAc();check(webServer.status==200 && airConditioners[0].state.power && samsungAc.sends==1 && activeAc==1,"repeated explicit AC power-on is idempotent and preserves physical selection");
 webServer.args={{"device","0"},{"power","off"}};handleWebApiAc();handleWebApiAc();check(webServer.status==200 && !airConditioners[0].state.power && samsungAc.sends==2 && activeAc==1,"repeated explicit AC power-off is idempotent and preserves physical selection");
 reset();activeAc=1;webServer.args={{"device","0"},{"temp","22"}};handleWebApiAc();handleWebApiAc();check(webServer.status==200 && airConditioners[0].state.temp==22 && airConditioners[1].state.temp==23 && acStateSends==1 && activeAc==1,"absolute AC temperature changes only the target once");
 for(const char* temp:{"15","31","bad","-1","22.5","9999"}){reset();webServer.args={{"device","0"},{"temp",temp}};handleWebApiAc();check(webServer.status==400 && airConditioners[0].state.temp==23 && acStateSends==0,"invalid AC temperature never changes state or sends IR");}
 for(const char* power:{"true","1","toggle","ON",""}){reset();webServer.args={{"device","0"},{"power",power}};handleWebApiAc();check(webServer.status==400 && !airConditioners[0].state.power && samsungAc.sends==0,"invalid explicit AC power never toggles hardware");}
 reset();webServer.args={{"device","0"},{"power","on"},{"action","7"}};handleWebApiAc();check(webServer.status==400 && samsungAc.sends==0,"AC request cannot combine explicit power with a toggle");
 reset();webServer.args={{"device","0"},{"temp","22"},{"action","1"}};handleWebApiAc();check(webServer.status==400 && acStateSends==0,"AC request cannot combine absolute and relative temperature");
 reset();webServer.args={{"device","0"}};handleWebApiAc();check(webServer.status==400,"AC request requires an action");
 reset();webServer.args={{"device","garbage"},{"power","on"}};handleWebApiAc();check(webServer.status==400 && samsungAc.sends==0,"malformed AC device never powers on device zero");
 runMouseAudioCases();
 std::cout<<failures<<" failed\n";return failures?1:0;
}
