#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <HijelHID_BLEKeyboard.h>
#include "esp_wifi.h"

HijelHID_BLEKeyboard keyboard("ESP32 Music Remote", "ESP32", 100);

#define BTN_VOL_DWN 8
#define BTN_PREV    7
#define BTN_NEXT    6
#define BTN_VOL_UP  5

const char* AP_SSID = "ESP32-Music-Remote";
const char* AP_PASS = "12345678";
const unsigned long CONFIG_TIMEOUT = 3 * 60 * 1000UL;

WebServer server(80);
Preferences prefs;

bool configMode = false;
unsigned long configStartTime = 0;

// ===================== ОС =====================
enum HostOS : uint8_t { OS_WINDOWS = 0, OS_MACOS };
HostOS currentOS = OS_WINDOWS;

// ===================== Действия =====================
enum ActionType : uint8_t { TYPE_NONE = 0, TYPE_MEDIA, TYPE_COMBO };

enum MediaAction : uint8_t {
  ACT_NONE = 0, ACT_VOL_UP, ACT_VOL_DOWN, ACT_MUTE,
  ACT_PLAY_PAUSE, ACT_NEXT, ACT_PREV, ACT_STOP, ACT_COUNT
};

const char* mediaNames[ACT_COUNT] = {
  "Нет действия", "Громкость +", "Громкость −", "Без звука",
  "Play / Pause", "Следующий трек", "Предыдущий трек", "Стоп"
};

struct ButtonAction {
  ActionType type = TYPE_NONE;
  MediaAction media = ACT_NONE;
  uint8_t modifiers = 0;
  uint8_t keycode = 0;
};

struct ButtonConfig {
  ButtonAction shortPress;
  ButtonAction longPress;
};

ButtonConfig btnConfig[4];

// ===================== Профили =====================
const int PROFILE_COUNT = 5;
char profileNames[PROFILE_COUNT][17];   // имя до 16 символов
uint8_t currentProfile = 0;

// ===================== Клавиши =====================
struct KeyItem { uint8_t code; const char* name; };

const KeyItem keyList[] = {
  {KEY_NONE, "— нет —"},
  {KEY_A,"A"},{KEY_B,"B"},{KEY_C,"C"},{KEY_D,"D"},{KEY_E,"E"},
  {KEY_F,"F"},{KEY_G,"G"},{KEY_H,"H"},{KEY_I,"I"},{KEY_J,"J"},
  {KEY_K,"K"},{KEY_L,"L"},{KEY_M,"M"},{KEY_N,"N"},{KEY_O,"O"},
  {KEY_P,"P"},{KEY_Q,"Q"},{KEY_R,"R"},{KEY_S,"S"},{KEY_T,"T"},
  {KEY_U,"U"},{KEY_V,"V"},{KEY_W,"W"},{KEY_X,"X"},{KEY_Y,"Y"},{KEY_Z,"Z"},
  {KEY_1,"1"},{KEY_2,"2"},{KEY_3,"3"},{KEY_4,"4"},{KEY_5,"5"},
  {KEY_6,"6"},{KEY_7,"7"},{KEY_8,"8"},{KEY_9,"9"},{KEY_0,"0"},
  {KEY_RETURN,"Enter"},{KEY_ESCAPE,"Esc"},{KEY_BACKSPACE,"Backspace"},
  {KEY_TAB,"Tab"},{KEY_SPACE,"Space"},{KEY_DELETE,"Delete"},
  {KEY_INSERT,"Insert"},{KEY_HOME,"Home"},{KEY_END,"End"},
  {KEY_PAGE_UP,"Page Up"},{KEY_PAGE_DOWN,"Page Down"},
  {KEY_UP,"↑ Up"},{KEY_DOWN,"↓ Down"},{KEY_LEFT,"← Left"},{KEY_RIGHT,"→ Right"},
  {KEY_F1,"F1"},{KEY_F2,"F2"},{KEY_F3,"F3"},{KEY_F4,"F4"},
  {KEY_F5,"F5"},{KEY_F6,"F6"},{KEY_F7,"F7"},{KEY_F8,"F8"},
  {KEY_F9,"F9"},{KEY_F10,"F10"},{KEY_F11,"F11"},{KEY_F12,"F12"},
  {KEY_PRINT_SCREEN,"Print Screen"},{KEY_PAUSE,"Pause"},
  {KEY_CAPS_LOCK,"Caps Lock"},{KEY_APPLICATION,"Menu"}
};
const int KEY_LIST_SIZE = sizeof(keyList) / sizeof(KeyItem);

// ===================== Кнопки =====================
bool lastState[4] = {HIGH, HIGH, HIGH, HIGH};
unsigned long pressTime[4] = {0};
bool longDone[4] = {false};
const unsigned long LONG_PRESS_TIME = 700;
const int btnPins[4] = {BTN_VOL_DWN, BTN_PREV, BTN_NEXT, BTN_VOL_UP};

// ===================== Выполнение =====================
void executeMedia(MediaAction act) {
  switch (act) {
    case ACT_VOL_UP:     keyboard.tap(MEDIA_VOLUME_UP);   break;
    case ACT_VOL_DOWN:   keyboard.tap(MEDIA_VOLUME_DOWN); break;
    case ACT_MUTE:       keyboard.tap(MEDIA_MUTE);        break;
    case ACT_PLAY_PAUSE: keyboard.tap(MEDIA_PLAY_PAUSE);  break;
    case ACT_NEXT:       keyboard.tap(MEDIA_NEXT_TRACK);  break;
    case ACT_PREV:       keyboard.tap(MEDIA_PREV_TRACK);  break;
    case ACT_STOP:       keyboard.tap(MEDIA_STOP);        break;
    default: break;
  }
}

void executeCombo(uint8_t modifiers, uint8_t keycode) {
  if (keycode == 0) return;
  if (modifiers & KEY_MOD_LCTRL)  keyboard.press(KEY_LCTRL);
  if (modifiers & KEY_MOD_LSHIFT) keyboard.press(KEY_LSHIFT);
  if (modifiers & KEY_MOD_LALT)   keyboard.press(KEY_LALT);
  if (modifiers & KEY_MOD_LGUI)   keyboard.press(KEY_LGUI);
  keyboard.press(keycode);
  delay(40);
  keyboard.releaseAll();
}

void executeAction(const ButtonAction& act) {
  if (act.type == TYPE_MEDIA) executeMedia(act.media);
  else if (act.type == TYPE_COMBO) executeCombo(act.modifiers, act.keycode);
}

// ===================== Работа с профилями =====================
void setDefaultProfileNames() {
  for (int i = 0; i < PROFILE_COUNT; i++) {
    snprintf(profileNames[i], 17, "Профиль %d", i + 1);
  }
}

void loadProfile(uint8_t idx) {
  if (idx >= PROFILE_COUNT) return;

  prefs.begin("profiles", true);
  String p = "p" + String(idx);

  currentOS = (HostOS)prefs.getUChar((p + "_os").c_str(), OS_WINDOWS);

  for (int i = 0; i < 4; i++) {
    String b = p + "_b" + String(i);
    btnConfig[i].shortPress.type      = (ActionType)prefs.getUChar((b + "st").c_str(), TYPE_MEDIA);
    btnConfig[i].shortPress.media     = (MediaAction)prefs.getUChar((b + "sm").c_str(), ACT_NONE);
    btnConfig[i].shortPress.modifiers = prefs.getUChar((b + "smod").c_str(), 0);
    btnConfig[i].shortPress.keycode   = prefs.getUChar((b + "sk").c_str(), 0);

    btnConfig[i].longPress.type      = (ActionType)prefs.getUChar((b + "lt").c_str(), TYPE_MEDIA);
    btnConfig[i].longPress.media     = (MediaAction)prefs.getUChar((b + "lm").c_str(), ACT_NONE);
    btnConfig[i].longPress.modifiers = prefs.getUChar((b + "lmod").c_str(), 0);
    btnConfig[i].longPress.keycode   = prefs.getUChar((b + "lk").c_str(), 0);
  }
  prefs.end();

  currentProfile = idx;

  // Запоминаем, какой профиль активен
  prefs.begin("btncfg", false);
  prefs.putUChar("curProf", currentProfile);
  prefs.end();
}

void saveProfile(uint8_t idx) {
  if (idx >= PROFILE_COUNT) return;

  prefs.begin("profiles", false);
  String p = "p" + String(idx);

  prefs.putUChar((p + "_os").c_str(), currentOS);

  for (int i = 0; i < 4; i++) {
    String b = p + "_b" + String(i);
    prefs.putUChar((b + "st").c_str(),  btnConfig[i].shortPress.type);
    prefs.putUChar((b + "sm").c_str(),  btnConfig[i].shortPress.media);
    prefs.putUChar((b + "smod").c_str(), btnConfig[i].shortPress.modifiers);
    prefs.putUChar((b + "sk").c_str(),  btnConfig[i].shortPress.keycode);

    prefs.putUChar((b + "lt").c_str(),  btnConfig[i].longPress.type);
    prefs.putUChar((b + "lm").c_str(),  btnConfig[i].longPress.media);
    prefs.putUChar((b + "lmod").c_str(), btnConfig[i].longPress.modifiers);
    prefs.putUChar((b + "lk").c_str(),  btnConfig[i].longPress.keycode);
  }
  prefs.end();

  currentProfile = idx;
  prefs.begin("btncfg", false);
  prefs.putUChar("curProf", currentProfile);
  prefs.end();
}

void loadProfileNames() {
  prefs.begin("profiles", true);
  for (int i = 0; i < PROFILE_COUNT; i++) {
    String key = "name" + String(i);
    String name = prefs.getString(key.c_str(), "");
    if (name.length() == 0) {
      snprintf(profileNames[i], 17, "Профиль %d", i + 1);
    } else {
      name.toCharArray(profileNames[i], 17);
    }
  }
  prefs.end();
}

void saveProfileName(uint8_t idx, const String& name) {
  if (idx >= PROFILE_COUNT) return;
  prefs.begin("profiles", false);
  prefs.putString(("name" + String(idx)).c_str(), name.substring(0, 16));
  prefs.end();
  name.substring(0, 16).toCharArray(profileNames[idx], 17);
}

// ===================== Загрузка при старте =====================
void loadAll() {
  setDefaultProfileNames();
  loadProfileNames();

  prefs.begin("btncfg", true);
  currentProfile = prefs.getUChar("curProf", 0);
  prefs.end();

  if (currentProfile >= PROFILE_COUNT) currentProfile = 0;
  loadProfile(currentProfile);

  // Если профили ещё пустые — ставим значения по умолчанию в профиль 0
  prefs.begin("profiles", true);
  bool firstRun = !prefs.isKey("p0_os");
  prefs.end();

  if (firstRun) {
    btnConfig[0].shortPress = {TYPE_MEDIA, ACT_VOL_DOWN, 0, 0};
    btnConfig[0].longPress  = {TYPE_MEDIA, ACT_MUTE, 0, 0};
    btnConfig[1].shortPress = {TYPE_MEDIA, ACT_PREV, 0, 0};
    btnConfig[1].longPress  = {TYPE_MEDIA, ACT_PREV, 0, 0};
    btnConfig[2].shortPress = {TYPE_MEDIA, ACT_NEXT, 0, 0};
    btnConfig[2].longPress  = {TYPE_MEDIA, ACT_NEXT, 0, 0};
    btnConfig[3].shortPress = {TYPE_MEDIA, ACT_VOL_UP, 0, 0};
    btnConfig[3].longPress  = {TYPE_MEDIA, ACT_PLAY_PAUSE, 0, 0};
    saveProfile(0);
  }
}

// ===================== HTML =====================
String modName(uint8_t mod) {
  if (currentOS == OS_MACOS) {
    if (mod == KEY_MOD_LCTRL)  return "Control";
    if (mod == KEY_MOD_LALT)   return "Option";
    if (mod == KEY_MOD_LSHIFT) return "Shift";
    if (mod == KEY_MOD_LGUI)   return "Command";
  } else {
    if (mod == KEY_MOD_LCTRL)  return "Ctrl";
    if (mod == KEY_MOD_LALT)   return "Alt";
    if (mod == KEY_MOD_LSHIFT) return "Shift";
    if (mod == KEY_MOD_LGUI)   return "Win";
  }
  return "?";
}

String buildKeySelect(const String& name, uint8_t selected) {
  String html = "<select name='" + name + "'>";
  for (int i = 0; i < KEY_LIST_SIZE; i++) {
    html += "<option value='" + String(keyList[i].code) + "'";
    if (keyList[i].code == selected) html += " selected";
    html += ">" + String(keyList[i].name) + "</option>";
  }
  html += "</select>";
  return html;
}

String buildMediaSelect(const String& name, MediaAction selected) {
  String html = "<select name='" + name + "'>";
  for (uint8_t i = 0; i < ACT_COUNT; i++) {
    html += "<option value='" + String(i) + "'";
    if (i == selected) html += " selected";
    html += ">" + String(mediaNames[i]) + "</option>";
  }
  html += "</select>";
  return html;
}

String buildActionUI(const String& prefix, const ButtonAction& act) {
  String html;

  // Тип действия
  html += "<label>Тип действия</label>";
  html += "<select name='" + prefix + "type' id='" + prefix + "type' onchange=\"toggleActionUI('" + prefix + "')\">";
  html += "<option value='0'" + String(act.type == TYPE_NONE  ? " selected" : "") + ">Нет действия</option>";
  html += "<option value='1'" + String(act.type == TYPE_MEDIA ? " selected" : "") + ">Медиа-команда</option>";
  html += "<option value='2'" + String(act.type == TYPE_COMBO ? " selected" : "") + ">Сочетание клавиш</option>";
  html += "</select>";

  // === Блок медиа ===
  html += "<div id='" + prefix + "media_block' style='display:" + String(act.type == TYPE_MEDIA ? "block" : "none") + "'>";
  html += "<label>Медиа-команда</label>";
  html += buildMediaSelect(prefix + "media", act.media);
  html += "</div>";

  // === Блок сочетания ===
  html += "<div id='" + prefix + "combo_block' style='display:" + String(act.type == TYPE_COMBO ? "block" : "none") + "'>";
  html += "<label>Модификаторы</label><div class='mods'>";

  uint8_t mods[4] = {KEY_MOD_LCTRL, KEY_MOD_LALT, KEY_MOD_LSHIFT, KEY_MOD_LGUI};
  for (int m = 0; m < 4; m++) {
    bool checked = act.modifiers & mods[m];
    html += "<label class='check'><input type='checkbox' name='" + prefix + "mod" + String(m) + "' value='1'";
    if (checked) html += " checked";
    html += "> " + modName(mods[m]) + "</label>";
  }
  html += "</div>";

  html += "<label>Основная клавиша</label>";
  html += buildKeySelect(prefix + "key", act.keycode);
  html += "</div>";

  return html;
}

// ===================== Web =====================
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Настройка кнопок</title>
<style>
  body{font-family:system-ui,sans-serif;max-width:520px;margin:16px auto;padding:0 12px;background:#f0f2f5}
  h1{font-size:1.35rem;text-align:center;margin-bottom:6px}
  .info{text-align:center;color:#666;font-size:0.9rem;margin-bottom:16px}
  .card{background:#fff;border-radius:12px;padding:16px;margin-bottom:14px;box-shadow:0 2px 8px rgba(0,0,0,.07)}
  .card h2{margin:0 0 12px;font-size:1.1rem}
  label{display:block;margin:10px 0 4px;font-size:0.9rem;color:#444}
  select,input[type=text]{width:100%;padding:10px;font-size:1rem;border-radius:8px;border:1px solid #ccc;box-sizing:border-box}
  .mods{display:flex;flex-wrap:wrap;gap:12px;margin:8px 0}
  .check{display:flex;align-items:center;gap:6px;font-size:0.95rem}
  button,.btn{width:100%;padding:12px;font-size:1rem;border:none;border-radius:10px;margin-top:8px;cursor:pointer}
  .btn-primary{background:#007aff;color:#fff}
  .btn-secondary{background:#e5e5ea;color:#000}
  .btn-danger{background:#ff3b30;color:#fff}
  .os-switch{display:flex;gap:24px;justify-content:center;margin:10px 0}
  .os-switch label{display:flex;align-items:center;gap:6px;font-size:1rem}
  .profile-row{display:flex;gap:8px;margin-top:8px}
  .profile-row select{flex:1}
  .profile-row button{width:auto;padding:10px 14px;margin-top:0}
  .small{font-size:0.85rem;color:#666;margin-top:6px}
</style>
<script>
function toggleActionUI(prefix) {
  const type = document.getElementById(prefix + 'type').value;
  document.getElementById(prefix + 'media_block').style.display = (type === '1') ? 'block' : 'none';
  document.getElementById(prefix + 'combo_block').style.display = (type === '2') ? 'block' : 'none';
}
</script>
</head>
<body>
  <h1>ESP32 Music Remote</h1>
  <p class="info">Режим настройки · Wi-Fi отключится через 3 минуты</p>

  <form action="/save" method="POST">
    <!-- Профили -->
    <div class="card">
      <h2>Профили</h2>
      <p class="small">Активный профиль: <b>)rawliteral";

  html += String(profileNames[currentProfile]);
  html += R"rawliteral(</b></p>

      <label>Выберите слот</label>
      <select name="profile_idx">
)rawliteral";

  for (int i = 0; i < PROFILE_COUNT; i++) {
    html += "<option value='" + String(i) + "'";
    if (i == currentProfile) html += " selected";
    html += ">" + String(i + 1) + ". " + String(profileNames[i]) + "</option>";
  }

  html += R"rawliteral(
      </select>

      <div class="profile-row">
        <button type="submit" name="action" value="load" class="btn btn-secondary">Загрузить</button>
        <button type="submit" name="action" value="save_slot" class="btn btn-primary">Сохранить в слот</button>
      </div>

      <label style="margin-top:14px">Имя слота</label>
      <input type="text" name="profile_name" maxlength="16" value=")rawliteral";

  html += String(profileNames[currentProfile]);
  html += R"rawliteral(" placeholder="Название профиля">
      <button type="submit" name="action" value="rename" class="btn btn-secondary">Переименовать слот</button>
    </div>

    <!-- ОС -->
    <div class="card">
      <h2>Операционная система</h2>
      <div class="os-switch">
)rawliteral";

  html += "<label><input type='radio' name='os' value='0'";
  if (currentOS == OS_WINDOWS) html += " checked";
  html += "> Windows</label>";
  html += "<label><input type='radio' name='os' value='1'";
  if (currentOS == OS_MACOS) html += " checked";
  html += "> macOS</label>";

  html += R"rawliteral(
      </div>
    </div>
)rawliteral";

  const char* titles[4] = {
    "Кнопка «Громкость −» (пин 8)",
    "Кнопка «Предыдущий»",
    "Кнопка «Следующий»",
    "Кнопка «Громкость +»"
  };

  for (int i = 0; i < 4; i++) {
    html += "<div class='card'><h2>" + String(titles[i]) + "</h2>";
    html += "<h3 style='font-size:0.95rem;margin:12px 0 6px;color:#555'>Короткое нажатие</h3>";
    html += buildActionUI("s" + String(i), btnConfig[i].shortPress);
    html += "<h3 style='font-size:0.95rem;margin:18px 0 6px;color:#555'>Длинное нажатие</h3>";
    html += buildActionUI("l" + String(i), btnConfig[i].longPress);
    html += "</div>";
  }

  html += R"rawliteral(
    <button type="submit" name="action" value="save_current" class="btn btn-primary">Сохранить текущие настройки</button>
  </form>
</body>
</html>
)rawliteral";

  server.send(200, "text/html; charset=utf-8", html);
}

void handleSave() {
  String action = server.hasArg("action") ? server.arg("action") : "save_current";
  int idx = server.hasArg("profile_idx") ? server.arg("profile_idx").toInt() : currentProfile;
  if (idx < 0 || idx >= PROFILE_COUNT) idx = 0;

  // Сначала читаем настройки кнопок и ОС из формы (на случай "Сохранить в слот" / "Сохранить текущие")
  if (server.hasArg("os")) {
    currentOS = (HostOS)server.arg("os").toInt();
  }

  for (int i = 0; i < 4; i++) {
    String pref = "s" + String(i);
    if (server.hasArg(pref + "type")) btnConfig[i].shortPress.type = (ActionType)server.arg(pref + "type").toInt();
    if (server.hasArg(pref + "media")) btnConfig[i].shortPress.media = (MediaAction)server.arg(pref + "media").toInt();
    btnConfig[i].shortPress.modifiers = 0;
    if (server.hasArg(pref + "mod0")) btnConfig[i].shortPress.modifiers |= KEY_MOD_LCTRL;
    if (server.hasArg(pref + "mod1")) btnConfig[i].shortPress.modifiers |= KEY_MOD_LALT;
    if (server.hasArg(pref + "mod2")) btnConfig[i].shortPress.modifiers |= KEY_MOD_LSHIFT;
    if (server.hasArg(pref + "mod3")) btnConfig[i].shortPress.modifiers |= KEY_MOD_LGUI;
    if (server.hasArg(pref + "key")) btnConfig[i].shortPress.keycode = server.arg(pref + "key").toInt();

    pref = "l" + String(i);
    if (server.hasArg(pref + "type")) btnConfig[i].longPress.type = (ActionType)server.arg(pref + "type").toInt();
    if (server.hasArg(pref + "media")) btnConfig[i].longPress.media = (MediaAction)server.arg(pref + "media").toInt();
    btnConfig[i].longPress.modifiers = 0;
    if (server.hasArg(pref + "mod0")) btnConfig[i].longPress.modifiers |= KEY_MOD_LCTRL;
    if (server.hasArg(pref + "mod1")) btnConfig[i].longPress.modifiers |= KEY_MOD_LALT;
    if (server.hasArg(pref + "mod2")) btnConfig[i].longPress.modifiers |= KEY_MOD_LSHIFT;
    if (server.hasArg(pref + "mod3")) btnConfig[i].longPress.modifiers |= KEY_MOD_LGUI;
    if (server.hasArg(pref + "key")) btnConfig[i].longPress.keycode = server.arg(pref + "key").toInt();
  }

  if (action == "load") {
    loadProfile(idx);
  }
  else if (action == "save_slot") {
    saveProfile(idx);
  }
  else if (action == "rename") {
    if (server.hasArg("profile_name")) {
      saveProfileName(idx, server.arg("profile_name"));
    }
  }
  else { // save_current
    saveProfile(currentProfile);
  }

  // Редирект обратно
  String html = R"rawliteral(
<!DOCTYPE html><html><head>
<meta charset="UTF-8"><meta http-equiv="refresh" content="0;url=/">
</head><body>Сохранено...</body></html>
)rawliteral";
  server.send(200, "text/html; charset=utf-8", html);
}

// ===================== Config mode =====================
void startConfigMode() {
  Serial.println("=== CONFIG MODE ===");
  WiFi.mode(WIFI_OFF); delay(200);
  WiFi.mode(WIFI_AP);  delay(200);

  if (WiFi.softAP(AP_SSID, AP_PASS, 6, 0, 4)) {
    esp_wifi_set_max_tx_power(40);
    Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("SoftAP failed");
    return;
  }

  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
  configMode = true;
  configStartTime = millis();
}

void stopConfigMode() {
  Serial.println("Config timeout — WiFi OFF");
  server.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  configMode = false;
}

// ===================== Setup & Loop =====================
void setup() {
  Serial.begin(115200);
  delay(200);

  for (int i = 0; i < 4; i++) pinMode(btnPins[i], INPUT_PULLUP);

  loadAll();

  if (digitalRead(BTN_VOL_DWN) == LOW) {
    delay(50);
    if (digitalRead(BTN_VOL_DWN) == LOW) startConfigMode();
  } else {
    Serial.println("Normal mode — only BLE");
  }

  keyboard.begin();
  Serial.println("BLE Keyboard ready");
}

void loop() {
  if (configMode) {
    server.handleClient();
    if (millis() - configStartTime > CONFIG_TIMEOUT) stopConfigMode();
  }

  if (!keyboard.isConnected()) {
    delay(20);
    return;
  }

  for (int i = 0; i < 4; i++) {
    bool state = digitalRead(btnPins[i]);

    if (state == LOW && lastState[i] == HIGH) {
      pressTime[i] = millis();
      longDone[i] = false;
    }
    if (state == LOW && !longDone[i] && millis() - pressTime[i] > LONG_PRESS_TIME) {
      executeAction(btnConfig[i].longPress);
      longDone[i] = true;
    }
    if (state == HIGH && lastState[i] == LOW) {
      if (!longDone[i]) executeAction(btnConfig[i].shortPress);
    }
    lastState[i] = state;
  }
  delay(20);
}