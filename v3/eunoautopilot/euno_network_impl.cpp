// euno_network_impl.cpp — implementazioni EEPROM/OP
#include <Arduino.h>
#include <EEPROM.h>
#include "euno_network.h"

// Layout semplice da 512:
// [lenSsid][ssid...][lenPass][pass...]
static const int EE_NET_BASE = 512;

// ============================================================================
// Salva credenziali OpenPlotter in EEPROM (resta disponibile se vuoi usarla)
// ============================================================================
void EunoNetwork::saveOpenPlotterCreds(const String& ssid, const String& pass){
  uint16_t p = EE_NET_BASE;
  EEPROM.write(p++, ssid.length() & 0xFF);
  for (size_t i=0; i<ssid.length(); ++i) EEPROM.write(p++, ssid[i]);
  EEPROM.write(p++, pass.length() & 0xFF);
  for (size_t i=0; i<pass.length(); ++i) EEPROM.write(p++, pass[i]);
  EEPROM.commit();
  Serial.println("[NET] OP creds saved to EEPROM");
}

// ============================================================================
// Helper globale: carica credenziali OP
// Ora ignora l'EEPROM e forza le credenziali di default da euno_network.h
// ============================================================================
bool EUNO_LOAD_OP_CREDS(EunoNetwork& net){
  // Forza sempre a usare le credenziali hard-coded (sta1)
  net.cfg.sta2_ssid = net.cfg.sta1_ssid;
  net.cfg.sta2_pass = net.cfg.sta1_pass;
  Serial.println("[NET] Using default STA creds: " + net.cfg.sta2_ssid);
  return true;
}
