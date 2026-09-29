#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

// Box2D v3.2.0 API headers
#include "box2d/box2d.h"
#include "box2d/collision.h"
#include "box2d/math_functions.h"

// Embedded School Handwriting Fonts
#include "font_patrick_hand.h"
#include "font_kalam_bold.h"

static Font g_fontTitle = {0};
static Font g_fontBody = {0};
static bool g_fontsReady = false;

static void InitGameFonts(void) {
    g_fontTitle = LoadFontFromMemory(".ttf", g_fontKalamBold_data, g_fontKalamBold_size, 42, NULL, 0);
    g_fontBody = LoadFontFromMemory(".ttf", g_fontPatrickHand_data, g_fontPatrickHand_size, 34, NULL, 0);
    SetTextureFilter(g_fontTitle.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(g_fontBody.texture, TEXTURE_FILTER_BILINEAR);
    g_fontsReady = true;
}

static void CloseGameFonts(void) {
    if (g_fontsReady) {
        UnloadFont(g_fontTitle);
        UnloadFont(g_fontBody);
        g_fontsReady = false;
    }
}

static void DrawSchoolText(const char* text, float x, float y, float size, Color col) {
    if (g_fontsReady) {
        DrawTextEx(g_fontBody, text, (Vector2){ x, y }, size, 1.0f, col);
    } else {
        DrawText(text, (int)x, (int)y, (int)size, col);
    }
}

static void DrawSchoolTextTitle(const char* text, float x, float y, float size, Color col) {
    if (g_fontsReady) {
        DrawTextEx(g_fontTitle, text, (Vector2){ x, y }, size, 1.0f, col);
    } else {
        DrawText(text, (int)x, (int)y, (int)size, col);
    }
}

static float MeasureSchoolText(const char* text, float size) {
    if (g_fontsReady) {
        return MeasureTextEx(g_fontBody, text, size, 1.0f).x;
    }
    return (float)MeasureText(text, (int)size);
}

static float MeasureSchoolTextTitle(const char* text, float size) {
    if (g_fontsReady) {
        return MeasureTextEx(g_fontTitle, text, size, 1.0f).x;
    }
    return (float)MeasureText(text, (int)size);
}

static void DrawGoldStar(float cx, float cy, float radius, bool filled) {
    if (!filled) {
        DrawCircleLines((int)cx, (int)cy, radius * 0.7f, (Color){ 120, 120, 130, 180 });
        return;
    }
    Vector2 points[10];
    for (int i = 0; i < 10; i++) {
        float r = (i % 2 == 0) ? radius : radius * 0.42f;
        float angle = (float)i * (PI / 5.0f) - PI * 0.5f;
        points[i] = (Vector2){ cx + cosf(angle) * r, cy + sinf(angle) * r };
    }
    for (int i = 0; i < 10; i++) {
        int next = (i + 1) % 10;
        DrawTriangle((Vector2){ cx, cy }, points[i], points[next], (Color){ 255, 205, 45, 255 });
    }
    for (int i = 0; i < 10; i++) {
        int next = (i + 1) % 10;
        DrawLineV(points[i], points[next], (Color){ 215, 150, 15, 255 });
    }
}

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 800
#define PIXELS_PER_METER 112.0f

#define TABLE_WIDTH_M 9.6f
#define TABLE_HEIGHT_M 5.1f
#define TABLE_CENTER_X 640.0f
#define TABLE_CENTER_Y 420.0f

#define TURN_TIME_LIMIT 16.0f

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#define EMSCRIPTEN_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define EMSCRIPTEN_EXPORT
#endif

// Player Control Types
typedef enum {
    CONTROL_HUMAN_LOCAL = 0,
    CONTROL_AI_BOT = 1,
    CONTROL_REMOTE_NETWORK = 2
} PlayerControlType;

// Match Types
typedef enum {
    MATCH_SINGLE_PLAYER_AI = 0, // P1=Human vs AI Bots (Single Player!)
    MATCH_LOCAL_PASS_PLAY = 1,  // All active players human (Local pass & play)
    MATCH_ONLINE_P2P = 2        // P1=Local vs P2=Remote peer (via WebRTC)
} MatchType;

static const char* g_matchTypeNames[3] = {
    "VS AI BOT",
    "LOCAL 2P",
    "ONLINE P2P"
};

// AI Bot Personalities / Difficulties
typedef enum {
    AI_FRESHMAN = 0,   // "Class Freshman": Slower, occasional loose flick (Casual)
    AI_DESKMATE = 1,   // "Desk Mate": Standard balanced aim (Medium)
    AI_PREFECT = 2,    // "Class Prefect": Sharp, direct aim (Hard)
    AI_OLYMPIAD = 3    // "Physics Prodigy": Optimal spin/torque sniper (Expert)
} AIDifficulty;

static const char* g_aiDifficultyNames[4] = {
    "FRESHMAN",
    "DESK MATE",
    "PREFECT",
    "PRODIGY"
};

// AI Turn Execution State
typedef enum {
    AI_STATE_DECIDE = 0,
    AI_STATE_AIMING = 1,
    AI_STATE_CHARGING = 2,
    AI_STATE_RELEASE = 3
} AIState;

// Game Modes
typedef enum {
    MODE_1V1 = 0,            // Classic Duel: Blue vs Red
    MODE_TEAMS_2V2 = 1,      // 2v2 Team Battle: Team Blue (P1+P3) vs Team Red (P2+P4) - Friendly Fire ON!
    MODE_BATTLE_ROYALE = 2   // 4-Player Free-For-All: Last Pen Standing!
} GameMode;

// Table Arena Types
typedef enum {
    TABLE_OPEN = 0,          // Classic open school desk: all 4 edges drop off into the void
    TABLE_FRONT_BARRIER = 1,  // Front desk backboard: top edge is solid wood, cannot fall off top!
    TABLE_DUAL_BARRIER = 2    // Double barricade: top & bottom edges are solid barriers (aisle duel)
} TableType;

static const char* g_tableTypeNames[3] = {
    "OPEN DESK",
    "FRONT BACKBOARD",
    "DUAL AISLE BARRIER"
};

static const char* g_gameModeNames[3] = {
    "1v1 DUEL",
    "2v2 TEAMS",
    "BATTLE ROYALE"
};

// Starting Stage / Rack Formations (Axis Orientations)
typedef enum {
    STAGE_VERTICAL = 0,     // Traditional School Desk: Vertical Y-axis upright (Broadside exposed, 1-hit KO potential!)
    STAGE_HORIZONTAL = 1,   // Tactical Head-to-Head: Horizontal X-axis facing (Defensive profile)
    STAGE_CROSS = 2         // Gauntlet Cross: Asymmetric (P1 horizontal vs opponents vertical)
} StageRack;

static const char* g_stageNames[3] = {
    "VERTICAL (TRAD)",
    "HORIZONTAL",
    "CROSS GAUNTLET"
};

// ==========================================
// PEN MARKET & BIRO PHYSICAL SPECIFICATIONS
// Real Box2D physical density, friction, caliber & elasticity
// ==========================================
typedef enum {
    PEN_MODEL_CLASSIC = 0,    // Classic Bic Biro (Balanced Standard)
    PEN_MODEL_JUMBO = 1,      // 4-Color Jumbo Tank (Fat Barrel & Heavy Mass, High Inertia)
    PEN_MODEL_RUBBER = 2,     // Rubber Matte Grip (High Surface Roughness & Table Friction)
    PEN_MODEL_CRYSTAL = 3,    // Slick Crystal Speed (Ultra-light, Low Friction Glider)
    PEN_MODEL_BRASS = 4       // Executive Metal Heavyweight (Dense Kinetic Sledgehammer)
} PenModelId;

#define NUM_PEN_MODELS 5

typedef struct {
    PenModelId modelId;
    const char* name;
    const char* subtitle;
    const char* descLine1;
    const char* descLine2;
    float halfLength;       // Half-length of capsule (meters)
    float radius;           // Capsule radius / barrel thickness (meters)
    float density;          // Mass density (kg/m^2)
    float friction;         // Surface roughness / Coulomb friction on desk
    float restitution;      // Bounciness / elasticity coefficient
    float linearDamping;    // Table rolling resistance / deceleration
    float angularDamping;   // Rotational spin decay
} PenModelDef;

static const PenModelDef g_penModels[NUM_PEN_MODELS] = {
    {
        .modelId = PEN_MODEL_CLASSIC,
        .name = "CLASSIC BIC",
        .subtitle = "Balanced Standard",
        .descLine1 = "The schoolyard classic benchmark.",
        .descLine2 = "Balanced speed & elastic rebound.",
        .halfLength = 1.08f,
        .radius = 0.052f,
        .density = 1.05f,
        .friction = 0.28f,
        .restitution = 0.70f,
        .linearDamping = 1.35f,
        .angularDamping = 1.85f
    },
    {
        .modelId = PEN_MODEL_JUMBO,
        .name = "JUMBO TANK",
        .subtitle = "Heavy & Fat Barrel",
        .descLine1 = "2.5x heavy mass with fat barrel.",
        .descLine2 = "Hard to displace, slower flick.",
        .halfLength = 1.05f,
        .radius = 0.076f,
        .density = 2.45f,
        .friction = 0.32f,
        .restitution = 0.55f,
        .linearDamping = 1.45f,
        .angularDamping = 2.10f
    },
    {
        .modelId = PEN_MODEL_RUBBER,
        .name = "RUBBER GRIP",
        .subtitle = "High Roughness",
        .descLine1 = "Knurled rubber sleeve with high u.",
        .descLine2 = "Brakes hard to prevent ring-outs.",
        .halfLength = 1.08f,
        .radius = 0.055f,
        .density = 1.30f,
        .friction = 0.65f,
        .restitution = 0.45f,
        .linearDamping = 2.25f,
        .angularDamping = 2.50f
    },
    {
        .modelId = PEN_MODEL_CRYSTAL,
        .name = "CRYSTAL SPEED",
        .subtitle = "Slick Glider",
        .descLine1 = "Ultra-slick, featherlight glider.",
        .descLine2 = "Long glides; vulnerable to hits.",
        .halfLength = 1.08f,
        .radius = 0.044f,
        .density = 0.65f,
        .friction = 0.14f,
        .restitution = 0.82f,
        .linearDamping = 0.85f,
        .angularDamping = 1.20f
    },
    {
        .modelId = PEN_MODEL_BRASS,
        .name = "BRASS HEAVY",
        .subtitle = "Metal Ram",
        .descLine1 = "Dense executive metal ballast.",
        .descLine2 = "Kinetic ram that clears the desk.",
        .halfLength = 1.04f,
        .radius = 0.056f,
        .density = 3.85f,
        .friction = 0.22f,
        .restitution = 0.68f,
        .linearDamping = 1.15f,
        .angularDamping = 1.60f
    }
};

// Game States
typedef enum {
    STATE_TITLE,
    STATE_AIMING,
    STATE_SIMULATING,
    STATE_EVALUATE,
    STATE_FALLING,
    STATE_ROUND_OVER,
    STATE_MATCH_OVER,
    STATE_PEN_MARKET
} GameState;

// Particle for impact sparks and confetti
typedef struct {
    Vector2 pos;
    Vector2 vel;
    Color color;
    float size;
    float life;
    float maxLife;
    bool isConfetti;
    float rotation;
    float rotSpeed;
} Particle;

#define MAX_PARTICLES 384
static Particle g_particles[MAX_PARTICLES];
static int g_particleCount = 0;

static void SpawnParticle(Vector2 pos, Vector2 vel, Color col, float size, float life, bool isConfetti) {
    if (g_particleCount >= MAX_PARTICLES) {
        memmove(&g_particles[0], &g_particles[1], sizeof(Particle) * (MAX_PARTICLES - 1));
        g_particleCount = MAX_PARTICLES - 1;
    }
    Particle* p = &g_particles[g_particleCount++];
    p->pos = pos;
    p->vel = vel;
    p->color = col;
    p->size = size;
    p->life = life;
    p->maxLife = life;
    p->isConfetti = isConfetti;
    p->rotation = (float)(rand() % 360);
    p->rotSpeed = ((float)(rand() % 200) - 100.0f) * 5.0f;
}

static void UpdateAndDrawParticles(float dt) {
    for (int i = 0; i < g_particleCount; i++) {
        Particle* p = &g_particles[i];
        p->life -= dt;
        if (p->life <= 0.0f) {
            g_particles[i] = g_particles[g_particleCount - 1];
            g_particleCount--;
            i--;
            continue;
        }

        p->pos.x += p->vel.x * dt;
        p->pos.y += p->vel.y * dt;
        p->rotation += p->rotSpeed * dt;

        float alpha = p->life / p->maxLife;
        Color c = p->color;
        c.a = (unsigned char)(alpha * 255.0f);

        if (p->isConfetti) {
            p->vel.y += 180.0f * dt;
            p->vel.x *= 0.99f;
            Rectangle rect = { p->pos.x, p->pos.y, p->size * 1.8f, p->size };
            DrawRectanglePro(rect, (Vector2){ rect.width * 0.5f, rect.height * 0.5f }, p->rotation, c);
        } else {
            p->vel.x *= 0.92f;
            p->vel.y *= 0.92f;
            DrawCircleV(p->pos, p->size * alpha, c);
        }
    }
}

// Coordinate conversions
static inline Vector2 ScreenFromWorld(b2Vec2 p) {
    return (Vector2){
        TABLE_CENTER_X + p.x * PIXELS_PER_METER,
        TABLE_CENTER_Y + p.y * PIXELS_PER_METER
    };
}

static inline b2Vec2 WorldFromScreen(Vector2 s) {
    return (b2Vec2){
        (s.x - TABLE_CENTER_X) / PIXELS_PER_METER,
        (s.y - TABLE_CENTER_Y) / PIXELS_PER_METER
    };
}

// Biro Pen representation
typedef struct {
    b2BodyId bodyId;
    b2ShapeId barrelShapeId;

    int playerIndex; // 0=Blue, 1=Red, 2=Green, 3=Black
    PenModelId modelId; // Currently equipped model from the market!
    const char* playerName;
    const char* shortName;
    Color primaryColor;
    Color accentColor;
    Color capColor;
    Color plugColor;
    Color inkColor;

    bool isFalling;
    bool isEliminated; // knocked off desk this round
    float fallProgress;
    b2Vec2 fallStartPos;
    float fallStartAngle;

    int score; // rounds won
} Biro;

// Procedural Audio Generation
typedef struct {
    Sound sndFlick;
    Sound sndClack;
    Sound sndFall;
    Sound sndScore;
    Sound sndBuzzer;
    Sound sndTick;
    bool audioReady;
} GameAudio;

static GameAudio g_audio = {0};

typedef struct {
    b2WorldId worldId;
    b2BodyId topBarrierBody;
    b2BodyId bottomBarrierBody;

    Rectangle deskRect;
    TableType currentTableType;
    GameMode currentGameMode;
    StageRack currentStage;
    MatchType matchType;
    AIDifficulty aiDifficulty;

    // Online multiplayer state
    int onlineLocalPlayerIndex; // 0 for host (Blue), 1 for guest (Red)
    bool isOnlineHost;
    bool isOnlineConnected;
    char onlineRoomCode[16];
    bool showOnlineModal;

    // AI Bot State
    AIState aiState;
    float aiActionTimer;
    float aiTargetAngle;
    float aiTargetPower;
    b2Vec2 aiTargetStrikePoint;

    // Pens & match state
    Biro biros[4];
    GameState state;
    int marketSelectedPlayer;
    int activePlayer;
    int roundNumber;
    int roundStartingPlayer;
    bool isFirstMoveOfRound;
    const char* roundOutcomeMsg;

    int teamBlueScore;
    int teamRedScore;

    // Aiming & turn timing
    float turnTimer;
    float arrowAngle;
    bool isCharging;
    float chargeTimer;
    b2Vec2 chosenStrikePoint;
    float lockedArrowAngle;
    int lastTickSecond;

    // Visuals & debug
    bool showDebugColliders;
    bool showTelemetry;
    float screenShake;
    float settleTimer;
    float stateTimer;
    float simTimer;
    int frameCount;

    // Testing / CLI params
    const char* autoScreenshotPath;
    bool autotestMode;
    bool startInMarket;

    // ---- Touch / Swipe control vs Desktop PC Mouse control ----
    // false = Desktop PC Mouse (rotating arrow, ruler charge, release to strike)
    // true  = Mobile Touch Swipe (swipe across biro, momentum power, spear move)
    bool    isTouchMode;               // false by default (PC Mouse mode)
    bool    isSwiping;                 // true while finger is held down (tracking gesture)
    Vector2 touchSwipeStart;           // screen position where the current swipe began
    Vector2 touchSwipeCurrent;         // screen position where finger currently is
    float   touchSwipeStartTime;       // GetTime() timestamp when swipe started
    float   swipeMissFeedbackTimer;    // timer to display "Swipe across the pen!" feedback

    // ---- Single Menu / Pause Modal ----
    bool    isPausedMenuOpen;          // true when user clicked [ ⏸ MENU ] or pressed ESC
} GameContext;

static GameContext g_game = {0};

#if defined(PLATFORM_WEB)
EM_JS(void, JS_SendNetworkStrike, (int player, float ptX, float ptY, float angle, float power), {
    if (typeof window.onLocalStrike === 'function') {
        window.onLocalStrike(player, ptX, ptY, angle, power);
    }
});
EM_JS(void, JS_SendNetworkSync, (int player, float x, float y, float angle, int elim, int score), {
    if (typeof window.onLocalSync === 'function') {
        window.onLocalSync(player, x, y, angle, elim, score);
    }
});
EM_JS(void, JS_SendRoundRestart, (int mode, int stage, int table), {
    if (typeof window.onLocalRoundRestart === 'function') {
        window.onLocalRoundRestart(mode, stage, table);
    }
});
EM_JS(void, JS_CopyRoomLink, (void), {
    if (typeof window.copyRoomLink === 'function') {
        window.copyRoomLink();
    }
});
EM_JS(void, JS_TweetChallenge, (void), {
    if (typeof window.tweetChallenge === 'function') {
        window.tweetChallenge();
    }
});
EM_JS(void, JS_RequestRoomCode, (void), {
    if (typeof window.initOnlineHost === 'function') {
        window.initOnlineHost();
    }
});
EM_JS(void, JS_ToggleNotebookMenu, (void), {
    if (typeof window.toggleNotebookMenu === 'function') {
        window.toggleNotebookMenu();
    }
});
#else
static void JS_SendNetworkStrike(int player, float ptX, float ptY, float angle, float power) { (void)player; (void)ptX; (void)ptY; (void)angle; (void)power; }
static void JS_SendNetworkSync(int player, float x, float y, float angle, int elim, int score) { (void)player; (void)x; (void)y; (void)angle; (void)elim; (void)score; }
static void JS_SendRoundRestart(int mode, int stage, int table) { (void)mode; (void)stage; (void)table; }
static void JS_CopyRoomLink(void) {}
static void JS_TweetChallenge(void) {}
static void JS_RequestRoomCode(void) {}
static void JS_ToggleNotebookMenu(void) {}
#endif

static Sound GenFlickSound(void) {
    int sampleRate = 44100;
    int frames = (int)(sampleRate * 0.07f);
    short* data = (short*)malloc(frames * sizeof(short));
    for (int i = 0; i < frames; i++) {
        float t = (float)i / (float)sampleRate;
        float env = expf(-t * 90.0f);
        float s = sinf(2.0f * 3.14159265f * (1800.0f - t * 12000.0f) * t) * env;
        float noise = ((float)rand() / (float)RAND_MAX * 2.0f - 1.0f) * env * 0.4f;
        float sample = (s * 0.7f + noise * 0.3f);
        data[i] = (short)(Clamp(sample, -1.0f, 1.0f) * 30000.0f);
    }
    Wave w = { .frameCount = frames, .sampleRate = sampleRate, .sampleSize = 16, .channels = 1, .data = data };
    Sound s = LoadSoundFromWave(w);
    UnloadWave(w);
    return s;
}

static Sound GenClackSound(void) {
    int sampleRate = 44100;
    int frames = (int)(sampleRate * 0.09f);
    short* data = (short*)malloc(frames * sizeof(short));
    for (int i = 0; i < frames; i++) {
        float t = (float)i / (float)sampleRate;
        float env = expf(-t * 70.0f);
        float s1 = sinf(2.0f * 3.14159265f * 980.0f * t);
        float s2 = sinf(2.0f * 3.14159265f * 2240.0f * t);
        float noise = ((float)rand() / (float)RAND_MAX * 2.0f - 1.0f) * expf(-t * 220.0f);
        float sample = (s1 * 0.5f + s2 * 0.3f + noise * 0.4f) * env;
        data[i] = (short)(Clamp(sample, -1.0f, 1.0f) * 31000.0f);
    }
    Wave w = { .frameCount = frames, .sampleRate = sampleRate, .sampleSize = 16, .channels = 1, .data = data };
    Sound s = LoadSoundFromWave(w);
    UnloadWave(w);
    return s;
}

static Sound GenFallSound(void) {
    int sampleRate = 44100;
    int frames = (int)(sampleRate * 0.45f);
    short* data = (short*)malloc(frames * sizeof(short));
    for (int i = 0; i < frames; i++) {
        float t = (float)i / (float)sampleRate;
        float progress = t / 0.45f;
        float freq = 850.0f * (1.0f - progress * 0.75f);
        float env = sinf(progress * 3.14159265f);
        float s = sinf(2.0f * 3.14159265f * freq * t) * env;
        data[i] = (short)(Clamp(s, -1.0f, 1.0f) * 26000.0f);
    }
    Wave w = { .frameCount = frames, .sampleRate = sampleRate, .sampleSize = 16, .channels = 1, .data = data };
    Sound s = LoadSoundFromWave(w);
    UnloadWave(w);
    return s;
}

static Sound GenScoreSound(void) {
    int sampleRate = 44100;
    int frames = (int)(sampleRate * 0.50f);
    short* data = (short*)malloc(frames * sizeof(short));
    for (int i = 0; i < frames; i++) {
        float t = (float)i / (float)sampleRate;
        float s = 0.0f;
        if (t < 0.14f) s = sinf(2.0f * 3.14159265f * 523.25f * t) * expf(-t * 8.0f);
        else if (t < 0.28f) s = sinf(2.0f * 3.14159265f * 659.25f * (t - 0.14f)) * expf(-(t - 0.14f) * 8.0f);
        else s = sinf(2.0f * 3.14159265f * 783.99f * (t - 0.28f)) * expf(-(t - 0.28f) * 6.0f);
        data[i] = (short)(Clamp(s, -1.0f, 1.0f) * 28000.0f);
    }
    Wave w = { .frameCount = frames, .sampleRate = sampleRate, .sampleSize = 16, .channels = 1, .data = data };
    Sound s = LoadSoundFromWave(w);
    UnloadWave(w);
    return s;
}

static Sound GenBuzzerSound(void) {
    int sampleRate = 44100;
    int frames = (int)(sampleRate * 0.30f);
    short* data = (short*)malloc(frames * sizeof(short));
    for (int i = 0; i < frames; i++) {
        float t = (float)i / (float)sampleRate;
        float env = (t < 0.24f) ? 1.0f : (1.0f - (t - 0.24f) / 0.06f);
        float s = (sinf(2.0f * 3.14159265f * 145.0f * t) > 0.0f ? 0.65f : -0.65f) * env;
        data[i] = (short)(s * 25000.0f);
    }
    Wave w = { .frameCount = frames, .sampleRate = sampleRate, .sampleSize = 16, .channels = 1, .data = data };
    Sound s = LoadSoundFromWave(w);
    UnloadWave(w);
    return s;
}

static Sound GenTickSound(void) {
    int sampleRate = 44100;
    int frames = (int)(sampleRate * 0.025f);
    short* data = (short*)malloc(frames * sizeof(short));
    for (int i = 0; i < frames; i++) {
        float t = (float)i / (float)sampleRate;
        float env = expf(-t * 220.0f);
        float s = sinf(2.0f * 3.14159265f * 2400.0f * t) * env;
        data[i] = (short)(s * 22000.0f);
    }
    Wave w = { .frameCount = frames, .sampleRate = sampleRate, .sampleSize = 16, .channels = 1, .data = data };
    Sound s = LoadSoundFromWave(w);
    UnloadWave(w);
    return s;
}

static void InitGameAudio(void) {
    InitAudioDevice();
    if (IsAudioDeviceReady()) {
        g_audio.audioReady = true;
        g_audio.sndFlick  = GenFlickSound();
        g_audio.sndClack  = GenClackSound();
        g_audio.sndFall   = GenFallSound();
        g_audio.sndScore  = GenScoreSound();
        g_audio.sndBuzzer = GenBuzzerSound();
        g_audio.sndTick   = GenTickSound();
    }
}

static void CloseGameAudio(void) {
    if (g_audio.audioReady) {
        UnloadSound(g_audio.sndFlick);
        UnloadSound(g_audio.sndClack);
        UnloadSound(g_audio.sndFall);
        UnloadSound(g_audio.sndScore);
        UnloadSound(g_audio.sndBuzzer);
        UnloadSound(g_audio.sndTick);
        CloseAudioDevice();
    }
}

// Equip a Pen Model onto a Biro: Updates Box2D Capsule Collider, Mass, Friction, and Damping
static void EquipPenModel(Biro* pen, PenModelId modelId) {
    pen->modelId = modelId;
    const PenModelDef* def = &g_penModels[modelId];

    // Destroy existing shape if valid
    if (b2Shape_IsValid(pen->barrelShapeId)) {
        b2DestroyShape(pen->barrelShapeId, true);
        pen->barrelShapeId = b2_nullShapeId;
    }

    // Create new capsule shape with model's physical parameters
    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = def->density;
    shapeDef.material.friction = def->friction;
    shapeDef.material.restitution = def->restitution;
    shapeDef.enableHitEvents = true;
    shapeDef.enableContactEvents = true;

    b2Capsule capsule = {
        .center1 = (b2Vec2){ -def->halfLength, 0.0f },
        .center2 = (b2Vec2){  def->halfLength, 0.0f },
        .radius  = def->radius
    };
    pen->barrelShapeId = b2CreateCapsuleShape(pen->bodyId, &shapeDef, &capsule);

    // Update body damping
    b2Body_SetLinearDamping(pen->bodyId, def->linearDamping);
    b2Body_SetAngularDamping(pen->bodyId, def->angularDamping);
}

// Biro Pen Construction: Smooth Single-Capsule Collider
static void CreateBiro(b2WorldId worldId, Biro* pen, int playerIndex, b2Vec2 spawnPos, float spawnAngle) {
    pen->playerIndex = playerIndex;
    pen->isFalling = false;
    pen->isEliminated = false;
    pen->fallProgress = 0.0f;
    pen->score = 0;
    pen->barrelShapeId = b2_nullShapeId;

    switch (playerIndex) {
        case 0: // P1 BLUE
            pen->playerName   = "PLAYER 1 (BLUE)";
            pen->shortName    = "P1 BLUE";
            pen->primaryColor = (Color){ 70, 150, 255, 230 };
            pen->accentColor  = (Color){ 30, 90, 210, 255 };
            pen->capColor     = (Color){ 12, 70, 195, 255 };
            pen->plugColor    = (Color){ 12, 70, 195, 255 };
            pen->inkColor     = (Color){ 8, 45, 145, 255 };
            break;
        case 1: // P2 RED
            pen->playerName   = "PLAYER 2 (RED)";
            pen->shortName    = "P2 RED";
            pen->primaryColor = (Color){ 255, 90, 90, 230 };
            pen->accentColor  = (Color){ 210, 40, 40, 255 };
            pen->capColor     = (Color){ 210, 25, 25, 255 };
            pen->plugColor    = (Color){ 210, 25, 25, 255 };
            pen->inkColor     = (Color){ 160, 15, 15, 255 };
            break;
        case 2: // P3 GREEN
            pen->playerName   = "PLAYER 3 (GREEN)";
            pen->shortName    = "P3 GREEN";
            pen->primaryColor = (Color){ 60, 210, 110, 230 };
            pen->accentColor  = (Color){ 25, 150, 60, 255 };
            pen->capColor     = (Color){ 18, 145, 55, 255 };
            pen->plugColor    = (Color){ 18, 145, 55, 255 };
            pen->inkColor     = (Color){ 10, 105, 35, 255 };
            break;
        case 3: // P4 BLACK
        default:
            pen->playerName   = "PLAYER 4 (BLACK)";
            pen->shortName    = "P4 BLACK";
            pen->primaryColor = (Color){ 120, 125, 135, 230 };
            pen->accentColor  = (Color){ 60, 65, 75, 255 };
            pen->capColor     = (Color){ 35, 35, 40, 255 };
            pen->plugColor    = (Color){ 35, 35, 40, 255 };
            pen->inkColor     = (Color){ 20, 20, 25, 255 };
            break;
    }

    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = spawnPos;
    bodyDef.rotation = b2MakeRot(spawnAngle);
    bodyDef.linearDamping = 1.35f;
    bodyDef.angularDamping = 1.85f;
    bodyDef.isBullet = false;
    bodyDef.enableSleep = false;
    bodyDef.name = pen->playerName;

    pen->bodyId = b2CreateBody(worldId, &bodyDef);

    // Equip default Classic model
    EquipPenModel(pen, PEN_MODEL_CLASSIC);
}

// Reset Biro to spawn position
static void ResetBiro(Biro* pen, b2Vec2 spawnPos, float spawnAngle) {
    pen->isFalling = false;
    pen->isEliminated = false;
    pen->fallProgress = 0.0f;
    b2Body_Enable(pen->bodyId);
    b2Body_SetTransform(pen->bodyId, spawnPos, b2MakeRot(spawnAngle));
    b2Body_SetLinearVelocity(pen->bodyId, (b2Vec2){ 0.0f, 0.0f });
    b2Body_SetAngularVelocity(pen->bodyId, 0.0f);
    b2Body_ClearForces(pen->bodyId);
}

// Draw realistic Biro Pen adapting to equipped model attributes
static void DrawBiroPen(const Biro* pen, bool showDebugColliders, float alpha) {
    if (pen->isEliminated && !pen->isFalling) return; // knocked off and dead

    const PenModelDef* def = &g_penModels[pen->modelId];
    float barrelRadiusPx = def->radius * PIXELS_PER_METER;
    float barrelThick = barrelRadiusPx * 2.0f;

    if (pen->isFalling) {
        float t = pen->fallProgress;
        float scale = 1.0f - t * 0.85f;
        float zDrop = t * t * 180.0f;
        Vector2 screenPos = ScreenFromWorld(pen->fallStartPos);
        screenPos.y += zDrop;
        float angleDeg = (pen->fallStartAngle + t * 24.0f) * RAD2DEG;

        Rectangle rect = { screenPos.x, screenPos.y, 260.0f * scale, barrelThick * scale };
        Color c = pen->capColor;
        c.a = (unsigned char)((1.0f - t) * 210.0f);
        DrawRectanglePro(rect, (Vector2){ rect.width * 0.5f, rect.height * 0.5f }, angleDeg, c);
        return;
    }

    b2Pos bodyPos = b2Body_GetPosition(pen->bodyId);
    b2Rot bodyRot = b2Body_GetRotation(pen->bodyId);
    float angleRad = b2Rot_GetAngle(bodyRot);
    float angleDeg = angleRad * RAD2DEG;
    Vector2 center = ScreenFromWorld(bodyPos);

    #define LOCAL_TO_SCREEN(lx, ly) \
        ScreenFromWorld(b2Body_GetWorldPoint(pen->bodyId, (b2Vec2){ (lx), (ly) }))

    // 1. DROP SHADOW (Adapts to barrel thickness)
    Vector2 shadowOffset = { 8.0f, 12.0f };
    Vector2 shadowCenter = Vector2Add(center, shadowOffset);
    Color shadowCol = (Color){ 18, 12, 8, (unsigned char)(65 * alpha) };
    DrawRectanglePro(
        (Rectangle){ shadowCenter.x, shadowCenter.y, 280.0f, barrelThick + 4.0f },
        (Vector2){ 140.0f, (barrelThick + 4.0f) * 0.5f },
        angleDeg,
        shadowCol
    );

    // 2. REAR END SECTION
    Vector2 plugStart = LOCAL_TO_SCREEN(-1.14f, 0.0f);
    Vector2 plugEnd   = LOCAL_TO_SCREEN(-0.96f, 0.0f);
    float plugThick = barrelThick * 0.95f;

    if (pen->modelId == PEN_MODEL_JUMBO) {
        // Jumbo 4-color pen: 4 colored clicker buttons jutting out from crown!
        DrawLineEx(plugStart, plugEnd, plugThick + 2.0f, (Color){ 20, 20, 30, (unsigned char)(140 * alpha) });
        DrawLineEx(plugStart, plugEnd, plugThick, (Color){ 220, 225, 235, (unsigned char)(255 * alpha) });
        // 4 slider buttons
        Vector2 b1 = LOCAL_TO_SCREEN(-1.22f, -0.045f);
        Vector2 b2 = LOCAL_TO_SCREEN(-1.22f, -0.015f);
        Vector2 b3 = LOCAL_TO_SCREEN(-1.22f,  0.015f);
        Vector2 b4 = LOCAL_TO_SCREEN(-1.22f,  0.045f);
        DrawLineEx(b1, LOCAL_TO_SCREEN(-1.14f, -0.045f), 4.0f, (Color){ 12, 70, 195, 255 });
        DrawLineEx(b2, LOCAL_TO_SCREEN(-1.14f, -0.015f), 4.0f, (Color){ 210, 25, 25, 255 });
        DrawLineEx(b3, LOCAL_TO_SCREEN(-1.14f,  0.015f), 4.0f, (Color){ 18, 145, 55, 255 });
        DrawLineEx(b4, LOCAL_TO_SCREEN(-1.14f,  0.045f), 4.0f, (Color){ 35, 35, 40, 255 });
    } else if (pen->modelId == PEN_MODEL_BRASS) {
        // Executive brass: metallic gold/chrome rear clicker
        DrawLineEx(plugStart, plugEnd, plugThick + 2.0f, (Color){ 35, 28, 12, (unsigned char)(140 * alpha) });
        DrawLineEx(plugStart, plugEnd, plugThick, (Color){ 225, 185, 95, (unsigned char)(255 * alpha) });
        DrawCircleV(plugStart, plugThick * 0.45f, (Color){ 255, 220, 140, (unsigned char)(255 * alpha) });
        Vector2 btn = LOCAL_TO_SCREEN(-1.20f, 0.0f);
        DrawLineEx(btn, plugStart, plugThick * 0.6f, (Color){ 240, 205, 120, (unsigned char)(255 * alpha) });
    } else {
        // Standard colored plug
        DrawLineEx(plugStart, plugEnd, plugThick + 2.0f, (Color){ 20, 20, 30, (unsigned char)(140 * alpha) });
        DrawLineEx(plugStart, plugEnd, plugThick, pen->plugColor);
        DrawCircleV(plugStart, plugThick * 0.46f, pen->plugColor);
    }

    // 3. MAIN BARREL
    Vector2 bStart = LOCAL_TO_SCREEN(-0.96f, 0.0f);
    Vector2 bEnd   = LOCAL_TO_SCREEN( 0.44f, 0.0f);

    if (pen->modelId == PEN_MODEL_JUMBO) {
        // Thick glossy ivory barrel with player accent band
        DrawLineEx(bStart, bEnd, barrelThick + 2.4f, (Color){ 40, 30, 20, (unsigned char)(120 * alpha) });
        DrawLineEx(bStart, bEnd, barrelThick, (Color){ 245, 248, 255, (unsigned char)(255 * alpha) });
        // Accent mid-band
        Vector2 bandStart = LOCAL_TO_SCREEN(-0.45f, 0.0f);
        Vector2 bandEnd   = LOCAL_TO_SCREEN(-0.15f, 0.0f);
        DrawLineEx(bandStart, bandEnd, barrelThick, pen->primaryColor);
        // Longitudinal highlight
        Vector2 hlStart = LOCAL_TO_SCREEN(-0.94f, -def->radius * 0.5f);
        Vector2 hlEnd   = LOCAL_TO_SCREEN( 0.42f, -def->radius * 0.5f);
        DrawLineEx(hlStart, hlEnd, 3.0f, (Color){ 255, 255, 255, (unsigned char)(210 * alpha) });
    } else if (pen->modelId == PEN_MODEL_RUBBER) {
        // Rear matte body + Front textured rubber sleeve with ribs
        DrawLineEx(bStart, bEnd, barrelThick + 2.2f, (Color){ 35, 30, 35, (unsigned char)(120 * alpha) });
        // Rear body: matte grey
        DrawLineEx(bStart, LOCAL_TO_SCREEN(0.0f, 0.0f), barrelThick, (Color){ 175, 180, 190, (unsigned char)(240 * alpha) });
        // Front rubber grip: charcoal with team accent
        Vector2 gripStart = LOCAL_TO_SCREEN(0.0f, 0.0f);
        Vector2 gripEnd   = LOCAL_TO_SCREEN(0.44f, 0.0f);
        DrawLineEx(gripStart, gripEnd, barrelThick + 1.0f, (Color){ 40, 45, 55, (unsigned char)(255 * alpha) });
        // Raised rubber traction ribs!
        for (int r = 1; r <= 4; r++) {
            Vector2 r1 = LOCAL_TO_SCREEN(0.08f * r, -def->radius * 0.9f);
            Vector2 r2 = LOCAL_TO_SCREEN(0.08f * r,  def->radius * 0.9f);
            DrawLineEx(r1, r2, 2.5f, pen->accentColor);
        }
    } else if (pen->modelId == PEN_MODEL_CRYSTAL) {
        // Ultra-slim clear crystal glass
        DrawLineEx(bStart, bEnd, barrelThick + 2.0f, (Color){ 30, 45, 60, (unsigned char)(100 * alpha) });
        DrawLineEx(bStart, bEnd, barrelThick, (Color){ 240, 248, 255, (unsigned char)(190 * alpha) });
        // Precision thin ink needle tube
        Vector2 inkStart = LOCAL_TO_SCREEN(-0.85f, 0.0f);
        Vector2 inkEnd   = LOCAL_TO_SCREEN( 0.42f, 0.0f);
        DrawLineEx(inkStart, inkEnd, 2.5f, pen->inkColor);
        // Crystal sparkle highlight
        Vector2 hlStart = LOCAL_TO_SCREEN(-0.94f, -def->radius * 0.45f);
        Vector2 hlEnd   = LOCAL_TO_SCREEN( 0.42f, -def->radius * 0.45f);
        DrawLineEx(hlStart, hlEnd, 1.8f, (Color){ 255, 255, 255, (unsigned char)(250 * alpha) });
    } else if (pen->modelId == PEN_MODEL_BRASS) {
        // Executive heavy brushed brass/chrome metal
        DrawLineEx(bStart, bEnd, barrelThick + 2.4f, (Color){ 45, 35, 15, (unsigned char)(130 * alpha) });
        DrawLineEx(bStart, bEnd, barrelThick, (Color){ 230, 205, 140, (unsigned char)(255 * alpha) });
        // Center joiner ring
        Vector2 ringStart = LOCAL_TO_SCREEN(-0.15f, 0.0f);
        Vector2 ringEnd   = LOCAL_TO_SCREEN(-0.08f, 0.0f);
        DrawLineEx(ringStart, ringEnd, barrelThick + 1.5f, (Color){ 255, 235, 180, (unsigned char)(255 * alpha) });
        // Metallic specular shine streak
        Vector2 hlStart = LOCAL_TO_SCREEN(-0.94f, -def->radius * 0.45f);
        Vector2 hlEnd   = LOCAL_TO_SCREEN( 0.42f, -def->radius * 0.45f);
        DrawLineEx(hlStart, hlEnd, 2.2f, (Color){ 255, 255, 255, (unsigned char)(220 * alpha) });
    } else {
        // Classic hexagonal crystal barrel
        DrawLineEx(bStart, bEnd, barrelThick + 2.2f, (Color){ 45, 30, 20, (unsigned char)(110 * alpha) });
        DrawLineEx(bStart, bEnd, barrelThick, (Color){ 238, 244, 252, (unsigned char)(210 * alpha) });

        Vector2 airGapStart = LOCAL_TO_SCREEN(-0.94f, 0.0f);
        Vector2 airGapEnd   = LOCAL_TO_SCREEN(-0.80f, 0.0f);
        DrawLineEx(airGapStart, airGapEnd, 3.0f, (Color){ 200, 215, 230, (unsigned char)(180 * alpha) });

        Vector2 inkStart = LOCAL_TO_SCREEN(-0.80f, 0.0f);
        Vector2 inkEnd   = LOCAL_TO_SCREEN( 0.42f, 0.0f);
        DrawLineEx(inkStart, inkEnd, 3.8f, pen->inkColor);

        Vector2 hl1Start = LOCAL_TO_SCREEN(-0.96f, -def->radius * 0.5f);
        Vector2 hl1End   = LOCAL_TO_SCREEN( 0.42f, -def->radius * 0.5f);
        DrawLineEx(hl1Start, hl1End, 2.0f, (Color){ 255, 255, 255, (unsigned char)(230 * alpha) });
    }

    // 4. FRONT CAP / CONE SECTION
    Vector2 capStart = LOCAL_TO_SCREEN(0.42f, 0.0f);
    Vector2 capEnd   = LOCAL_TO_SCREEN(1.14f, 0.0f);
    float capThick = barrelThick * 1.12f;

    if (pen->modelId == PEN_MODEL_JUMBO || pen->modelId == PEN_MODEL_RUBBER || pen->modelId == PEN_MODEL_BRASS) {
        // Retractable nose cone + tungsten nib
        Color coneCol = (pen->modelId == PEN_MODEL_BRASS) ? (Color){ 255, 230, 160, 255 } : (Color){ 200, 208, 218, 255 };
        DrawLineEx(capStart, capEnd, capThick + 1.0f, (Color){ 30, 30, 40, (unsigned char)(140 * alpha) });
        DrawLineEx(capStart, capEnd, capThick * 0.85f, coneCol);
        // Extended ball nib tip
        Vector2 nibPt = LOCAL_TO_SCREEN(1.19f, 0.0f);
        DrawLineEx(capEnd, nibPt, 3.0f, (Color){ 50, 50, 60, 255 });
        DrawCircleV(nibPt, 2.0f, (Color){ 20, 20, 30, 255 });
    } else {
        // Standard colored cap
        DrawLineEx(capStart, capEnd, capThick + 2.0f, (Color){ 15, 18, 28, (unsigned char)(150 * alpha) });
        DrawLineEx(capStart, capEnd, capThick, pen->capColor);

        Vector2 capHlStart = LOCAL_TO_SCREEN(0.44f, -def->radius * 0.52f);
        Vector2 capHlEnd   = LOCAL_TO_SCREEN(1.10f, -def->radius * 0.52f);
        DrawLineEx(capHlStart, capHlEnd, 2.0f, (Color){ 255, 255, 255, (unsigned char)(160 * alpha) });

        DrawCircleV(capStart, capThick * 0.48f, pen->capColor);
        DrawCircleV(capEnd, capThick * 0.45f, pen->capColor);
        DrawCircleV(LOCAL_TO_SCREEN(1.15f, 0.0f), 2.8f, (Color){ 10, 10, 20, (unsigned char)(255 * alpha) });
    }

    // 5. ASYMMETRIC POCKET CLIP
    Vector2 clipRoot = LOCAL_TO_SCREEN(0.96f, def->radius * 1.05f);
    Vector2 clipEnd  = LOCAL_TO_SCREEN(0.56f, def->radius * 1.25f);
    Vector2 clipHook = LOCAL_TO_SCREEN(0.53f, def->radius * 1.50f);
    Color clipCol = (pen->modelId == PEN_MODEL_BRASS) ? (Color){ 255, 220, 130, 255 } : pen->capColor;
    DrawLineEx(clipRoot, clipEnd, 4.5f, clipCol);
    DrawLineEx(clipEnd, clipHook, 4.0f, clipCol);
    DrawCircleV(clipHook, 2.5f, clipCol);

    // DEBUG COLLIDERS (if toggled with [C])
    if (showDebugColliders) {
        DrawCircleLines((int)center.x, (int)center.y, 8.0f, YELLOW);
        b2Pos comWorld = b2Body_GetWorldCenter(pen->bodyId);
        DrawCircleV(ScreenFromWorld(comWorld), 3.0f, RED);
        DrawLineV(center, ScreenFromWorld(b2Add(bodyPos, b2Body_GetLinearVelocity(pen->bodyId))), RED);
    }

    #undef LOCAL_TO_SCREEN
}

// Setup or Rebuild Static Table Barrier Colliders in Box2D v3
static void SetupTableColliders(b2WorldId worldId, TableType tableType, b2BodyId* topBarrierId, b2BodyId* bottomBarrierId) {
    if (b2Body_IsValid(*topBarrierId)) {
        b2DestroyBody(*topBarrierId);
        *topBarrierId = b2_nullBodyId;
    }
    if (b2Body_IsValid(*bottomBarrierId)) {
        b2DestroyBody(*bottomBarrierId);
        *bottomBarrierId = b2_nullBodyId;
    }

    float halfDeskW = TABLE_WIDTH_M * 0.5f;
    float halfDeskH = TABLE_HEIGHT_M * 0.5f;
    float barrierThick = 0.32f; // 32cm thick solid barrier wall

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = 0.0f; // static body
    shapeDef.material.friction = 0.25f;
    shapeDef.material.restitution = 0.72f; // crisp wooden ricochet!
    shapeDef.enableHitEvents = true;
    shapeDef.enableContactEvents = true;

    // Top Barrier (Front Desk Backboard)
    if (tableType == TABLE_FRONT_BARRIER || tableType == TABLE_DUAL_BARRIER) {
        b2BodyDef bodyDef = b2DefaultBodyDef();
        bodyDef.type = b2_staticBody;
        bodyDef.position = (b2Vec2){ 0.0f, -halfDeskH - barrierThick * 0.5f + 0.02f };
        bodyDef.name = "FrontDeskBackboard";
        *topBarrierId = b2CreateBody(worldId, &bodyDef);

        b2Polygon box = b2MakeBox(halfDeskW + 0.40f, barrierThick * 0.5f);
        b2CreatePolygonShape(*topBarrierId, &shapeDef, &box);
    }

    // Bottom Barrier
    if (tableType == TABLE_DUAL_BARRIER) {
        b2BodyDef bodyDef = b2DefaultBodyDef();
        bodyDef.type = b2_staticBody;
        bodyDef.position = (b2Vec2){ 0.0f, halfDeskH + barrierThick * 0.5f - 0.02f };
        bodyDef.name = "BottomAisleBarrier";
        *bottomBarrierId = b2CreateBody(worldId, &bodyDef);

        b2Polygon box = b2MakeBox(halfDeskW + 0.40f, barrierThick * 0.5f);
        b2CreatePolygonShape(*bottomBarrierId, &shapeDef, &box);
    }
}

// Classroom Desk and Arena Barrier Rendering
static void DrawSchoolDesk(Rectangle deskRect, TableType tableType) {
    // 1. Desk Base Drop Shadow on Classroom Tile Floor
    Rectangle shadowRect = { deskRect.x + 18, deskRect.y + 22, deskRect.width, deskRect.height };
    DrawRectangleRounded(shadowRect, 0.025f, 8, (Color){ 10, 14, 12, 175 });

    // 2. Beveled Dark Desk Rim (Aged Walnut Edge)
    DrawRectangleRounded(
        (Rectangle){ deskRect.x - 7, deskRect.y - 7, deskRect.width + 14, deskRect.height + 14 },
        0.030f, 8, (Color){ 62, 36, 18, 255 }
    );
    // Outer desk corner metal brackets with screws
    float cornerOffsets[4][2] = {
        { deskRect.x - 5, deskRect.y - 5 },
        { deskRect.x + deskRect.width - 15, deskRect.y - 5 },
        { deskRect.x - 5, deskRect.y + deskRect.height - 15 },
        { deskRect.x + deskRect.width - 15, deskRect.y + deskRect.height - 15 }
    };
    for (int c = 0; c < 4; c++) {
        DrawRectangle((int)cornerOffsets[c][0], (int)cornerOffsets[c][1], 20, 20, (Color){ 90, 65, 35, 230 });
        DrawCircle((int)cornerOffsets[c][0] + 10, (int)cornerOffsets[c][1] + 10, 2.5f, (Color){ 45, 30, 15, 255 });
    }

    // 3. Desk Wooden Tabletop Surface (Warm Schoolroom Oak / Honey Pine)
    DrawRectangleRounded(deskRect, 0.025f, 8, (Color){ 188, 126, 74, 255 });

    // Multi-frequency organic wood grain strips
    for (int i = 0; i < (int)deskRect.height; i += 16) {
        Color grainCol;
        if (i % 48 == 0) grainCol = (Color){ 158, 98, 52, 75 };
        else if (i % 32 == 0) grainCol = (Color){ 208, 146, 92, 60 };
        else grainCol = (Color){ 172, 112, 64, 45 };
        DrawRectangle((int)deskRect.x + 4, (int)deskRect.y + i, (int)deskRect.width - 8, (i % 32 == 0) ? 3 : 1, grainCol);
    }

    // Carved Pencil/Pen Resting Groove along the top edge of desk
    Rectangle pencilGroove = { deskRect.x + 140, deskRect.y + 14, deskRect.width - 280, 10 };
    DrawRectangleRounded(pencilGroove, 0.5f, 4, (Color){ 110, 62, 32, 230 });
    DrawRectangleRoundedLines(pencilGroove, 0.5f, 4, (Color){ 75, 40, 18, 255 });
    DrawLine((int)pencilGroove.x + 6, (int)pencilGroove.y + 1, (int)(pencilGroove.x + pencilGroove.width - 6), (int)pencilGroove.y + 1, (Color){ 45, 22, 10, 190 });
    DrawLine((int)pencilGroove.x + 6, (int)(pencilGroove.y + pencilGroove.height), (int)(pencilGroove.x + pencilGroove.width - 6), (int)(pencilGroove.y + pencilGroove.height), (Color){ 225, 165, 105, 130 });

    // Classroom Wooden Ruler Inset along the bottom lip of desk
    DrawRectangle((int)deskRect.x + 30, (int)(deskRect.y + deskRect.height - 24), (int)deskRect.width - 60, 18, (Color){ 170, 112, 62, 210 });
    DrawRectangleLines((int)deskRect.x + 30, (int)(deskRect.y + deskRect.height - 24), (int)deskRect.width - 60, 18, (Color){ 130, 80, 40, 240 });
    int cmCount = 0;
    for (int x = (int)deskRect.x + 40; x < (int)(deskRect.x + deskRect.width - 40); x += 14) {
        int h = (cmCount % 5 == 0) ? 12 : 6;
        DrawLine(x, (int)(deskRect.y + deskRect.height - 24), x, (int)(deskRect.y + deskRect.height - 24 + h), (Color){ 80, 45, 20, 190 });
        if (cmCount % 5 == 0 && x + 16 < (int)(deskRect.x + deskRect.width - 50)) {
            DrawSchoolText(TextFormat("%d", cmCount * 2), (float)x - 4, (float)(deskRect.y + deskRect.height - 18), 12, (Color){ 90, 52, 24, 210 });
        }
        cmCount++;
    }

    // Iconic Schoolyard Biro Ink Scribble Tests (Students testing dried ballpoint ink!)
    // 1. Blue Biro Swirl Test (Left side)
    Vector2 blueSwirl = { deskRect.x + 110, deskRect.y + 90 };
    for (int a = 0; a < 360; a += 18) {
        float rad = (float)a * DEG2RAD;
        float r = 10.0f + sinf((float)a * 0.1f) * 6.0f;
        DrawCircle((int)(blueSwirl.x + cosf(rad) * r), (int)(blueSwirl.y + sinf(rad) * r), 1.6f, (Color){ 22, 55, 145, 175 });
    }
    DrawSchoolText("pen works!", blueSwirl.x - 24, blueSwirl.y + 18, 13, (Color){ 22, 55, 145, 150 });

    // 2. Red Biro Scribble Test (Right side)
    Vector2 redSwirl = { deskRect.x + deskRect.width - 120, deskRect.y + 110 };
    for (int a = 0; a < 360; a += 22) {
        float rad = (float)a * DEG2RAD;
        float r = 8.0f + cosf((float)a * 0.12f) * 5.0f;
        DrawCircle((int)(redSwirl.x + cosf(rad) * r), (int)(redSwirl.y + sinf(rad) * r), 1.5f, (Color){ 185, 32, 40, 160 });
    }

    // Carved Student Initials & Desk Doodles (Authentic School Carvings with chisel relief)
    // Carved compass rosette scratch
    DrawCircleLines((int)deskRect.x + 220, (int)deskRect.y + 130, 20.0f, (Color){ 100, 60, 30, 110 });
    DrawCircleLines((int)deskRect.x + 220, (int)deskRect.y + 130, 10.0f, (Color){ 100, 60, 30, 80 });

    // Chiseled initials: "T + M"
    DrawSchoolText("T + M", deskRect.x + 60, deskRect.y + 360, 18, (Color){ 95, 55, 26, 160 });
    DrawSchoolText("CLASS OF '04", deskRect.x + 48, deskRect.y + 382, 13, (Color){ 95, 55, 26, 120 });

    // Tic-Tac-Toe scratch
    int tttX = (int)(deskRect.x + deskRect.width - 230);
    int tttY = (int)(deskRect.y + 260);
    DrawLine(tttX + 16, tttY, tttX + 16, tttY + 44, (Color){ 95, 55, 26, 130 });
    DrawLine(tttX + 32, tttY, tttX + 32, tttY + 44, (Color){ 95, 55, 26, 130 });
    DrawLine(tttX, tttY + 14, tttX + 48, tttY + 14, (Color){ 95, 55, 26, 130 });
    DrawLine(tttX, tttY + 28, tttX + 48, tttY + 28, (Color){ 95, 55, 26, 130 });
    DrawSchoolText("X", (float)tttX + 2, (float)tttY - 2, 14, (Color){ 22, 55, 145, 140 });
    DrawSchoolText("O", (float)tttX + 18, (float)tttY + 12, 14, (Color){ 185, 32, 40, 140 });
    DrawSchoolText("X", (float)tttX + 34, (float)tttY + 26, 14, (Color){ 22, 55, 145, 140 });

    // Mathematical / Physics Scratches
    DrawSchoolText("t = r x F", deskRect.x + 280, deskRect.y + deskRect.height - 58, 17, (Color){ 90, 52, 24, 150 });
    DrawSchoolText("p = m * v", deskRect.x + 440, deskRect.y + deskRect.height - 58, 17, (Color){ 90, 52, 24, 140 });
    DrawSchoolText("u = Ff / Fn", deskRect.x + 600, deskRect.y + deskRect.height - 58, 17, (Color){ 90, 52, 24, 140 });
    DrawSchoolText("BOX2D v3.2 CLASS DESK", deskRect.x + deskRect.width - 240, deskRect.y + deskRect.height - 58, 16, (Color){ 105, 62, 32, 150 });

    // Subtle edge highlight
    DrawRectangleRoundedLines(deskRect, 0.025f, 8, (Color){ 225, 165, 105, 90 });

    // 4. FRONT DESK BACKBOARD / SOLID BARRICADE (Top Edge)
    if (tableType == TABLE_FRONT_BARRIER || tableType == TABLE_DUAL_BARRIER) {
        Rectangle railRect = { deskRect.x - 8, deskRect.y - 18, deskRect.width + 16, 24 };
        DrawRectangleRounded((Rectangle){ railRect.x, railRect.y + 12, railRect.width, 14 }, 0.0f, 0, (Color){ 30, 18, 10, 160 }); // drop shadow
        DrawRectangleRounded(railRect, 0.20f, 6, (Color){ 70, 36, 16, 255 }); // dark mahogany beam
        DrawRectangleRoundedLines(railRect, 0.20f, 6, (Color){ 110, 60, 30, 255 });

        DrawLine((int)railRect.x + 4, (int)railRect.y + 2, (int)(railRect.x + railRect.width - 4), (int)railRect.y + 2, (Color){ 180, 105, 55, 200 });

        Rectangle grooveRect = { deskRect.x + 80, deskRect.y - 10, deskRect.width - 160, 8 };
        DrawRectangleRounded(grooveRect, 0.5f, 4, (Color){ 35, 18, 8, 230 });
        DrawRectangleRoundedLines(grooveRect, 0.5f, 4, (Color){ 90, 48, 22, 200 });

        float bracketPositions[] = { 0.06f, 0.32f, 0.68f, 0.94f };
        for (int b = 0; b < 4; b++) {
            float bx = railRect.x + railRect.width * bracketPositions[b];
            DrawRectangle((int)bx - 8, (int)railRect.y - 2, 16, 26, (Color){ 160, 130, 60, 240 });
            DrawRectangleLines((int)bx - 8, (int)railRect.y - 2, 16, 26, (Color){ 220, 185, 90, 255 });
            DrawCircle((int)bx, (int)railRect.y + 6, 2.5f, (Color){ 60, 45, 20, 255 });
            DrawCircle((int)bx, (int)railRect.y + 18, 2.5f, (Color){ 60, 45, 20, 255 });
        }

        DrawSchoolText("FRONT DESK BACKBOARD [SOLID BARRICADE - CANNOT FALL OFF TOP]", deskRect.x + deskRect.width * 0.5f - 240, deskRect.y - 34, 14, (Color){ 245, 215, 145, 255 });
    }

    // 5. AISLE RETENTION BARRIER (Bottom Edge)
    if (tableType == TABLE_DUAL_BARRIER) {
        Rectangle botRail = { deskRect.x - 8, deskRect.y + deskRect.height - 6, deskRect.width + 16, 22 };
        DrawRectangleRounded(botRail, 0.20f, 6, (Color){ 70, 36, 16, 255 });
        DrawRectangleRoundedLines(botRail, 0.20f, 6, (Color){ 110, 60, 30, 255 });
        DrawLine((int)botRail.x + 4, (int)botRail.y + 2, (int)(botRail.x + botRail.width - 4), (int)botRail.y + 2, (Color){ 180, 105, 55, 200 });

        float bracketPositions[] = { 0.08f, 0.35f, 0.65f, 0.92f };
        for (int b = 0; b < 4; b++) {
            float bx = botRail.x + botRail.width * bracketPositions[b];
            DrawRectangle((int)bx - 8, (int)botRail.y - 2, 16, 24, (Color){ 160, 130, 60, 240 });
            DrawRectangleLines((int)bx - 8, (int)botRail.y - 2, 16, 24, (Color){ 220, 185, 90, 255 });
        }

        DrawSchoolText("AISLE WOODEN LIP [SOLID BARRICADE - LEFT & RIGHT DROP-OFFS ONLY]", deskRect.x + deskRect.width * 0.5f - 240, deskRect.y + deskRect.height - 24, 13, (Color){ 95, 58, 30, 220 });
    }
}

// Check if mouse hovers over a pen along its entire length
static bool IsPointInBiro(const Biro* pen, b2Vec2 worldPoint, float toleranceMeters) {
    if (pen->isEliminated || pen->isFalling) return false;
    b2Vec2 local = b2Body_GetLocalPoint(pen->bodyId, worldPoint);
    const PenModelDef* def = &g_penModels[pen->modelId];
    float minX = -def->halfLength - 0.14f - toleranceMeters;
    float maxX =  def->halfLength + 0.14f + toleranceMeters;
    float minY = -def->radius - 0.05f - toleranceMeters;
    float maxY =  def->radius + 0.10f + toleranceMeters;

    return (local.x >= minX && local.x <= maxX && local.y >= minY && local.y <= maxY);
}

// Fall detection adapting to selected Table Arena Barricades
static bool CheckPenFallOffTable(b2Vec2 pos, TableType tableType) {
    float tableXMin = -TABLE_WIDTH_M * 0.5f;
    float tableXMax =  TABLE_WIDTH_M * 0.5f;
    float tableYMin = -TABLE_HEIGHT_M * 0.5f;
    float tableYMax =  TABLE_HEIGHT_M * 0.5f;

    if (pos.x < tableXMin || pos.x > tableXMax) return true;

    if (tableType == TABLE_OPEN) {
        if (pos.y < tableYMin) return true;
    }

    if (tableType == TABLE_OPEN || tableType == TABLE_FRONT_BARRIER) {
        if (pos.y > tableYMax) return true;
    }

    return false;
}

// Helper to draw a sleek visual preview of a pen model inside the market cards
static void DrawMarketPenPreview(PenModelId modelId, Color primaryColor, Color capColor, Color plugColor, Color inkColor,
                                Vector2 center, float angleDeg, float scale) {
    const PenModelDef* def = &g_penModels[modelId];
    float barrelThick = def->radius * 2.0f * PIXELS_PER_METER * scale;

    rlPushMatrix();
    rlTranslatef(center.x, center.y, 0.0f);
    rlRotatef(angleDeg, 0.0f, 0.0f, 1.0f);

    // Drop Shadow
    DrawRectanglePro(
        (Rectangle){ 6.0f * scale, 8.0f * scale, 180.0f * scale, barrelThick + 2.0f },
        (Vector2){ 90.0f * scale, (barrelThick + 2.0f) * 0.5f },
        0.0f,
        (Color){ 12, 8, 6, 80 }
    );

    float len = 170.0f * scale;
    float halfL = len * 0.5f;

    // Rear Plug / Sliders
    if (modelId == PEN_MODEL_JUMBO) {
        DrawRectanglePro((Rectangle){ -halfL - 6.0f * scale, 0.0f, 14.0f * scale, barrelThick * 0.9f }, (Vector2){ 7.0f * scale, barrelThick * 0.45f }, 0.0f, (Color){ 220, 225, 235, 255 });
        // 4 color slider tabs
        DrawRectanglePro((Rectangle){ -halfL - 14.0f * scale, -barrelThick * 0.35f, 8.0f * scale, 3.0f * scale }, (Vector2){ 4.0f * scale, 1.5f * scale }, 0.0f, (Color){ 12, 70, 195, 255 });
        DrawRectanglePro((Rectangle){ -halfL - 14.0f * scale, -barrelThick * 0.12f, 8.0f * scale, 3.0f * scale }, (Vector2){ 4.0f * scale, 1.5f * scale }, 0.0f, (Color){ 210, 25, 25, 255 });
        DrawRectanglePro((Rectangle){ -halfL - 14.0f * scale,  barrelThick * 0.12f, 8.0f * scale, 3.0f * scale }, (Vector2){ 4.0f * scale, 1.5f * scale }, 0.0f, (Color){ 18, 145, 55, 255 });
        DrawRectanglePro((Rectangle){ -halfL - 14.0f * scale,  barrelThick * 0.35f, 8.0f * scale, 3.0f * scale }, (Vector2){ 4.0f * scale, 1.5f * scale }, 0.0f, (Color){ 35, 35, 40, 255 });
    } else if (modelId == PEN_MODEL_BRASS) {
        DrawRectanglePro((Rectangle){ -halfL - 6.0f * scale, 0.0f, 12.0f * scale, barrelThick * 0.9f }, (Vector2){ 6.0f * scale, barrelThick * 0.45f }, 0.0f, (Color){ 225, 185, 95, 255 });
        DrawRectanglePro((Rectangle){ -halfL - 14.0f * scale, 0.0f, 8.0f * scale, barrelThick * 0.5f }, (Vector2){ 4.0f * scale, barrelThick * 0.25f }, 0.0f, (Color){ 240, 205, 120, 255 });
    } else {
        DrawRectanglePro((Rectangle){ -halfL - 4.0f * scale, 0.0f, 10.0f * scale, barrelThick * 0.9f }, (Vector2){ 5.0f * scale, barrelThick * 0.45f }, 0.0f, plugColor);
    }

    // Main Barrel
    if (modelId == PEN_MODEL_JUMBO) {
        DrawRectanglePro((Rectangle){ -halfL + 50.0f * scale, 0.0f, 110.0f * scale, barrelThick }, (Vector2){ 55.0f * scale, barrelThick * 0.5f }, 0.0f, (Color){ 245, 248, 255, 255 });
        DrawRectanglePro((Rectangle){ -halfL + 45.0f * scale, 0.0f, 24.0f * scale, barrelThick }, (Vector2){ 12.0f * scale, barrelThick * 0.5f }, 0.0f, primaryColor);
    } else if (modelId == PEN_MODEL_RUBBER) {
        DrawRectanglePro((Rectangle){ -halfL + 25.0f * scale, 0.0f, 60.0f * scale, barrelThick }, (Vector2){ 30.0f * scale, barrelThick * 0.5f }, 0.0f, (Color){ 175, 180, 190, 255 });
        DrawRectanglePro((Rectangle){ -halfL + 75.0f * scale, 0.0f, 50.0f * scale, barrelThick + 1.2f }, (Vector2){ 25.0f * scale, (barrelThick + 1.2f) * 0.5f }, 0.0f, (Color){ 40, 45, 55, 255 });
        // Traction ribs
        for (int k = 0; k < 4; k++) {
            DrawRectanglePro((Rectangle){ -halfL + (60.0f + k * 10.0f) * scale, 0.0f, 2.5f * scale, barrelThick }, (Vector2){ 1.25f * scale, barrelThick * 0.5f }, 0.0f, primaryColor);
        }
    } else if (modelId == PEN_MODEL_CRYSTAL) {
        DrawRectanglePro((Rectangle){ -halfL + 50.0f * scale, 0.0f, 110.0f * scale, barrelThick }, (Vector2){ 55.0f * scale, barrelThick * 0.5f }, 0.0f, (Color){ 240, 248, 255, 210 });
        DrawRectanglePro((Rectangle){ -halfL + 50.0f * scale, 0.0f, 95.0f * scale, 2.5f * scale }, (Vector2){ 47.5f * scale, 1.25f * scale }, 0.0f, inkColor);
    } else if (modelId == PEN_MODEL_BRASS) {
        DrawRectanglePro((Rectangle){ -halfL + 50.0f * scale, 0.0f, 110.0f * scale, barrelThick }, (Vector2){ 55.0f * scale, barrelThick * 0.5f }, 0.0f, (Color){ 230, 205, 140, 255 });
        DrawRectanglePro((Rectangle){ -halfL + 50.0f * scale, 0.0f, 8.0f * scale, barrelThick + 1.5f }, (Vector2){ 4.0f * scale, (barrelThick + 1.5f) * 0.5f }, 0.0f, (Color){ 255, 235, 180, 255 });
    } else {
        // Classic Bic
        DrawRectanglePro((Rectangle){ -halfL + 50.0f * scale, 0.0f, 110.0f * scale, barrelThick }, (Vector2){ 55.0f * scale, barrelThick * 0.5f }, 0.0f, (Color){ 238, 244, 252, 220 });
        DrawRectanglePro((Rectangle){ -halfL + 55.0f * scale, 0.0f, 90.0f * scale, 3.2f * scale }, (Vector2){ 45.0f * scale, 1.6f * scale }, 0.0f, inkColor);
    }

    // Front Cap / Cone
    if (modelId == PEN_MODEL_JUMBO || modelId == PEN_MODEL_RUBBER || modelId == PEN_MODEL_BRASS) {
        Color coneCol = (modelId == PEN_MODEL_BRASS) ? (Color){ 255, 230, 160, 255 } : (Color){ 200, 208, 218, 255 };
        DrawRectanglePro((Rectangle){ halfL - 10.0f * scale, 0.0f, 30.0f * scale, barrelThick * 0.85f }, (Vector2){ 15.0f * scale, barrelThick * 0.425f }, 0.0f, coneCol);
        DrawRectanglePro((Rectangle){ halfL + 7.0f * scale, 0.0f, 6.0f * scale, 2.5f * scale }, (Vector2){ 3.0f * scale, 1.25f * scale }, 0.0f, (Color){ 30, 30, 40, 255 });
    } else {
        DrawRectanglePro((Rectangle){ halfL - 10.0f * scale, 0.0f, 40.0f * scale, barrelThick * 1.08f }, (Vector2){ 20.0f * scale, barrelThick * 0.54f }, 0.0f, capColor);
        DrawCircleV((Vector2){ halfL + 10.0f * scale, 0.0f }, barrelThick * 0.54f, capColor);
    }

    // Clip
    Color clipCol = (modelId == PEN_MODEL_BRASS) ? (Color){ 255, 220, 130, 255 } : capColor;
    DrawRectanglePro((Rectangle){ halfL - 12.0f * scale, barrelThick * 0.65f, 28.0f * scale, 3.5f * scale }, (Vector2){ 14.0f * scale, 1.75f * scale }, 0.0f, clipCol);

    rlPopMatrix();
}

// Classroom Notebook Pause / Settings Modal
static void DrawPauseMenuModal(Vector2 mouse) {
    // Dim background overlay
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){ 10, 15, 12, 210 });

    Rectangle modal = { 390, 125, 500, 470 };

    // Drop shadow
    DrawRectangleRounded((Rectangle){ modal.x + 6, modal.y + 6, modal.width, modal.height }, 0.05f, 4, (Color){ 0, 0, 0, 140 });

    // Notebook Cream Paper
    DrawRectangleRounded(modal, 0.05f, 4, (Color){ 250, 246, 238, 255 });
    DrawRectangleRoundedLines(modal, 0.05f, 4, (Color){ 20, 20, 20, 255 });

    // Top spiral holes binding strip
    DrawRectangle(modal.x, modal.y, modal.width, 36, (Color){ 235, 228, 215, 255 });
    DrawLine(modal.x, modal.y + 36, modal.x + modal.width, modal.y + 36, (Color){ 190, 180, 160, 255 });
    for (int i = 0; i < 9; i++) {
        float hx = modal.x + 35 + i * 53;
        DrawCircle(hx, modal.y + 18, 6.0f, (Color){ 30, 30, 30, 255 });
        DrawCircleLines(hx, modal.y + 18, 6.0f, (Color){ 80, 80, 80, 255 });
    }

    // Header Title
    DrawSchoolTextTitle("CLASSROOM PAUSED", modal.x + modal.width * 0.5f - 110, modal.y + 48, 26, (Color){ 17, 17, 17, 255 });
    DrawSchoolText("Active match is frozen - Biro positions are preserved", modal.x + modal.width * 0.5f - 145, modal.y + 78, 13, (Color){ 80, 80, 80, 255 });

    // Match status pill
    Rectangle statusPill = { modal.x + 35, modal.y + 102, modal.width - 70, 30 };
    DrawRectangleRounded(statusPill, 0.2f, 4, (Color){ 240, 235, 222, 255 });
    DrawRectangleRoundedLines(statusPill, 0.2f, 4, (Color){ 180, 175, 160, 255 });
    const char* summary = TextFormat("%s  |  %s  |  %s",
        g_gameModeNames[g_game.currentGameMode],
        g_tableTypeNames[g_game.currentTableType],
        g_stageNames[g_game.currentStage]);
    DrawSchoolText(summary, statusPill.x + 12, statusPill.y + 8, 12, (Color){ 40, 40, 40, 255 });

    // 5 Action Buttons
    const char* btnTitles[] = {
        ">  RESUME MATCH",
        "R  RESTART ROUND",
        "S  DESK SETUP / LOBBY",
        "P  PEN MARKET (STATIONERY)",
        "X  NEW MATCH / RESET"
    };

    for (int i = 0; i < 5; i++) {
        Rectangle btn = { modal.x + 40, modal.y + 148 + i * 56, modal.width - 80, 46 };
        bool isHover = CheckCollisionPointRec(mouse, btn);

        Color bg = isHover ? (Color){ 20, 20, 20, 255 } : (Color){ 255, 255, 255, 255 };
        Color fg = isHover ? (Color){ 250, 246, 238, 255 } : (Color){ 20, 20, 20, 255 };
        Color border = (Color){ 20, 20, 20, 255 };

        DrawRectangleRounded(btn, 0.15f, 4, bg);
        DrawRectangleRoundedLines(btn, 0.15f, 4, border);

        int fsz = (i == 0) ? 17 : 15;
        int tw = MeasureText(btnTitles[i], fsz);
        DrawSchoolTextTitle(btnTitles[i], btn.x + btn.width * 0.5f - tw * 0.5f, btn.y + (btn.height - fsz) * 0.5f - 2, fsz, fg);
    }

    if (g_game.matchType == MATCH_ONLINE_P2P) {
        DrawSchoolText("* Note: In Online P2P, desks remain synchronized.", modal.x + 50, modal.y + modal.height - 24, 11, (Color){ 100, 100, 100, 255 });
    }
}

// HUD Scoreboard & Dynamic Turn Banners with Non-Pausing High-Pressure Turn Timer
static void DrawHUD(void) {
    int activePlayer = g_game.activePlayer;
    const Biro* biros = g_game.biros;
    GameMode mode = g_game.currentGameMode;
    TableType tableType = g_game.currentTableType;
    StageRack stage = g_game.currentStage;
    int teamBlueScore = g_game.teamBlueScore;
    int teamRedScore = g_game.teamRedScore;
    GameState state = g_game.state;
    float turnTimer = g_game.turnTimer;
    MatchType matchType = g_game.matchType;
    AIDifficulty aiDiff = g_game.aiDifficulty;

    // 1. Top Desk Blotter / Chalkboard Bar
    DrawRectangle(0, 0, SCREEN_WIDTH, 68, (Color){ 28, 36, 32, 252 });
    DrawLine(0, 68, SCREEN_WIDTH, 68, (Color){ 65, 82, 72, 255 });
    DrawLine(0, 69, SCREEN_WIDTH, 69, (Color){ 15, 20, 18, 180 });

    // Brass corner rivets on blotter
    DrawCircle(10, 10, 3.0f, (Color){ 180, 145, 65, 255 });
    DrawCircle(SCREEN_WIDTH - 10, 10, 3.0f, (Color){ 180, 145, 65, 255 });

    // Title: Classroom Chalk / Gold Foil
    DrawSchoolTextTitle("BIRO CLASH", 14, 10, 26, (Color){ 255, 218, 105, 255 });
    DrawSchoolText("DESK PHYSICS", 16, 42, 13, (Color){ 195, 220, 205, 220 });

    // 2. SINGLE PROMINENT MENU BUTTON [ || MENU ]
    Rectangle menuBtn = { 152, 12, 126, 44 };
    bool isMenuHover = CheckCollisionPointRec(GetMousePosition(), menuBtn);
    Color menuBg = isMenuHover ? (Color){ 20, 20, 20, 255 } : (Color){ 250, 246, 238, 255 };
    Color menuFg = isMenuHover ? (Color){ 250, 246, 238, 255 } : (Color){ 20, 20, 20, 255 };

    DrawRectangleRounded(menuBtn, 0.15f, 4, menuBg);
    DrawRectangleRoundedLines(menuBtn, 0.15f, 4, (Color){ 20, 20, 20, 255 });
    DrawSchoolTextTitle("[ || MENU ]", menuBtn.x + 14, menuBtn.y + 12, 17, menuFg);

    // 3. CALM MATCH SETUP INFO STRIP (Quiet reading slip, no clutter buttons)
    Rectangle infoStrip = { 292, 12, 490, 44 };
    DrawRectangleRounded(infoStrip, 0.12f, 4, (Color){ 245, 241, 232, 235 });
    DrawRectangleRoundedLines(infoStrip, 0.12f, 4, (Color){ 160, 155, 140, 200 });

    const char* modeStr = g_gameModeNames[mode];
    const char* tableStr = g_tableTypeNames[tableType];
    const char* stageStr = g_stageNames[stage];
    DrawSchoolText("ACTIVE DESK SETUP", infoStrip.x + 12, infoStrip.y + 4, 11, (Color){ 90, 85, 75, 255 });

    if (matchType == MATCH_ONLINE_P2P) {
        const char* rCode = (strlen(g_game.onlineRoomCode) > 0) ? g_game.onlineRoomCode : "PEN1";
        const char* connStr = g_game.isOnlineConnected ? "ONLINE: CONNECTED" : "ONLINE: WAITING...";
        DrawSchoolTextTitle(TextFormat("%s  |  %s  |  ROOM: %s (%s)", modeStr, tableStr, rCode, connStr),
                            infoStrip.x + 12, infoStrip.y + 20, 13, (Color){ 20, 20, 20, 255 });
    } else {
        DrawSchoolTextTitle(TextFormat("%s  |  %s  |  %s", modeStr, tableStr, stageStr),
                            infoStrip.x + 12, infoStrip.y + 20, 13, (Color){ 20, 20, 20, 255 });
    }

    // SCORES DISPLAY (School Report Card Slips with Gold Star Stickers)
    if (mode == MODE_1V1) {
        Rectangle p1Box = { 804, 12, 90, 44 };
        DrawRectangleRounded(p1Box, 0.15f, 4, (Color){ 235, 242, 255, 250 });
        DrawRectangleRoundedLines(p1Box, 0.15f, 4, (Color){ 45, 95, 205, 255 });
        const char* p1Label = (matchType == MATCH_ONLINE_P2P && !g_game.isOnlineHost) ? "P1 (HOST)" : "P1 BLUE";
        DrawSchoolTextTitle(p1Label, p1Box.x + 6, p1Box.y + 4, 13, (Color){ 22, 65, 170, 255 });
        for (int i = 0; i < 3; i++) {
            DrawGoldStar(p1Box.x + 16 + i * 26, p1Box.y + 28, 7.0f, i < biros[0].score);
        }

        DrawSchoolTextTitle("VS", 899, 22, 15, (Color){ 180, 195, 185, 255 });

        Rectangle p2Box = { 922, 12, 90, 44 };
        DrawRectangleRounded(p2Box, 0.15f, 4, (Color){ 255, 238, 238, 250 });
        DrawRectangleRoundedLines(p2Box, 0.15f, 4, (Color){ 210, 45, 55, 255 });
        const char* p2Label = (matchType == MATCH_SINGLE_PLAYER_AI) ? "P2 (AI)" :
                              ((matchType == MATCH_ONLINE_P2P && g_game.isOnlineHost) ? "P2 (GUEST)" : "P2 RED");
        DrawSchoolTextTitle(p2Label, p2Box.x + 6, p2Box.y + 4, 13, (Color){ 195, 30, 40, 255 });
        for (int i = 0; i < 3; i++) {
            DrawGoldStar(p2Box.x + 16 + i * 26, p2Box.y + 28, 7.0f, i < biros[1].score);
        }
    } else if (mode == MODE_TEAMS_2V2) {
        Rectangle t1Box = { 804, 12, 94, 44 };
        DrawRectangleRounded(t1Box, 0.15f, 4, (Color){ 235, 242, 255, 250 });
        DrawRectangleRoundedLines(t1Box, 0.15f, 4, (Color){ 45, 95, 205, 255 });
        DrawSchoolTextTitle("TEAM BLUE", t1Box.x + 5, t1Box.y + 4, 13, (Color){ 22, 65, 170, 255 });
        DrawCircle(t1Box.x + 78, t1Box.y + 11, 3.5f, biros[0].isEliminated ? GRAY : (Color){ 22, 65, 170, 255 });
        DrawCircle(t1Box.x + 87, t1Box.y + 11, 3.5f, biros[2].isEliminated ? GRAY : (Color){ 25, 135, 65, 255 });
        for (int i = 0; i < 3; i++) {
            DrawGoldStar(t1Box.x + 16 + i * 26, t1Box.y + 28, 7.0f, i < teamBlueScore);
        }

        DrawSchoolTextTitle("VS", 902, 22, 15, (Color){ 180, 195, 185, 255 });

        Rectangle t2Box = { 926, 12, 94, 44 };
        DrawRectangleRounded(t2Box, 0.15f, 4, (Color){ 255, 238, 238, 250 });
        DrawRectangleRoundedLines(t2Box, 0.15f, 4, (Color){ 210, 45, 55, 255 });
        DrawSchoolTextTitle("TEAM RED", t2Box.x + 5, t2Box.y + 4, 13, (Color){ 195, 30, 40, 255 });
        DrawCircle(t2Box.x + 78, t2Box.y + 11, 3.5f, biros[1].isEliminated ? GRAY : (Color){ 210, 35, 45, 255 });
        DrawCircle(t2Box.x + 87, t2Box.y + 11, 3.5f, biros[3].isEliminated ? GRAY : (Color){ 45, 45, 50, 255 });
        for (int i = 0; i < 3; i++) {
            DrawGoldStar(t2Box.x + 16 + i * 26, t2Box.y + 28, 7.0f, i < teamRedScore);
        }
    } else { // MODE_BATTLE_ROYALE
        Color pCols[4] = { (Color){ 22, 65, 170, 255 }, (Color){ 195, 30, 40, 255 }, (Color){ 25, 135, 65, 255 }, (Color){ 45, 45, 50, 255 } };
        Color pBgs[4] = { (Color){ 235, 242, 255, 250 }, (Color){ 255, 238, 238, 250 }, (Color){ 235, 255, 240, 250 }, (Color){ 245, 245, 248, 250 } };
        for (int i = 0; i < 4; i++) {
            Rectangle pBox = { 804 + i * 51, 12, 48, 44 };
            DrawRectangleRounded(pBox, 0.15f, 4, pBgs[i]);
            DrawRectangleRoundedLines(pBox, 0.15f, 4, biros[i].isEliminated ? GRAY : pCols[i]);
            DrawSchoolTextTitle(TextFormat("P%d", i + 1), pBox.x + 3, pBox.y + 4, 13, biros[i].isEliminated ? GRAY : pCols[i]);
            if (biros[i].isEliminated) {
                DrawSchoolTextTitle("OUT", pBox.x + 20, pBox.y + 4, 12, RED);
            } else {
                DrawSchoolTextTitle(TextFormat("%dp", biros[i].score), pBox.x + 20, pBox.y + 4, 12, (Color){ 180, 130, 20, 255 });
            }
            DrawGoldStar(pBox.x + pBox.width * 0.5f, pBox.y + 28, 6.5f, !biros[i].isEliminated && biros[i].score > 0);
        }
    }

    // High Pressure Turn Timer Clock & Active Banner
    if (state == STATE_AIMING) {
        Color timerCol = (turnTimer < 4.0f) ? (Color){ 240, 50, 60, 255 } :
                         (turnTimer < 7.0f ? (Color){ 255, 175, 45, 255 } : (Color){ 105, 225, 135, 255 });
        bool flash = (turnTimer < 4.0f) && ((int)(turnTimer * 6.0f) % 2 == 0);

        Rectangle timerRect = { 1024, 12, 88, 44 };
        DrawRectangleRounded(timerRect, 0.15f, 4, (Color){ 20, 26, 24, 240 });
        DrawRectangleRoundedLines(timerRect, 0.15f, 4, flash ? RED : timerCol);
        DrawSchoolText("EXAM TIMER", timerRect.x + 6, timerRect.y + 4, 11, flash ? WHITE : timerCol);
        DrawSchoolTextTitle(TextFormat("%.1fs", turnTimer), timerRect.x + 6, timerRect.y + 18, 18, flash ? WHITE : timerCol);

        Color pCols[4] = { (Color){ 22, 65, 160, 255 }, (Color){ 210, 35, 45, 255 }, (Color){ 25, 135, 65, 255 }, (Color){ 45, 45, 50, 255 } };
        Color pBannerBg[4] = { (Color){ 225, 238, 255, 250 }, (Color){ 255, 230, 230, 250 }, (Color){ 228, 252, 235, 250 }, (Color){ 238, 238, 242, 250 } };

        Rectangle bannerRect = { 1118, 12, SCREEN_WIDTH - 1128, 44 };
        DrawRectangleRounded(bannerRect, 0.15f, 4, pBannerBg[activePlayer]);
        DrawRectangleRoundedLines(bannerRect, 0.15f, 4, pCols[activePlayer]);

        bool isAiTurn = (matchType == MATCH_SINGLE_PLAYER_AI && activePlayer != 0);
        bool isRemoteTurn = (matchType == MATCH_ONLINE_P2P && activePlayer != g_game.onlineLocalPlayerIndex);

        if (isAiTurn) {
            const char* title = g_game.isFirstMoveOfRound ? TextFormat("%s (1ST MOVE)", biros[activePlayer].shortName) : TextFormat("%s (AI BOT)", biros[activePlayer].shortName);
            DrawSchoolTextTitle(title, (float)bannerRect.x + 6, (float)bannerRect.y + 4, 14, pCols[activePlayer]);
            DrawSchoolText("CALCULATING FLICK...", (float)bannerRect.x + 6, (float)bannerRect.y + 24, 12, (Color){ 180, 110, 30, 255 });
        } else if (isRemoteTurn) {
            const char* title = g_game.isFirstMoveOfRound ? TextFormat("%s (1ST MOVE)", biros[activePlayer].shortName) : TextFormat("%s (REMOTE)", biros[activePlayer].shortName);
            DrawSchoolTextTitle(title, (float)bannerRect.x + 6, (float)bannerRect.y + 4, 14, pCols[activePlayer]);
            DrawSchoolText("OPPONENT AIMING...", (float)bannerRect.x + 6, (float)bannerRect.y + 24, 12, (Color){ 180, 110, 30, 255 });
        } else {
            const char* title = g_game.isFirstMoveOfRound ? TextFormat("%s (YOUR 1ST MOVE!)", biros[activePlayer].shortName) : TextFormat("%s TO STRIKE", biros[activePlayer].shortName);
            DrawSchoolTextTitle(title, (float)bannerRect.x + 6, (float)bannerRect.y + 4, 15, pCols[activePlayer]);
            const PenModelDef* def = &g_penModels[biros[activePlayer].modelId];
            float mass = b2Body_GetMass(biros[activePlayer].bodyId);
            DrawSchoolText(TextFormat("[%s | %.2fkg]", def->name, mass), (float)bannerRect.x + 6, (float)bannerRect.y + 24, 12, (Color){ 60, 65, 75, 255 });
        }
    } else if (state == STATE_SIMULATING) {
        const char* simText = "SIMULATING COLLISION...";
        float tw = MeasureSchoolTextTitle(simText, 16);
        DrawSchoolTextTitle(simText, SCREEN_WIDTH - tw - 20, 24, 16, (Color){ 255, 170, 50, 255 });
    } else if (state == STATE_PEN_MARKET) {
        const char* mktText = "BROWSING PEN MARKET...";
        float tw = MeasureSchoolTextTitle(mktText, 16);
        DrawSchoolTextTitle(mktText, SCREEN_WIDTH - tw - 20, 24, 16, (Color){ 255, 215, 80, 255 });
    }

    // Bottom Controls Ruler Bar
    Rectangle botRuler = { 0, SCREEN_HEIGHT - 36, SCREEN_WIDTH, 36 };
    DrawRectangleRec(botRuler, (Color){ 42, 32, 22, 245 });
    DrawLine(0, SCREEN_HEIGHT - 36, SCREEN_WIDTH, SCREEN_HEIGHT - 36, (Color){ 75, 55, 38, 255 });
    for (int x = 10; x < SCREEN_WIDTH - 10; x += 10) {
        int h = (x % 50 == 0) ? 8 : 4;
        DrawLine(x, SCREEN_HEIGHT - 36, x, SCREEN_HEIGHT - 36 + h, (Color){ 130, 95, 60, 160 });
    }
    if (g_game.isTouchMode) {
        DrawSchoolText("MOBILE TOUCH: [SWIPE ACROSS THE BIRO TO FLICK]  |  Fast Swipe = Power  |  [MENU]: Pause & Settings",
                       18, SCREEN_HEIGHT - 25, 14, (Color){ 238, 225, 195, 230 });
    } else {
        DrawSchoolText("PC MOUSE: [HOVER ON PEN] + [HOLD CLICK TO CHARGE RULER] + [RELEASE TO FLICK]  |  [ESC] / [M]: Pause & Settings",
                       18, SCREEN_HEIGHT - 25, 14, (Color){ 238, 225, 195, 230 });
    }

    // Clickable toggle button in bottom right corner
    Rectangle ctrlToggleRect = { SCREEN_WIDTH - 200, SCREEN_HEIGHT - 32, 190, 26 };
    bool hovToggle = CheckCollisionPointRec(GetMousePosition(), ctrlToggleRect);
    DrawRectangleRounded(ctrlToggleRect, 0.25f, 4, hovToggle ? (Color){ 55, 65, 58, 255 } : (Color){ 28, 22, 16, 255 });
    DrawRectangleRoundedLines(ctrlToggleRect, 0.25f, 4, (Color){ 200, 190, 170, 255 });
    const char* ctrlTxt = g_game.isTouchMode ? "[ TOUCH SWIPE ]" : "[ PC MOUSE AIM ]";
    float ctw = MeasureSchoolTextTitle(ctrlTxt, 13);
    DrawSchoolTextTitle(ctrlTxt, ctrlToggleRect.x + ctrlToggleRect.width * 0.5f - ctw * 0.5f, ctrlToggleRect.y + 5, 13, (Color){ 250, 246, 238, 255 });
}

// Live Physics Telemetry Box (Toggleable with [T])
static void DrawTelemetry(void) {
    int activeCount = (g_game.currentGameMode == MODE_1V1) ? 2 : 4;
    const Biro* biros = g_game.biros;
    Rectangle box = { 20, 80, 290, 240 };
    DrawRectangleRec(box, (Color){ 12, 16, 22, 230 });
    DrawRectangleLinesEx(box, 1.5f, (Color){ 60, 80, 110, 255 });

    DrawText("BOX2D v3 TELEMETRY", (int)box.x + 12, (int)box.y + 10, 14, GREEN);
    DrawLine((int)box.x + 10, (int)box.y + 28, (int)(box.x + box.width - 10), (int)box.y + 28, (Color){ 50, 70, 95, 255 });

    Color pCols[4] = { SKYBLUE, (Color){ 255, 110, 110, 255 }, GREEN, LIGHTGRAY };
    for (int i = 0; i < activeCount; i++) {
        if (biros[i].isEliminated) {
            DrawText(TextFormat("%s: ELIMINATED", biros[i].shortName), (int)box.x + 12, (int)box.y + 35 + i * 44, 11, DARKGRAY);
            continue;
        }
        b2Vec2 v = b2Body_GetLinearVelocity(biros[i].bodyId);
        float w  = b2Body_GetAngularVelocity(biros[i].bodyId);
        float speed = b2Length(v);
        float rpm = (w * 60.0f) / (2.0f * 3.14159265f);
        float mass = b2Body_GetMass(biros[i].bodyId);
        const PenModelDef* def = &g_penModels[biros[i].modelId];

        DrawText(TextFormat("%s [%s]:", biros[i].shortName, def->name), (int)box.x + 12, (int)box.y + 34 + i * 44, 11, pCols[i]);
        DrawText(TextFormat(" Spd: %.2fm/s (%.0f km/h) | Spin: %.0f RPM", speed, speed * 3.6f, rpm), (int)box.x + 12, (int)box.y + 48 + i * 44, 10, LIGHTGRAY);
        DrawText(TextFormat(" Mass: %.2fkg | Fric: %.2f | Caliber: %.1fmm", mass, def->friction, def->radius * 200.0f), (int)box.x + 12, (int)box.y + 60 + i * 44, 10, (Color){ 160, 200, 240, 230 });
    }

    DrawText("Sub-Steps: 8 (480Hz Integration)", (int)box.x + 12, (int)(box.y + box.height - 18), 10, GOLD);
}

// Reset all biros according to selected Game Mode and Stage Axis Orientation
static void ResetAllBirosForModeAndStage(Biro biros[4], GameMode mode, StageRack stage) {
    float piOver2 = 1.5707963f;

    if (mode == MODE_1V1) {
        b2Vec2 p1Pos = { -2.4f, 0.0f };
        b2Vec2 p2Pos = {  2.4f, 0.0f };
        float p1Angle = 0.0f;
        float p2Angle = 3.14159265f;

        if (stage == STAGE_VERTICAL) {
            p1Angle = -piOver2;
            p2Angle = -piOver2;
        } else if (stage == STAGE_CROSS) {
            p1Angle = 0.0f;
            p2Angle = -piOver2;
        }

        ResetBiro(&biros[0], p1Pos, p1Angle);
        ResetBiro(&biros[1], p2Pos, p2Angle);

        b2Body_Disable(biros[2].bodyId);
        biros[2].isEliminated = true;
        b2Body_Disable(biros[3].bodyId);
        biros[3].isEliminated = true;

    } else if (mode == MODE_TEAMS_2V2) {
        b2Vec2 p1Pos = { -2.6f, -0.90f };
        b2Vec2 p3Pos = { -2.6f,  0.90f };
        b2Vec2 p2Pos = {  2.6f, -0.90f };
        b2Vec2 p4Pos = {  2.6f,  0.90f };

        float p1Angle = 0.0f;
        float p3Angle = 0.0f;
        float p2Angle = 3.14159265f;
        float p4Angle = 3.14159265f;

        if (stage == STAGE_VERTICAL) {
            p1Angle = -piOver2;
            p3Angle = -piOver2;
            p2Angle = -piOver2;
            p4Angle = -piOver2;
        } else if (stage == STAGE_CROSS) {
            p1Angle = 0.0f;
            p3Angle = 0.0f;
            p2Angle = -piOver2;
            p4Angle = -piOver2;
        }

        ResetBiro(&biros[0], p1Pos, p1Angle);
        ResetBiro(&biros[2], p3Pos, p3Angle);
        ResetBiro(&biros[1], p2Pos, p2Angle);
        ResetBiro(&biros[3], p4Pos, p4Angle);

    } else { // MODE_BATTLE_ROYALE
        b2Vec2 p1Pos = { -2.6f, -0.90f };
        b2Vec2 p2Pos = {  2.6f, -0.90f };
        b2Vec2 p3Pos = { -2.6f,  0.90f };
        b2Vec2 p4Pos = {  2.6f,  0.90f };

        float p1Angle =  0.35f;
        float p2Angle =  2.79f;
        float p3Angle = -0.35f;
        float p4Angle = -2.79f;

        if (stage == STAGE_VERTICAL) {
            p1Angle = -piOver2;
            p2Angle = -piOver2;
            p3Angle = -piOver2;
            p4Angle = -piOver2;
        } else if (stage == STAGE_CROSS) {
            p1Angle = 0.0f;
            p2Angle = -piOver2;
            p3Angle = -piOver2;
            p4Angle = 3.14159265f;
        }

        ResetBiro(&biros[0], p1Pos, p1Angle);
        ResetBiro(&biros[1], p2Pos, p2Angle);
        ResetBiro(&biros[2], p3Pos, p3Angle);
        ResetBiro(&biros[3], p4Pos, p4Angle);
    }
}

// Find next non-eliminated player index
static int GetNextActivePlayer(int current, const Biro biros[4], int numPlayers) {
    int next = current;
    for (int k = 0; k < numPlayers; k++) {
        next = (next + 1) % numPlayers;
        if (!biros[next].isEliminated) return next;
    }
    return current;
}

// Determine which player takes the opening strike of the round (strictly alternating across rounds!)
static int GetRoundStartingPlayer(int roundNumber, GameMode mode) {
    int numP = (mode == MODE_1V1) ? 2 : 4;
    int starter = (roundNumber - 1) % numP;
    if (starter < 0) starter = 0;
    return starter;
}

// Reset turn state for the beginning of a round, giving the first move to the alternating starter
static void ResetTurnForRound(int roundNumber) {
    int numP = (g_game.currentGameMode == MODE_1V1) ? 2 : 4;
    int startingPlayer = GetRoundStartingPlayer(roundNumber, g_game.currentGameMode);

    if (g_game.biros[startingPlayer].isEliminated) {
        startingPlayer = GetNextActivePlayer(startingPlayer, g_game.biros, numP);
    }

    g_game.roundStartingPlayer = startingPlayer;
    g_game.isFirstMoveOfRound = true;
    g_game.activePlayer = startingPlayer;
    g_game.turnTimer = TURN_TIME_LIMIT;
    g_game.isCharging = false;
    g_game.state = STATE_AIMING;
    g_game.aiState = AI_STATE_DECIDE;
    g_game.aiActionTimer = 0.0f;
    g_game.lastTickSecond = -1;
}

static void ResetMatchScoresAndBiros(void) {
    g_game.teamBlueScore = 0;
    g_game.teamRedScore = 0;
    for (int i = 0; i < 4; i++) g_game.biros[i].score = 0;
    g_game.roundNumber = 1;
    ResetAllBirosForModeAndStage(g_game.biros, g_game.currentGameMode, g_game.currentStage);
    ResetTurnForRound(g_game.roundNumber);
}

// Calculate AI Bot physics targeting, contact point and power
static void CalculateAIBotStrike(int botIndex, b2Vec2* outStrikePoint, float* outAngle, float* outPower) {
    Biro* bot = &g_game.biros[botIndex];
    b2Pos botPos = b2Body_GetPosition(bot->bodyId);
    const PenModelDef* botDef = &g_penModels[bot->modelId];

    int targetOpponent = -1;
    float bestScore = -999999.0f;
    int numP = (g_game.currentGameMode == MODE_1V1) ? 2 : 4;

    for (int i = 0; i < numP; i++) {
        if (i == botIndex || g_game.biros[i].isEliminated) continue;

        if (g_game.currentGameMode == MODE_TEAMS_2V2) {
            bool botIsBlue = (botIndex == 0 || botIndex == 2);
            bool targetIsBlue = (i == 0 || i == 2);
            if (botIsBlue == targetIsBlue) continue;
        }

        b2Pos oppPos = b2Body_GetPosition(g_game.biros[i].bodyId);
        float dist = b2Distance((b2Vec2){ botPos.x, botPos.y }, (b2Vec2){ oppPos.x, oppPos.y });

        float distScore = 12.0f - dist;
        float edgeDistX = (TABLE_WIDTH_M * 0.5f) - fabsf(oppPos.x);
        float edgeDistY = (TABLE_HEIGHT_M * 0.5f) - fabsf(oppPos.y);
        float minEdgeDist = fminf(edgeDistX, edgeDistY);
        float vulnBonus = (3.0f - Clamp(minEdgeDist, 0.0f, 3.0f)) * 3.5f;

        float totalScore = distScore + vulnBonus;
        if (totalScore > bestScore) {
            bestScore = totalScore;
            targetOpponent = i;
        }
    }

    if (targetOpponent < 0) {
        *outStrikePoint = (b2Vec2){ -0.5f, 0.0f };
        *outAngle = 0.0f;
        *outPower = 0.5f;
        return;
    }

    Biro* opp = &g_game.biros[targetOpponent];
    b2Pos oppPos = b2Body_GetPosition(opp->bodyId);
    float oppAngle = b2Rot_GetAngle(b2Body_GetRotation(opp->bodyId));
    float dx = oppPos.x - botPos.x;
    float dy = oppPos.y - botPos.y;
    float dist = sqrtf(dx * dx + dy * dy);
    float baseAngle = atan2f(dy, dx);

    AIDifficulty diff = g_game.aiDifficulty;

    if (diff == AI_FRESHMAN) {
        float jitterAng = ((float)(rand() % 100) / 100.0f - 0.5f) * 0.42f;
        *outAngle = baseAngle + jitterAng;
        *outStrikePoint = (b2Vec2){ -0.2f + ((float)(rand() % 40) / 100.0f - 0.2f), 0.0f };
        float p = 0.35f + dist * 0.10f + ((float)(rand() % 40) / 100.0f - 0.2f);
        *outPower = Clamp(p, 0.25f, 0.90f);

    } else if (diff == AI_DESKMATE) {
        float jitterAng = ((float)(rand() % 100) / 100.0f - 0.5f) * 0.14f;
        *outAngle = baseAngle + jitterAng;
        *outStrikePoint = (b2Vec2){ -botDef->halfLength * 0.65f, 0.0f };
        float p = 0.42f + dist * 0.12f + ((float)(rand() % 20) / 100.0f - 0.1f);
        *outPower = Clamp(p, 0.35f, 0.88f);

    } else if (diff == AI_PREFECT) {
        float jitterAng = ((float)(rand() % 100) / 100.0f - 0.5f) * 0.05f;
        float targetOffset = (sinf(oppAngle) > 0.0f ? 0.30f : -0.30f);
        float aimDy = (oppPos.y + targetOffset) - botPos.y;
        float aimDx = oppPos.x - botPos.x;
        *outAngle = atan2f(aimDy, aimDx) + jitterAng;
        *outStrikePoint = (b2Vec2){ -botDef->halfLength * 0.80f, 0.0f };
        float p = 0.48f + dist * 0.13f;
        *outPower = Clamp(p, 0.45f, 0.95f);

    } else { // AI_OLYMPIAD (Physics Prodigy)
        float cliffDirX = (oppPos.x > 0) ? 1.0f : -1.0f;
        float cliffDirY = (oppPos.y > 0) ? 1.0f : -1.0f;
        float distToXEdge = (TABLE_WIDTH_M * 0.5f) - fabsf(oppPos.x);
        float distToYEdge = (TABLE_HEIGHT_M * 0.5f) - fabsf(oppPos.y);

        Vector2 pushVec;
        if (distToXEdge < distToYEdge || g_game.currentTableType != TABLE_OPEN) {
            pushVec = (Vector2){ cliffDirX, 0.0f };
        } else {
            pushVec = (Vector2){ 0.0f, cliffDirY };
        }

        Vector2 desiredImpact = { oppPos.x - pushVec.x * 0.3f, oppPos.y - pushVec.y * 0.3f };
        float aimAngle = atan2f(desiredImpact.y - botPos.y, desiredImpact.x - botPos.x);

        *outAngle = aimAngle;
        *outStrikePoint = (b2Vec2){ -botDef->halfLength * 0.88f, 0.0f };
        float p = 0.55f + dist * 0.14f;
        *outPower = Clamp(p, 0.50f, 1.0f);
    }
}

typedef struct {
    bool isValid;
    b2Vec2 localStrikePoint;
    float angle;
    float powerFrac;
    const char* strikeType;
} SwipeStrikeResult;

// Mobile Web Swipe Calculation
// Cross-checks swipe line against active biro to determine contact point, angle, and impulse power
static SwipeStrikeResult CalculateSwipeStrike(const Biro* biro, Vector2 startScr, Vector2 endScr, float duration) {
    SwipeStrikeResult res = {0};
    float distPx = Vector2Distance(startScr, endScr);
    if (distPx < 20.0f) {
        res.isValid = false;
        return res;
    }

    b2Vec2 startWorld = WorldFromScreen(startScr);
    b2Vec2 endWorld   = WorldFromScreen(endScr);
    b2Vec2 startLocal = b2Body_GetLocalPoint(biro->bodyId, startWorld);
    b2Vec2 endLocal   = b2Body_GetLocalPoint(biro->bodyId, endWorld);

    const PenModelDef* def = &g_penModels[biro->modelId];
    float halfL = def->halfLength;

    // Power calculation from swipe speed and distance:
    float speed = distPx / fmaxf(duration, 0.035f);
    float speedNorm = (speed - 180.0f) / 1500.0f;
    float distNorm  = (distPx - 25.0f) / 260.0f;
    float power = Clamp(speedNorm * 0.70f + distNorm * 0.30f, 0.15f, 1.0f);

    float dirWorldX = endWorld.x - startWorld.x;
    float dirWorldY = endWorld.y - startWorld.y;
    float angle = atan2f(dirWorldY, dirWorldX);

    // Direction in local pen coordinates:
    float dirLocalX = endLocal.x - startLocal.x;

    bool hit = false;
    float strikeX = 0.0f;
    const char* sType = "FLICK";

    // 1. Spear strike: swipe starting near or behind the tail (-halfL) and moving forward along pen (+X)
    if (startLocal.x <= -halfL + 0.40f && fabsf(startLocal.y) <= 0.38f && dirLocalX > 0.12f) {
        hit = true;
        strikeX = -halfL * 0.88f;
        sType = "SPEAR FLICK!";
    }
    // 2. Direct line crossing through y = 0
    else if ((startLocal.y <= 0.0f && endLocal.y >= 0.0f) || (startLocal.y >= 0.0f && endLocal.y <= 0.0f)) {
        float dy = endLocal.y - startLocal.y;
        float t = (fabsf(dy) > 0.0001f) ? (-startLocal.y / dy) : 0.5f;
        t = Clamp(t, 0.0f, 1.0f);
        float xCross = startLocal.x + t * dirLocalX;
        if (xCross >= -halfL - 0.28f && xCross <= halfL + 0.28f) {
            hit = true;
            strikeX = Clamp(xCross, -halfL * 0.88f, halfL * 0.88f);
            if (strikeX < -halfL * 0.45f) sType = "TAIL SPIN!";
            else if (strikeX > halfL * 0.45f) sType = "TIP HOOK!";
            else sType = "CENTER PUSH!";
        }
    }
    // 3. Swipe started on or very near the pen body and sliced away
    else if (startLocal.x >= -halfL - 0.22f && startLocal.x <= halfL + 0.22f && fabsf(startLocal.y) <= 0.30f) {
        hit = true;
        strikeX = Clamp(startLocal.x, -halfL * 0.88f, halfL * 0.88f);
        if (strikeX < -halfL * 0.45f) sType = "TAIL SPIN!";
        else if (strikeX > halfL * 0.45f) sType = "TIP HOOK!";
        else sType = "CENTER PUSH!";
    }
    // 4. Closest distance between swipe segment and pen spine segment [-halfL, halfL]
    else {
        float pX = Clamp(startLocal.x, -halfL, halfL);
        float dStart = hypotf(startLocal.x - pX, startLocal.y);
        float pX2 = Clamp(endLocal.x, -halfL, halfL);
        float dEnd = hypotf(endLocal.x - pX2, endLocal.y);
        float minD = fminf(dStart, dEnd);
        if (minD <= 0.28f) {
            hit = true;
            strikeX = (dStart < dEnd) ? pX : pX2;
            if (strikeX < -halfL * 0.45f) sType = "TAIL SPIN!";
            else if (strikeX > halfL * 0.45f) sType = "TIP HOOK!";
            else sType = "CENTER PUSH!";
        }
    }

    if (hit) {
        res.isValid = true;
        res.localStrikePoint = (b2Vec2){ strikeX, 0.0f };
        res.angle = angle;
        res.powerFrac = power;
        res.strikeType = sType;
    } else {
        res.isValid = false;
    }
    return res;
}

// Common Strike Execution function (Human, AI Bot, and Remote Network)
static void ExecuteFlickStrike(int playerIndex, b2Vec2 strikeLocalPoint, float angle, float powerFrac) {
    if (playerIndex < 0 || playerIndex >= 4) return;
    Biro* biro = &g_game.biros[playerIndex];
    if (biro->isEliminated) return;

    float maxImpulse = 2.65f;
    float impulseMag = powerFrac * maxImpulse;

    Vector2 dir = { cosf(angle), sinf(angle) };
    b2Vec2 impulse = { dir.x * impulseMag, dir.y * impulseMag };

    b2Pos hitPtWorld = b2Body_GetWorldPoint(biro->bodyId, strikeLocalPoint);
    b2Body_ApplyLinearImpulse(biro->bodyId, impulse, hitPtWorld, true);

    if (g_audio.audioReady) {
        SetSoundPitch(g_audio.sndFlick, 0.85f + powerFrac * 0.45f);
        PlaySound(g_audio.sndFlick);
    }

    Vector2 strikeScr = ScreenFromWorld(hitPtWorld);
    for (int i = 0; i < 16; i++) {
        float ang = (float)(rand() % 360) * DEG2RAD;
        float spd = (float)(rand() % 220 + 80);
        Vector2 spdVec = { cosf(ang) * spd, sinf(ang) * spd };
        SpawnParticle(strikeScr, spdVec, (Color){ 255, 220, 120, 255 }, 3.5f, 0.25f, false);
    }

    g_game.settleTimer = 0.0f;
    g_game.simTimer = 0.0f;
    g_game.turnTimer = TURN_TIME_LIMIT;
    g_game.lastTickSecond = -1;
    g_game.isCharging = false;
    g_game.isFirstMoveOfRound = false;
    g_game.state = STATE_SIMULATING;

    // Transmit over WebRTC if this was our local turn in Online mode
    if (g_game.matchType == MATCH_ONLINE_P2P && playerIndex == g_game.onlineLocalPlayerIndex) {
        JS_SendNetworkStrike(playerIndex, strikeLocalPoint.x, strikeLocalPoint.y, angle, powerFrac);
    }
}

// Update AI Bot Turn (Simulate natural thinking, reticle aim sweep, and ruler charging)
static void UpdateAIBotTurn(float dt) {
    if (g_game.state != STATE_AIMING) return;
    int cur = g_game.activePlayer;
    if (g_game.biros[cur].isEliminated) return;

    bool isAI = false;
    if (g_game.matchType == MATCH_SINGLE_PLAYER_AI && cur != 0) {
        isAI = true;
    }

    if (!isAI) return;

    g_game.aiActionTimer += dt;

    switch (g_game.aiState) {
        case AI_STATE_DECIDE: {
            if (g_game.aiActionTimer >= 0.35f) {
                b2Vec2 pt;
                float angle, power;
                CalculateAIBotStrike(cur, &pt, &angle, &power);
                g_game.chosenStrikePoint = pt;
                g_game.aiTargetAngle = angle;
                g_game.aiTargetPower = power;
                g_game.aiState = AI_STATE_AIMING;
                g_game.aiActionTimer = 0.0f;
            }
            break;
        }

        case AI_STATE_AIMING: {
            float diff = g_game.aiTargetAngle - g_game.arrowAngle;
            while (diff < -PI) diff += 2.0f * PI;
            while (diff > PI)  diff -= 2.0f * PI;

            float turnSpeed = 4.2f;
            if (fabsf(diff) < turnSpeed * dt || g_game.aiActionTimer >= 0.85f) {
                g_game.arrowAngle = g_game.aiTargetAngle;
                g_game.lockedArrowAngle = g_game.aiTargetAngle;
                g_game.isCharging = true;
                g_game.chargeTimer = 0.0f;
                g_game.aiState = AI_STATE_CHARGING;
                g_game.aiActionTimer = 0.0f;
            } else {
                g_game.arrowAngle += (diff > 0 ? 1.0f : -1.0f) * turnSpeed * dt;
                if (g_game.arrowAngle > 2.0f * PI) g_game.arrowAngle -= 2.0f * PI;
                if (g_game.arrowAngle < 0.0f) g_game.arrowAngle += 2.0f * PI;
            }
            break;
        }

        case AI_STATE_CHARGING: {
            g_game.chargeTimer += dt * 3.2f;
            float rawPower = sinf(g_game.chargeTimer) * 0.5f + 0.5f;
            float currentFrac = Clamp(rawPower, 0.15f, 1.0f);

            if (currentFrac >= g_game.aiTargetPower || g_game.aiActionTimer >= 1.2f) {
                g_game.aiState = AI_STATE_RELEASE;
                g_game.aiActionTimer = 0.0f;
            }
            break;
        }

        case AI_STATE_RELEASE: {
            float rawPower = sinf(g_game.chargeTimer) * 0.5f + 0.5f;
            float powerFrac = Clamp(rawPower, 0.15f, 1.0f);
            ExecuteFlickStrike(cur, g_game.chosenStrikePoint, g_game.lockedArrowAngle, powerFrac);
            g_game.aiState = AI_STATE_DECIDE;
            g_game.aiActionTimer = 0.0f;
            break;
        }
    }
}

// Draw the Online Multiplayer Duel Modal Dialog
static void DrawOnlineModal(Vector2 mouse) {
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){ 12, 16, 14, 215 });

    Rectangle modal = { SCREEN_WIDTH * 0.5f - 300, SCREEN_HEIGHT * 0.5f - 190, 600, 380 };
    Rectangle shadow = { modal.x + 10, modal.y + 12, modal.width, modal.height };
    DrawRectangleRounded(shadow, 0.03f, 6, (Color){ 10, 12, 10, 180 });

    DrawRectangleRounded(modal, 0.03f, 6, (Color){ 250, 246, 236, 255 });
    DrawRectangleRoundedLines(modal, 0.03f, 6, (Color){ 22, 60, 160, 255 });
    DrawRectangleLines((int)modal.x + 3, (int)modal.y + 3, (int)modal.width - 6, (int)modal.height - 6, (Color){ 22, 60, 160, 160 });

    for (int y = (int)modal.y + 60; y < (int)(modal.y + modal.height - 15); y += 22) {
        DrawLine((int)modal.x + 12, y, (int)(modal.x + modal.width - 12), y, (Color){ 180, 205, 235, 110 });
    }
    DrawLine((int)modal.x + 36, (int)modal.y + 10, (int)modal.x + 36, (int)(modal.y + modal.height - 10), (Color){ 220, 75, 75, 140 });

    DrawSchoolTextTitle("CLASSROOM P2P MULTIPLAYER DUEL", modal.x + 50, modal.y + 16, 24, (Color){ 22, 60, 160, 255 });
    DrawSchoolText("100% FREE PEER-TO-PEER WEBRTC | ZERO SERVER COMPUTING OR COST", modal.x + 50, modal.y + 44, 12, (Color){ 195, 34, 42, 255 });

    Rectangle codeBox = { modal.x + 50, modal.y + 78, modal.width - 100, 68 };
    DrawRectangleRounded(codeBox, 0.12f, 4, (Color){ 238, 244, 255, 255 });
    DrawRectangleRoundedLines(codeBox, 0.12f, 4, (Color){ 35, 80, 190, 255 });

    DrawSchoolText("SECRET REVENUE-FREE ROOM CODE:", codeBox.x + 16, codeBox.y + 8, 12, (Color){ 85, 95, 110, 255 });
    const char* roomDisplay = (strlen(g_game.onlineRoomCode) > 0) ? g_game.onlineRoomCode : "PEN1";
    DrawText(TextFormat("CODE:   %s", roomDisplay), codeBox.x + 16, codeBox.y + 28, 24, (Color){ 22, 55, 165, 255 });

    Rectangle statusBox = { modal.x + 50, modal.y + 160, modal.width - 100, 48 };
    if (g_game.isOnlineConnected) {
        DrawRectangleRounded(statusBox, 0.12f, 4, (Color){ 235, 252, 240, 255 });
        DrawRectangleRoundedLines(statusBox, 0.12f, 4, (Color){ 25, 145, 70, 255 });
        DrawSchoolTextTitle("[ CONNECTED! ] OPPONENT SEATED AT THE DESK!", statusBox.x + 16, statusBox.y + 12, 18, (Color){ 20, 125, 60, 255 });
    } else {
        DrawRectangleRounded(statusBox, 0.12f, 4, (Color){ 255, 246, 230, 255 });
        DrawRectangleRoundedLines(statusBox, 0.12f, 4, (Color){ 215, 140, 30, 255 });
        DrawSchoolTextTitle("[ WAITING FOR OPPONENT TO OPEN LINK... ]", statusBox.x + 16, statusBox.y + 12, 16, (Color){ 180, 105, 15, 255 });
    }

    Rectangle copyBtn = { modal.x + 50, modal.y + 224, 240, 44 };
    bool hoverCopy = CheckCollisionPointRec(mouse, copyBtn);
    DrawRectangleRounded(copyBtn, 0.18f, 4, hoverCopy ? (Color){ 22, 60, 160, 255 } : (Color){ 238, 244, 255, 255 });
    DrawRectangleRoundedLines(copyBtn, 0.18f, 4, (Color){ 22, 60, 160, 255 });
    const char* copyTxt = "[ COPY INVITE LINK ]";
    float ctw = MeasureSchoolTextTitle(copyTxt, 15);
    DrawSchoolTextTitle(copyTxt, copyBtn.x + copyBtn.width * 0.5f - ctw * 0.5f, copyBtn.y + 13, 15, hoverCopy ? WHITE : (Color){ 22, 60, 160, 255 });

    if (hoverCopy && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        JS_CopyRoomLink();
        if (g_audio.audioReady) PlaySound(g_audio.sndScore);
    }

    Rectangle tweetBtn = { modal.x + 310, modal.y + 224, 240, 44 };
    bool hoverTweet = CheckCollisionPointRec(mouse, tweetBtn);
    DrawRectangleRounded(tweetBtn, 0.18f, 4, hoverTweet ? (Color){ 195, 34, 42, 255 } : (Color){ 255, 238, 240, 255 });
    DrawRectangleRoundedLines(tweetBtn, 0.18f, 4, (Color){ 195, 34, 42, 255 });
    const char* tweetTxt = "[ POST ON X / TWITTER ]";
    float ttw = MeasureSchoolTextTitle(tweetTxt, 15);
    DrawSchoolTextTitle(tweetTxt, tweetBtn.x + tweetBtn.width * 0.5f - ttw * 0.5f, tweetBtn.y + 13, 15, hoverTweet ? WHITE : (Color){ 195, 34, 42, 255 });

    if (hoverTweet && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        JS_TweetChallenge();
        if (g_audio.audioReady) PlaySound(g_audio.sndScore);
    }

    Rectangle closeBtn = { modal.x + 190, modal.y + 288, 220, 38 };
    bool hoverClose = CheckCollisionPointRec(mouse, closeBtn);
    DrawRectangleRounded(closeBtn, 0.18f, 4, hoverClose ? (Color){ 50, 55, 65, 255 } : (Color){ 240, 242, 245, 255 });
    DrawRectangleRoundedLines(closeBtn, 0.18f, 4, (Color){ 70, 75, 85, 255 });
    const char* closeTxt = "[ RETURN TO DESK ]";
    float cltw = MeasureSchoolTextTitle(closeTxt, 14);
    DrawSchoolTextTitle(closeTxt, closeBtn.x + closeBtn.width * 0.5f - cltw * 0.5f, closeBtn.y + 10, 14, hoverClose ? WHITE : (Color){ 50, 55, 65, 255 });

    if (hoverClose && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        g_game.showOnlineModal = false;
        if (g_audio.audioReady) PlaySound(g_audio.sndTick);
    }

    DrawSchoolText("Tip: Send your link to anyone. Both browsers connect directly P2P with zero server compute!",
                   modal.x + 50, modal.y + 342, 12, (Color){ 95, 100, 115, 255 });
}

// Draw the Pen Market & Loadout Garage Dialog - Styled as an Open School Exercise Notebook / Stationery Ledger
static void DrawPenMarketModal(Biro biros[4], int* selectedPlayerIndex, int activePlayerCount, Vector2 mouse) {
    // 1. Dim background with soft classroom blur
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){ 14, 18, 16, 215 });

    // 2. Open School Notebook Page
    Rectangle modal = { 65, 45, 1150, 690 };
    Rectangle shadow = { modal.x + 14, modal.y + 18, modal.width, modal.height };
    DrawRectangleRounded(shadow, 0.02f, 8, (Color){ 10, 12, 10, 175 });

    // Notebook Paper Base (Warm Ivory)
    DrawRectangleRounded(modal, 0.02f, 8, (Color){ 250, 246, 236, 255 });
    DrawRectangleRoundedLines(modal, 0.02f, 8, (Color){ 215, 205, 185, 255 });

    // Ruled Blue Horizontal Lines across page
    for (int y = (int)modal.y + 64; y < (int)(modal.y + modal.height - 20); y += 22) {
        DrawLine((int)modal.x + 12, y, (int)(modal.x + modal.width - 12), y, (Color){ 175, 205, 235, 110 });
    }

    // Left Red Margin Line
    DrawLine((int)modal.x + 40, (int)modal.y + 12, (int)modal.x + 40, (int)(modal.y + modal.height - 12), (Color){ 220, 75, 75, 150 });

    // Spiral Wire Binding Rings along the top of notebook
    for (int sx = (int)modal.x + 65; sx < (int)(modal.x + modal.width - 65); sx += 34) {
        DrawRectangle(sx, (int)modal.y - 6, 8, 14, (Color){ 65, 70, 75, 240 });
        DrawRectangleLines(sx, (int)modal.y - 6, 8, 14, (Color){ 40, 45, 50, 255 });
        DrawLine(sx + 2, (int)modal.y - 4, sx + 2, (int)modal.y + 6, (Color){ 160, 165, 175, 220 });
    }

    // Header in Royal Blue & Teacher Red Ink
    DrawSchoolTextTitle("CLASSROOM STATIONERY LEDGER & LOADOUT", modal.x + 52, modal.y + 16, 26, (Color){ 22, 55, 145, 255 });
    DrawSchoolText("OFFICIAL BOX2D PHYSICAL PROFILES: MASS, ROUGHNESS, CALIBER & RESTITUTION",
                   modal.x + 52, modal.y + 44, 13, (Color){ 195, 34, 42, 255 });

    // Close Button [P / ESC] - Red Ink Rubber Stamp
    Rectangle closeBtn = { modal.x + modal.width - 145, modal.y + 16, 120, 34 };
    bool hoverClose = CheckCollisionPointRec(mouse, closeBtn);
    DrawRectangleRounded(closeBtn, 0.20f, 4, hoverClose ? (Color){ 225, 45, 55, 255 } : (Color){ 250, 240, 240, 255 });
    DrawRectangleRoundedLines(closeBtn, 0.20f, 4, (Color){ 195, 35, 45, 255 });
    DrawRectangleLines((int)closeBtn.x + 2, (int)closeBtn.y + 2, (int)closeBtn.width - 4, (int)closeBtn.height - 4, (Color){ 195, 35, 45, hoverClose ? 255 : 160 });
    DrawSchoolTextTitle("[X] CLOSE [ESC]", closeBtn.x + 12, closeBtn.y + 6, 15, hoverClose ? WHITE : (Color){ 195, 35, 45, 255 });

    // Student Selection Tabs - Styled as Notebook Index Tabs
    float tabW = (modal.width - 76.0f) / (float)activePlayerCount;
    Color pInkCols[4] = {
        (Color){ 22, 58, 150, 255 },
        (Color){ 195, 34, 42, 255 },
        (Color){ 25, 125, 60, 255 },
        (Color){ 35, 38, 44, 255 }
    };
    Color pTabBg[4] = {
        (Color){ 225, 238, 255, 255 },
        (Color){ 255, 230, 230, 255 },
        (Color){ 228, 252, 235, 255 },
        (Color){ 238, 238, 242, 255 }
    };

    for (int p = 0; p < activePlayerCount; p++) {
        Rectangle tab = { modal.x + 50 + p * tabW, modal.y + 68, tabW - 10, 42 };
        bool isSel = (p == *selectedPlayerIndex);
        bool hoverTab = CheckCollisionPointRec(mouse, tab);

        if (hoverTab && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            *selectedPlayerIndex = p;
            if (g_audio.audioReady) PlaySound(g_audio.sndTick);
        }

        DrawRectangleRounded(tab, 0.20f, 4, isSel ? pTabBg[p] : (hoverTab ? (Color){ 242, 238, 228, 255 } : (Color){ 235, 230, 218, 255 }));
        DrawRectangleRoundedLines(tab, 0.20f, 4, isSel ? pInkCols[p] : (Color){ 180, 170, 155, 255 });

        DrawCircle((int)tab.x + 18, (int)tab.y + 21, 6.0f, pInkCols[p]);
        DrawSchoolTextTitle(biros[p].shortName, tab.x + 32, tab.y + 9, 16, isSel ? pInkCols[p] : (Color){ 70, 75, 85, 255 });
        const char* eqModelName = g_penModels[biros[p].modelId].name;
        DrawSchoolText(TextFormat("[%s]", eqModelName), tab.x + tab.width - MeasureSchoolText(TextFormat("[%s]", eqModelName), 13) - 14, tab.y + 12, 13, isSel ? (Color){ 40, 45, 55, 255 } : (Color){ 130, 135, 145, 255 });
    }

    // 5 Pen Model Cards - Styled as Stationery Spec Cards
    float cardW = 212.0f;
    float cardH = 508.0f;
    float cardSpacing = 13.0f;
    float startX = modal.x + 28;
    float startY = modal.y + 124;

    const Biro* currentSelBiro = &biros[*selectedPlayerIndex];

    for (int i = 0; i < NUM_PEN_MODELS; i++) {
        const PenModelDef* def = &g_penModels[i];
        Rectangle cRect = { startX + i * (cardW + cardSpacing), startY, cardW, cardH };
        bool isEquipped = (currentSelBiro->modelId == (PenModelId)i);
        bool isCardHover = CheckCollisionPointRec(mouse, cRect);

        // Card Drop Shadow
        Rectangle cShadow = { cRect.x + 4, cRect.y + 6, cRect.width, cRect.height };
        DrawRectangleRounded(cShadow, 0.03f, 6, (Color){ 20, 25, 20, 40 });

        // Card Body (Ivory Card Stock)
        DrawRectangleRounded(cRect, 0.03f, 6, isCardHover ? (Color){ 255, 255, 250, 255 } : (Color){ 253, 250, 244, 255 });
        DrawRectangleRoundedLines(cRect, 0.03f, 6, isEquipped ? (Color){ 195, 34, 42, 255 } : (isCardHover ? (Color){ 22, 58, 150, 255 } : (Color){ 195, 185, 170, 255 }));

        // Paperclip / Masking Tape accent at top of card
        if (isEquipped) {
            Rectangle clipRec = { cRect.x + cRect.width * 0.5f - 18, cRect.y - 6, 36, 14 };
            DrawRectangleRounded(clipRec, 0.3f, 4, (Color){ 238, 226, 195, 255 });
            DrawRectangleRoundedLines(clipRec, 0.3f, 4, (Color){ 195, 175, 140, 255 });
            DrawLine((int)clipRec.x + 4, (int)clipRec.y + 6, (int)(clipRec.x + clipRec.width - 4), (int)clipRec.y + 6, (Color){ 180, 160, 125, 180 });
        }

        // Card Title & Subtitle in Handwriting
        DrawSchoolTextTitle(def->name, cRect.x + 14, cRect.y + 12, 17, isEquipped ? (Color){ 195, 34, 42, 255 } : (Color){ 24, 30, 42, 255 });
        DrawSchoolText(def->subtitle, cRect.x + 14, cRect.y + 34, 13, (Color){ 22, 60, 150, 255 });

        // Visual Preview Box (Wooden Desk Cutout with Grain)
        Rectangle prevBox = { cRect.x + 12, cRect.y + 54, cRect.width - 24, 108 };
        DrawRectangleRounded(prevBox, 0.05f, 4, (Color){ 188, 126, 74, 255 });
        DrawRectangleRoundedLines(prevBox, 0.05f, 4, (Color){ 130, 80, 42, 255 });

        for (int gy = (int)prevBox.y + 12; gy < (int)(prevBox.y + prevBox.height); gy += 18) {
            DrawLine((int)prevBox.x + 2, gy, (int)(prevBox.x + prevBox.width - 2), gy, (Color){ 162, 102, 55, 60 });
        }

        Vector2 prevCenter = { prevBox.x + prevBox.width * 0.5f, prevBox.y + prevBox.height * 0.5f };
        DrawMarketPenPreview((PenModelId)i, currentSelBiro->primaryColor, currentSelBiro->capColor,
                             currentSelBiro->plugColor, currentSelBiro->inkColor, prevCenter, -15.0f, 0.65f);

        // Real Box2D Physics Metrics - Styled as Ruler Tracks with Ink Level Fill
        float yStat = cRect.y + 175;

        // Metric 1: Mass
        float approxMass = (2.0f * def->halfLength * 2.0f * def->radius + PI * def->radius * def->radius) * def->density;
        DrawSchoolText(TextFormat("MASS: %.2f kg", approxMass), cRect.x + 14, yStat, 13, (Color){ 35, 40, 50, 255 });
        DrawRectangle((int)cRect.x + 14, (int)yStat + 17, (int)(cRect.width - 28), 6, (Color){ 215, 210, 200, 255 });
        float massFrac = Clamp(approxMass / 1.05f, 0.08f, 1.0f);
        DrawRectangle((int)cRect.x + 14, (int)yStat + 17, (int)((cRect.width - 28) * massFrac), 6, (Color){ 22, 60, 160, 255 });

        // Metric 2: Roughness (Friction)
        yStat += 32;
        DrawSchoolText(TextFormat("ROUGHNESS: u = %.2f", def->friction), cRect.x + 14, yStat, 13, (Color){ 35, 40, 50, 255 });
        DrawRectangle((int)cRect.x + 14, (int)yStat + 17, (int)(cRect.width - 28), 6, (Color){ 215, 210, 200, 255 });
        float fricFrac = Clamp(def->friction / 0.70f, 0.08f, 1.0f);
        DrawRectangle((int)cRect.x + 14, (int)yStat + 17, (int)((cRect.width - 28) * fricFrac), 6, (def->friction > 0.5f ? (Color){ 195, 34, 42, 255 } : (Color){ 25, 135, 65, 255 }));

        // Metric 3: Caliber / Thickness
        yStat += 32;
        DrawSchoolText(TextFormat("CALIBER: %.1f mm", def->radius * 200.0f), cRect.x + 14, yStat, 13, (Color){ 35, 40, 50, 255 });
        DrawRectangle((int)cRect.x + 14, (int)yStat + 17, (int)(cRect.width - 28), 6, (Color){ 215, 210, 200, 255 });
        float calFrac = Clamp((def->radius - 0.04f) / 0.04f, 0.10f, 1.0f);
        DrawRectangle((int)cRect.x + 14, (int)yStat + 17, (int)((cRect.width - 28) * calFrac), 6, (Color){ 215, 140, 30, 255 });

        // Metric 4: Restitution / Bounciness
        yStat += 32;
        DrawSchoolText(TextFormat("RESTITUTION: e = %.2f", def->restitution), cRect.x + 14, yStat, 13, (Color){ 35, 40, 50, 255 });
        DrawRectangle((int)cRect.x + 14, (int)yStat + 17, (int)(cRect.width - 28), 6, (Color){ 215, 210, 200, 255 });
        float restFrac = Clamp(def->restitution, 0.10f, 1.0f);
        DrawRectangle((int)cRect.x + 14, (int)yStat + 17, (int)((cRect.width - 28) * restFrac), 6, (Color){ 135, 65, 195, 255 });

        // Metric 5: Table Drag / Damping
        yStat += 32;
        DrawSchoolText(TextFormat("TABLE DRAG: %.2f /s", def->linearDamping), cRect.x + 14, yStat, 13, (Color){ 95, 100, 110, 255 });

        // Description Paragraph in Blue Ink
        DrawSchoolText(def->descLine1, cRect.x + 14, cRect.y + 360, 13, (Color){ 22, 55, 145, 235 });
        DrawSchoolText(def->descLine2, cRect.x + 14, cRect.y + 378, 13, (Color){ 22, 55, 145, 235 });

        // Equip Action Button - Styled as a Rubber Stamp
        Rectangle equipBtn = { cRect.x + 12, cRect.y + 448, cRect.width - 24, 40 };
        bool isBtnHover = CheckCollisionPointRec(mouse, equipBtn);

        if (isEquipped) {
            DrawRectangleRounded(equipBtn, 0.20f, 4, (Color){ 235, 250, 238, 255 });
            DrawRectangleRoundedLines(equipBtn, 0.20f, 4, (Color){ 25, 135, 65, 255 });
            DrawRectangleLines((int)equipBtn.x + 2, (int)equipBtn.y + 2, (int)equipBtn.width - 4, (int)equipBtn.height - 4, (Color){ 25, 135, 65, 180 });
            const char* inCaseTxt = "[ APPROVED IN CASE ]";
            float txtW = MeasureSchoolTextTitle(inCaseTxt, 14);
            DrawSchoolTextTitle(inCaseTxt, equipBtn.x + equipBtn.width * 0.5f - txtW * 0.5f, equipBtn.y + 11, 14, (Color){ 25, 135, 65, 255 });
        } else {
            DrawRectangleRounded(equipBtn, 0.20f, 4, isBtnHover ? (Color){ 22, 60, 160, 255 } : (Color){ 242, 245, 252, 255 });
            DrawRectangleRoundedLines(equipBtn, 0.20f, 4, (Color){ 22, 60, 160, 255 });
            DrawRectangleLines((int)equipBtn.x + 2, (int)equipBtn.y + 2, (int)equipBtn.width - 4, (int)equipBtn.height - 4, isBtnHover ? WHITE : (Color){ 22, 60, 160, 150 });
            const char* btnTxt = TextFormat("EQUIP TO P%d", *selectedPlayerIndex + 1);
            float tw = MeasureSchoolTextTitle(btnTxt, 14);
            DrawSchoolTextTitle(btnTxt, equipBtn.x + equipBtn.width * 0.5f - tw * 0.5f, equipBtn.y + 11, 14, isBtnHover ? WHITE : (Color){ 22, 60, 160, 255 });

            if (isBtnHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                EquipPenModel(&biros[*selectedPlayerIndex], (PenModelId)i);
                if (g_audio.audioReady) PlaySound(g_audio.sndScore);
            }
        }
    }

    // Bottom Navigation Bar Hint
    DrawSchoolText("STATIONERY CONTROLS:  [P] or [ESC] Close  |  [1] - [4] Select Student  |  Click [EQUIP] to arm student with selected biro",
                   modal.x + 48, modal.y + modal.height - 28, 14, (Color){ 85, 90, 100, 255 });
}

// Main Update & Render Frame Callback (Usable by both Emscripten and Desktop Loop)
static void UpdateDrawFrame(void) {
    float dt = GetFrameTime();
    if (dt > 0.033f) dt = 0.033f;

    Vector2 mouse = GetMousePosition();
    b2Vec2 mouseWorld = WorldFromScreen(mouse);

    // Screen Shake decay
    if (g_game.screenShake > 0.0f) {
        g_game.screenShake -= dt * 15.0f;
        if (g_game.screenShake < 0.0f) g_game.screenShake = 0.0f;
    }

    int numP = (g_game.currentGameMode == MODE_1V1) ? 2 : 4;

    // Single Menu Button [ ⏸ MENU ]
    Rectangle menuBtn = { 152, 12, 126, 44 };
    bool clickHandled = false;
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && mouse.y < 68 && g_game.state != STATE_PEN_MARKET && !g_game.showOnlineModal) {
        if (CheckCollisionPointRec(mouse, menuBtn)) {
            g_game.isPausedMenuOpen = !g_game.isPausedMenuOpen;
            if (g_audio.audioReady) PlaySound(g_audio.sndTick);
            JS_ToggleNotebookMenu();
            clickHandled = true;
        }
    }

    // Handle clicks inside Classroom Pause Menu Modal
    if (g_game.isPausedMenuOpen) {
        Rectangle modal = { 390, 125, 500, 470 };
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            for (int i = 0; i < 5; i++) {
                Rectangle btn = { modal.x + 40, modal.y + 148 + i * 56, modal.width - 80, 46 };
                if (CheckCollisionPointRec(mouse, btn)) {
                    if (g_audio.audioReady) PlaySound(g_audio.sndTick);
                    if (i == 0) {
                        // 1. Resume Match (keeps pens in place!)
                        g_game.isPausedMenuOpen = false;
                    } else if (i == 1) {
                        // 2. Restart Round
                        ResetAllBirosForModeAndStage(g_game.biros, g_game.currentGameMode, g_game.currentStage);
                        ResetTurnForRound(g_game.roundNumber);
                        g_game.isPausedMenuOpen = false;
                    } else if (i == 2) {
                        // 3. Desk Setup / Lobby
                        JS_ToggleNotebookMenu();
                    } else if (i == 3) {
                        // 4. Pen Market
                        g_game.state = STATE_PEN_MARKET;
                        g_game.marketSelectedPlayer = g_game.activePlayer;
                        g_game.isPausedMenuOpen = false;
                    } else if (i == 4) {
                        // 5. New Match / Reset
                        ResetMatchScoresAndBiros();
                        g_game.isPausedMenuOpen = false;
                    }
                    clickHandled = true;
                    break;
                }
            }
        }
    }

    // Toggle Mobile Touch vs Desktop PC Mouse mode via bottom-right button
    Rectangle ctrlToggleRect = { SCREEN_WIDTH - 215, SCREEN_HEIGHT - 32, 205, 26 };
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, ctrlToggleRect)) {
        g_game.isTouchMode = !g_game.isTouchMode;
        if (g_audio.audioReady) PlaySound(g_audio.sndTick);
        clickHandled = true;
    }

    // Global Hotkeys
    if (IsKeyPressed(KEY_C)) g_game.showDebugColliders = !g_game.showDebugColliders;
    if (IsKeyPressed(KEY_T)) g_game.showTelemetry = !g_game.showTelemetry;

    if (IsKeyPressed(KEY_M) && g_game.state != STATE_PEN_MARKET && !g_game.showOnlineModal) {
        g_game.isPausedMenuOpen = !g_game.isPausedMenuOpen;
        JS_ToggleNotebookMenu();
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        if (g_game.state == STATE_PEN_MARKET) {
            g_game.state = STATE_AIMING;
        } else if (g_game.showOnlineModal) {
            g_game.showOnlineModal = false;
        } else {
            g_game.isPausedMenuOpen = !g_game.isPausedMenuOpen;
            JS_ToggleNotebookMenu();
        }
    }

    if (IsKeyPressed(KEY_R) && g_game.state != STATE_PEN_MARKET && !g_game.showOnlineModal && !g_game.isPausedMenuOpen) {
        ResetMatchScoresAndBiros();
    }

    if (IsKeyPressed(KEY_P) && !g_game.isPausedMenuOpen) {
        if (g_game.state == STATE_PEN_MARKET) {
            g_game.state = STATE_AIMING;
        } else if (g_game.state == STATE_AIMING || g_game.state == STATE_ROUND_OVER) {
            g_game.state = STATE_PEN_MARKET;
            g_game.marketSelectedPlayer = g_game.activePlayer;
        }
    }

    // Ensure active player is not eliminated
    if (g_game.biros[g_game.activePlayer].isEliminated) {
        g_game.activePlayer = GetNextActivePlayer(g_game.activePlayer, g_game.biros, numP);
    }
    Biro* currentBiro = &g_game.biros[g_game.activePlayer];

    // Quick cycle pen model with [K]
    if (IsKeyPressed(KEY_K) && g_game.state == STATE_AIMING && !g_game.showOnlineModal && !g_game.isPausedMenuOpen) {
        EquipPenModel(currentBiro, (PenModelId)((currentBiro->modelId + 1) % NUM_PEN_MODELS));
        if (g_audio.audioReady) PlaySound(g_audio.sndTick);
    }

    // Run AI Bot if it's the AI's turn!
    if (!g_game.showOnlineModal && !g_game.isPausedMenuOpen) {
        if (g_game.state == STATE_AIMING && g_game.matchType == MATCH_SINGLE_PLAYER_AI && g_game.activePlayer != 0) {
            UpdateAIBotTurn(dt);
        }
    }

    // STATE LOGIC
    switch (g_game.state) {
        case STATE_PEN_MARKET: {
            if (IsKeyPressed(KEY_ONE)) g_game.marketSelectedPlayer = 0;
            if (IsKeyPressed(KEY_TWO) && numP >= 2) g_game.marketSelectedPlayer = 1;
            if (IsKeyPressed(KEY_THREE) && numP >= 3) g_game.marketSelectedPlayer = 2;
            if (IsKeyPressed(KEY_FOUR) && numP >= 4) g_game.marketSelectedPlayer = 3;
            if (IsKeyPressed(KEY_ESCAPE)) g_game.state = STATE_AIMING;

            Rectangle closeBtn = { 65 + 1150 - 130, 50 + 20, 105, 32 };
            if (CheckCollisionPointRec(mouse, closeBtn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                g_game.state = STATE_AIMING;
            }
            break;
        }

        case STATE_AIMING: {
            if (g_game.showOnlineModal || g_game.isPausedMenuOpen) break;

            g_game.turnTimer -= dt;
            int curSec = (int)g_game.turnTimer;
            if (g_game.turnTimer < 3.5f && curSec != g_game.lastTickSecond && curSec >= 0) {
                g_game.lastTickSecond = curSec;
                if (g_audio.audioReady) PlaySound(g_audio.sndTick);
            }

            bool isLocalTurn = false;
            if (g_game.matchType == MATCH_LOCAL_PASS_PLAY) isLocalTurn = true;
            else if (g_game.matchType == MATCH_SINGLE_PLAYER_AI) isLocalTurn = (g_game.activePlayer == 0);
            else if (g_game.matchType == MATCH_ONLINE_P2P) isLocalTurn = (g_game.activePlayer == g_game.onlineLocalPlayerIndex);

            if (isLocalTurn) {
                if (g_game.swipeMissFeedbackTimer > 0.0f) {
                    g_game.swipeMissFeedbackTimer -= dt;
                }

                if (g_game.isTouchMode) {
                    // ========================================================
                    // MOBILE SWIPE CONTROL (Swipe line across biro or rear spear)
                    // ========================================================
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !clickHandled) {
                        g_game.isSwiping = true;
                        g_game.touchSwipeStart = mouse;
                        g_game.touchSwipeCurrent = mouse;
                        g_game.touchSwipeStartTime = (float)GetTime();
                    }

                    if (g_game.isSwiping) {
                        g_game.touchSwipeCurrent = mouse;
                    }

                    if (g_game.isSwiping && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                        g_game.isSwiping = false;
                        float swipeDur = (float)GetTime() - g_game.touchSwipeStartTime;
                        SwipeStrikeResult res = CalculateSwipeStrike(currentBiro, g_game.touchSwipeStart, mouse, swipeDur);
                        if (res.isValid) {
                            g_game.chosenStrikePoint = res.localStrikePoint;
                            g_game.lockedArrowAngle = res.angle;
                            ExecuteFlickStrike(g_game.activePlayer, res.localStrikePoint, res.angle, res.powerFrac);
                        } else {
                            if (Vector2Distance(g_game.touchSwipeStart, mouse) >= 22.0f) {
                                g_game.swipeMissFeedbackTimer = 1.8f;
                            }
                        }
                    }

                    // Autotest fallback for CI test
                    if (g_game.autotestMode && g_game.frameCount == 8 && !g_game.startInMarket) {
                        b2Vec2 strikePt = (b2Vec2){ -0.85f, 0.0f };
                        ExecuteFlickStrike(g_game.activePlayer, strikePt, 0.15f, 0.85f);
                    }
                } else {
                    // ========================================================
                    // DESKTOP MOUSE CONTROL (Classic Rotating Arrow + Ruler Charge)
                    // ========================================================
                    if (!g_game.isCharging) {
                        g_game.arrowAngle += 3.6f * dt;
                        if (g_game.arrowAngle > 2.0f * PI) g_game.arrowAngle -= 2.0f * PI;

                        bool mouseOverPen = IsPointInBiro(currentBiro, mouseWorld, 0.20f);
                        if (mouseOverPen) {
                            b2Vec2 local = b2Body_GetLocalPoint(currentBiro->bodyId, mouseWorld);
                            const PenModelDef* def = &g_penModels[currentBiro->modelId];
                            local.x = Clamp(local.x, -def->halfLength + 0.05f, def->halfLength - 0.05f);
                            local.y = 0.0f;
                            g_game.chosenStrikePoint = local;
                        }
                    }

                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !g_game.isCharging && !clickHandled) {
                        g_game.isCharging = true;
                        g_game.chargeTimer = 0.0f;
                        g_game.lockedArrowAngle = g_game.arrowAngle;
                    } else if (g_game.autotestMode && g_game.frameCount >= 5 && !g_game.isCharging && !g_game.startInMarket) {
                        g_game.isCharging = true;
                        g_game.chosenStrikePoint = (b2Vec2){ -0.85f, 0.0f };
                        g_game.lockedArrowAngle = 0.15f;
                        g_game.chargeTimer = 1.30f;
                    }

                    if (g_game.isCharging) {
                        if (!g_game.autotestMode) g_game.chargeTimer += dt * 3.2f;
                        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) g_game.isCharging = false;
                    }

                    bool shouldRelease = false;
                    if (g_game.isCharging && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                        shouldRelease = true;
                    } else if (g_game.autotestMode && g_game.frameCount == 8 && !g_game.startInMarket) {
                        shouldRelease = true;
                    }

                    if (shouldRelease) {
                        float rawPower = sinf(g_game.chargeTimer) * 0.5f + 0.5f;
                        float powerFrac = Clamp(rawPower, 0.15f, 1.0f);
                        ExecuteFlickStrike(g_game.activePlayer, g_game.chosenStrikePoint, g_game.lockedArrowAngle, powerFrac);
                    }
                }

                if (g_game.turnTimer <= 0.0f) {
                    if (g_game.isCharging) {
                        float rawPower = sinf(g_game.chargeTimer) * 0.5f + 0.5f;
                        float powerFrac = Clamp(rawPower, 0.15f, 1.0f);
                        ExecuteFlickStrike(g_game.activePlayer, g_game.chosenStrikePoint, g_game.lockedArrowAngle, powerFrac);
                    } else {
                        if (g_audio.audioReady) PlaySound(g_audio.sndBuzzer);
                        g_game.activePlayer = GetNextActivePlayer(g_game.activePlayer, g_game.biros, numP);
                        g_game.turnTimer = TURN_TIME_LIMIT;
                        g_game.isCharging = false;
                        g_game.isSwiping = false;
                        g_game.lastTickSecond = -1;
                        g_game.aiState = AI_STATE_DECIDE;
                        g_game.aiActionTimer = 0.0f;
                        break;
                    }
                }
            } else {
                if (g_game.turnTimer <= 0.0f) {
                    if (g_audio.audioReady) PlaySound(g_audio.sndBuzzer);
                    g_game.activePlayer = GetNextActivePlayer(g_game.activePlayer, g_game.biros, numP);
                    g_game.turnTimer = TURN_TIME_LIMIT;
                    g_game.isCharging = false;
                    g_game.lastTickSecond = -1;
                    g_game.aiState = AI_STATE_DECIDE;
                    g_game.aiActionTimer = 0.0f;
                }
            }
            break;
        }

        case STATE_SIMULATING: {
            g_game.simTimer += dt;
            b2World_Step(g_game.worldId, dt, 8);

            for (int i = 0; i < numP; i++) {
                if (g_game.biros[i].isEliminated) continue;
                b2Vec2 v = b2Body_GetLinearVelocity(g_game.biros[i].bodyId);
                float speed = b2Length(v);
                float maxSpeed = 16.0f;
                if (speed > maxSpeed) {
                    b2Body_SetLinearVelocity(g_game.biros[i].bodyId, (b2Vec2){ v.x * (maxSpeed / speed), v.y * (maxSpeed / speed) });
                }
                float w = b2Body_GetAngularVelocity(g_game.biros[i].bodyId);
                float maxW = 35.0f;
                if (fabsf(w) > maxW) {
                    b2Body_SetAngularVelocity(g_game.biros[i].bodyId, (w > 0 ? maxW : -maxW));
                }
            }

            b2ContactEvents contactEvents = b2World_GetContactEvents(g_game.worldId);
            for (int i = 0; i < contactEvents.hitCount; i++) {
                b2ContactHitEvent hit = contactEvents.hitEvents[i];
                float approach = hit.approachSpeed;

                if (approach > 0.3f) {
                    if (g_audio.audioReady) {
                        float vol = Clamp(approach / 6.0f, 0.25f, 1.0f);
                        float pitch = 0.85f + ((float)(rand() % 40) / 100.0f);
                        SetSoundVolume(g_audio.sndClack, vol);
                        SetSoundPitch(g_audio.sndClack, pitch);
                        PlaySound(g_audio.sndClack);
                    }

                    if (approach > 1.8f) {
                        g_game.screenShake = fmaxf(g_game.screenShake, Clamp(approach * 1.8f, 2.0f, 8.0f));
                    }

                    Vector2 hitScr = ScreenFromWorld(hit.point);
                    for (int k = 0; k < 12; k++) {
                        float spread = ((float)(rand() % 100) / 100.0f - 0.5f) * 2.0f;
                        Vector2 v = {
                            (hit.normal.x + spread * 0.8f) * approach * 40.0f,
                            (hit.normal.y + spread * 0.8f) * approach * 40.0f
                        };
                        SpawnParticle(hitScr, v, (Color){ 255, 235, 150, 255 }, 4.0f, 0.35f, false);
                    }
                }
            }

            bool anyNewFall = false;
            for (int i = 0; i < numP; i++) {
                if (g_game.biros[i].isEliminated || g_game.biros[i].isFalling) continue;
                b2Pos pos = b2Body_GetPosition(g_game.biros[i].bodyId);
                if (CheckPenFallOffTable(pos, g_game.currentTableType)) {
                    g_game.biros[i].isFalling = true;
                    g_game.biros[i].fallProgress = 0.0f;
                    g_game.biros[i].fallStartPos = pos;
                    g_game.biros[i].fallStartAngle = b2Rot_GetAngle(b2Body_GetRotation(g_game.biros[i].bodyId));
                    if (g_audio.audioReady) PlaySound(g_audio.sndFall);
                    anyNewFall = true;
                }
            }

            if (anyNewFall) {
                g_game.state = STATE_FALLING;
                g_game.stateTimer = 0.0f;
                break;
            }

            bool allStopped = true;
            for (int i = 0; i < numP; i++) {
                if (g_game.biros[i].isEliminated) continue;
                b2Vec2 v = b2Body_GetLinearVelocity(g_game.biros[i].bodyId);
                float w  = b2Body_GetAngularVelocity(g_game.biros[i].bodyId);
                if (b2Length(v) >= 0.04f || fabsf(w) >= 0.06f) {
                    allStopped = false;
                    break;
                }
            }

            if (allStopped) {
                g_game.settleTimer += dt;
                if (g_game.settleTimer > 0.25f) {
                    for (int i = 0; i < numP; i++) {
                        if (!g_game.biros[i].isEliminated) {
                            b2Body_SetLinearVelocity(g_game.biros[i].bodyId, (b2Vec2){ 0.0f, 0.0f });
                            b2Body_SetAngularVelocity(g_game.biros[i].bodyId, 0.0f);
                        }
                    }

                    if (g_game.matchType == MATCH_ONLINE_P2P && g_game.isOnlineHost) {
                        for (int i = 0; i < numP; i++) {
                            b2Pos p = b2Body_GetPosition(g_game.biros[i].bodyId);
                            float a = b2Rot_GetAngle(b2Body_GetRotation(g_game.biros[i].bodyId));
                            JS_SendNetworkSync(i, p.x, p.y, a, g_game.biros[i].isEliminated ? 1 : 0, g_game.biros[i].score);
                        }
                    }

                    g_game.activePlayer = GetNextActivePlayer(g_game.activePlayer, g_game.biros, numP);
                    g_game.turnTimer = TURN_TIME_LIMIT;
                    g_game.aiState = AI_STATE_DECIDE;
                    g_game.aiActionTimer = 0.0f;
                    g_game.state = STATE_AIMING;
                }
            } else if (g_game.simTimer > 3.5f) {
                for (int i = 0; i < numP; i++) {
                    if (!g_game.biros[i].isEliminated) {
                        b2Body_SetLinearVelocity(g_game.biros[i].bodyId, (b2Vec2){ 0.0f, 0.0f });
                        b2Body_SetAngularVelocity(g_game.biros[i].bodyId, 0.0f);
                    }
                }

                if (g_game.matchType == MATCH_ONLINE_P2P && g_game.isOnlineHost) {
                    for (int i = 0; i < numP; i++) {
                        b2Pos p = b2Body_GetPosition(g_game.biros[i].bodyId);
                        float a = b2Rot_GetAngle(b2Body_GetRotation(g_game.biros[i].bodyId));
                        JS_SendNetworkSync(i, p.x, p.y, a, g_game.biros[i].isEliminated ? 1 : 0, g_game.biros[i].score);
                    }
                }

                g_game.activePlayer = GetNextActivePlayer(g_game.activePlayer, g_game.biros, numP);
                g_game.turnTimer = TURN_TIME_LIMIT;
                g_game.aiState = AI_STATE_DECIDE;
                g_game.aiActionTimer = 0.0f;
                g_game.state = STATE_AIMING;
            } else {
                g_game.settleTimer = 0.0f;
            }
            break;
        }

        case STATE_FALLING: {
            g_game.stateTimer += dt;
            float fallDuration = 0.8f;

            for (int i = 0; i < numP; i++) {
                if (g_game.biros[i].isFalling) {
                    g_game.biros[i].fallProgress = Clamp(g_game.stateTimer / fallDuration, 0.0f, 1.0f);
                }
            }

            if (g_game.stateTimer >= fallDuration) {
                for (int i = 0; i < numP; i++) {
                    if (g_game.biros[i].isFalling) {
                        g_game.biros[i].isFalling = false;
                        g_game.biros[i].isEliminated = true;
                        b2Body_Disable(g_game.biros[i].bodyId);
                    }
                }

                bool roundOver = false;

                if (g_game.currentGameMode == MODE_1V1) {
                    roundOver = true;
                    if (g_game.biros[0].isEliminated && g_game.biros[1].isEliminated) {
                        g_game.roundOutcomeMsg = "DOUBLE KNOCKOUT! BOTH BIROS FELL OFF!";
                    } else if (g_game.biros[0].isEliminated) {
                        g_game.biros[1].score++;
                        g_game.roundOutcomeMsg = (g_game.matchType == MATCH_SINGLE_PLAYER_AI) ? "AI BOT SCORES!" : "PLAYER 2 (RED) SCORES!";
                        if (g_audio.audioReady) PlaySound(g_audio.sndScore);
                    } else if (g_game.biros[1].isEliminated) {
                        g_game.biros[0].score++;
                        g_game.roundOutcomeMsg = "PLAYER 1 (BLUE) SCORES!";
                        if (g_audio.audioReady) PlaySound(g_audio.sndScore);
                    }
                } else if (g_game.currentGameMode == MODE_TEAMS_2V2) {
                    int blueAlive = (g_game.biros[0].isEliminated ? 0 : 1) + (g_game.biros[2].isEliminated ? 0 : 1);
                    int redAlive  = (g_game.biros[1].isEliminated ? 0 : 1) + (g_game.biros[3].isEliminated ? 0 : 1);

                    if (blueAlive == 0 && redAlive == 0) {
                        roundOver = true;
                        g_game.roundOutcomeMsg = "ALL PENS KNOCKED OFF! DRAW ROUND!";
                    } else if (blueAlive == 0) {
                        roundOver = true;
                        g_game.teamRedScore++;
                        g_game.roundOutcomeMsg = "TEAM RED (RED + BLACK) WINS THE ROUND!";
                        if (g_audio.audioReady) PlaySound(g_audio.sndScore);
                    } else if (redAlive == 0) {
                        roundOver = true;
                        g_game.teamBlueScore++;
                        g_game.roundOutcomeMsg = "TEAM BLUE (BLUE + GREEN) WINS THE ROUND!";
                        if (g_audio.audioReady) PlaySound(g_audio.sndScore);
                    } else {
                        roundOver = false;
                        g_game.activePlayer = GetNextActivePlayer(g_game.activePlayer, g_game.biros, 4);
                        g_game.turnTimer = TURN_TIME_LIMIT;
                        g_game.aiState = AI_STATE_DECIDE;
                        g_game.aiActionTimer = 0.0f;
                        g_game.state = STATE_AIMING;
                        break;
                    }
                } else { // MODE_BATTLE_ROYALE
                    int aliveCount = 0;
                    int lastSurvivor = -1;
                    for (int i = 0; i < 4; i++) {
                        if (!g_game.biros[i].isEliminated) {
                            aliveCount++;
                            lastSurvivor = i;
                        }
                    }

                    if (aliveCount <= 1) {
                        roundOver = true;
                        if (aliveCount == 1) {
                            g_game.biros[lastSurvivor].score++;
                            g_game.roundOutcomeMsg = TextFormat("%s IS THE LAST PEN STANDING!", g_game.biros[lastSurvivor].playerName);
                            if (g_audio.audioReady) PlaySound(g_audio.sndScore);
                        } else {
                            g_game.roundOutcomeMsg = "TOTAL CARNAGE! NO BIROS SURVIVED!";
                        }
                    } else {
                        roundOver = false;
                        g_game.activePlayer = GetNextActivePlayer(g_game.activePlayer, g_game.biros, 4);
                        g_game.turnTimer = TURN_TIME_LIMIT;
                        g_game.aiState = AI_STATE_DECIDE;
                        g_game.aiActionTimer = 0.0f;
                        g_game.state = STATE_AIMING;
                        break;
                    }
                }

                if (roundOver) {
                    bool matchOver = false;
                    if (g_game.currentGameMode == MODE_1V1) {
                        if (g_game.biros[0].score >= 3 || g_game.biros[1].score >= 3) matchOver = true;
                    } else if (g_game.currentGameMode == MODE_TEAMS_2V2) {
                        if (g_game.teamBlueScore >= 3 || g_game.teamRedScore >= 3) matchOver = true;
                    } else {
                        for (int i = 0; i < 4; i++) {
                            if (g_game.biros[i].score >= 3) { matchOver = true; break; }
                        }
                    }

                    if (matchOver) {
                        g_game.state = STATE_MATCH_OVER;
                        for (int i = 0; i < 180; i++) {
                            Color confCols[5] = { GOLD, SKYBLUE, RED, LIME, ORANGE };
                            Vector2 p = { (float)(rand() % SCREEN_WIDTH), (float)(rand() % 300 - 150) };
                            Vector2 v = { ((float)(rand() % 100) / 100.0f - 0.5f) * 120.0f, (float)(rand() % 150 + 60) };
                            SpawnParticle(p, v, confCols[rand() % 5], (float)(rand() % 8 + 6), 3.5f, true);
                        }
                    } else {
                        g_game.state = STATE_ROUND_OVER;
                    }
                }
            }
            break;
        }

        case STATE_ROUND_OVER: {
            if (IsKeyPressed(KEY_SPACE) || (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !g_game.showOnlineModal)) {
                g_game.roundNumber++;
                ResetAllBirosForModeAndStage(g_game.biros, g_game.currentGameMode, g_game.currentStage);
                ResetTurnForRound(g_game.roundNumber);
            }
            break;
        }

        case STATE_MATCH_OVER: {
            if (IsKeyPressed(KEY_R) || IsKeyPressed(KEY_SPACE) || (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !g_game.showOnlineModal)) {
                ResetMatchScoresAndBiros();
            }
            break;
        }

        default:
            break;
    }

    // DRAWING
    BeginDrawing();
    ClearBackground((Color){ 24, 38, 28, 255 });

    rlPushMatrix();
    if (g_game.screenShake > 0.05f) {
        float ox = ((float)(rand() % 100) / 100.0f - 0.5f) * g_game.screenShake * 2.0f;
        float oy = ((float)(rand() % 100) / 100.0f - 0.5f) * g_game.screenShake * 2.0f;
        rlTranslatef(ox, oy, 0.0f);
    }

    // 1. Draw School Table Desk
    DrawSchoolDesk(g_game.deskRect, g_game.currentTableType);

    // 1b. Opening Break Desk Notification (Telegraphs Alternating First Move)
    if (g_game.state == STATE_AIMING && g_game.isFirstMoveOfRound) {
        int starter = g_game.roundStartingPlayer;
        const char* note = TextFormat("[ ROUND %d OPENING MOVE: %s STRIKES FIRST ]", g_game.roundNumber, g_game.biros[starter].playerName);
        float tw = MeasureSchoolTextTitle(note, 13);
        Rectangle noteBox = { TABLE_CENTER_X - tw * 0.5f - 14, g_game.deskRect.y + 14.0f, tw + 28, 24 };
        DrawRectangleRounded(noteBox, 0.35f, 4, (Color){ 24, 20, 16, 215 });
        DrawRectangleRoundedLines(noteBox, 0.35f, 4, (Color){ 235, 205, 130, 220 });
        DrawSchoolTextTitle(note, noteBox.x + 14, noteBox.y + 4, 13, (Color){ 250, 238, 210, 255 });
    }

    // 2. Active Pen Subtle Aura
    if (g_game.state == STATE_AIMING && !currentBiro->isEliminated) {
        b2Pos pos = b2Body_GetPosition(currentBiro->bodyId);
        Vector2 pScr = ScreenFromWorld(pos);
        float pulse = (sinf((float)GetTime() * 5.0f) + 1.0f) * 0.5f;
        Color auraCol = currentBiro->accentColor;
        auraCol.a = (unsigned char)(25 + pulse * 45);
        DrawRectanglePro(
            (Rectangle){ pScr.x, pScr.y, 310.0f, 32.0f },
            (Vector2){ 155.0f, 16.0f },
            b2Rot_GetAngle(b2Body_GetRotation(currentBiro->bodyId)) * RAD2DEG,
            auraCol
        );
    }

    // 3. Draw Biro Pens
    for (int i = 0; i < numP; i++) {
        DrawBiroPen(&g_game.biros[i], g_game.showDebugColliders, 1.0f);
    }

    // 4. Aiming & Strike Controls Visualization (Mobile Swipe vs Desktop Mouse)
    if (g_game.state == STATE_AIMING && !currentBiro->isEliminated) {
        if (g_game.isTouchMode) {
            // ================================================================
            // MOBILE SWIPE RENDERING
            // ================================================================
            if (g_game.isSwiping) {
                float dur = fmaxf((float)GetTime() - g_game.touchSwipeStartTime, 0.02f);
                SwipeStrikeResult preview = CalculateSwipeStrike(currentBiro, g_game.touchSwipeStart, g_game.touchSwipeCurrent, dur);

                float distPx = Vector2Distance(g_game.touchSwipeStart, g_game.touchSwipeCurrent);
                Vector2 sStart = g_game.touchSwipeStart;
                Vector2 sEnd   = g_game.touchSwipeCurrent;

                if (distPx > 8.0f) {
                    float sAng = atan2f(sEnd.y - sStart.y, sEnd.x - sStart.x);
                    Vector2 sDir = { cosf(sAng), sinf(sAng) };

                    Color swipeCol = (Color){ 180, 210, 240, 160 };
                    if (preview.isValid) {
                        swipeCol = (preview.powerFrac < 0.5f) ?
                            ColorLerp((Color){ 30, 180, 90, 255 }, (Color){ 245, 180, 40, 255 }, preview.powerFrac * 2.0f) :
                            ColorLerp((Color){ 245, 180, 40, 255 }, (Color){ 230, 45, 45, 255 }, (preview.powerFrac - 0.5f) * 2.0f);
                    }

                    // Draw swipe line
                    DrawLineEx(sStart, sEnd, preview.isValid ? 5.5f : 3.5f, swipeCol);
                    DrawCircleV(sStart, 6.0f, swipeCol);

                    // Arrowhead at finger tip
                    Vector2 tipHead1 = { sEnd.x - sDir.x * 15.0f + sDir.y * 9.0f, sEnd.y - sDir.y * 15.0f - sDir.x * 9.0f };
                    Vector2 tipHead2 = { sEnd.x - sDir.x * 15.0f - sDir.y * 9.0f, sEnd.y - sDir.y * 15.0f + sDir.x * 9.0f };
                    DrawTriangle(sEnd, tipHead1, tipHead2, swipeCol);

                    if (preview.isValid) {
                        // Highlight contact point on the biro
                        b2Pos contactWorld = b2Body_GetWorldPoint(currentBiro->bodyId, preview.localStrikePoint);
                        Vector2 contactScr = ScreenFromWorld(contactWorld);
                        DrawCircleV(contactScr, 8.0f, (Color){ 255, 230, 70, 255 });
                        DrawCircleLines((int)contactScr.x, (int)contactScr.y, 14.0f, WHITE);
                        DrawCircleLines((int)contactScr.x, (int)contactScr.y, 22.0f, swipeCol);

                        // Impulse projection vector from contact point
                        float impLen = 50.0f + preview.powerFrac * 80.0f;
                        Vector2 impTip = { contactScr.x + cosf(preview.angle) * impLen, contactScr.y + sinf(preview.angle) * impLen };
                        DrawLineEx(contactScr, impTip, 4.0f, swipeCol);
                        Vector2 iDir = { cosf(preview.angle), sinf(preview.angle) };
                        Vector2 iHead1 = { impTip.x - iDir.x * 12.0f + iDir.y * 7.0f, impTip.y - iDir.y * 12.0f - iDir.x * 7.0f };
                        Vector2 iHead2 = { impTip.x - iDir.x * 12.0f - iDir.y * 7.0f, impTip.y - iDir.y * 12.0f + iDir.x * 7.0f };
                        DrawTriangle(impTip, iHead1, iHead2, swipeCol);

                        // Floating power badge near finger
                        const char* badgeTxt = TextFormat("%s  [%d%% POWER]", preview.strikeType, (int)(preview.powerFrac * 100.0f));
                        float bw = MeasureSchoolTextTitle(badgeTxt, 14);
                        Rectangle bRec = { sEnd.x - bw * 0.5f - 8, sEnd.y - 36, bw + 16, 24 };
                        DrawRectangleRounded(bRec, 0.25f, 4, (Color){ 16, 22, 18, 235 });
                        DrawRectangleRoundedLines(bRec, 0.25f, 4, swipeCol);
                        DrawSchoolTextTitle(badgeTxt, bRec.x + 8, bRec.y + 4, 14, swipeCol);
                    } else {
                        // Gentle hint indicating cut is needed
                        const char* sliceTxt = "Cross through the biro...";
                        float sw = MeasureSchoolText(sliceTxt, 13);
                        DrawSchoolText(sliceTxt, sEnd.x - sw * 0.5f, sEnd.y - 25, 13, (Color){ 200, 220, 240, 180 });
                    }
                }
            } else {
                // Persistent on-desk hint banner for touch users
                Rectangle hintBanner = { SCREEN_WIDTH * 0.5f - 270, 716, 540, 44 };
                DrawRectangleRounded(hintBanner, 0.18f, 4, (Color){ 20, 26, 22, 220 });
                DrawRectangleRoundedLines(hintBanner, 0.18f, 4, (Color){ 215, 175, 60, 230 });
                const char* tHint = "SWIPE ACROSS BIRO TO FLICK!";
                float thw = MeasureSchoolTextTitle(tHint, 15);
                DrawSchoolTextTitle(tHint, hintBanner.x + hintBanner.width * 0.5f - thw * 0.5f, hintBanner.y + 6, 15, (Color){ 255, 230, 140, 255 });
                const char* tSub = "Fast swipe = Momentum  |  Swipe along rear = Spear Flick";
                float tsw = MeasureSchoolText(tSub, 13);
                DrawSchoolText(tSub, hintBanner.x + hintBanner.width * 0.5f - tsw * 0.5f, hintBanner.y + 24, 13, (Color){ 190, 210, 200, 230 });

                if (g_game.swipeMissFeedbackTimer > 0.0f) {
                    Rectangle warnBox = { SCREEN_WIDTH * 0.5f - 230, 665, 460, 34 };
                    DrawRectangleRounded(warnBox, 0.20f, 4, (Color){ 45, 15, 18, 235 });
                    DrawRectangleRoundedLines(warnBox, 0.20f, 4, (Color){ 220, 50, 60, 255 });
                    const char* warnTxt = "MISSED PEN! Swipe to cross the biro barrel or rear.";
                    float ww = MeasureSchoolTextTitle(warnTxt, 13);
                    DrawSchoolTextTitle(warnTxt, warnBox.x + warnBox.width * 0.5f - ww * 0.5f, warnBox.y + 8, 13, (Color){ 255, 190, 190, 255 });
                }
            }
        } else {
            // ================================================================
            // DESKTOP MOUSE RENDERING (Rotating Arrow + 15cm Ruler)
            // ================================================================
            b2Pos hitPtWorld = b2Body_GetWorldPoint(currentBiro->bodyId, g_game.chosenStrikePoint);
            Vector2 hitScr = ScreenFromWorld(hitPtWorld);

            float currentAngle = g_game.isCharging ? g_game.lockedArrowAngle : g_game.arrowAngle;
            Vector2 arrowDir = { cosf(currentAngle), sinf(currentAngle) };

            DrawCircleV(hitScr, 6.0f, (Color){ 22, 60, 160, 255 });
            DrawCircleLines((int)hitScr.x, (int)hitScr.y, 9.0f, (Color){ 255, 235, 175, 220 });

            DrawCircleLines((int)hitScr.x, (int)hitScr.y, 45.0f, (Color){ 255, 235, 175, 45 });
            DrawCircleLines((int)hitScr.x, (int)hitScr.y, 70.0f, (Color){ 255, 235, 175, 30 });

            if (!g_game.isCharging) {
                float arrowLen = 65.0f;
                Vector2 arrowTip = { hitScr.x + arrowDir.x * arrowLen, hitScr.y + arrowDir.y * arrowLen };

                DrawLineEx(hitScr, arrowTip, 4.0f, (Color){ 255, 215, 60, 230 });

                Vector2 headP1 = {
                    arrowTip.x - arrowDir.x * 14.0f + arrowDir.y * 8.0f,
                    arrowTip.y - arrowDir.y * 14.0f - arrowDir.x * 8.0f
                };
                Vector2 headP2 = {
                    arrowTip.x - arrowDir.x * 14.0f - arrowDir.y * 8.0f,
                    arrowTip.y - arrowDir.y * 14.0f + arrowDir.x * 8.0f
                };
                DrawTriangle(arrowTip, headP1, headP2, (Color){ 255, 215, 60, 255 });
                DrawCircleLines((int)hitScr.x, (int)hitScr.y, arrowLen, (Color){ 255, 255, 255, 45 });
            } else {
                float rawPower = sinf(g_game.chargeTimer) * 0.5f + 0.5f;
                float powerFrac = Clamp(rawPower, 0.15f, 1.0f);

                float minLen = 50.0f;
                float maxLen = 140.0f;
                float arrowLen = minLen + powerFrac * (maxLen - minLen);

                Vector2 arrowTip = { hitScr.x + arrowDir.x * arrowLen, hitScr.y + arrowDir.y * arrowLen };

                Color powerCol = (powerFrac < 0.5f) ?
                    ColorLerp((Color){ 30, 160, 80, 255 }, (Color){ 245, 180, 40, 255 }, powerFrac * 2.0f) :
                    ColorLerp((Color){ 245, 180, 40, 255 }, (Color){ 225, 40, 40, 255 }, (powerFrac - 0.5f) * 2.0f);

                DrawLineEx(hitScr, arrowTip, 5.5f, powerCol);

                Vector2 headP1 = {
                    arrowTip.x - arrowDir.x * 16.0f + arrowDir.y * 10.0f,
                    arrowTip.y - arrowDir.y * 16.0f - arrowDir.x * 10.0f
                };
                Vector2 headP2 = {
                    arrowTip.x - arrowDir.x * 16.0f - arrowDir.y * 10.0f,
                    arrowTip.y - arrowDir.y * 16.0f + arrowDir.x * 10.0f
                };
                DrawTriangle(arrowTip, headP1, headP2, powerCol);

                // 15 cm Wooden School Ruler Power Bar
                int pBarW = 340;
                int pBarH = 26;
                int pBarX = SCREEN_WIDTH / 2 - pBarW / 2;
                int pBarY = 724;

                Rectangle rulerRec = { pBarX, pBarY, pBarW, pBarH };
                DrawRectangleRounded(rulerRec, 0.15f, 4, (Color){ 236, 212, 160, 255 });
                DrawRectangleRoundedLines(rulerRec, 0.15f, 4, (Color){ 150, 110, 60, 255 });
                DrawLine(pBarX + 4, pBarY + 3, pBarX + pBarW - 4, pBarY + 3, (Color){ 255, 240, 205, 180 });

                for (int cm = 0; cm <= 15; cm++) {
                    int tx = pBarX + 15 + cm * 20;
                    DrawLine(tx, pBarY + 2, tx, pBarY + 10, (Color){ 90, 55, 25, 230 });
                    if (cm % 3 == 0) {
                        DrawSchoolText(TextFormat("%d", cm), tx - 4, pBarY + 10, 11, (Color){ 85, 50, 20, 220 });
                    }
                    if (cm < 15) {
                        DrawLine(tx + 10, pBarY + 2, tx + 10, pBarY + 6, (Color){ 120, 80, 40, 180 });
                    }
                }

                int fillW = (int)((pBarW - 30) * powerFrac);
                Rectangle inkTrack = { pBarX + 15, pBarY + 17, pBarW - 30, 6 };
                DrawRectangleRounded(inkTrack, 0.5f, 2, (Color){ 180, 150, 105, 160 });
                Rectangle inkFill = { pBarX + 15, pBarY + 17, fillW, 6 };
                DrawRectangleRounded(inkFill, 0.5f, 2, powerCol);

                const char* pTxt = TextFormat("FLICK IMPULSE: %d%%  (RELEASE TO STRIKE!)", (int)(powerFrac * 100.0f));
                float txtW = MeasureSchoolText(pTxt, 15);
                DrawSchoolText(pTxt, SCREEN_WIDTH / 2 - txtW / 2, pBarY - 20, 15, (Color){ 255, 235, 175, 255 });

                // [x CANCEL] button – right of the ruler bar
                Rectangle cancelBtnR = { (float)(pBarX + pBarW + 12), (float)(pBarY - 4), 96.0f, (float)(pBarH + 8) };
                bool hoverCancel = CheckCollisionPointRec(mouse, cancelBtnR);
                DrawRectangleRounded(cancelBtnR, 0.18f, 4, hoverCancel ? (Color){ 195, 34, 42, 255 } : (Color){ 250, 240, 240, 255 });
                DrawRectangleRoundedLines(cancelBtnR, 0.18f, 4, (Color){ 195, 34, 42, 255 });
                DrawSchoolTextTitle("x CANCEL", cancelBtnR.x + 10, cancelBtnR.y + 7, 13, hoverCancel ? WHITE : (Color){ 195, 34, 42, 255 });
            }
        }
    }

    UpdateAndDrawParticles(dt);
    rlPopMatrix();

    // 6. Draw HUD & Telemetry
    DrawHUD();

    if (g_game.showTelemetry) {
        DrawTelemetry();
    }

    // 7. Modals / Overlay Banners
    if (g_game.state == STATE_ROUND_OVER) {
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){ 12, 16, 14, 185 });

        Rectangle modal = { SCREEN_WIDTH * 0.5f - 270, SCREEN_HEIGHT * 0.5f - 118, 540, 236 };
        DrawRectangleRounded(modal, 0.04f, 6, (Color){ 250, 246, 236, 255 });
        DrawRectangleRoundedLines(modal, 0.04f, 6, (Color){ 195, 34, 42, 255 });
        DrawRectangleLines((int)modal.x + 3, (int)modal.y + 3, (int)modal.width - 6, (int)modal.height - 6, (Color){ 195, 34, 42, 180 });

        DrawSchoolTextTitle("RECESS BELL: ROUND FINISHED!", modal.x + 95, modal.y + 18, 22, (Color){ 195, 34, 42, 255 });
        DrawSchoolText(g_game.roundOutcomeMsg, modal.x + 40, modal.y + 58, 16, (Color){ 30, 35, 45, 255 });

        if (g_game.currentGameMode == MODE_1V1) {
            DrawSchoolTextTitle(TextFormat("MATCH SCORE:  BLUE [%d]  -  [%d] RED", g_game.biros[0].score, g_game.biros[1].score),
                                modal.x + 105, modal.y + 98, 18, (Color){ 22, 60, 160, 255 });
        } else if (g_game.currentGameMode == MODE_TEAMS_2V2) {
            DrawSchoolTextTitle(TextFormat("TEAM SCORE:  BLUE [%d]  -  [%d] RED", g_game.teamBlueScore, g_game.teamRedScore),
                                modal.x + 115, modal.y + 98, 18, (Color){ 22, 60, 160, 255 });
        } else {
            DrawSchoolTextTitle(TextFormat("SCORES:  P1:%d  P2:%d  P3:%d  P4:%d",
                                           g_game.biros[0].score, g_game.biros[1].score, g_game.biros[2].score, g_game.biros[3].score),
                                modal.x + 110, modal.y + 98, 18, (Color){ 22, 60, 160, 255 });
        }

        int nextStarter = GetRoundStartingPlayer(g_game.roundNumber + 1, g_game.currentGameMode);
        const char* nextStarterName = g_game.biros[nextStarter].playerName;
        Color pStarterCols[4] = { (Color){ 22, 65, 160, 255 }, (Color){ 210, 35, 45, 255 }, (Color){ 25, 135, 65, 255 }, (Color){ 45, 45, 50, 255 } };

        const char* nextInfo = TextFormat("NEXT ROUND: %s TAKES THE FIRST MOVE!", nextStarterName);
        float ntw = MeasureSchoolTextTitle(nextInfo, 15);
        DrawSchoolTextTitle(nextInfo, modal.x + modal.width * 0.5f - ntw * 0.5f, modal.y + 138, 15, pStarterCols[nextStarter]);

        DrawSchoolText("PRESS [SPACE] OR CLICK TO START NEXT ROUND", modal.x + 105, modal.y + 180, 14, (Color){ 195, 34, 42, 255 });
    } else if (g_game.state == STATE_MATCH_OVER) {
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){ 12, 16, 14, 210 });

        Rectangle modal = { SCREEN_WIDTH * 0.5f - 300, SCREEN_HEIGHT * 0.5f - 130, 600, 260 };
        DrawRectangleRounded(modal, 0.04f, 6, (Color){ 252, 248, 238, 255 });
        DrawRectangleRoundedLines(modal, 0.04f, 6, (Color){ 215, 150, 25, 255 });
        DrawRectangleLines((int)modal.x + 4, (int)modal.y + 4, (int)modal.width - 8, (int)modal.height - 8, (Color){ 215, 150, 25, 180 });

        const char* champText = "";
        Color champCol = (Color){ 22, 60, 160, 255 };
        if (g_game.currentGameMode == MODE_1V1) {
            champText = (g_game.biros[0].score >= 3) ? "BLUE BIRO IS THE CLASS CHAMPION!" : "RED BIRO IS THE CLASS CHAMPION!";
            champCol = (g_game.biros[0].score >= 3) ? (Color){ 22, 60, 160, 255 } : (Color){ 195, 34, 42, 255 };
        } else if (g_game.currentGameMode == MODE_TEAMS_2V2) {
            champText = (g_game.teamBlueScore >= 3) ? "TEAM BLUE (BLUE+GREEN) ARE CLASS CHAMPIONS!" : "TEAM RED (RED+BLACK) ARE CLASS CHAMPIONS!";
            champCol = (g_game.teamBlueScore >= 3) ? (Color){ 22, 60, 160, 255 } : (Color){ 195, 34, 42, 255 };
        } else {
            int champ = 0;
            for (int i = 1; i < 4; i++) {
                if (g_game.biros[i].score > g_game.biros[champ].score) champ = i;
            }
            champText = TextFormat("%s IS THE CLASS CHAMPION!", g_game.biros[champ].playerName);
            champCol = g_game.biros[champ].primaryColor;
        }

        DrawSchoolTextTitle("CLASS TOURNAMENT VICTORY!", modal.x + 130, modal.y + 24, 26, (Color){ 215, 140, 20, 255 });
        DrawSchoolTextTitle(champText, modal.x + 45, modal.y + 75, 20, champCol);

        if (g_game.currentGameMode == MODE_TEAMS_2V2) {
            DrawSchoolTextTitle(TextFormat("FINAL SCORE:  BLUE [%d]  -  [%d] RED", g_game.teamBlueScore, g_game.teamRedScore),
                                modal.x + 140, modal.y + 135, 20, (Color){ 30, 35, 45, 255 });
        } else if (g_game.currentGameMode == MODE_1V1) {
            DrawSchoolTextTitle(TextFormat("FINAL SCORE:  BLUE [%d]  -  [%d] RED", g_game.biros[0].score, g_game.biros[1].score),
                                modal.x + 140, modal.y + 135, 20, (Color){ 30, 35, 45, 255 });
        } else {
            DrawSchoolTextTitle(TextFormat("P1: %d  |  P2: %d  |  P3: %d  |  P4: %d",
                                           g_game.biros[0].score, g_game.biros[1].score, g_game.biros[2].score, g_game.biros[3].score),
                                modal.x + 120, modal.y + 135, 18, (Color){ 30, 35, 45, 255 });
        }

        DrawSchoolText("PRESS [R] OR [SPACE] TO COMMENCE NEW TOURNAMENT", modal.x + 110, modal.y + 200, 16, (Color){ 195, 34, 42, 255 });
    } else if (g_game.state == STATE_PEN_MARKET) {
        DrawPenMarketModal(g_game.biros, &g_game.marketSelectedPlayer, numP, mouse);
    }

    if (g_game.showOnlineModal) {
        DrawOnlineModal(mouse);
    }

    if (g_game.isPausedMenuOpen) {
        DrawPauseMenuModal(mouse);
    }

    if (IsKeyPressed(KEY_F12)) {
        TakeScreenshot("biro_game_screenshot.png");
    }

    EndDrawing();

    g_game.frameCount++;
    if (g_game.autoScreenshotPath != NULL) {
        if (strstr(g_game.autoScreenshotPath, "zero_aim") && g_game.frameCount >= 7) {
            TakeScreenshot(g_game.autoScreenshotPath);
#if !defined(PLATFORM_WEB)
            exit(0);
#endif
        } else if (strstr(g_game.autoScreenshotPath, "market") && g_game.frameCount >= 5) {
            TakeScreenshot(g_game.autoScreenshotPath);
#if !defined(PLATFORM_WEB)
            exit(0);
#endif
        } else if (g_game.frameCount >= 36) {
            TakeScreenshot(g_game.autoScreenshotPath);
#if !defined(PLATFORM_WEB)
            exit(0);
#endif
        }
    }
}

// Emscripten Exported C Functions (Callable from JavaScript for PeerJS WebRTC integration)
EMSCRIPTEN_EXPORT void ApplyRemoteStrike(int playerIndex, float ptX, float ptY, float angle, float powerFrac) {
    g_game.chosenStrikePoint = (b2Vec2){ ptX, ptY };
    g_game.lockedArrowAngle = angle;
    ExecuteFlickStrike(playerIndex, (b2Vec2){ ptX, ptY }, angle, powerFrac);
}

EMSCRIPTEN_EXPORT void ApplyRemoteSync(int playerIndex, float posX, float posY, float angle, int isEliminated, int score) {
    if (playerIndex < 0 || playerIndex >= 4) return;
    Biro* biro = &g_game.biros[playerIndex];
    b2Body_SetTransform(biro->bodyId, (b2Vec2){ posX, posY }, b2MakeRot(angle));
    b2Body_SetLinearVelocity(biro->bodyId, (b2Vec2){ 0.0f, 0.0f });
    b2Body_SetAngularVelocity(biro->bodyId, 0.0f);
    biro->isEliminated = (bool)isEliminated;
    biro->score = score;
    if (biro->isEliminated) {
        b2Body_Disable(biro->bodyId);
    }
}

EMSCRIPTEN_EXPORT void SetMatchMode(int matchType) {
    g_game.matchType = (MatchType)(matchType % 3);
}

EMSCRIPTEN_EXPORT void SetGameMode(int mode) {
    g_game.currentGameMode = (GameMode)(mode % 3);
    ResetMatchScoresAndBiros();
}

EMSCRIPTEN_EXPORT void SetAIDifficulty(int diff) {
    g_game.aiDifficulty = (AIDifficulty)(diff % 4);
}

EMSCRIPTEN_EXPORT void SetOnlineRole(int isHost) {
    g_game.matchType = MATCH_ONLINE_P2P;
    g_game.isOnlineHost = (bool)isHost;
    g_game.onlineLocalPlayerIndex = isHost ? 0 : 1;
}

EMSCRIPTEN_EXPORT void SetOnlineRoomCode(const char* code) {
    if (code) {
        strncpy(g_game.onlineRoomCode, code, sizeof(g_game.onlineRoomCode) - 1);
        g_game.onlineRoomCode[sizeof(g_game.onlineRoomCode) - 1] = '\0';
    }
}

EMSCRIPTEN_EXPORT void SetOnlineConnectionStatus(int connected) {
    g_game.isOnlineConnected = (bool)connected;
}

EMSCRIPTEN_EXPORT void SetTouchControlMode(int enable) {
    g_game.isTouchMode = (bool)enable;
}

EMSCRIPTEN_EXPORT void RestartMatchFromNetwork(int mode, int stage, int table) {
    g_game.currentGameMode = (GameMode)(mode % 3);
    g_game.currentStage = (StageRack)(stage % 3);
    g_game.currentTableType = (TableType)(table % 3);
    SetupTableColliders(g_game.worldId, g_game.currentTableType, &g_game.topBarrierBody, &g_game.bottomBarrierBody);
    ResetAllBirosForModeAndStage(g_game.biros, g_game.currentGameMode, g_game.currentStage);
    ResetTurnForRound(g_game.roundNumber);
}

EMSCRIPTEN_EXPORT int GetGameActivePlayer(void) {
    return g_game.activePlayer;
}

EMSCRIPTEN_EXPORT int GetGameMatchState(void) {
    return (int)g_game.state;
}

EMSCRIPTEN_EXPORT void SetStageAxis(int stage) {
    g_game.currentStage = (StageRack)(stage % 3);
    ResetAllBirosForModeAndStage(g_game.biros, g_game.currentGameMode, g_game.currentStage);
}

EMSCRIPTEN_EXPORT void SetTableType(int table) {
    g_game.currentTableType = (TableType)(table % 3);
    SetupTableColliders(g_game.worldId, g_game.currentTableType, &g_game.topBarrierBody, &g_game.bottomBarrierBody);
}

EMSCRIPTEN_EXPORT void ToggleGamePause(void) {
    g_game.isPausedMenuOpen = !g_game.isPausedMenuOpen;
}

EMSCRIPTEN_EXPORT void SetGamePaused(int paused) {
    g_game.isPausedMenuOpen = (bool)paused;
}

EMSCRIPTEN_EXPORT void RestartCurrentRound(void) {
    ResetAllBirosForModeAndStage(g_game.biros, g_game.currentGameMode, g_game.currentStage);
    ResetTurnForRound(g_game.roundNumber);
}

EMSCRIPTEN_EXPORT void ResetFullMatch(void) {
    ResetMatchScoresAndBiros();
}

// Main Game Application
int main(int argc, char** argv) {
    g_game.currentTableType = TABLE_FRONT_BARRIER;
    g_game.currentGameMode = MODE_1V1;
    g_game.currentStage = STAGE_VERTICAL;
    g_game.matchType = MATCH_SINGLE_PLAYER_AI;
    g_game.aiDifficulty = AI_DESKMATE;
    g_game.onlineLocalPlayerIndex = 0;
    g_game.isOnlineHost = true;
    strcpy(g_game.onlineRoomCode, "BIRO1");
    g_game.turnTimer = TURN_TIME_LIMIT;
    g_game.chosenStrikePoint = (b2Vec2){ -0.85f, 0.0f };

    int customP1Model = -1;
    int customP2Model = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            g_game.autoScreenshotPath = argv[++i];
        } else if (strcmp(argv[i], "--autotest") == 0) {
            g_game.autotestMode = true;
        } else if (strcmp(argv[i], "--arena") == 0 && i + 1 < argc) {
            g_game.currentTableType = (TableType)(atoi(argv[++i]) % 3);
        } else if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            g_game.currentGameMode = (GameMode)(atoi(argv[++i]) % 3);
        } else if (strcmp(argv[i], "--stage") == 0 && i + 1 < argc) {
            g_game.currentStage = (StageRack)(atoi(argv[++i]) % 3);
        } else if (strcmp(argv[i], "--match") == 0 && i + 1 < argc) {
            g_game.matchType = (MatchType)(atoi(argv[++i]) % 3);
        } else if (strcmp(argv[i], "--ai-diff") == 0 && i + 1 < argc) {
            g_game.aiDifficulty = (AIDifficulty)(atoi(argv[++i]) % 4);
        } else if (strcmp(argv[i], "--pause") == 0) {
            g_game.isPausedMenuOpen = true;
        } else if (strcmp(argv[i], "--market") == 0) {
            g_game.startInMarket = true;
        } else if (strcmp(argv[i], "--round") == 0 && i + 1 < argc) {
            g_game.roundNumber = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--p1-model") == 0 && i + 1 < argc) {
            customP1Model = atoi(argv[++i]) % NUM_PEN_MODELS;
        } else if (strcmp(argv[i], "--p2-model") == 0 && i + 1 < argc) {
            customP2Model = atoi(argv[++i]) % NUM_PEN_MODELS;
        }
    }

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "BIRO CLASH - School Desk Physics");
    SetTargetFPS(60);

    InitGameAudio();
    InitGameFonts();

    // Box2D World Initialization
    b2WorldDef worldDef = b2DefaultWorldDef();
    worldDef.gravity = (b2Vec2){ 0.0f, 0.0f };
    g_game.worldId = b2CreateWorld(&worldDef);

    // School Desk Bounds
    g_game.deskRect = (Rectangle){
        TABLE_CENTER_X - (TABLE_WIDTH_M * 0.5f * PIXELS_PER_METER),
        TABLE_CENTER_Y - (TABLE_HEIGHT_M * 0.5f * PIXELS_PER_METER),
        TABLE_WIDTH_M * PIXELS_PER_METER,
        TABLE_HEIGHT_M * PIXELS_PER_METER
    };

    // Table Arena Barricades
    g_game.topBarrierBody = b2_nullBodyId;
    g_game.bottomBarrierBody = b2_nullBodyId;
    SetupTableColliders(g_game.worldId, g_game.currentTableType, &g_game.topBarrierBody, &g_game.bottomBarrierBody);

    // Create 4 Biro Pens
    CreateBiro(g_game.worldId, &g_game.biros[0], 0, (b2Vec2){ -3.2f, -1.25f }, 0.0f);
    CreateBiro(g_game.worldId, &g_game.biros[1], 1, (b2Vec2){  3.2f, -1.25f }, 3.14159265f);
    CreateBiro(g_game.worldId, &g_game.biros[2], 2, (b2Vec2){ -3.2f,  1.25f }, 0.0f);
    CreateBiro(g_game.worldId, &g_game.biros[3], 3, (b2Vec2){  3.2f,  1.25f }, 3.14159265f);

    if (customP1Model >= 0) EquipPenModel(&g_game.biros[0], (PenModelId)customP1Model);
    if (customP2Model >= 0) EquipPenModel(&g_game.biros[1], (PenModelId)customP2Model);

    ResetAllBirosForModeAndStage(g_game.biros, g_game.currentGameMode, g_game.currentStage);

    if (g_game.roundNumber <= 0) g_game.roundNumber = 1;
    ResetTurnForRound(g_game.roundNumber);
    if (g_game.startInMarket) g_game.state = STATE_PEN_MARKET;
    g_game.roundOutcomeMsg = "";

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
    while (!WindowShouldClose()) {
        UpdateDrawFrame();
    }

    // Cleanup
    b2DestroyWorld(g_game.worldId);
    CloseGameAudio();
    CloseGameFonts();
    CloseWindow();
#endif

    return 0;
}
