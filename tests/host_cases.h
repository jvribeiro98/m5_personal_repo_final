#pragma once
int failures=0;
void check(bool condition,const char* name){std::cout<<(condition?"PASS ":"FAIL ")<<name<<"\n";if(!condition)failures++;}
void reset(){wifiConnecting=false;wifiConnectPending=false;wifiEditingSavedIndex=-1;savedNetworkCount=1;savedNetworks[0].ssid="home";savedNetworks[0].password="correct-password";WiFi.current=WL_DISCONNECTED;WiFi.ssid="";webServer.args.clear();webServer.body="";activeTv=0;activeAc=0;
#ifdef HAS_ASYNC_SCAN
 wifiScanRunning=false;wifiKeyboardActive=false;wifiAutoCandidatesReady=false;wifiAutoCandidateCursor=0;wifiScanHasResults=false;
#endif
}
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
 reset();wifiSelectedSavedIndex=0;selected=3;drawnBottom=0;drawWifiSavedDetail();check(drawnBottom<=122,"saved-network delete option fits above the footer");
 check(jsonEscape("a\rb\tc")=="a\\rb\\tc","SSID control characters produce valid JSON");
 reset();activeAc=2;airConditioners[2].state.temp=17;executeAcAction(0);check(airConditioners[2].state.temp==17,"Coolix never stores a temperature below its protocol minimum");
 reset();WiFi.networks={{"weak",-70},{"strong",-30}};scanNetworksNow();check(WiFi.scanWasAsync,"manual Wi-Fi scan returns without blocking the UI");
 reset();savedNetworkCount=2;savedNetworks[0].ssid="weak";savedNetworks[1].ssid="strong";wifiPendingSsid="";wifiReconnectCursor=0;tryAutoConnectStrongest();
#ifdef HAS_ASYNC_SCAN
 WiFi.scanResult=2;processWifiScan();
#endif
 check(wifiPendingSsid=="strong","automatic connection chooses the strongest visible saved network");
 reset();WiFi.startedSsid="";beginWifiConnection("home","correct-password",WifiConnectSource::PHYSICAL);bool cancelled=false;wifiKeyboard("Password","",true,cancelled);check(WiFi.startedSsid=="home","keyboard continues processing a pending Wi-Fi connection");
 reset();wifiAutoCandidatesReady=true;beginWifiConnection("home","correct-password",WifiConnectSource::AUTO_RECONNECT);processWifiConnection();WiFi.current=WL_CONNECTED;WiFi.ssid="home";processWifiConnection();check(!wifiAutoCandidatesReady,"a successful connection invalidates the scan used for the old retry cycle");
 reset();savedNetworkCount=3;savedNetworks[1].ssid="B";savedNetworks[2].ssid="C";screen=Screen::WIFI_DELETE_CONFIRM;wifiSelectedSavedIndex=1;deleteSavedNetwork(0);check(screen==Screen::WIFI_SAVED_LIST && wifiSelectedSavedIndex==-1,"web deletion cancels a physical confirmation whose index became stale");
 reset();savedNetworkCount=2;savedNetworks[1].ssid="other";wifiEditingSavedIndex=0;beginWifiConnection("other","password",WifiConnectSource::EDIT_VERIFY);check(!wifiConnectPending,"physical SSID edit cannot create a duplicate saved network");
 std::cout<<failures<<" failed\n";return failures?1:0;
}
