#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

/* ================= WIFI ================= */
const char* ssid = "Wokwi-GUEST";
const char* password = "";

/* ================= MQTT (HiveMQ Cloud) ================= */
const char* MQTT_HOST = "fc2100de598448ed8db99c8295a51745.s1.eu.hivemq.cloud";
const int   MQTT_PORT = 8883;
const char* MQTT_USER = "marwa";
const char* MQTT_PASS = "rrrtttyyy.@1M";

/* ================= TOPICS ================= */
const char* T_LUX         = "project/capteur/luminosite";

const char* T_CMD_MODE    = "project/commande/mode";
const char* T_CMD_INT     = "project/commande/intensite";
const char* T_CMD_COL     = "project/commande/couleur";
const char* T_CMD_SCENE   = "project/commande/scene";

const char* T_STAT_SYS    = "project/status/systeme";
const char* T_STAT_MODE   = "project/status/mode_actif";
const char* T_STAT_INT    = "project/status/intensite_appliquee";

/* ================= PINS ================= */
#define RED_PIN   25
#define GREEN_PIN 26
#define BLUE_PIN  27
#define LDR_PIN   34

/* ================= PWM ================= */
#define PWM_FREQ 5000
#define PWM_RES  8

WiFiClientSecure net;
PubSubClient mqtt(net);

/* ================= STATE ================= */
String currentMode = "auto"; // texte publié dans project/status/mode_actif
int currentPct = 0;

/* ================= RGB HELPERS ================= */
void setRGB(int r, int g, int b) {
  r = constrain(r, 0, 255);
  g = constrain(g, 0, 255);
  b = constrain(b, 0, 255);
  // Wokwi utilise ledcWrite directement sur les pins
  ledcWrite(RED_PIN, r);
  ledcWrite(GREEN_PIN, g);
  ledcWrite(BLUE_PIN, b);
}

void applyIntensityPct(int pct) {
  pct = constrain(pct, 0, 100);
  currentPct = pct;
  int pwm = map(pct, 0, 100, 0, 255);
  // intensité simple en blanc (R=G=B)
  setRGB(pwm, pwm, pwm);
}

void applySceneColorByName(const String& name) {
  if (name == "concentration")      setRGB(0, 120, 255);
  else if (name == "reunion")       setRGB(255, 255, 255);
  else if (name == "detente")       setRGB(255, 140, 0);
  else if (name == "energie")       setRGB(0, 255, 120);
  else if (name == "auto")          setRGB(255, 255, 255);
  else if (name == "manuel")        setRGB(255, 255, 255);
}

bool parseRGBString(const String& s, int &r, int &g, int &b) {
  int p1 = s.indexOf(',');
  int p2 = s.indexOf(',', p1 + 1);
  if (p1 < 0 || p2 < 0) return false;
  r = s.substring(0, p1).toInt();
  g = s.substring(p1 + 1, p2).toInt();
  b = s.substring(p2 + 1).toInt();
  return true;
}

/* ================= PUBLISH HELPERS ================= */
void publishSystemStatus(const char* etat) {
  StaticJsonDocument<64> doc;
  doc["etat"] = etat;
  char buf[64];
  serializeJson(doc, buf);
  mqtt.publish(T_STAT_SYS, buf, true); // retain
}

void publishModeStatus(const String& mode) {
  mqtt.publish(T_STAT_MODE, mode.c_str(), true); // retain, STRING
}

void publishIntensityStatus(int pct) {
  StaticJsonDocument<64> doc;
  doc["pct"] = pct;
  char buf[64];
  serializeJson(doc, buf);
  mqtt.publish(T_STAT_INT, buf, true); // retain
}

/* ================= MQTT CALLBACK ================= */
void onMessage(char* topic, byte* payload, unsigned int length) {
  String t = topic;
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  Serial.print("📩 Message reçu - Topic: ");
  Serial.print(t);
  Serial.print(" | Payload: ");
  Serial.println(msg);

  StaticJsonDocument<192> doc;
  DeserializationError error = deserializeJson(doc, msg);
  if (error) {
    Serial.print("❌ Erreur JSON: ");
    Serial.println(error.c_str());
    return;
  }

  // ----- commande intensité -----
  if (t == T_CMD_INT) {
    if (doc.containsKey("pct")) {
      int pct = doc["pct"];
      applyIntensityPct(pct);
      publishIntensityStatus(pct);
      Serial.print("✅ Intensité appliquée: ");
      Serial.print(pct);
      Serial.println("%");
    }
    return;
  }

  // ----- commande mode -----
  if (t == T_CMD_MODE) {
    if (doc.containsKey("mode")) {
      String mode = doc["mode"].as<String>();
      mode.toLowerCase();
      
      if (mode.length() == 0) return;

      // Remarque:
      // - "auto" est utilisé par Marwa pour la logique auto (lux + météo).
      // - "manuel" sert juste à activer le lock côté Marwa.
      // - Les scènes ("detente", "concentration"...) sont aussi des modes affichés.
      currentMode = mode;

      // Si on reçoit une scène dans "mode" (possible depuis certains dashboards),
      // on applique la couleur correspondante :
      if (currentMode != "auto" && currentMode != "manuel") {
        applySceneColorByName(currentMode);
      }

      publishModeStatus(currentMode);
      Serial.print("✅ Mode changé: ");
      Serial.println(currentMode);
    }
    return;
  }

  // ----- commande couleur -----
  if (t == T_CMD_COL) {
    if (doc.containsKey("couleur")) {
      String c = doc["couleur"].as<String>();
      int r, g, b;
      if (parseRGBString(c, r, g, b)) {
        setRGB(r, g, b);
        Serial.print("✅ Couleur appliquée: ");
        Serial.print(r);
        Serial.print(",");
        Serial.print(g);
        Serial.print(",");
        Serial.println(b);
      }
    }
    return;
  }

  // ----- commande scène -----
  if (t == T_CMD_SCENE) {
    if (doc.containsKey("scene")) {
      String scene = doc["scene"].as<String>();
      scene.toLowerCase();
      
      if (scene.length() == 0) return;

      // mêmes presets que Marwa
      int pct = 60;
      String col = "255,255,255";

      if (scene == "detente")              { pct = 40; col = "255,140,0"; }
      else if (scene == "concentration")   { pct = 70; col = "0,120,255"; }
      else if (scene == "reunion")         { pct = 80; col = "255,255,255"; }
      else if (scene == "energie")         { pct = 55; col = "0,255,120"; }

      currentMode = scene;
      publishModeStatus(scene);

      applyIntensityPct(pct);
      publishIntensityStatus(pct);

      int r, g, b;
      if (parseRGBString(col, r, g, b)) setRGB(r, g, b);

      Serial.print("✅ Scène appliquée: ");
      Serial.println(scene);
    }
    return;
  }
}

/* ================= WIFI ================= */
void connectWiFi() {
  Serial.println();
  Serial.println("╔═══════════════════════════════════════╗");
  Serial.println("║        CONNEXION AU WIFI...           ║");
  Serial.println("╚═══════════════════════════════════════╝");
  
  Serial.print("📶 Connexion à: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  int tentatives = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    tentatives++;
    if (tentatives > 40) { // 20 secondes timeout
      Serial.println();
      Serial.println("❌ Échec de connexion WiFi");
      return;
    }
  }
  
  Serial.println();
  Serial.println("✅ WIFI CONNECTÉ!");
  Serial.print("📍 Adresse IP: ");
  Serial.println(WiFi.localIP());
}

/* ================= MQTT ================= */
void connectMQTT() {
  Serial.println();
  Serial.println("╔═══════════════════════════════════════╗");
  Serial.println("║        CONNEXION MQTT...              ║");
  Serial.println("╚═══════════════════════════════════════╝");
  
  net.setInsecure(); // OK pour proto; en prod: utiliser le CA root
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMessage);

  // Will message: {"etat":"offline"} (retain)
  StaticJsonDocument<64> willDoc;
  willDoc["etat"] = "offline";
  char willBuf[64];
  serializeJson(willDoc, willBuf);

  int tentatives = 0;
  while (!mqtt.connected()) {
    Serial.print("🔄 Tentative MQTT #");
    Serial.println(tentatives + 1);
    
    if (mqtt.connect("esp32-asmaa", MQTT_USER, MQTT_PASS, T_STAT_SYS, 0, true, willBuf)) {
      Serial.println("✅ MQTT CONNECTÉ!");
      
      publishSystemStatus("online");
      Serial.println("📤 État système: online");
      
      mqtt.subscribe(T_CMD_MODE);
      mqtt.subscribe(T_CMD_INT);
      mqtt.subscribe(T_CMD_COL);
      mqtt.subscribe(T_CMD_SCENE);
      Serial.println("👂 Abonné aux topics de commande");
      
      publishModeStatus(currentMode);
      publishIntensityStatus(currentPct);
    } else {
      Serial.print("❌ Échec (code: ");
      Serial.print(mqtt.state());
      Serial.println(")");
      delay(2000);
    }
    
    tentatives++;
    if (tentatives > 5) {
      Serial.println("⚠️  Impossible de se connecter à MQTT");
      break;
    }
  }
}

/* ================= SETUP ================= */
void setup() {
  Serial.begin(115200);
  delay(2000); // Attendre pour le Serial
  
  Serial.println();
  Serial.println("╔═══════════════════════════════════════╗");
  Serial.println("║      ESP32 SMART LIGHTING SYSTEM      ║");
  Serial.println("║           DÉMARRAGE...                ║");
  Serial.println("╚═══════════════════════════════════════╝");
  Serial.println();

  // Configuration PWM pour Wokwi
  Serial.println("⚙️  Configuration des LEDs PWM...");
  
  // Dans Wokwi, on utilise ledcAttach directement
  ledcAttach(RED_PIN, PWM_FREQ, PWM_RES);
  ledcAttach(GREEN_PIN, PWM_FREQ, PWM_RES);
  ledcAttach(BLUE_PIN, PWM_FREQ, PWM_RES);
  
  Serial.println("✅ LEDs configurées");

  // Allumer en blanc faible au démarrage
  Serial.println("💡 Test des LEDs...");
  setRGB(50, 50, 50);
  delay(1000);
  setRGB(0, 0, 0);
  Serial.println("✅ Test LEDs OK");

  connectWiFi();
  connectMQTT();
  
  Serial.println();
  Serial.println("╔═══════════════════════════════════════╗");
  Serial.println("║      SYSTÈME PRÊT À FONCTIONNER       ║");
  Serial.println("╚═══════════════════════════════════════╝");
  Serial.println();
}

/* ================= LOOP ================= */
unsigned long lastSend = 0;
unsigned long lastDisplay = 0;

void loop() {
  // Reconnexion MQTT si nécessaire
  if (!mqtt.connected()) {
    Serial.println("⚠️  MQTT déconnecté, tentative de reconnexion...");
    connectMQTT();
  }
  
  mqtt.loop();

  // Envoyer la luminosité toutes les 2 secondes
  if (millis() - lastSend > 2000) {
    lastSend = millis();
    
    int raw = analogRead(LDR_PIN);
    int lux = map(raw, 0, 4095, 0, 1000);
    
    Serial.print("🌞 Luminosité mesurée: ");
    Serial.print(lux);
    Serial.print(" lux (raw: ");
    Serial.print(raw);
    Serial.println(")");

    StaticJsonDocument<96> doc;
    doc["lux"] = lux;
    doc["raw"] = raw;

    char buf[96];
    serializeJson(doc, buf);
    
    if (mqtt.publish(T_LUX, buf, false)) {
      Serial.println("📤 Luminosité envoyée au serveur");
    } else {
      Serial.println("❌ Erreur d'envoi de la luminosité");
    }
  }

  // Afficher le status toutes les 10 secondes
  if (millis() - lastDisplay > 10000) {
    lastDisplay = millis();
    
    Serial.println();
    Serial.println("──────────────────────────────────────");
    Serial.println("📊 SYNTHÈSE PERIODIQUE");
    Serial.println("──────────────────────────────────────");
    Serial.print("🎛️  Mode actuel: ");
    Serial.println(currentMode);
    Serial.print("💡 Intensité: ");
    Serial.print(currentPct);
    Serial.println("%");
    Serial.print("📡 WiFi: ");
    Serial.println(WiFi.status() == WL_CONNECTED ? "Connecté" : "Déconnecté");
    Serial.print("📡 MQTT: ");
    Serial.println(mqtt.connected() ? "Connecté" : "Déconnecté");
    Serial.println("──────────────────────────────────────");
  }
  
  delay(100); // Petite pause pour éviter de surcharger
}