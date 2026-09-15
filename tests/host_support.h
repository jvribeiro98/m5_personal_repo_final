#pragma once
int savedWrites=0;
int emittedDevice=-1;
int acStateSends=0;
int drawnBottom=0;
FakeDisplay uiCanvas;
FakeDisplay& getGfx(){return uiCanvas;}
bool syncClockFromInternet(int32_t){return true;}
void processDeviceSerial(){}
void processVoiceTransport(){}
void updateBatteryState(bool){}
void showToast(const String&,uint16_t=900){}
void saveAcState(uint8_t){}
void applySamsungState(const AcState&){}
void applyMideaState(const AcState&){}
void applyCoolixState(const AcState&){}
void sendTvCommand(TvCommand){emittedDevice=activeTv;}
void sendAcState(const String&){emittedDevice=activeAc;acStateSends++;}
void cycleAcMode(){}
void cycleAcFan(){}
void toggleAcSwing(){}
void toggleAcTurbo(){}
void cycleAcSleep(){}
void drawScreen(){}
void processWifiMaintenance(){}
void ButtonCState::update(){}
bool ButtonCState::wasClicked() const{return false;}
bool ButtonCState::wasHeld() const{return false;}
void saveSavedNetworks(){savedWrites++;}
void saveWebUiPreference(){}
void stopSetupAccessPoint(){}
void markSavedNetworkSuccess(const String&){}
void markSavedNetworkFailure(const String&,SavedNetworkFailure){}
void drawTitle(const String&,const String&){}
void drawListItem(uint8_t,int y,const String&,const String& = "");
void drawListItem(uint8_t,int y,const String&,const String&){drawnBottom=max(drawnBottom,y+23);}
int8_t findSavedNetwork(const String& s){for(int i=0;i<savedNetworkCount;i++)if(savedNetworks[i].ssid==s)return i;return -1;}
bool upsertSavedNetwork(const String& s,const String& p){if(savedNetworkCount>=10)return false;savedNetworks[savedNetworkCount].ssid=s;savedNetworks[savedNetworkCount++].password=p;return true;}
String jsonEscape(String);
String acStateJson(uint8_t);
const char* tvCommandName(TvCommand){return "POWER";}
const char* acModeName(AcMode){return "FRIO";}
const char* acFanName(AcFan){return "AUTO";}
String sleepName(const AcDevice&){return "OFF";}
