Scriptname TopicInfoPatcher extends ScriptObject Native

int Function PatchTopicInfo (Topic tpTopic, string text) Native Global

Function ClearCache() Native Global

Function SetOverrideFileName(Topic TPInfo, string name) Native Global

string Function StringRemoveWhiteSpace(string text) native Global

; filetype = 3 PNG 
; filetype = 2 TGA

Function TakeScreenShot(string filename, int filetype, int sstype = 1) native global
bool Function isMenuModeActive() native global

; Functions to save and restore GameSettings

bool Function saveFloat(string key) native global
bool Function saveInt(string key) native global
bool Function restoreFloat(string key) native global
bool Function restoreInt(string key) native global

Actor [] Function GetLastCrossHairActor() native Global
;ObjectReference Function GetLastCrosshairRef() native global
;String Function GetActorName() native global


