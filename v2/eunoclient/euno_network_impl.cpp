// euno_network_impl.cpp — implementazioni EEPROM/OP
#include <Arduino.h>
#include <EEPROM.h>
#include "euno_network.h"

// Layout semplice da 512:
// [lenSsid][ssid...][lenPass][pass...]
static const int EE_NET_BASE = 512;

// Salva credenziali OpenPlotter in EEPROM
void EunoNetwork::saveOpenPlotterCreds(const String& ssid, const String& pass){
  uint16_t p = EE_NET_BASE;
  EEPROM.write(p++, ssid.length() & 0xFF);
  for (size_t i=0; i<ssid.length(); ++i) EEPROM.write(p++, ssid[i]);
  EEPROM.write(p++, pass.length() & 0xFF);
  for (size_t i=0; i<pass.length(); ++i) EEPROM.write(p++, pass[i]);
  EEPROM.commit();
  Serial.println("[NET] OP creds saved to EEPROM");
}

// Helper globale: carica credenziali OP in net.cfg.sta2_*
bool EUNO_LOAD_OP_CREDS(EunoNetwork& net){
  uint16_t p = EE_NET_BASE;
  uint8_t ls = EEPROM.read(p++);
  if (ls==0xFF) { Serial.println("[NET] no OP creds"); return false; }

  String ssid, pass;
  for (uint8_t i=0; i<ls; ++i) ssid += char(EEPROM.read(p++));
  uint8_t lp = EEPROM.read(p++);
  for (uint8_t i=0; i<lp; ++i) pass += char(EEPROM.read(p++));

  if (ssid.length()){
    net.cfg.sta2_ssid = ssid;
    net.cfg.sta2_pass = pass;
    Serial.println("[NET] OP creds loaded: " + ssid);
    return true;
  }
  return false;
}
