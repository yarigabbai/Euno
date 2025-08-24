#pragma once
#include <Arduino.h>

// Callback che devi “collegare” alle tue funzioni reali:
struct EunoCmdAPI {
  std::function<void(int)>  onDelta = [](int){};
  std::function<void(void)> onToggle = [](){};
  std::function<void(const String&,int)> onSetParam = [](const String&,int){};
  std::function<void(const String&)> onMode = [](const String&){};
  std::function<void(const String&)> onCal  = [](const String&){};
  std::function<void(bool)> onExtBrg = [](bool){};
  std::function<void(int)> onExternalBearing = [](int){};
  std::function<void(const String&,const String&)> onOpenPlotterFrame = [](const String&,const String&){}; // raw pass-through if needed
};

// Estrae valore da “KEY=VALUE” dentro NMEA
static inline String nmeaGet(const String& line, const String& key){
  int s = line.indexOf(key + "=");
  if (s<0) return "";
  int e = line.indexOf(",", s);
  if (e<0) e = line.length();
  return line.substring(s + key.length() + 1, e);
}

static inline void parseNMEAClientLine(const String& line, EunoCmdAPI& api){
  // 1) Bearing esterno da RMB/APB
  if (line.startsWith("$GPRMB") || line.startsWith("$GNRMB")){
    // campo 11 = bearing to dest (°true)
    int comma=0, start=0; String f11="";
    for (int i=0;i<(int)line.length();i++){
      if (line[i]==','){ comma++; if (comma==10){ start=i+1; } else if (comma==11){ f11 = line.substring(start,i); break; } }
    }
    int brg = (int)round(f11.toFloat());
    if (brg>=0 && brg<360) api.onExternalBearing(brg);
    api.onOpenPlotterFrame("RMB", line);
    return;
  }
  if (line.startsWith("$GPAPB") || line.startsWith("$GNAPB")){
    api.onOpenPlotterFrame("APB", line);
    return;
  }
  if (line.startsWith("$HDT") || line.startsWith("$HDG") || line.startsWith("$GPRMC") || line.startsWith("$GNRMC")){
    api.onOpenPlotterFrame("NAV", line);
    return;
  }

  // 2) Comandi unificati
  if (line.startsWith("$PEUNO,CMD")){
    // esempi:
    // $PEUNO,CMD,DELTA=+1
    // $PEUNO,CMD,TOGGLE=1
    // $PEUNO,CMD,SET,V_min=120
    // $PEUNO,CMD,MODE=COMPASS
    // $PEUNO,CMD,CAL=MAG
    // $PEUNO,CMD,EXTBRG=ON

    if (line.indexOf("DELTA=")>0){
      int v = nmeaGet(line, "DELTA").toInt();
      api.onDelta(v); return;
    }
    if (line.indexOf("TOGGLE=")>0){
      api.onToggle(); return;
    }
    if (line.indexOf("SET,")>0){
      // estrai "SET,<name>=<val>"
      int p = line.indexOf("SET,"); String rest = line.substring(p+4);
      int eq = rest.indexOf('=');
      String name = rest.substring(0, eq);
      int val = rest.substring(eq+1).toInt();
      api.onSetParam(name, val); return;
    }
    if (line.indexOf("MODE=")>0){
      api.onMode(nmeaGet(line, "MODE")); return;
    }
    if (line.indexOf("CAL=")>0){
      api.onCal(nmeaGet(line, "CAL")); return;
    }
    if (line.indexOf("EXTBRG=")>0){
      String s = nmeaGet(line, "EXTBRG");
      api.onExtBrg(s=="ON"||s=="on"||s=="1"); return;
    }
  }
}
