#include "DebugManager.h"

void DebugManager::setUp(bool newDebug) {
  _debug = newDebug;
  if (_debug) {
    Serial.begin(115200);
    // let Serial begin
    delay(500);
    debugMsg("");
    debugMsg("--------------------- START ---------------------");
  } else {
    debugMsg("");
    debugMsg("---------------------- END ----------------------");
    Serial.end();
  }  
}

void DebugManager::debugMsg(String message) {
  if (_debug) {
    Serial.println(message);
    Serial.flush();
  }
}

bool DebugManager::getDebug() {
  return _debug;
}
