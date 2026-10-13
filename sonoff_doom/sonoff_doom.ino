/*
  SONOFF DOOM 1.0 (firmware v6.5) - still a smart switch: TOGGLE (30 ms pulse) + SWITCH (on/off) in the page,
                   first-boot Wi-Fi setup portal, admin login, physical button, /relay API
  (v5: doors (E / right click), imps that wake up and open doors, cheat popups)
  (ESP8285, Sonoff RF R2 POWER v1.0)
  - Walls, imp, weapons, palette and COLORMAP lighting taken from doom1.wad
    (converted on the PC by wad2sonoff.py -> doom_assets.h, read from flash)
  - Renders 16 columns at a time (no full framebuffer), streams them via WebSocket (port 81),
    only the pixels that differ from floor/ceiling are sent
  - Browser: classic Doom menu, status bar with Doomguy face, music (MUS) and sound effects
  - Commands: keyboard + mouse (pointer lock), mouse wheel / 2 / 3 = change weapon, E / right click = use
  - Shooting: LED blink only (the relay drives the load: TOGGLE / SWITCH / physical button / API)
  - First boot: open Wi-Fi "SONOFF-DOOM-SETUP" -> captive page -> home Wi-Fi + admin user/password
  - Physical button: short press = toggle load, hold 10 s = factory reset (back to setup)
  - API: http://<ip>/relay?cmd=on|off|toggle|pulse  (same admin user/password, Basic auth)
  - Telemetry: fps, heap, render/send time per frame, KB/s

  BEFORE COMPILING: python wad2sonoff.py doom1.wad   (in this folder, rerun after editing index_html.h)
  OTA: network port "sonoff-doom" in Arduino IDE or http://<ip>/update (password = admin password)

  Board: "Generic ESP8285 Module", CPU 160 MHz, Flash "1MB (FS:none OTA:~502KB)"
  Libraries: ESP8266 core, "WebSockets" by Markus Sattler (Links2004)
*/
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoOTA.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <EEPROM.h>
#if __has_include("doom_assets.h")
#include "doom_assets.h"

#else
#error "doom_assets.h missing: first run 'python wad2sonoff.py doom1.wad' in the sketch folder"
#endif
#if !defined(DA_STBAR_LEN) || !defined(DA_SPR_CROPPED) || !defined(DA_DOOR_TILE)
#error "doom_assets.h is old: regenerate it with 'python wad2sonoff.py doom1.wad'"
#endif
#ifndef DA_INDEX_LEN
#include "index_html.h"          // fallback: uncompressed page (d
oom_assets.h generated without --html)
#endif

// ---------------- CONFIG ----------------
// Wi-Fi and admin credentials are NOT here any more: they are set in the setup page at first boot
// and stored in flash (EEPROM sector). Hold the physical button 10 s to erase them.
const char* SETUP_SSID = "SONOFF-DOOM-SETUP";   // open hotspot for the first configuration
const char* HOSTNAME   = "sonoff-doom";         // http://sonoff-doom.local + OTA port name
#define DEFAULT_USER   "doom-admin"
#define PULSE_MS       30             // TOGGLE: relay pulse length (the LED follows the relay for the same time)
#define WIFI_FAIL_MS   20000          // home Wi-Fi not reachable -> setup hotspot again (pages stay protected)
#define RESET_HOLD_MS  10000          // physical button held -> factory reset
#define FRAME_MS      40              // target ~25 fps (actual fps measured on the HUD)
#define MOUSE_RAD     0.0025f         // radians per mouse unit (browser already applies sensitivity)
#define MAX_TURN      0.30f           // max rotation applied per game update (radians): no sudden spins
#define MAX_BACKLOG   0.60f           // mouse rotation still pending beyond this is dropped
#define IMP_HP        6               // imp health in damage units
#define PISTOL_DMG    3               // pistol: 2 hits kill an imp
#define PELLET_DMG    2               // shotgun: 7 pellets, 3 hits kill an imp
#define PELLETS       7
#define PELLET_SPREAD 0.098f          // +-5.6 degrees, like Doom
#define PISTOL_MS     350             // fire rate
#define SHOTGUN_MS    1000            // shotgun fire + pump (Doom: 37 tics)
#define SWITCH_MS     360             // lower + raise weapon
#define IMP_ATK_MS    1000            // imp melee cooldown (damage 3..24 like Doom)
#define START_AMMO    50              // bullets at start (as in Doom)
#define MAX_AMMO      200
#define START_SHELLS  8               // shells at start (Doom shotgun pickup)
#define MAX_SHELLS    50
#define AMMO_PER_KILL 10              // bullets gained per kill (not in Doom: no pickups here)
#define SHELLS_PER_KILL 4             // shells gained per kill
#define DEATH_MS      3000            // dead time before restart
#define LIGHT_K       1.5f            // distance -> light level (higher = darker sooner)
#define LIGHT_MAX     (DA_LIGHTS - 3) // darkest level used
#define DOOR_SPEED    2.0f            // door open fraction per second (Doom: ~0.5 s)
#define DOOR_WAIT_MS  4300            // door stays open (Doom VDOORWAIT = 150 tics)
#define DOOR_PASS     0.85f           // open fraction needed to walk through
#define USE_RANGE     1.3f            // reach of the "use" key (map units)
#define WAKE_DIST     12.0f           // imps notice the player within this range (if visible)
#define NOISE_DIST    7.0f            // gunshots wake imps within this range

// Weapon frames in doom_assets.h (order of WEAPON_FRAMES in wad2sonoff.py)
#define WPN_PISTOL_IDLE  0
#define WPN_PISTOL_FIRE  1
#define WPN_SHOTGUN_IDLE 2
#define WPN_SHOTGUN_F1   3
#define WPN_SHOTGUN_F2   4
#define WPN_SHOTGUN_B    5
#define WPN_SHOTGUN_C    6
#define WPN_SHOTGUN_D    7
#if DA_WPN_FRAMES < 8
#error "doom_assets.h is old: regenerate it with 'python wad2sonoff.py doom1.wad' (shotgun needed)"
#endif

// Sonoff Basic R2 pins (outputs only)
#define PIN_RELAY 12
#define PIN_LED   13   // active LOW (on = load on)
#define PIN_BTN   0    // physical button (active LOW)
void ledFlash(uint32_t ms);
void applyLed();

// Input bitmask (same as the browser)
#define K_FWD   1
#define K_BACK  2
#define K_LEFT  4
#define K_RIGHT 8
#define K_STRL  16
#define K_STRR  32
#define K_FIRE  64

// ---------------- RENDER ----------------
#define W     128
#define H     80
#define STRIP 16
#define TEX   DA_TEX
#define SPRS  DA_SPR
#define MAPW  16
#define MAPH  16

// 1-3 walls, 4 door. Start room (south) -> door -> corridor -> doors -> west / east rooms
static const char MAP_DATA[MAPH][MAPW + 1] = {
  "1111111111111111",
  "1000000130000001",
  "1022000130003301",
  "1020000130000301",
  "1000000130000001",
  "1000000130000001",
  "1000000130000001",
  "1114111111114111",
  "1000000000000001",
  "1000000000000001",
  "1222222422222221",
  "1000000000000001",
  "1000330000330001",
  "1000000000000001",
  "1000000000000001",
  "1111111111111111"
};
static inline uint8_t mapAt(int x, int y) {
  if (x < 0 || y < 0 || x >= MAPW || y >= MAPH) return 1;
  return MAP_DATA[y][x] - '0';
}

// ---------------- DOORS ----------------
#define MAX_DOORS 8
struct Door { uint8_t x, y; float open; int8_t dir; uint32_t openedAt; };   // dir: 1 opening, -1 closing
Door doors[MAX_DOORS];
uint8_t ndoors = 0;

int doorAt(int x, int y) {
  for (int i = 0; i < ndoors; i++) if (doors[i].x == x && doors[i].y == y) return i;
  return -1;
}
void initDoors() {
  ndoors = 0;
  for (int y = 0; y < MAPH; y++)
    for (int x = 0; x < MAPW; x++)
      if (mapAt(x, y) == DA_DOOR_TILE && ndoors < MAX_DOORS) {
        doors[ndoors].x = x; doors[ndoors].y = y; doors[ndoors].open = 0; doors[ndoors].dir = 0; ndoors++;
      }
}
// blocks movement / bullets / sight?
static inline bool solidAt(int x, int y) {
  uint8_t t = mapAt(x, y);
  if (!t) return false;
  if (t != DA_DOOR_TILE) return true;
  int d = doorAt(x, y);
  return d < 0 || doors[d].open < DOOR_PASS;
}

ESP8266WebServer server(80);
WebSocketsServer ws(81);
ESP8266HTTPUpdateServer httpUpdater;
bool otaBusy = false;
int8_t client = -1;

// Game / menu state
bool paused = true;             // starts in the menu
bool needFrame = false;         // render one frame while paused (menu background)
int8_t previewNum = -1;         // browser that needs the fixed wall scene as menu background
bool previewMode = false;       // true only while that scene is rendered (no sprites, no zbuf, pistol idle)
#define PREVIEW_X 7.5f          // menu background: STARTAN3 wall of the start room, ~1 map unit away,
#define PREVIEW_Y 13.95f        //   same view for everybody, whatever the game is doing
#define PREVIEW_ANG (PI / 2)
uint8_t skill = 2;              // 0 I'm too young to die .. 4 Nightmare

// Player
float px = 7.5f, py = 13.5f, ang = -PI / 2;
float dirX, dirY, planeX, planeY;
int hp = 100, ammo = START_AMMO, shells = START_SHELLS;
uint8_t weapon = 0, nextWeapon = 0;   // 0 pistol, 1 shotgun
uint32_t switchAt = 0;                // weapon change in progress since
bool dead = false;
uint32_t deathAt = 0;
uint16_t kills = 0;
float bobPhase = 0, bobAmp = 0;

// Enemies (awake = has seen or heard the player, like Doom monsters)
struct Enemy { float x, y, sx, sy; bool alive; int8_t hp; uint32_t deadAt, painAt; float walk; uint32_t atkAt; bool awake; };
Enemy en[] = {
  {3.5f, 4.5f, 3.5f, 4.5f, true, IMP_HP, 0, 0, 0, 0, false},
  {11.5f, 4.5f, 11.5f, 4.5f, true, IMP_HP, 0, 0, 0, 0, false},
  {12.5f, 8.5f, 12.5f, 8.5f, true, IMP_HP, 0, 0, 0, 0, false},
  {4.5f, 2.5f, 4.5f, 2.5f, true, IMP_HP, 0, 0, 0, 0, false},
};
const int NEN = sizeof(en) / sizeof(en[0]);

// Per-frame projected sprites
struct SprProj { float tY; int screenX, size; uint8_t idx, frame, light; };
SprProj spr[NEN];
int nspr = 0;

// Per-frame weapon state
int wpnFrame = 0, wpnX = 0, wpnY = 0;

float zbuf[W];
uint8_t buf[2 + STRIP * (2 + H)];   // per column: start, length, only the non floor/ceiling pixels
uint8_t colTmp[H];
uint8_t cmap[DA_LIGHTS][256];   // COLORMAP copied to RAM (fast lookup)
uint8_t rowCol[H];              // pre-shaded floor/ceiling colour per row

uint8_t webKeys = 0;
int32_t lastYaw = 0;            // last absolute mouse yaw received from the browser (mouse units)
bool yawInit = false;
float yawPending = 0;           // rotation still to apply (radians)
uint32_t lastFire = 0, ledRestore = 0, lastFrame = 0, lastStats = 0, lastUpd = 0;
uint16_t frames = 0; float fps = 0;
uint32_t tRender = 0, tSend = 0, bytesSent = 0;   // per-second accumulators (us, us, bytes)

static inline uint8_t lightFor(float dist) {
  int l = (int)(dist * LIGHT_K);
  return l < 0 ? 0 : (l > LIGHT_MAX ? LIGHT_MAX : l);
}

void initTables() {
  memcpy_P(cmap, DA_CMAP, sizeof(cmap));
  for (int y = 0; y < H; y++) {
    float p = fabsf(y - H / 2 + 0.5f);          // rows from horizon
    float rowDist = (H / 2.0f) / p;             // floor/ceiling distance seen by this row
    rowCol[y] = cmap[lightFor(rowDist)][y < H / 2 ? DA_CEIL : DA_FLOOR];
  }
}

// ---------------- GAME ----------------
void updateDir() {
  dirX = cosf(ang); dirY = sinf(ang);
  planeX = -dirY * 0.66f; planeY = dirX * 0.66f;
}

bool freeAt(float x, float y) {
  const float m = 0.2f;
  return !solidAt(x - m, y - m) && !solidAt(x + m, y - m) && !solidAt(x - m, y + m) && !solidAt(x + m, y + m);
}

// Line of sight between two points (walls and closed doors block)
bool lineOfSight(float x0, float y0, float x1, float y1) {
  float dx = x1 - x0, dy = y1 - y0, d = sqrtf(dx * dx + dy * dy);
  int n = (int)(d * 8) + 1;
  for (int i = 1; i < n; i++) {
    float t = (float)i / n;
    if (solidAt((int)(x0 + dx * t), (int)(y0 + dy * t))) return false;
  }
  return true;
}

void doorEvent(int i, int open) {
  if (client < 0) return;
  float dx = doors[i].x + 0.5f - px, dy = doors[i].y + 0.5f - py;
  char m[48];
  snprintf(m, sizeof(m), "{\"ev\":\"door\",\"o\":%d,\"d\":%.1f}", open, sqrtf(dx * dx + dy * dy));
  ws.sendTXT(client, m);
}
void openDoor(int i) {
  if (doors[i].dir == 1 || (doors[i].open >= 1 && doors[i].dir == 0)) { doors[i].openedAt = millis(); return; }
  doors[i].dir = 1; doorEvent(i, 1);
}
bool occupied(int x, int y) {          // player or a live imp inside the door tile
  if ((int)px == x && (int)py == y) return true;
  for (int i = 0; i < NEN; i++) if (en[i].alive && (int)en[i].x == x && (int)en[i].y == y) return true;
  return false;
}
void updateDoors(float dt, uint32_t now) {
  for (int i = 0; i < ndoors; i++) {
    Door &d = doors[i];
    if (d.dir == 1) {
      d.open += DOOR_SPEED * dt;
      if (d.open >= 1) { d.open = 1; d.dir = 0; d.openedAt = now; }
    } else if (d.dir == -1) {
      if (occupied(d.x, d.y)) { d.dir = 1; doorEvent(i, 1); continue; }   // Doom: blocked door goes back up
      d.open -= DOOR_SPEED * dt;
      if (d.open <= 0) { d.open = 0; d.dir = 0; }
    } else if (d.open >= 1 && now - d.openedAt > DOOR_WAIT_MS && !occupied(d.x, d.y)) {
      d.dir = -1; doorEvent(i, 0);
    }
  }
}

// "Use" key: open the door in front of the player, or grunt like Doom
void doUse() {
  if (dead) return;
  for (float t = 0.3f; t <= USE_RANGE; t += 0.1f) {
    int x = (int)(px + dirX * t), y = (int)(py + dirY * t);
    uint8_t tile = mapAt(x, y);
    if (!tile) continue;
    int d = tile == DA_DOOR_TILE ? doorAt(x, y) : -1;
    if (d >= 0) {
      if (doors[d].dir == 1 || doors[d].open >= 1) { doors[d].dir = -1; doorEvent(d, 0); }   // use again = close
      else openDoor(d);
      return;
    }
    break;
  }
  if (client >= 0) ws.sendTXT(client, "{\"ev\":\"noway\"}");
}

bool tryMove(float &x, float &y, float dx, float dy) {
  bool moved = false;
  if (freeAt(x + dx, y)) { x += dx; moved = true; }
  if (freeAt(x, y + dy)) { y += dy; moved = true; }
  return moved;
}

// Skill tuning (Doom-like): damage taken, imp speed, imp respawn time
float skillDmg()   { return skill == 0 ? 0.5f : 1.0f; }
float impSpeed()   { return skill >= 4 ? 1.2f : (skill == 3 ? 0.85f : 0.7f); }
uint32_t respawnMs() { return skill >= 4 ? 4000 : 8000; }

void sendState() {
  if (client < 0) return;
  char m[112];
  snprintf(m, sizeof(m), "{\"st\":1,\"hp\":%d,\"am\":%d,\"sh\":%d,\"wp\":%d,\"dead\":%d,\"pause\":%d,\"sk\":%d}",
           hp, ammo, shells, weapon, dead ? 1 : 0, paused ? 1 : 0, skill);
  ws.sendTXT(client, m);
}

void resetGame() {
  px = 7.5f; py = 13.5f; ang = -PI / 2; hp = 100; ammo = START_AMMO; shells = START_SHELLS; kills = 0; dead = false;
  weapon = nextWeapon = 0; switchAt = 0; lastFire = 0;
  initDoors();
  for (int i = 0; i < NEN; i++) {
    en[i].x = en[i].sx; en[i].y = en[i].sy; en[i].alive = true; en[i].hp = IMP_HP;
    en[i].painAt = 0; en[i].atkAt = 0; en[i].walk = 0; en[i].awake = false;
  }
  updateDir();
  sendState();
}

bool hasAmmo(uint8_t w) { return w == 0 ? ammo > 0 : shells > 0; }

void selectWeapon(uint8_t w) {
  if (w > 1 || dead || !hasAmmo(w)) return;          // like Doom: no switching to an empty weapon
  uint8_t target = switchAt ? nextWeapon : weapon;
  if (w == target) return;
  uint32_t now = millis();
  if (!switchAt) switchAt = now;
  nextWeapon = w;
  if (client >= 0) {
    char m[40];
    snprintf(m, sizeof(m), "{\"ev\":\"wsw\",\"wp\":%d}", w);
    ws.sendTXT(client, m);
  }
}

// Takes the index (not Enemy&): Arduino IDE auto-generates prototypes at the top of the
// sketch, before struct Enemy is declared, so custom types in signatures fail to compile.
uint8_t enemyFrame(int i, uint32_t now) {
  const Enemy &e = en[i];
  if (!e.alive) {
    uint32_t t = (now - e.deadAt) / 110;
    return SPR_DIE0 + (t >= SPR_DIE_N ? SPR_DIE_N - 1 : t);
  }
  if (now - e.painAt < 160) return SPR_PAIN;
  return SPR_WALK0 + (((uint32_t)(e.walk * 3.0f)) & 3);
}

void projectSprites(uint32_t now) {
  if (previewMode) return;                           // wall scene: no imps
  float invDet = 1.0f / (planeX * dirY - dirX * planeY);
  nspr = 0;
  for (int i = 0; i < NEN; i++) {
    float sx = en[i].x - px, sy = en[i].y - py;
    float tX = invDet * (dirY * sx - dirX * sy);
    float tY = invDet * (-planeY * sx + planeX * sy);
    if (tY < 0.2f) continue;
    SprProj p;
    p.tY = tY; p.idx = i;
    p.screenX = (int)((W / 2) * (1 + tX / tY));
    p.size = (int)(H / tY);
    p.frame = enemyFrame(i, now);
    p.light = lightFor(tY);
    spr[nspr++] = p;
  }
  // sort far -> near
  for (int a = 0; a < nspr; a++)
    for (int b = a + 1; b < nspr; b++)
      if (spr[b].tY > spr[a].tY) { SprProj t = spr[a]; spr[a] = spr[b]; spr[b] = t; }
}

// Nearest live imp hit by a ray through screen column col (-1 = none)
int traceShot(int col) {
  if (col < 0 || col >= W) return -1;
  int best = -1; float bestD = 1e9;
  for (int k = 0; k < nspr; k++) {
    if (!en[spr[k].idx].alive) continue;
    if (abs(spr[k].screenX - col) < spr[k].size / 4 + 1 && spr[k].tY < zbuf[col] && spr[k].tY < bestD) {
      best = spr[k].idx; bestD = spr[k].tY;
    }
  }
  return best;
}

void doFire() {
  uint32_t now = millis();
  if (dead || switchAt) return;
  if (now - lastFire < (weapon ? SHOTGUN_MS : PISTOL_MS)) return;
  if (!hasAmmo(weapon)) {                              // out of ammo: switch like Doom
    if (hasAmmo(1 - weapon)) selectWeapon(1 - weapon);
    return;
  }
  lastFire = now;
  if (weapon == 0) ammo--; else shells--;
  ledFlash(weapon ? 120 : 70);                          // muzzle flash on the LED (the relay is the load)
  for (int i = 0; i < NEN; i++) {                       // gunshot noise wakes nearby imps
    float dx = en[i].x - px, dy = en[i].y - py;
    if (dx * dx + dy * dy < NOISE_DIST * NOISE_DIST) en[i].awake = true;
  }

  int8_t dmg[NEN] = {0};
  if (weapon == 0) {
    int t = traceShot(W / 2);
    if (t >= 0) dmg[t] += PISTOL_DMG;
  } else {
    for (int p = 0; p < PELLETS; p++) {                // 7 pellets with random spread
      float a = ((float)random(-1000, 1001) / 1000.0f) * PELLET_SPREAD;
      int col = W / 2 + (int)roundf(tanf(a) / 0.66f * (W / 2));
      int t = traceShot(col);
      if (t >= 0) dmg[t] += PELLET_DMG;
    }
  }
  int hits = 0, killsNow = 0;
  for (int i = 0; i < NEN; i++) {
    if (!dmg[i]) continue;
    Enemy &e = en[i];
    hits++;
    e.painAt = now; e.awake = true;
    e.hp -= dmg[i];
    if (e.hp <= 0) {
      e.alive = false; e.deadAt = now; kills++; killsNow++;
      ammo += AMMO_PER_KILL;   if (ammo > MAX_AMMO) ammo = MAX_AMMO;
      shells += SHELLS_PER_KILL; if (shells > MAX_SHELLS) shells = MAX_SHELLS;
    }
  }
  if (client >= 0) {
    char m[96];
    snprintf(m, sizeof(m), "{\"ev\":\"fire\",\"wp\":%d,\"hit\":%d,\"kill\":%d,\"am\":%d,\"sh\":%d}",
             weapon, hits, killsNow, ammo, shells);
    ws.sendTXT(client, m);
  }
}

void updateGame(float dt) {
  uint32_t now = millis();
  if (dead) {                                        // Doomguy is dead: wait, then restart
    yawPending = 0;
    if (now - deathAt > DEATH_MS) resetGame();
    return;
  }
  // weapon change: swap at the bottom of the lowering animation
  if (switchAt && weapon != nextWeapon && now - switchAt >= SWITCH_MS / 2) { weapon = nextWeapon; sendState(); }
  if (switchAt && now - switchAt >= SWITCH_MS) switchAt = 0;

  uint8_t k = webKeys;
  const float mv = 2.5f * dt, rot = 2.2f * dt;
  if (k & K_LEFT)  ang -= rot;
  if (k & K_RIGHT) ang += rot;
  if (yawPending != 0) {                             // smooth mouse: bounded step per update
    if (yawPending > MAX_BACKLOG) yawPending = MAX_BACKLOG;
    else if (yawPending < -MAX_BACKLOG) yawPending = -MAX_BACKLOG;
    float step = yawPending;
    if (step > MAX_TURN) step = MAX_TURN; else if (step < -MAX_TURN) step = -MAX_TURN;
    ang += step; yawPending -= step;
  }
  if (ang > PI) ang -= 2 * PI; else if (ang < -PI) ang += 2 * PI;
  updateDir();
  bool walking = false;
  if (k & K_FWD)  walking |= tryMove(px, py, dirX * mv, dirY * mv);
  if (k & K_BACK) walking |= tryMove(px, py, -dirX * mv, -dirY * mv);
  if (k & K_STRL) walking |= tryMove(px, py, dirY * mv, -dirX * mv);
  if (k & K_STRR) walking |= tryMove(px, py, -dirY * mv, dirX * mv);
  if (k & K_FIRE) doFire();
  updateDoors(dt, now);

  // weapon bob
  bobAmp += ((walking ? 1.0f : 0.0f) - bobAmp) * (dt * 8 > 1 ? 1 : dt * 8);
  bobPhase += dt * 9;
  if (bobPhase > 2 * PI) bobPhase -= 2 * PI;

  float spd = impSpeed();
  for (int i = 0; i < NEN; i++) {
    Enemy &e = en[i];
    float dx = px - e.x, dy = py - e.y, d = sqrtf(dx * dx + dy * dy);
    if (!e.alive) {
      if (now - e.deadAt > respawnMs()) {
        float rx = px - e.sx, ry = py - e.sy;
        if (rx * rx + ry * ry > 9) { e.x = e.sx; e.y = e.sy; e.alive = true; e.hp = IMP_HP; e.walk = 0; e.awake = false; }
      }
      continue;
    }
    if (now - e.painAt < 300) continue;               // pain stun
    if (!e.awake) {                                    // sleeping until it sees the player
      if (d < WAKE_DIST && lineOfSight(e.x, e.y, px, py)) e.awake = true;
      else continue;
    }
    if (d > 1.0f) {
      float mx = dx / d * spd * dt, my = dy / d * spd * dt;
      if (tryMove(e.x, e.y, mx, my)) e.walk += spd * dt;
      // a closed door in the way: imps open it, like Doom monsters
      int ax = (int)(e.x + (mx > 0 ? 0.4f : mx < 0 ? -0.4f : 0)), ay = (int)(e.y + (my > 0 ? 0.4f : my < 0 ? -0.4f : 0));
      int dd = mapAt(ax, (int)e.y) == DA_DOOR_TILE ? doorAt(ax, (int)e.y)
             : mapAt((int)e.x, ay) == DA_DOOR_TILE ? doorAt((int)e.x, ay) : -1;
      if (dd >= 0 && doors[dd].open < 1 && doors[dd].dir != 1) openDoor(dd);
    }
    if (d < 1.0f && (e.atkAt == 0 || now - e.atkAt >= IMP_ATK_MS)) {   // imp melee
      e.atkAt = now;
      int dmg = (int)((random(8) + 1) * 3 * skillDmg());   // Doom imp scratch: 3..24
      if (dmg < 1) dmg = 1;
      hp -= dmg; if (hp < 0) hp = 0;
      float rel = atan2f(e.y - py, e.x - px) - ang;     // where the hit comes from
      while (rel > PI) rel -= 2 * PI;
      while (rel < -PI) rel += 2 * PI;
      int dir = fabsf(rel) < PI / 4 ? 0 : (rel > 0 ? 1 : -1);   // 0 front, 1 right, -1 left
      if (client >= 0) {
        char m[64];
        snprintf(m, sizeof(m), "{\"ev\":\"hurt\",\"d\":%d,\"dir\":%d,\"hp\":%d}", dmg, dir, hp);
        ws.sendTXT(client, m);
      }
    }
  }
  if (hp <= 0 && !dead) { dead = true; deathAt = now; sendState(); }
}

// ---------------- RAYCAST (one column) ----------------
// Sprites (far -> near) whose distance is in [minD, maxD). Frames are cropped to their bounding box in flash.
void drawSprites(int x, uint8_t *col, float minD, float maxD) {
  if (previewMode) return;
  for (int k = 0; k < nspr; k++) {
    SprProj &p = spr[k];
    if (p.tY >= maxD || p.tY < minD) continue;
    int sz = p.size; if (sz < 2) continue;
    int sx0 = p.screenX - sz / 2;
    if (x < sx0 || x >= sx0 + sz) continue;
    int stx = (x - sx0) * SPRS / sz;
    int fx0 = DA_SPR_X0[p.frame], fw = DA_SPR_W[p.frame];
    if (stx < fx0 || stx >= fx0 + fw) continue;
    int fy0 = DA_SPR_Y0[p.frame], fh = DA_SPR_H[p.frame];
    const uint8_t *scol = DA_SPRITES + DA_SPR_OFS[p.frame] + (uint32_t)(stx - fx0) * fh;
    const uint8_t *scm = cmap[p.light];
    int sy0 = H / 2 - sz / 2;
    int ys = sy0 < 0 ? 0 : sy0, ye = sy0 + sz > H ? H : sy0 + sz;
    uint32_t sstep = ((uint32_t)SPRS << 16) / sz;
    uint32_t spos = (uint32_t)(ys - sy0) * sstep;
    for (int yy = ys; yy < ye; yy++, spos += sstep) {
      int ty = (int)(spos >> 16) - fy0;
      if (ty < 0) continue;
      if (ty >= fh) break;
      uint8_t v = pgm_read_byte(scol + ty);
      if (v != DA_TRANSP) col[yy] = scm[v];
    }
  }
}

// Draw a textured wall slice. topFrac..1 of the wall height is visible from the top (doors: 1 - open),
// texOfs shifts the texture up (door rising). Rows outside the slice are left untouched.
void drawSlice(uint8_t *col, uint8_t tile, int side, float dist, float rdx, float rdy, float visible, float texOfs) {
  float wx = side == 0 ? py + dist * rdy : px + dist * rdx;
  wx -= floorf(wx);
  int tx = (int)(wx * TEX) & (TEX - 1);
  if ((side == 0 && rdx > 0) || (side == 1 && rdy < 0)) tx = TEX - 1 - tx;   // no mirrored textures
  const uint8_t *wcol = DA_WALLS + ((uint32_t)(tile - 1) * TEX + tx) * TEX;
  int l = lightFor(dist) + (side ? 1 : 0);                    // Doom-style fake contrast
  const uint8_t *cm = cmap[l > LIGHT_MAX ? LIGHT_MAX : l];
  int lh = (int)(H / dist); if (lh < 1) lh = 1;
  int ds = H / 2 - lh / 2, de = ds + (int)(lh * visible);
  int y0 = ds < 0 ? 0 : ds, y1 = de > H ? H : de;
  uint32_t step = ((uint32_t)TEX << 16) / lh;
  uint32_t pos = (uint32_t)(y0 - ds) * step + (uint32_t)(texOfs * TEX * 65536.0f);
  for (int y = y0; y < y1; y++) { col[y] = cm[pgm_read_byte(wcol + ((pos >> 16) & (TEX - 1)))]; pos += step; }
}

void renderColumn(int x, uint8_t *col) {
  float camX = 2.0f * x / W - 1;
  float rdx = dirX + planeX * camX, rdy = dirY + planeY * camX;
  int mx = (int)px, my = (int)py;
  float ddx = rdx == 0 ? 1e30f : fabsf(1 / rdx);
  float ddy = rdy == 0 ? 1e30f : fabsf(1 / rdy);
  int stepX, stepY; float sdx, sdy;
  if (rdx < 0) { stepX = -1; sdx = (px - mx) * ddx; } else { stepX = 1; sdx = (mx + 1.0f - px) * ddx; }
  if (rdy < 0) { stepY = -1; sdy = (py - my) * ddy; } else { stepY = 1; sdy = (my + 1.0f - py) * ddy; }
  int side = 0; uint8_t tile = 0;
  // first (partially) open door met by the ray: drawn over what is behind it
  int dIdx = -1, dSide = 0; float dDist = 0;
  for (int i = 0; i < 64 && !tile; i++) {
    if (sdx < sdy) { sdx += ddx; mx += stepX; side = 0; } else { sdy += ddy; my += stepY; side = 1; }
    tile = mapAt(mx, my);
    if (tile == DA_DOOR_TILE) {
      int d = doorAt(mx, my);
      if (d >= 0 && doors[d].open > 0) {             // see through the gap: keep tracing
        if (dIdx < 0 && doors[d].open < 1) { dIdx = d; dSide = side; dDist = side == 0 ? sdx - ddx : sdy - ddy; }
        tile = 0;
      }
    }
  }
  float dist = side == 0 ? sdx - ddx : sdy - ddy;
  if (dist < 0.05f) dist = 0.05f;
  if (dDist < 0.05f) dDist = 0.05f;
  if (tile > DA_NWALLS) tile = DA_NWALLS;
  if (!tile) tile = 1;

  memcpy(col, rowCol, H);                                     // floor / ceiling
  drawSlice(col, tile, side, dist, rdx, rdy, 1.0f, 0.0f);     // wall (or closed door)
  if (dIdx < 0) {
    if (!previewMode) zbuf[x] = dist;
    drawSprites(x, col, 0, dist);
  } else {                                                    // rising door: bottom gap shows the room behind
    float o = doors[dIdx].open;
    if (!previewMode) zbuf[x] = o < 0.5f ? dDist : dist;      // bullets pass through the gap at eye level
    drawSprites(x, col, dDist, dist);
    drawSlice(col, DA_DOOR_TILE, dSide, dDist, rdx, rdy, 1.0f - o, o);
    drawSprites(x, col, 0, dDist);
  }

  // Weapon overlay (full bright)
  int wx0 = DA_WPN_X[wpnFrame] + wpnX;
  int ww = DA_WPN_W[wpnFrame], wh = DA_WPN_H[wpnFrame];
  if (x >= wx0 && x < wx0 + ww) {
    const uint8_t *wc = DA_WPN + DA_WPN_OFS[wpnFrame] + (uint32_t)(x - wx0) * wh;
    int wy0 = DA_WPN_Y[wpnFrame] + wpnY;
    for (int i = 0; i < wh; i++) {
      int yy = wy0 + i;
      if (yy < 0 || yy >= H) continue;
      uint8_t v = pgm_read_byte(wc + i);
      if (v != DA_TRANSP) col[yy] = v;
    }
  }
}

// Weapon sprite for the current time (Doom psprite timings, 1 tic = 28.6 ms)
void weaponAnim(uint32_t now) {
  if (previewMode) { wpnFrame = WPN_PISTOL_IDLE; wpnX = 0; wpnY = 1; return; }
  uint32_t t = now - lastFire;
  if (weapon == 0) {
    wpnFrame = (lastFire && t < 120) ? WPN_PISTOL_FIRE : WPN_PISTOL_IDLE;
  } else if (!lastFire || t >= SHOTGUN_MS) wpnFrame = WPN_SHOTGUN_IDLE;
  else if (t < 115) wpnFrame = WPN_SHOTGUN_F1;
  else if (t < 200) wpnFrame = WPN_SHOTGUN_F2;
  else if (t < 345) wpnFrame = WPN_SHOTGUN_B;
  else if (t < 490) wpnFrame = WPN_SHOTGUN_C;
  else if (t < 605) wpnFrame = WPN_SHOTGUN_D;
  else if (t < 750) wpnFrame = WPN_SHOTGUN_C;
  else if (t < 890) wpnFrame = WPN_SHOTGUN_B;
  else wpnFrame = WPN_SHOTGUN_IDLE;
  bool firing = wpnFrame != WPN_PISTOL_IDLE && wpnFrame != WPN_SHOTGUN_IDLE;
  wpnX = (int)(cosf(bobPhase) * 3 * bobAmp);
  wpnY = (int)(fabsf(sinf(bobPhase)) * 3 * bobAmp) + (firing ? 0 : 1);
  if (switchAt) {                                    // lower, swap, raise
    uint32_t s = now - switchAt, half = SWITCH_MS / 2;
    int drop = s < half ? (int)(s * 40 / half) : (int)((SWITCH_MS - (s > SWITCH_MS ? SWITCH_MS : s)) * 40 / half);
    wpnY += drop;
  }
  if (dead) wpnY += 60;
}

// Render the current scene and send it to one browser. stats = count it in the fps / timing telemetry.
// (No default arguments: the Arduino IDE auto-prototype generator does not like them.)
void renderTo(int8_t target, bool stats) {
  if (target < 0) return;
  uint32_t now = millis();
  projectSprites(now);
  weaponAnim(now);
  for (int x0 = 0; x0 < W; x0 += STRIP) {
    buf[0] = x0; buf[1] = STRIP;
    size_t n = 2;
    uint32_t t0 = micros();
    for (int c = 0; c < STRIP; c++) {
      renderColumn(x0 + c, colTmp);
      // floor/ceiling rows are drawn by the browser: send only the span that differs
      int ys = 0, ye = H;
      while (ys < H && colTmp[ys] == rowCol[ys]) ys++;
      while (ye > ys && colTmp[ye - 1] == rowCol[ye - 1]) ye--;
      buf[n++] = ys; buf[n++] = ye - ys;
      memcpy(buf + n, colTmp + ys, ye - ys); n += ye - ys;
    }
    uint32_t t1 = micros();
    ws.sendBIN(target, buf, n);
    uint32_t t2 = micros();
    if (stats) { tRender += t1 - t0; tSend += t2 - t1; bytesSent += n; }
    yield();
  }
  if (stats) frames++;
}
void renderAndSend() { renderTo(client, true); }

// Fixed wall scene for the menu background. Camera and game state are restored afterwards, so a
// browser that opens the page never disturbs a game in progress.
void renderPreviewTo(int8_t target) {
  float sx = px, sy = py, sa = ang;
  px = PREVIEW_X; py = PREVIEW_Y; ang = PREVIEW_ANG; updateDir();
  previewMode = true;
  renderTo(target, false);
  previewMode = false;
  px = sx; py = sy; ang = sa; updateDir();
}

void sendPalette(uint8_t num) {
  buf[0] = 0xFF; buf[1] = 0;                 // marker: palette (strip x0 is never 255)
  memcpy_P(buf + 2, DA_PAL, 768);
  memcpy(buf + 770, rowCol, H);              // floor/ceiling colour per row (drawn by the browser)
  ws.sendBIN(num, buf, 770 + H);
}

// =====================================================================================
// ---------------- DEVICE: relay, LED, physical button, settings in flash -------------
// =====================================================================================
struct Config {
  uint32_t magic;
  char ssid[33], wpass[65];        // home Wi-Fi
  char user[33], apass[65];        // admin login (pages, API, OTA)
  uint8_t relay;                   // last load state, restored after a power cut
  uint8_t pad[3];
  uint32_t crc;
};
#define CFG_MAGIC 0xD00D5E66
Config cfg;
bool configured = false;           // false = first boot: setup portal only
volatile bool relayOn = false;
volatile bool pulsing = false;
bool cfgDirty = false; uint32_t dirtyAt = 0;
uint32_t restartAt = 0;
char token[17];                    // per-boot session token for the WebSocket (given to logged-in pages)

uint32_t cfgCrc() {                // FNV-1a over everything but crc
  const uint8_t *p = (const uint8_t *)&cfg; uint32_t h = 2166136261u;
  for (size_t i = 0; i < offsetof(Config, crc); i++) { h ^= p[i]; h *= 16777619u; }
  return h;
}
void loadConfig() {
  EEPROM.begin(sizeof(Config));
  EEPROM.get(0, cfg);
  configured = cfg.magic == CFG_MAGIC && cfg.crc == cfgCrc() && cfg.ssid[0] && cfg.user[0] && cfg.apass[0];
  if (!configured) { memset(&cfg, 0, sizeof(cfg)); strcpy(cfg.user, DEFAULT_USER); }
  cfg.ssid[32] = cfg.wpass[64] = cfg.user[32] = cfg.apass[64] = 0;
}
void saveConfig() {
  cfg.magic = CFG_MAGIC; cfg.crc = cfgCrc();
  EEPROM.put(0, cfg); EEPROM.commit();
  cfgDirty = false;
}
void factoryReset() {
  memset(&cfg, 0, sizeof(cfg));
  EEPROM.put(0, cfg); EEPROM.commit();
}

// ---- relay ----
// The 30 ms pulse is ended by hardware timer1 (interrupt), so it stays 30 ms even while a frame is rendered.
// The LED always mirrors the PHYSICAL relay (LED on = relay closed), so during the pulse it blinks too.
// Both pins are restored together inside the interrupt (register writes only: it runs from IRAM).
void IRAM_ATTR pulseEnd() {
  if (relayOn) { GPOS = 1 << PIN_RELAY; GPOC = 1 << PIN_LED; }   // relay closed, LED on (active low)
  else         { GPOC = 1 << PIN_RELAY; GPOS = 1 << PIN_LED; }   // relay open,   LED off
  pulsing = false;
  timer1_disable();
}
static inline bool relayPhys() { return relayOn ^ pulsing; }     // what the contact is doing right now
void applyLed() { digitalWrite(PIN_LED, relayPhys() ? LOW : HIGH); }
void ledFlash(uint32_t ms) { digitalWrite(PIN_LED, relayPhys() ? HIGH : LOW); ledRestore = millis() + ms; }

void relayBroadcast() {
  char m[40];
  snprintf(m, sizeof(m), "{\"ev\":\"relay\",\"on\":%d}", relayOn ? 1 : 0);
  ws.broadcastTXT(m);
}
void relaySet(bool on) {
  relayOn = on;
  if (!pulsing) digitalWrite(PIN_RELAY, on ? HIGH : LOW);
  applyLed();
  cfg.relay = on; cfgDirty = true; dirtyAt = millis();      // saved 2 s later (fewer flash writes)
  relayBroadcast();
  Serial.printf("Relay %s\n", on ? "ON" : "OFF");
}
void relayPulse() {                                          // invert for PULSE_MS, then back
  if (pulsing) return;
  pulsing = true;
  digitalWrite(PIN_RELAY, relayOn ? LOW : HIGH);
  digitalWrite(PIN_LED, relayOn ? HIGH : LOW);                // LED follows the relay: lit for the whole pulse
  timer1_isr_init();
  timer1_attachInterrupt(pulseEnd);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);            // 80 MHz / 16 = 5 MHz
  timer1_write(PULSE_MS * 5000);
  ws.broadcastTXT("{\"ev\":\"pulse\"}");
}

// ---- physical button: short press = toggle, hold 10 s = factory reset ----
void handleButton(uint32_t now) {
  static bool prev = false; static uint32_t downAt = 0;
  bool down = !digitalRead(PIN_BTN);
  if (down && !prev) downAt = now;
  if (down && now - downAt > 3000) {                         // warning: fast blink while held
    digitalWrite(PIN_LED, (now / 100) & 1);
    if (now - downAt > RESET_HOLD_MS) {
      Serial.println("Factory reset");
      factoryReset();
      for (int i = 0; i < 30; i++) { digitalWrite(PIN_LED, i & 1); delay(60); }
      ESP.restart();
    }
  }
  if (!down && prev) {
    uint32_t d = now - downAt;
    if (d > 40 && d < 1000) relaySet(!relayOn);
    applyLed();
  }
  prev = down;
}

// =====================================================================================
// ---------------- WEB: login, setup / settings page, API -----------------------------
// =====================================================================================
DNSServer dns;
bool apOn = false, otaStarted = false, staWasUp = false;
uint32_t staLostAt = 0;
String scanList;                   // <option> list of Wi-Fi networks

bool authOk() {                    // Basic auth with the admin user/password (no login before setup)
  if (!configured) return true;
  if (server.authenticate(cfg.user, cfg.apass)) return true;
  server.requestAuthentication(BASIC_AUTH, "SONOFF DOOM");
  return false;
}
String htmlEsc(const char *s) {
  String o;
  for (; *s; s++) {
    if (*s == '<') o += F("&lt;"); else if (*s == '>') o += F("&gt;");
    else if (*s == '&') o += F("&amp;"); else if (*s == '"') o += F("&quot;"); else o += *s;
  }
  return o;
}
void scanWifi() {
  int n = WiFi.scanNetworks();
  scanList = "";
  for (int i = 0; i < n && i < 20; i++) {
    String s = WiFi.SSID(i);
    if (!s.length() || scanList.indexOf("\"" + s + "\"") >= 0) continue;
    scanList += F("<option value=\""); scanList += htmlEsc(s.c_str()); scanList += F("\">");
    scanList += String(WiFi.RSSI(i)); scanList += F(" dBm</option>");
  }
  WiFi.scanDelete();
}

static const char PAGE_HEAD[] PROGMEM = R"(<!doctype html><html><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1"><title>SONOFF DOOM SETUP</title><style>
html{min-height:100%;background:radial-gradient(ellipse at 50% 0,#1c1e24 0,#121419 50%,#0a0b0e 100%) fixed}
body{margin:0;color:#f65;font:15px monospace;display:flex;justify-content:center}
.b{width:min(94vw,440px);margin:24px 0;padding:18px 22px;background:#2e2e2e;box-shadow:0 8px 28px #0008;border:3px solid;border-color:#666 #222 #222 #666}
h1{font-size:22px;letter-spacing:3px;margin:0 0 4px;color:#f44;text-shadow:2px 2px #400}h2{font-size:13px;color:#aaa;margin:0 0 16px}
label{display:block;margin:14px 0 4px;color:#fc6}input{width:100%;box-sizing:border-box;padding:8px;font:15px monospace;
background:#000;color:#eee;border:2px solid;border-color:#222 #666 #666 #222}small{color:#888}
button{margin-top:20px;width:100%;padding:12px;font:bold 17px monospace;letter-spacing:4px;color:#f44;background:#3a3a3a;
border:3px solid;border-color:#888 #111 #111 #888;cursor:pointer}button:active{border-color:#111 #888 #888 #111}
.e{color:#fff;background:#600;padding:8px;margin:10px 0}a{color:#fc6}.r{background:#300;color:#f88}</style></head><body><div class=b>)";

void sendSetupPage(const String &err) {
  String h = FPSTR(PAGE_HEAD);
  h += configured ? F("<h1>SETTINGS</h1><h2>SONOFF DOOM -- by Piuppi</h2>")
                  : F("<h1>SONOFF DOOM</h1><h2>FIRST SETUP -- by Piuppi</h2>");
  if (err.length()) { h += F("<div class=e>"); h += err; h += F("</div>"); }
  h += F("<form method=post action=/save><label>HOME WI-FI</label><input name=s list=n required maxlength=32 value=\"");
  h += htmlEsc(cfg.ssid);
  h += F("\" placeholder=\"choose or type\"><datalist id=n>"); h += scanList;
  h += F("</datalist><small><a href=\"/scan\">rescan networks</a></small><label>WI-FI PASSWORD</label><input name=w type=password maxlength=64");
  h += configured ? F(" placeholder=\"(unchanged)\">") : F(" placeholder=\"empty for open networks\">");
  h += F("<label>ADMIN USER</label><input name=u required maxlength=32 value=\""); h += htmlEsc(cfg.user);
  h += F("\"><label>ADMIN PASSWORD</label><input name=a type=password maxlength=64");
  h += configured ? F(" placeholder=\"(unchanged)\">") : F(" required minlength=8 placeholder=\"min 8 characters\">");
  h += F("<label>REPEAT ADMIN PASSWORD</label><input name=b type=password maxlength=64><button>SAVE</button></form>");
  if (configured)
    h += F("<form method=post action=/reset onsubmit=\"return confirm('Erase Wi-Fi and admin settings?')\">"
           "<button class=r>FACTORY RESET</button></form><p><a href=\"/\">&lt; back to the game</a></p>");
  else
    h += F("<p><small>The Sonoff will reboot and join your Wi-Fi. Then open http://sonoff-doom.local "
           "or its IP (router / serial monitor) and log in with the admin user.</small></p>");
  h += F("</div></body></html>");
  server.send(200, "text/html", h);
}
void sendMsgPage(const __FlashStringHelper *msg) {
  String h = FPSTR(PAGE_HEAD);
  h += F("<h1>SONOFF DOOM</h1><p>"); h += msg; h += F("</p></div></body></html>");
  server.send(200, "text/html", h);
}

void handleSave() {
  if (!authOk()) return;
  String s = server.arg("s"), w = server.arg("w"), u = server.arg("u"), a = server.arg("a"), b = server.arg("b");
  s.trim(); u.trim();
  if (!s.length() || s.length() > 32) { sendSetupPage(F("Wi-Fi name missing.")); return; }
  if (w.length() > 64 || (w.length() && w.length() < 8)) { sendSetupPage(F("Wi-Fi password: 8-64 characters, or empty for open networks.")); return; }
  if (!u.length() || u.length() > 32) { sendSetupPage(F("Admin user missing.")); return; }
  if (a.length() || !configured) {
    if (a.length() < 8 || a.length() > 64) { sendSetupPage(F("Admin password: 8-64 characters.")); return; }
    if (a != b) { sendSetupPage(F("The two admin passwords are different.")); return; }
  }
  bool keepW = configured && !w.length() && s == cfg.ssid;
  strlcpy(cfg.ssid, s.c_str(), sizeof(cfg.ssid));
  if (!keepW) strlcpy(cfg.wpass, w.c_str(), sizeof(cfg.wpass));
  strlcpy(cfg.user, u.c_str(), sizeof(cfg.user));
  if (a.length()) strlcpy(cfg.apass, a.c_str(), sizeof(cfg.apass));
  cfg.relay = relayOn;
  saveConfig();
  sendMsgPage(F("SAVED. REBOOTING...<br><br>Connect to your home Wi-Fi, then open "
                "<a href=\"http://sonoff-doom.local\">http://sonoff-doom.local</a> or the Sonoff IP."));
  restartAt = millis() + 1500;
}

void handleRelayApi() {
  if (!authOk()) return;
  String c = server.arg("cmd");
  if (c == "on") relaySet(true); else if (c == "off") relaySet(false);
  else if (c == "toggle") relaySet(!relayOn); else if (c == "pulse") relayPulse();
  server.send(200, "application/json", relayOn ? "{\"relay\":\"on\"}" : "{\"relay\":\"off\"}");
}

void startAP() {                   // setup hotspot (open) + captive DNS
  if (apOn) return;
  WiFi.mode(configured ? WIFI_AP_STA : WIFI_AP_STA);
  WiFi.softAP(SETUP_SSID);
  dns.start(53, "*", WiFi.softAPIP());
  apOn = true;
  Serial.printf("Setup hotspot %s - http://%s\n", SETUP_SSID, WiFi.softAPIP().toString().c_str());
}
void stopAP() {
  if (!apOn) return;
  dns.stop(); WiFi.softAPdisconnect(true); WiFi.mode(WIFI_STA);
  apOn = false;
  Serial.println("Setup hotspot off");
}

void setupWeb() {
  // captive portal: unknown pages (phone/PC connectivity checks) -> setup / login page
  server.onNotFound([]() {
    if (apOn) { server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true); server.send(302, "text/plain", ""); }
    else server.send(404, "text/plain", "not found");
  });
  server.on("/save", HTTP_POST, handleSave);
  server.on("/scan", []() { if (!authOk()) return; scanWifi(); server.sendHeader("Location", configured ? "/settings" : "/"); server.send(302); });
  if (!configured) {                                            // first boot: only the setup page
    server.on("/", []() { sendSetupPage(""); });
    return;
  }
  server.on("/settings", []() { if (authOk()) sendSetupPage(""); });
  server.on("/reset", HTTP_POST, []() {
    if (!authOk()) return;
    factoryReset();
    sendMsgPage(F("FACTORY RESET. REBOOTING...<br><br>Connect to the Wi-Fi SONOFF-DOOM-SETUP to configure it again."));
    restartAt = millis() + 1500;
  });
  server.on("/relay", handleRelayApi);
  server.on("/token", []() { if (!authOk()) return; server.sendHeader("Cache-Control", "no-store"); server.send(200, "text/plain", token); });
#ifdef DA_INDEX_LEN
  server.on("/", []() {                             // game page (gzip, from flash)
    if (!authOk()) return;
    server.sendHeader("Content-Encoding", "gzip");
    server.send_P(200, "text/html", (PGM_P)DA_INDEX_GZ, DA_INDEX_LEN);
  });
#else
  server.on("/", []() { if (authOk()) server.send_P(200, "text/html", INDEX_HTML); });
#endif
  server.on("/stbar.bin", []() {                    // status bar, menu, font, sounds, palettes (gzip)
    if (!authOk()) return;
    server.sendHeader("Content-Encoding", "gzip");
    server.send_P(200, "application/octet-stream", (PGM_P)DA_STBAR_GZ, DA_STBAR_LEN);
  });
  server.on("/music.bin", []() {                    // MUS tracks from the WAD (gzip, from flash)
    if (!authOk()) return;
#ifdef DA_MUSIC_LEN
    server.sendHeader("Content-Encoding", "gzip");
    server.send_P(200, "application/octet-stream", (PGM_P)DA_MUSIC_GZ, DA_MUSIC_LEN);
#else
    server.send(404, "text/plain", "no music");
#endif
  });
  httpUpdater.setup(&server, "/update", cfg.user, cfg.apass);   // browser upload of the .bin
}

// ---------------- WEBSOCKET ----------------
// Browser -> Sonoff: G take the game, k<mask> keys, a<yaw> mouse, p palette, f frame, P0/P1 pause,
//                    n<skill> new game, w0/w1 weapon, w+/w- next/previous, u use (doors), r restart,
//                    Rt switch toggle, R1 / R0 switch on / off, Rp button pulse
// Every logged-in browser can use TOGGLE / SWITCH; only one of them (the last that starts / resumes) plays.
#ifndef WEBSOCKETS_SERVER_CLIENT_MAX
#define WEBSOCKETS_SERVER_CLIENT_MAX 5
#endif
bool wsAuth[WEBSOCKETS_SERVER_CLIENT_MAX];
int8_t wsKick = -1;

void claimGame(uint8_t num) {
  if (client == num) return;
  if (client >= 0) ws.sendTXT(client, "{\"ev\":\"kicked\"}");
  client = num; webKeys = 0; yawInit = false; yawPending = 0;
  needFrame = true;
  sendState();
}

void onWs(uint8_t num, WStype_t type, uint8_t *payload, size_t len) {
  if (num >= WEBSOCKETS_SERVER_CLIENT_MAX) return;
  switch (type) {
    case WStype_CONNECTED: {
      char url[64]; size_t n = len < sizeof(url) - 1 ? len : sizeof(url) - 1;
      memcpy(url, payload, n); url[n] = 0;
      wsAuth[num] = strstr(url, token) != nullptr;               // page got the token after logging in
      if (!wsAuth[num]) { wsKick = num; break; }
      char m[40];
      snprintf(m, sizeof(m), "{\"ev\":\"relay\",\"on\":%d}", relayOn ? 1 : 0);
      ws.sendTXT(num, m);
      break;
    }
    case WStype_DISCONNECTED:
      wsAuth[num] = false;
      if (num == client) { client = -1; webKeys = 0; }
      break;
    case WStype_TEXT: {
      if (!wsAuth[num] || !len) break;
      char c = payload[0];
      if (c == 'R' && len > 1) {                                  // device: any logged-in browser
        if (payload[1] == 't') relaySet(!relayOn);
        else if (payload[1] == '1') relaySet(true);
        else if (payload[1] == '0') relaySet(false);
        else if (payload[1] == 'p') relayPulse();
        break;
      }
      if (c == 'p') {                                             // palette + the wall scene for the menu background
        sendPalette(num);
        if (!(num == client && !paused)) previewNum = num;        // (a running game already streams frames)
        break;
      }
      if (c == 'W') { previewNum = num; break; }                  // wall scene again (menu opened outside a game)
      if (c == 'G') { claimGame(num); break; }
      if (num != client) break;                                   // game commands: only the player
      if (c == 'k' && len > 1) webKeys = paused ? 0 : (uint8_t)atoi((char *)payload + 1);
      else if (c == 'a' && len > 1) {              // absolute mouse yaw: lost/merged messages can't cause jumps
        int32_t v = atol((char *)payload + 1);
        if (yawInit && !paused) yawPending += (v - lastYaw) * MOUSE_RAD;
        lastYaw = v; yawInit = true;
      }
      else if (c == 'f') needFrame = true;
      else if (c == 'Q') {                                        // quit to the title menu: pause + wall scene
        paused = true; webKeys = 0; yawPending = 0; needFrame = false; previewNum = num;
        lastUpd = millis();
        sendState();
      }
      else if (c == 'P' && len > 1) {
        paused = payload[1] == '1'; webKeys = 0; yawPending = 0;
        if (paused) needFrame = true;
        lastUpd = millis();
        sendState();
      }
      else if (c == 'n' && len > 1) {              // new game from the menu
        int s = payload[1] - '0';
        skill = s < 0 ? 0 : (s > 4 ? 4 : s);
        paused = false; webKeys = 0; yawPending = 0; lastUpd = millis();
        resetGame();
      }
      else if (c == 'w' && len > 1 && !paused) {
        if (payload[1] == '+' || payload[1] == '-') selectWeapon(1 - (switchAt ? nextWeapon : weapon));
        else selectWeapon(payload[1] - '0');
      }
      else if (c == 'u' && !paused) doUse();
      else if (c == 'r') resetGame();
      break;
    }
    default: break;
  }
}

// =====================================================================================
void setup() {
  Serial.begin(115200);
  loadConfig();
  relayOn = configured && cfg.relay;                         // restore the load after a power cut
  pinMode(PIN_RELAY, OUTPUT); digitalWrite(PIN_RELAY, relayOn ? HIGH : LOW);
  pinMode(PIN_LED, OUTPUT);   applyLed();
  pinMode(PIN_BTN, INPUT_PULLUP);
  initTables();
  initDoors();
  randomSeed(RANDOM_REG32);
  snprintf(token, sizeof(token), "%08x%08x", (unsigned)RANDOM_REG32, (unsigned)RANDOM_REG32);

  WiFi.persistent(false);                                    // credentials live in our config, not in the SDK
  WiFi.hostname(HOSTNAME);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  if (!configured) {
    Serial.println("\nSONOFF DOOM - not configured: setup mode");
    WiFi.mode(WIFI_AP_STA);
    scanWifi();
    startAP();
  } else {
    Serial.printf("\nSONOFF DOOM - joining Wi-Fi \"%s\"\n", cfg.ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg.ssid, cfg.wpass);
    staLostAt = millis();
  }
  setupWeb();
  server.begin();

  if (configured) {
    ArduinoOTA.setHostname(HOSTNAME);
    ArduinoOTA.setPassword(cfg.apass);
    ArduinoOTA.onStart([]() {
      otaBusy = true;
      if (client >= 0) { ws.disconnect(client); client = -1; }
      Serial.println("OTA: update started");
    });
    ArduinoOTA.onEnd([]() { Serial.println("OTA: done, rebooting"); });
    ArduinoOTA.onError([](ota_error_t e) { otaBusy = false; Serial.printf("OTA: error %u\n", e); });
    ws.begin(); ws.onEvent(onWs);
  }

  updateDir();
  lastUpd = millis();
}

void wifiWatch(uint32_t now) {
  bool up = WiFi.status() == WL_CONNECTED;
  if (up && !staWasUp) {
    Serial.printf("Wi-Fi OK - http://%s  or  http://%s.local\n", WiFi.localIP().toString().c_str(), HOSTNAME);
    if (!otaStarted) { ArduinoOTA.begin(); MDNS.addService("http", "tcp", 80); otaStarted = true; }
  }
  if (!up && staWasUp) staLostAt = now;
  staWasUp = up;
  if (!up && !apOn && now - staLostAt > WIFI_FAIL_MS) startAP();          // home Wi-Fi unreachable
  if (up && apOn && WiFi.softAPgetStationNum() == 0) stopAP();            // back home, nobody on the hotspot
}

void loop() {
  uint32_t now = millis();
  if (apOn) dns.processNextRequest();
  if (otaStarted) ArduinoOTA.handle();
  server.handleClient();
  if (restartAt && (int32_t)(now - restartAt) >= 0) ESP.restart();
  handleButton(now);
  if (cfgDirty && now - dirtyAt > 2000) saveConfig();
  if (ledRestore && (int32_t)(now - ledRestore) >= 0) { applyLed(); ledRestore = 0; }

  if (!configured) {                                         // setup mode: slow LED blink, no game
    digitalWrite(PIN_LED, (now / 500) & 1);
    return;
  }
  wifiWatch(now);
  if (otaBusy || Update.isRunning()) return;                 // pause the game during an update
  ws.loop();
  if (wsKick >= 0) { ws.disconnect(wsKick); wsKick = -1; }   // WebSocket without a valid token

  if (previewNum >= 0) {                                     // menu background for a browser that is not playing
    int8_t t = previewNum; previewNum = -1;
    if (wsAuth[t]) renderPreviewTo(t);
  }

  if (client < 0) { lastUpd = now; return; }   // pause with no player

  if (paused) {                                // menu open: game frozen, one frame as background
    lastUpd = now;
    if (needFrame) { needFrame = false; renderAndSend(); }
    return;
  }

  float dt = (now - lastUpd) / 1000.0f; if (dt > 0.1f) dt = 0.1f;
  lastUpd = now;
  updateGame(dt);

  if (now - lastFrame >= FRAME_MS) { lastFrame = now; renderAndSend(); }

  if (now - lastStats >= 1000) {
    uint32_t span = now - lastStats;
    fps = frames * 1000.0f / span;
    float rMs = frames ? tRender / 1000.0f / frames : 0;   // avg render time per frame
    float sMs = frames ? tSend / 1000.0f / frames : 0;     // avg send time per frame
    float kbs = bytesSent / 1024.0f * 1000.0f / span;      // throughput
    frames = 0; tRender = tSend = bytesSent = 0; lastStats = now;
    char m[176];
    snprintf(m, sizeof(m),
             "{\"fps\":%.1f,\"heap\":%u,\"hp\":%d,\"am\":%d,\"sh\":%d,\"kills\":%u,\"r\":%.1f,\"s\":%.1f,\"kbs\":%.0f}",
             fps, ESP.getFreeHeap(), hp, ammo, shells, kills, rMs, sMs, kbs);
    ws.sendTXT(client, m);
  }
}
