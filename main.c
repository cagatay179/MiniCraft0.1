/*
 * MiniCraft PSP  -  PSP / PSP Go icin Minecraft tarzi voxel oyunu (PSPSDK / pspgu)
 *
 * KONTROLLER
 *   Analog              : yuru
 *   Ucgen/X/Kare/Daire  : bakis (yukari / asagi / sola / saga)
 *   START               : zipla (suda yuz). Cift basinca ucus (yaratici modda)
 *   SELECT x2 (cift tik): menu ac / kapat
 *   SELECT basili + KARE: eldeki esyayi at
 *   SELECT basili + UCGEN: envanter ac / kapat
 *   L                   : kir / saldir (basili tut)
 *   R                   : blok koy / yemek ye (basili tut)
 *   D-Pad Sol/Sag       : hotbar sec        D-Pad Yukari: kosu
 *   D-Pad Asagi         : comel (kenardan dusmez) / ucarken alcal
 *   Envanterde SELECT + KARE : imlecin ustundeki esyayi at
 *   Hayatta kalma: agac kir -> tahta -> cubuk -> kazma/balta/kurek/kilic (envanter > URETIM)
 *   Cakmak (demir kulce + cakmak tasi): R ile TNT yak / ates cikar. Ates tahtayi ve yapragi yakar.
 *   Calisma masasi (4 tahta) ve ocak (8 kirma tas) yakininda uretim / eritme yapilir.
 *   Dokular program acilirken kodla uretilir (16x16 piksel sanati, ek dosya gerekmez)
 */
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspiofilemgr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

PSP_MODULE_INFO("MiniCraft", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(16 * 1024);

#define SCR_W 480
#define SCR_H 272
#define BUF_W 512
#define WX 128
#define WY 64
#define WZ 128
#define CS 16
#define NCX (WX / CS)
#define NCZ (WZ / CS)
#define SEA 24
#define PI 3.14159265f
#define RGBA(r,g,b,a) ((u32)(((a)<<24)|((b)<<16)|((g)<<8)|(r)))
#define IDX(x,y,z) ((y)*WX*WZ + (z)*WX + (x))
#define VT3 (GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_3D)

/* ---------------- callbacks ---------------- */
static int exit_callback(int a, int b, void *c) { sceKernelExitGame(); return 0; }
static int callback_thread(SceSize args, void *argp) {
    int id = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(id);
    sceKernelSleepThreadCB();
    return 0;
}
static void setup_callbacks(void) {
    int t = sceKernelCreateThread("update_thread", callback_thread, 0x11, 0xFA0, 0, 0);
    if (t >= 0) sceKernelStartThread(t, 0, 0);
}

/* ---------------- GU ---------------- */
static unsigned int __attribute__((aligned(16))) list[262144];

static void init_gu(void) {
    sceGuInit();
    sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(GU_PSM_8888, (void *)0, BUF_W);
    sceGuDispBuffer(SCR_W, SCR_H, (void *)0x88000, BUF_W);
    sceGuDepthBuffer((void *)0x110000, BUF_W);
    sceGuOffset(2048 - (SCR_W / 2), 2048 - (SCR_H / 2));
    sceGuViewport(2048, 2048, SCR_W, SCR_H);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, SCR_W, SCR_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuFrontFace(GU_CW);
    sceGuShadeModel(GU_SMOOTH);
    sceGuEnable(GU_CLIP_PLANES);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

/* ---------------- 2D cizim + yazi tipi (3x5) ---------------- */
typedef struct { u32 c; short x, y, z, pad; } V2;
typedef struct { float u, v; u32 c; float x, y, z; } V3;
typedef struct { float u, v; u32 c; short x, y, z, pad; } V2T;
#define MAXR 6000
static V2 rb[MAXR * 2];
static int rn;

static void flush2d(void) {
    if (!rn) return;
    V2 *m = sceGuGetMemory(rn * 2 * sizeof(V2));
    memcpy(m, rb, rn * 2 * sizeof(V2));
    sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, rn * 2, 0, m);
    rn = 0;
}
static void rect(int x, int y, int w, int h, u32 c) {
    if (rn >= MAXR) flush2d();
    V2 *a = &rb[rn * 2];
    a[0].c = c; a[0].x = x;     a[0].y = y;     a[0].z = 0; a[0].pad = 0;
    a[1].c = c; a[1].x = x + w; a[1].y = y + h; a[1].z = 0; a[1].pad = 0;
    rn++;
}

#define G(a,b,c,d,e) (((a)<<12)|((b)<<9)|((c)<<6)|((d)<<3)|(e))
static const char CHS[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ.:-+/!?%,()<>=";
static const unsigned short GL[50] = {
    G(7,5,5,5,7),G(2,6,2,2,7),G(7,1,7,4,7),G(7,1,7,1,7),G(5,5,7,1,1),
    G(7,4,7,1,7),G(7,4,7,5,7),G(7,1,1,1,1),G(7,5,7,5,7),G(7,5,7,1,7),
    G(2,5,7,5,5),G(6,5,6,5,6),G(7,4,4,4,7),G(6,5,5,5,6),G(7,4,6,4,7),
    G(7,4,6,4,4),G(7,4,5,5,7),G(5,5,7,5,5),G(7,2,2,2,7),G(1,1,1,5,2),
    G(5,5,6,5,5),G(4,4,4,4,7),G(5,7,7,5,5),G(7,5,5,5,5),G(2,5,5,5,2),
    G(6,5,6,4,4),G(2,5,5,7,3),G(6,5,6,5,5),G(3,4,2,1,6),G(7,2,2,2,2),
    G(5,5,5,5,7),G(5,5,5,5,2),G(5,5,7,7,5),G(5,5,2,5,5),G(5,5,2,2,2),
    G(7,1,2,4,7),
    G(0,0,0,0,2),G(0,2,0,2,0),G(0,0,7,0,0),G(0,2,7,2,0),G(1,1,2,4,4),
    G(2,2,2,0,2),G(6,1,2,0,2),G(5,1,2,4,5),G(0,0,0,2,4),G(1,2,2,2,1),
    G(4,2,2,2,4),G(1,2,4,2,1),G(4,2,1,2,4),G(0,7,0,7,0)
};
static void text(int x, int y, int s, u32 col, const char *t) {
    for (; *t; t++) {
        char c = *t;
        if (c >= 'a' && c <= 'z') c -= 32;
        const char *p = c ? strchr(CHS, c) : NULL;
        if (p) {
            unsigned short g = GL[p - CHS];
            for (int r = 0; r < 5; r++)
                for (int k = 0; k < 3; k++)
                    if ((g >> (14 - (r * 3 + k))) & 1) rect(x + k * s, y + r * s, s, s, col);
        }
        x += 4 * s;
    }
}
static void ptext(int x, int y, int s, u32 col, const char *t) {
    text(x + 1, y + 1, s, RGBA(0,0,0,200), t);
    text(x, y, s, col, t);
}
static int twidth(const char *t, int s) { return (int)strlen(t) * 4 * s; }
static void ctext(int cx, int y, int s, u32 col, const char *t) { ptext(cx - twidth(t, s) / 2, y, s, col, t); }

/* ---------------- yardimcilar ---------------- */
static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static u32 seed, rngs = 12345;
static u32 rnd(void) { rngs = rngs * 1664525u + 1013904223u; return rngs >> 8; }
static float rndf(void) { return (rnd() & 0xFFFF) / 65536.0f; }
static u32 hsh(int x, int y, int z) {
    u32 h = (u32)x * 374761393u + (u32)y * 668265263u + (u32)z * 2147483629u + seed * 1274126177u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}
static float hf(int x, int y, int z) { return (hsh(x, y, z) & 0xFFFF) / 65535.0f; }
static float sm(float t) { return t * t * (3 - 2 * t); }
static float lerp(float a, float b, float t) { return a + (b - a) * t; }
static float vn2(float x, float z) {
    int ix = (int)floorf(x), iz = (int)floorf(z);
    float fx = sm(x - ix), fz = sm(z - iz);
    return lerp(lerp(hf(ix,0,iz), hf(ix+1,0,iz), fx), lerp(hf(ix,0,iz+1), hf(ix+1,0,iz+1), fx), fz);
}
static float fbm2(float x, float z) {
    float s = 0, a = 0.5f, n = 0;
    for (int i = 0; i < 4; i++) { s += vn2(x, z) * a; n += a; a *= 0.5f; x *= 2; z *= 2; }
    return s / n;
}
static float vn3(float x, float y, float z) {
    int ix = (int)floorf(x), iy = (int)floorf(y), iz = (int)floorf(z);
    float fx = sm(x - ix), fy = sm(y - iy), fz = sm(z - iz);
    float c00 = lerp(hf(ix,iy,iz),     hf(ix+1,iy,iz),     fx);
    float c10 = lerp(hf(ix,iy+1,iz),   hf(ix+1,iy+1,iz),   fx);
    float c01 = lerp(hf(ix,iy,iz+1),   hf(ix+1,iy,iz+1),   fx);
    float c11 = lerp(hf(ix,iy+1,iz+1), hf(ix+1,iy+1,iz+1), fx);
    return lerp(lerp(c00, c10, fy), lerp(c01, c11, fy), fz);
}

/* ---------------- bloklar ---------------- */
enum { AIR, GRASS, DIRT, STONE, COBBLE, SAND, GRAVEL, LOG, LEAVES, PLANKS, WATER, BEDROCK,
       COAL, IRON, GOLD, DIAMOND, GLASS, BRICK, SNOW, TNT, GLOW, BOOKS, WORKBENCH, FURNACE, FIRE, PORK, COOKED, STICK,
       FLINT, INGOT_I, INGOT_G, POWDER, APPLE, FSTEEL, TOOL0, TOOL_LAST = TOOL0 + 15, NB };
#define TOOL(k,t) (TOOL0 + (k) * 4 + (t))
#define C1(r,g,b) {r,g,b,r,g,b,r,g,b}
static const struct { const char *n; u8 c[9]; float h; } BI[NB] = {
    {"HAVA", C1(0,0,0), 0},
    {"CIM", {98,160,60, 125,100,60, 121,85,55}, 0.6f},
    {"TOPRAK", C1(121,85,55), 0.5f},
    {"TAS", C1(125,125,125), 1.8f},
    {"KIRMA TAS", C1(100,100,100), 2.0f},
    {"KUM", C1(219,207,142), 0.5f},
    {"CAKIL", C1(136,126,126), 0.6f},
    {"ODUN", {160,130,80, 100,78,45, 160,130,80}, 1.5f},
    {"YAPRAK", C1(50,125,35), 0.2f},
    {"TAHTA", C1(170,135,80), 1.2f},
    {"SU", C1(50,90,220), 0},
    {"KAYA DIBI", C1(40,40,40), -1},
    {"KOMUR", C1(60,60,60), 2.5f},
    {"DEMIR CEVHERI", C1(205,165,135), 2.5f},
    {"ALTIN CEVHERI", C1(245,212,60), 2.8f},
    {"ELMAS", C1(80,230,220), 3.0f},
    {"CAM", C1(200,230,240), 0.3f},
    {"TUGLA", C1(150,70,55), 2.0f},
    {"KAR", C1(245,248,252), 0.3f},
    {"TNT", {200,60,50, 215,85,70, 200,60,50}, 0.0f},
    {"PARLAKTAS", C1(255,220,120), 0.5f},
    {"KITAPLIK", {170,135,80, 110,80,50, 170,135,80}, 1.2f},
    {"CALISMA MASASI", {170,135,80, 150,110,60, 170,135,80}, 1.5f},
    {"OCAK", C1(110,110,112), 2.5f},
    {"ATES", C1(255,150,30), 0},
    {"CIG ET", C1(235,140,140), 0},
    {"PISMIS ET", C1(150,85,45), 0},
    {"CUBUK", C1(126,92,48), 0},
    {"CAKMAK TASI", C1(80,82,90), 0},
    {"DEMIR KULCE", C1(200,200,208), 0},
    {"ALTIN KULCE", C1(240,200,60), 0},
    {"BARUT", C1(110,110,114), 0},
    {"ELMA", C1(205,35,35), 0},
    {"CAKMAK", C1(150,152,160), 0},
    {"TAHTA KAZMA", C1(160,122,66), 0},
    {"TAS KAZMA", C1(130,130,130), 0},
    {"DEMIR KAZMA", C1(224,224,228), 0},
    {"ELMAS KAZMA", C1(90,230,222), 0},
    {"TAHTA BALTA", C1(160,122,66), 0},
    {"TAS BALTA", C1(130,130,130), 0},
    {"DEMIR BALTA", C1(224,224,228), 0},
    {"ELMAS BALTA", C1(90,230,222), 0},
    {"TAHTA KUREK", C1(160,122,66), 0},
    {"TAS KUREK", C1(130,130,130), 0},
    {"DEMIR KUREK", C1(224,224,228), 0},
    {"ELMAS KUREK", C1(90,230,222), 0},
    {"TAHTA KILIC", C1(160,122,66), 0},
    {"TAS KILIC", C1(130,130,130), 0},
    {"DEMIR KILIC", C1(224,224,228), 0},
    {"ELMAS KILIC", C1(90,230,222), 0}
};

static u8 world[WX * WY * WZ];
static signed char topy[WX * WZ];
static u8 hmap[WX * WZ];

static int inb(int x, int y, int z) { return x >= 0 && x < WX && y >= 0 && y < WY && z >= 0 && z < WZ; }
static u8 gb(int x, int y, int z) {
    if (y >= WY) return AIR;
    if (!inb(x, y, z)) return BEDROCK;
    return world[IDX(x, y, z)];
}
static int solid_b(u8 b) { return b != AIR && b != WATER && b != FIRE; }
static int blocks_light(u8 b) { return b != AIR && b != WATER && b != GLASS && b != FIRE; }

/* ---------------- dokular: kodla uretilen 16x16 piksel sanati (128x128 atlas) ---------------- */
#define ATW 128
#define ATH 256
static u32 __attribute__((aligned(16))) atlas[ATW * ATH];
enum { T_GRASS_T, T_GRASS_S, T_DIRT, T_STONE, T_COBBLE, T_SAND, T_GRAVEL, T_LOG_S, T_LOG_T, T_LEAVES,
       T_PLANKS, T_WATER, T_BEDROCK, T_COAL, T_IRON, T_GOLD, T_DIAM, T_GLASS, T_BRICK, T_SNOW,
       T_TNT_S, T_TNT_T, T_TNT_B, T_GLOW, T_BOOKS, T_PORK, T_COOKED, T_STICK, T_CRACK,
       T_TOOL = 38, T_BENCH_T = 54, T_BENCH_S, T_FURN_T, T_FURN_S, T_FIRE, T_FLINT, T_INGOT_I, T_INGOT_G,
       T_POWDER, T_APPLE, T_FSTEEL, T_END };

/* blok -> {ust, yan, alt} doku */
static const u8 TILE[NB][3] = {
    {0,0,0},
    {T_GRASS_T, T_GRASS_S, T_DIRT}, {T_DIRT,T_DIRT,T_DIRT}, {T_STONE,T_STONE,T_STONE}, {T_COBBLE,T_COBBLE,T_COBBLE},
    {T_SAND,T_SAND,T_SAND}, {T_GRAVEL,T_GRAVEL,T_GRAVEL}, {T_LOG_T, T_LOG_S, T_LOG_T}, {T_LEAVES,T_LEAVES,T_LEAVES},
    {T_PLANKS,T_PLANKS,T_PLANKS}, {T_WATER,T_WATER,T_WATER}, {T_BEDROCK,T_BEDROCK,T_BEDROCK},
    {T_COAL,T_COAL,T_COAL}, {T_IRON,T_IRON,T_IRON}, {T_GOLD,T_GOLD,T_GOLD}, {T_DIAM,T_DIAM,T_DIAM},
    {T_GLASS,T_GLASS,T_GLASS}, {T_BRICK,T_BRICK,T_BRICK}, {T_SNOW,T_SNOW,T_SNOW},
    {T_TNT_T, T_TNT_S, T_TNT_B}, {T_GLOW,T_GLOW,T_GLOW}, {T_PLANKS, T_BOOKS, T_PLANKS},
    {T_BENCH_T, T_BENCH_S, T_PLANKS}, {T_FURN_T, T_FURN_S, T_FURN_T}, {T_FIRE, T_FIRE, T_FIRE}
};
static int item_tile(u8 id) {
    switch (id) {
    case PORK: return T_PORK;      case COOKED: return T_COOKED;  case STICK: return T_STICK;
    case FLINT: return T_FLINT;    case INGOT_I: return T_INGOT_I; case INGOT_G: return T_INGOT_G;
    case POWDER: return T_POWDER;  case APPLE: return T_APPLE;    case FSTEEL: return T_FSTEEL;
    }
    return T_TOOL + (id - TOOL0);
}
static void tile_uv(int t, float u, float v, float *U, float *V) {
    *U = ((t & 7) * 16 + 0.04f + u * 15.92f) * (1.0f / ATW);
    *V = ((t >> 3) * 16 + 0.04f + v * 15.92f) * (1.0f / ATH);
}
static void face_uvp(int f, const int *p, int tile, float *U, float *V) {
    float u, v;
    switch (f) {
    case 0: u = 1 - p[2]; v = 1 - p[1]; break;
    case 1: u = p[2];     v = 1 - p[1]; break;
    case 2: case 3: u = p[0]; v = p[2]; break;
    case 4: u = p[0];     v = 1 - p[1]; break;
    default: u = 1 - p[0]; v = 1 - p[1]; break;
    }
    tile_uv(tile, u, v, U, V);
}

static float tz(int t, int x, int y) {
    u32 h = (u32)(t * 7919 + 13) * 374761393u + (u32)(x + 100) * 668265263u + (u32)(y + 100) * 2147483629u;
    h = (h ^ (h >> 13)) * 1274126177u; h ^= h >> 16;
    return (h & 0xFFFF) / 65535.0f;
}
static float tvn(int t, float x, float y) {
    x *= 0.25f; y *= 0.25f;
    int ix = (int)floorf(x), iy = (int)floorf(y);
    float fx = sm(x - ix), fy = sm(y - iy);
    return lerp(lerp(tz(t, ix, iy), tz(t, ix + 1, iy), fx), lerp(tz(t, ix, iy + 1), tz(t, ix + 1, iy + 1), fx), fy);
}
static u32 col3(int r, int g, int b, float m, int a) {
    int R = (int)(r * m), G = (int)(g * m), B = (int)(b * m);
    R = R < 0 ? 0 : (R > 255 ? 255 : R);
    G = G < 0 ? 0 : (G > 255 ? 255 : G);
    B = B < 0 ? 0 : (B > 255 ? 255 : B);
    return RGBA(R, G, B, a);
}
static u32 stone_px(int t, int x, int y) {
    float m = 0.74f + tz(t, x, y) * 0.2f + tvn(t + 50, x, y) * 0.3f;
    return col3(128, 128, 128, m, 255);
}
static u32 tool_px(int kind, int tier, int x, int y) {
    static const u8 TC[4][3] = {{160,122,66}, {130,130,130}, {224,224,228}, {90,230,222}};
    float px = x + 0.5f, py = y + 0.5f;
    float u = (px - py + 12) * 0.7071f, w = (px + py - 16) * 0.7071f, aw = fabsf(w);
    int head = 0, hand = 0;
    switch (kind) {
    case 0: {
        float dx = px - 6, dy = py - 10, r = sqrtf(dx * dx + dy * dy), uu = (dx - dy) * 0.7071f;
        head = r >= 5.0f && r <= 7.3f && uu >= 0.45f * r;
        hand = aw < 0.75f && u >= 0.5f && u <= 12.5f; break; }
    case 1:
        head = u >= 7.4f && u <= 12.4f && w >= -0.9f && w <= 1.8f + 3.2f * sinf(PI * (u - 7.4f) / 5.0f);
        hand = aw < 0.75f && u >= 0.5f && u <= 12.8f; break;
    case 2: {
        float a = (u - 12.0f) / 3.4f, b = w / 2.3f;
        head = a * a + b * b <= 1.0f;
        hand = aw < 0.75f && u >= 0.5f && u <= 10.0f; break; }
    case 3:
        head = u >= 5.5f && u <= 15.6f && aw <= 1.15f * (1 - (u - 5.5f) / 12.0f) + 0.15f;
        hand = (aw < 0.75f && u >= 0.8f && u < 4.4f) || (u >= 4.4f && u <= 5.5f && aw <= 3.0f); break;
    default:
        hand = aw < 0.75f && u >= 1.5f && u <= 13.5f; break;
    }
    float n = tz(40 + kind, x, y);
    if (head) return col3(TC[tier][0], TC[tier][1], TC[tier][2], 0.85f + n * 0.2f + (w > 0.5f ? 0.1f : (w < -0.5f ? -0.15f : 0)), 255);
    if (hand) return col3(126, 92, 48, 0.85f + n * 0.25f, 255);
    return 0;
}
static u32 cobble_px(int t, int x, int y) {
    float n = tz(t, x, y);
    int row = y / 5, off = (row & 1) * 2, cx = (x + off) / 5;
    if (y % 5 == 4 || (x + off) % 5 == 4) return col3(70, 70, 70, 0.9f + n * 0.2f, 255);
    return col3(125, 125, 125, 0.72f + tz(t, cx, row) * 0.45f + n * 0.12f, 255);
}
static u32 tile_px(int t, int x, int y) {
    float n = tz(t, x, y), n2 = tz(t + 70, x / 2, y / 2), bl = tvn(t + 30, x, y);
    switch (t) {
    case T_GRASS_T: return col3(92, 150, 52, 0.72f + n * 0.25f + bl * 0.3f, 255);
    case T_GRASS_S: {
        int edge = 3 + (tz(t, x, 99) > 0.55f ? 1 : 0) + (tz(t, x, 98) > 0.8f ? 1 : 0);
        if (y < edge) return col3(92, 150, 52, 0.72f + n * 0.3f, 255);
        return col3(121, 86, 58, 0.76f + n * 0.3f + bl * 0.15f, 255); }
    case T_DIRT: {
        float m = 0.76f + n * 0.28f + bl * 0.2f;
        if (n2 > 0.93f) return col3(140, 140, 140, m, 255);
        return col3(121, 86, 58, m, 255); }
    case T_STONE: return stone_px(t, x, y);
    case T_COBBLE: {
        int row = y / 5, off = (row & 1) * 2, cx = (x + off) / 5;
        if (y % 5 == 4 || (x + off) % 5 == 4) return col3(70, 70, 70, 0.9f + n * 0.2f, 255);
        return col3(125, 125, 125, 0.72f + tz(t, cx, row) * 0.45f + n * 0.12f, 255); }
    case T_SAND: return col3(222, 208, 144, 0.85f + n * 0.14f + bl * 0.1f, 255);
    case T_GRAVEL: return col3(138, 128, 126, 0.62f + n2 * 0.5f + n * 0.25f, 255);
    case T_LOG_S: return col3(104, 80, 48, 0.6f + tz(t, x, 0) * 0.5f + n * 0.14f, 255);
    case T_LOG_T: {
        if (x < 1 || x > 14 || y < 1 || y > 14) return col3(104, 80, 48, 0.7f + n * 0.4f, 255);
        float dx = x - 7.5f, dy = y - 7.5f, d = sqrtf(dx * dx + dy * dy);
        int ring = (int)(d * 0.9f) & 1;
        return ring ? col3(175, 140, 88, 0.9f + n * 0.15f, 255) : col3(148, 114, 68, 0.9f + n * 0.15f, 255); }
    case T_LEAVES: {
        float m = 0.45f + n * 0.7f + bl * 0.3f;
        if (n2 > 0.86f) m *= 0.45f;
        return col3(52, 128, 34, m, 255); }
    case T_PLANKS: {
        int row = y / 4, jx = (row & 1) ? 5 : 11;
        if (y % 4 == 3 || x == jx) return col3(110, 82, 44, 0.9f + n * 0.15f, 255);
        return col3(172, 136, 82, 0.86f + tz(t, x / 4, row) * 0.22f + n * 0.1f, 255); }
    case T_WATER: {
        float w = sinf(x * 0.9f + y * 0.45f) * 0.5f + sinf(y * 1.1f - x * 0.3f) * 0.5f;
        return col3(46, 90, 215, 0.88f + w * 0.1f + n * 0.06f, 255); }
    case T_BEDROCK: {
        float m = 0.35f + n * 0.9f;
        if (n2 > 0.6f) m += 0.25f;
        return col3(85, 85, 85, m, 255); }
    case T_COAL: case T_IRON: case T_GOLD: case T_DIAM: {
        static const u8 OC[4][3] = {{28,28,30}, {214,172,142}, {245,210,60}, {86,228,220}};
        int k = t - T_COAL, hit = 0;
        for (int i = 0; i < 4; i++) {
            int cx = 2 + (int)(tz(t, i, 200) * 12), cy = 2 + (int)(tz(t, i, 201) * 12);
            int dx = x - cx, dy = y - cy;
            if (dx * dx + dy * dy <= 4 + (i & 1) && tz(t + 1, x, y) > 0.08f) hit = 1;
        }
        if (hit) return col3(OC[k][0], OC[k][1], OC[k][2], 0.85f + n * 0.3f, 255);
        return stone_px(t, x, y); }
    case T_GLASS:
        if (x == 0 || y == 0 || x == 15 || y == 15) return RGBA(215, 238, 248, 255);
        if ((x == y && x > 1 && x < 6) || (x == y + 1 && x > 2 && x < 6)) return RGBA(255, 255, 255, 170);
        return RGBA(200, 230, 240, 60);
    case T_BRICK: {
        int row = y / 4, off = (row & 1) * 4, cx = (x + off) / 8;
        if (y % 4 == 3 || (x + off) % 8 == 7) return col3(185, 180, 170, 0.9f + n * 0.15f, 255);
        return col3(150, 70, 55, 0.76f + tz(t, cx, row) * 0.3f + n * 0.12f, 255); }
    case T_SNOW: return col3(246, 249, 253, 0.96f + n * 0.04f - (n2 > 0.93f ? 0.06f : 0), 255);
    case T_TNT_S: {
        if (y >= 4 && y <= 10) {
            int gx = x - 2, gy = y - 5;
            if (gy >= 0 && gy < 5 && gx >= 0 && gx < 11 && (gx % 4) < 3) {
                static const int L3[3] = {29, 23, 29}; /* T N T */
                if ((GL[L3[gx / 4]] >> (14 - (gy * 3 + gx % 4))) & 1) return RGBA(40, 40, 40, 255);
            }
            return col3(228, 222, 206, 0.92f + n * 0.12f, 255);
        }
        if (y == 3 || y == 11) return col3(120, 30, 25, 1.0f, 255);
        return col3(204, 54, 44, 0.85f + n * 0.2f, 255); }
    case T_TNT_T:
        if (x >= 5 && x <= 10 && y >= 5 && y <= 10) return col3(70, 70, 70, 0.8f + n * 0.4f, 255);
        return col3(204, 54, 44, 0.85f + n * 0.2f, 255);
    case T_TNT_B: return col3(204, 54, 44, 0.7f + n * 0.2f, 255);
    case T_GLOW: return col3(255, 200, 100, 0.55f + n2 * 0.7f + n * 0.2f, 255);
    case T_BOOKS: {
        static const u8 PB[5][3] = {{150,40,40}, {40,90,150}, {50,130,60}, {140,100,40}, {110,60,130}};
        if (y == 0 || y == 7 || y == 8 || y == 15) return col3(172, 136, 82, 0.85f + n * 0.2f, 255);
        int shelf = y < 8 ? 0 : 1, pi = (int)(tz(t, x / 2 + shelf * 5, 5) * 5) % 5;
        float m = 0.8f + (((y + shelf) & 3) == 1 ? 0.2f : 0.0f) + n * 0.12f - (x & 1) * 0.12f;
        return col3(PB[pi][0], PB[pi][1], PB[pi][2], m, 255); }
    case T_PORK: case T_COOKED: {
        float dx = (x - 7.5f) / 6.2f, dy = (y - 8.0f) / 4.6f, d = dx * dx + dy * dy;
        int cook = t == T_COOKED;
        if (d > 1.0f) return 0;
        if (d > 0.72f) return cook ? col3(110, 60, 30, 1.0f, 255) : col3(200, 100, 105, 1.0f, 255);
        if (cook) return col3(160, 92, 50, (((x + y) & 3) == 0 ? 0.78f : 1.0f) * (0.9f + n * 0.2f), 255);
        if (d > 0.45f && (x + y) % 5 == 0) return col3(250, 235, 235, 1.0f, 255);
        return col3(236, 142, 146, 0.9f + n * 0.2f, 255); }
    case T_BENCH_T:
        if (x == 0 || y == 0 || x == 15 || y == 15 || x == 7 || x == 8 || y == 7 || y == 8) return col3(96, 66, 34, 0.9f + n * 0.2f, 255);
        return col3(172, 136, 82, 0.86f + n * 0.2f, 255);
    case T_BENCH_S:
        if (y < 3 || y > 12 || x == 0 || x == 15) return col3(96, 66, 34, 0.9f + n * 0.2f, 255);
        if (x >= 3 && x <= 7 && y >= 5 && y <= 6) return col3(190, 190, 200, 1.0f, 255);
        if (x >= 3 && x <= 4 && y >= 7 && y <= 10) return col3(120, 80, 40, 1.0f, 255);
        if (x >= 9 && x <= 13 && y >= 5 && y <= 6) return col3(120, 120, 130, 1.0f, 255);
        if (x == 11 && y >= 7 && y <= 11) return col3(120, 80, 40, 1.0f, 255);
        return col3(172, 136, 82, 0.86f + n * 0.2f, 255);
    case T_FURN_T:
        if (x >= 4 && x <= 11 && y >= 4 && y <= 11) return col3(35, 35, 38, 0.9f + n * 0.3f, 255);
        return cobble_px(t, x, y);
    case T_FURN_S:
        if (x >= 3 && x <= 12 && y >= 8 && y <= 13) {
            if (y == 13) return col3(200, 90, 30, 0.8f + n * 0.5f, 255);
            if (y == 10 && (x & 1)) return col3(60, 60, 64, 1.0f, 255);
            return col3(24, 24, 26, 0.9f + n * 0.3f, 255);
        }
        if (x >= 4 && x <= 11 && y >= 3 && y <= 4) return col3(30, 30, 32, 1.0f, 255);
        return cobble_px(t, x, y);
    case T_FIRE: {
        float hgt = (6.0f + tz(t, x, 7) * 8.0f) * (1.0f - fabsf(x - 7.5f) / 11.0f);
        int dy = 15 - y;
        if (dy < hgt - tz(t, x, y) * 2.5f) {
            if (dy < hgt * 0.3f) return RGBA(255, 244, 150, 240);
            if (dy < hgt * 0.65f) return RGBA(255, 170, 40, 235);
            return RGBA(230, 70, 20, 225);
        }
        return 0; }
    case T_FLINT: {
        float dx = x - 7.5f, dy = y - 8.5f;
        if (fabsf(dx) * 0.85f + fabsf(dy) * 0.7f > 5.2f - n * 0.8f) return 0;
        float m = 0.8f + n * 0.3f;
        if (dx + dy < -2.5f) m += 0.35f;
        if (dx + dy > 3.5f) m -= 0.25f;
        return col3(104, 108, 120, m, 255); }
    case T_INGOT_I: case T_INGOT_G: {
        int g = t == T_INGOT_G;
        int top = (y >= 5 && y <= 7 && x >= 4 + (7 - y) && x < 12 + (7 - y));
        int front = (y >= 8 && y <= 10 && x >= 4 && x < 12);
        int side = (x >= 12 && x < 14 && y >= 6 && y <= 10);
        if (!top && !front && !side) return 0;
        float m = 0.95f + n * 0.1f;
        if (top) return g ? col3(255, 236, 120, m, 255) : col3(236, 236, 242, m, 255);
        if (front) return g ? col3(226, 184, 52, m, 255) : col3(186, 186, 196, m, 255);
        return g ? col3(176, 132, 30, m, 255) : col3(140, 140, 150, m, 255); }
    case T_POWDER: {
        float w = (y - 5) * 0.9f;
        if (y < 5 || y > 13 || fabsf(x - 7.5f) > w) return 0;
        if (n > 0.9f) return col3(50, 50, 52, 1.0f, 255);
        return col3(120, 120, 124, 0.7f + n * 0.7f, 255); }
    case T_APPLE: {
        float dx = x - 8.0f, dy = y - 9.5f, d = dx * dx + dy * dy;
        if (y >= 3 && y <= 4 && x == 8) return col3(100, 70, 35, 1.0f, 255);
        if (y == 3 && x >= 9 && x <= 11) return col3(70, 170, 50, 1.0f, 255);
        if (d > 27.0f) return 0;
        if (d > 20.0f) return col3(130, 20, 20, 1.0f, 255);
        if (x == 6 && y == 7) return RGBA(255, 190, 190, 255);
        return col3(205, 35, 35, 0.9f + n * 0.15f - dx * 0.03f, 255); }
    case T_FSTEEL: {
        float dx = x - 5.5f, dy = y - 10.5f, d = sqrtf(dx * dx + dy * dy);
        if (d >= 2.6f && d <= 4.3f) return col3(190, 192, 202, 0.9f + n * 0.2f, 255);
        float fx = x - 11.0f, fy = y - 5.0f;
        if (fabsf(fx) * 0.9f + fabsf(fy) * 0.8f <= 3.2f) return col3(100, 104, 116, 0.85f + n * 0.3f + (fx + fy < -1 ? 0.3f : 0.0f), 255);
        if (x >= 7 && x <= 9 && y >= 8 && y <= 9) return col3(190, 192, 202, 0.95f, 255);
        return 0; }
    case T_STICK: return tool_px(4, 0, x, y);
    default:
        if (t >= T_TOOL && t < T_TOOL + 16) return tool_px((t - T_TOOL) / 4, (t - T_TOOL) % 4, x, y);
        if (t >= T_CRACK && t < T_CRACK + 10) {
            int st = t - T_CRACK;
            float a = fabsf(tvn(t, x + 0.5f, y + 0.5f) - 0.5f), b = fabsf(tvn(t + 20, x + 0.5f, y + 0.5f) - 0.5f);
            float th = 0.012f + st * 0.006f;
            if (a < th || b < th) return RGBA(25, 25, 25, 210);
        }
        return 0;
    }
}
static void gen_tex(void) {
    for (int t = 0; t < T_END; t++)
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++)
                atlas[((t >> 3) * 16 + y) * ATW + (t & 7) * 16 + x] = tile_px(t, x, y);
    sceKernelDcacheWritebackAll();
}
static void setup_tex(void) {
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexImage(0, ATW, ATH, ATW, atlas);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuTexScale(1.0f, 1.0f);
    sceGuTexOffset(0.0f, 0.0f);
    sceGuAlphaFunc(GU_GREATER, 0, 0xff);
    sceGuEnable(GU_ALPHA_TEST);
}

/* ---------------- chunk / mesh ---------------- */
typedef struct { V3 *v; unsigned short *ix; int n, cap, ni, capi; } VB;
typedef struct { VB op, tr; int dirty; } Chunk;
static Chunk ch[NCX * NCZ];

static void update_top(int x, int z) {
    for (int y = WY - 1; y >= 0; y--)
        if (blocks_light(world[IDX(x, y, z)])) { topy[x + WX * z] = y; return; }
    topy[x + WX * z] = -1;
}
static int lit(int x, int y, int z) {
    if (y >= WY || x < 0 || z < 0 || x >= WX || z >= WZ) return 1;
    return y > topy[x + WX * z];
}
static void mark(int x, int z) {
    int cx = x / CS, cz = z / CS;
    ch[cz * NCX + cx].dirty = 1;
    if (x % CS == 0 && cx > 0) ch[cz * NCX + cx - 1].dirty = 1;
    if (x % CS == CS - 1 && cx < NCX - 1) ch[cz * NCX + cx + 1].dirty = 1;
    if (z % CS == 0 && cz > 0) ch[(cz - 1) * NCX + cx].dirty = 1;
    if (z % CS == CS - 1 && cz < NCZ - 1) ch[(cz + 1) * NCX + cx].dirty = 1;
}
static void sb(int x, int y, int z, u8 b) {
    if (!inb(x, y, z)) return;
    world[IDX(x, y, z)] = b;
    update_top(x, z);
    mark(x, z);
}

static void vpush(VB *b, u32 c, float u, float v, float x, float y, float z) {
    if (b->n >= b->cap) {
        b->cap = b->cap ? b->cap * 2 : 4096;
        b->v = (V3 *)realloc(b->v, b->cap * sizeof(V3));
    }
    V3 *p = &b->v[b->n++];
    p->u = u; p->v = v; p->c = c; p->x = x; p->y = y; p->z = z;
}
static void ipush(VB *b, int i) {
    if (b->ni >= b->capi) {
        b->capi = b->capi ? b->capi * 2 : 6144;
        b->ix = (unsigned short *)realloc(b->ix, b->capi * sizeof(unsigned short));
    }
    b->ix[b->ni++] = (unsigned short)i;
}

static const int FN[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
static const int FC[6][4][3] = {
    {{1,0,0},{1,1,0},{1,1,1},{1,0,1}},
    {{0,0,1},{0,1,1},{0,1,0},{0,0,0}},
    {{0,1,0},{0,1,1},{1,1,1},{1,1,0}},
    {{0,0,1},{0,0,0},{1,0,0},{1,0,1}},
    {{0,0,1},{1,0,1},{1,1,1},{0,1,1}},
    {{1,0,0},{0,0,0},{0,1,0},{1,1,0}}
};
static const float FSH[6] = {0.78f, 0.78f, 1.0f, 0.5f, 0.88f, 0.88f};

static int vis(u8 b, u8 n) {
    if (b == WATER) return n == AIR || n == FIRE;
    if (b == FIRE) return n == AIR || n == WATER || n == GLASS || n == LEAVES;
    if (b == GLASS) return n == AIR || n == WATER || n == LEAVES || n == FIRE;
    if (b == LEAVES) return n == AIR || n == WATER || n == GLASS || n == FIRE;
    return n == AIR || n == WATER || n == GLASS || n == LEAVES || n == FIRE;
}
static u32 mk(const u8 *c, float s, int a) {
    int r = (int)(c[0] * s), g = (int)(c[1] * s), b = (int)(c[2] * s);
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return RGBA(r, g, b, a);
}

/* kose basina ortam golgelemesi (ambient occlusion) */
static float ao_at(int x, int y, int z, int f, int k) {
    const int *n = FN[f], *p = FC[f][k];
    int ax = n[0] ? 0 : (n[1] ? 1 : 2), b1 = (ax + 1) % 3, b2 = (ax + 2) % 3;
    int d1[3] = {0, 0, 0}, d2[3] = {0, 0, 0};
    d1[b1] = p[b1] ? 1 : -1; d2[b2] = p[b2] ? 1 : -1;
    int bx = x + n[0], by = y + n[1], bz = z + n[2];
    int s1 = blocks_light(gb(bx + d1[0], by + d1[1], bz + d1[2]));
    int s2 = blocks_light(gb(bx + d2[0], by + d2[1], bz + d2[2]));
    int c = blocks_light(gb(bx + d1[0] + d2[0], by + d1[1] + d2[1], bz + d1[2] + d2[2]));
    int lvl = (s1 && s2) ? 0 : 3 - (s1 + s2 + c);
    static const float AOV[4] = {0.55f, 0.72f, 0.86f, 1.0f};
    return AOV[lvl];
}

static void trim(VB *b) {
    if (b->n == 0) { free(b->v); free(b->ix); b->v = NULL; b->ix = NULL; b->cap = b->capi = b->ni = 0; return; }
    b->v = (V3 *)realloc(b->v, b->n * sizeof(V3));
    b->ix = (unsigned short *)realloc(b->ix, b->ni * sizeof(unsigned short));
    b->cap = b->n; b->capi = b->ni;
}

static void build_chunk(int cx, int cz) {
    Chunk *c = &ch[cz * NCX + cx];
    c->op.n = c->op.ni = 0; c->tr.n = c->tr.ni = 0; c->dirty = 0;
    for (int y = 0; y < WY; y++)
        for (int z = cz * CS; z < cz * CS + CS; z++)
            for (int x = cx * CS; x < cx * CS + CS; x++) {
                u8 b = world[IDX(x, y, z)];
                if (!b) continue;
                VB *vb = (b == WATER || b == GLASS || b == FIRE) ? &c->tr : &c->op;
                float var = 0.92f + (hsh(x, y, z) & 7) * 0.012f;
                float wh = 1.0f;
                if (b == WATER && gb(x, y + 1, z) != WATER) wh = 0.88f;
                for (int f = 0; f < 6; f++) {
                    int nx = x + FN[f][0], ny = y + FN[f][1], nz = z + FN[f][2];
                    if (!vis(b, gb(nx, ny, nz))) continue;
                    float lt = lit(nx, ny, nz) ? 1.0f : 0.5f;
                    if (b == GLOW || b == FIRE) lt = 1.2f;
                    float sh = FSH[f] * lt * var;
                    int alpha = b == WATER ? 170 : 255;
                    static const u8 WHITE[3] = {255, 255, 255};
                    int tile = TILE[b][f == 2 ? 0 : (f == 3 ? 2 : 1)];
                    float ao[4];
                    for (int k = 0; k < 4; k++) ao[k] = (b == WATER || b == GLOW || b == GLASS || b == FIRE) ? 1.0f : ao_at(x, y, z, f, k);
                    static const int T0[6] = {0, 1, 2, 0, 2, 3}, T1[6] = {1, 2, 3, 1, 3, 0};
                    const int *T = (ao[1] + ao[3] > ao[0] + ao[2]) ? T1 : T0;
                    int base = vb->n;
                    if (base + 4 > 65535) continue;
                    for (int k = 0; k < 4; k++) {
                        float u, v;
                        face_uvp(f, FC[f][k], tile, &u, &v);
                        vpush(vb, mk(WHITE, sh * ao[k], alpha), u, v, x + FC[f][k][0], y + FC[f][k][1] * wh, z + FC[f][k][2]);
                    }
                    for (int t = 0; t < 6; t++) ipush(vb, base + T[t]);
                }
            }
    trim(&c->op);
    trim(&c->tr);
}

static void rebuild_all(void) {
    for (int z = 0; z < NCZ; z++) for (int x = 0; x < NCX; x++) build_chunk(x, z);
    sceKernelDcacheWritebackAll();
}

/* ---------------- dunya uretimi ---------------- */
static void gen_world(u32 sd) {
    seed = sd; rngs = sd ^ 0x9E3779B9u;
    memset(world, 0, sizeof(world));
    for (int z = 0; z < WZ; z++)
        for (int x = 0; x < WX; x++) {
            float n = fbm2(x * 0.02f, z * 0.02f);
            float m = fbm2(x * 0.008f + 200, z * 0.008f + 200);
            float bio = fbm2(x * 0.012f + 900, z * 0.012f + 900);
            int h = (int)(24 + (n - 0.5f) * 36 + fmaxf(0, m - 0.55f) * 90);
            if (h < 3) h = 3;
            if (h > 56) h = 56;
            hmap[x + WX * z] = h;
            int desert = bio > 0.62f;
            for (int y = 0; y <= h; y++) {
                u8 b;
                if (y == 0) b = BEDROCK;
                else if (y < h - 3) {
                    b = STONE;
                    if (y > 3 && y < h - 1) {
                        float c1 = vn3(x * 0.07f, y * 0.1f, z * 0.07f);
                        float c2 = vn3(x * 0.07f + 43, y * 0.1f + 17, z * 0.07f + 91);
                        if (fabsf(c1 - 0.5f) < 0.055f && fabsf(c2 - 0.5f) < 0.055f) b = AIR;
                    }
                    if (b == STONE) {
                        u32 r = rnd() % 1000;
                        if (r < 14) b = COAL;
                        else if (r < 22 && y < 36) b = IRON;
                        else if (r < 26 && y < 20) b = GOLD;
                        else if (r < 28 && y < 13) b = DIAMOND;
                        else if (r < 40) b = GRAVEL;
                    }
                } else if (y < h) b = (h <= SEA + 1 || desert) ? SAND : DIRT;
                else {
                    if (h <= SEA + 1 || desert) b = SAND;
                    else if (h > 44) b = SNOW;
                    else b = GRASS;
                    if (h < SEA - 3 && (rnd() & 3) == 0) b = GRAVEL;
                }
                world[IDX(x, y, z)] = b;
            }
            for (int y = h + 1; y <= SEA; y++) world[IDX(x, y, z)] = WATER;
        }
    /* agaclar */
    for (int z = 3; z < WZ - 3; z++)
        for (int x = 3; x < WX - 3; x++) {
            int h = hmap[x + WX * z];
            if (world[IDX(x, h, z)] != GRASS || rnd() % 90 != 0) continue;
            int th = 4 + rnd() % 3;
            if (h + th + 2 >= WY) continue;
            for (int dy = th - 2; dy <= th + 1; dy++) {
                int r = dy >= th ? 1 : 2;
                for (int dz = -r; dz <= r; dz++)
                    for (int dx = -r; dx <= r; dx++) {
                        if (r == 2 && abs(dx) == 2 && abs(dz) == 2 && (rnd() & 1)) continue;
                        if (world[IDX(x + dx, h + dy, z + dz)] == AIR) world[IDX(x + dx, h + dy, z + dz)] = LEAVES;
                    }
            }
            for (int dy = 1; dy <= th; dy++) world[IDX(x, h + dy, z)] = LOG;
        }
    for (int z = 0; z < WZ; z++) for (int x = 0; x < WX; x++) update_top(x, z);
}

/* ---------------- oyun durumu ---------------- */
typedef struct { float x, y, z, vx, vy, vz; int ground; float impact; } Ent;
typedef struct { u8 id, n; u16 d; } Slot; /* d = asinma (dayaniklilik) */
typedef struct { int alive, type; Ent e; float yaw, hp, hurt, timer, dx, dz, atk; } Mob;
typedef struct { int alive; u8 id; u16 d; Ent e; float age, delay; } Item;
#define MAXMOB 14
#define MAXITEM 48
enum { ST_PLAY, ST_MENU, ST_INV, ST_DEAD, ST_HELP, ST_TITLE };

static Ent pl;
static float yaw, pitch, hp = 20, airt = 10, inv_t, atkcd, brkcd, placecd, brk_p, nameT, gt, lastStart = -9, spawnT = 3;
static int mode, flying, sprint, hot, state = ST_PLAY, headW;
static int brk_x = -1, brk_y, brk_z;
static Slot inv[36];
static Mob mobs[MAXMOB];
static Item items[MAXITEM];
static float dayT = 0.2f, dayf = 1;
static int timeMode, rdist = 5, lookSens = 3;
static float spawnx, spawny, spawnz;
static char msg[64];
static float msgT;
static int menuSel, invTab, invCur, invHeld = -1, palCur, recCur;
static float food = 20, sat = 5, exh, starveT, eatT, swingP, equipT, bobT, handSpd, fovAdd, eyeH = 1.62f;
static int sneak, titSel, titMsg, lastHot = -1;
static u8 lastHotId;
static u16 add_d, spawn_d;

static void say(const char *s) { strncpy(msg, s, 63); msg[63] = 0; msgT = 2.5f; }

/* ---- envanter ---- */
static int maxs(u8 id) { return (id >= TOOL0 || id == FSTEEL) ? 1 : 64; }
static int add_item(u8 id, int n) {
    int mx = maxs(id);
    for (int i = 0; i < 36 && n > 0; i++)
        if (inv[i].id == id && inv[i].n < mx) { int t = mx - inv[i].n; if (t > n) t = n; inv[i].n += t; n -= t; }
    for (int i = 0; i < 36 && n > 0; i++)
        if (!inv[i].id) { int t = n > mx ? mx : n; inv[i].id = id; inv[i].n = t; inv[i].d = add_d; n -= t; }
    add_d = 0;
    return n;
}
static int count_item(u8 id) { int c = 0; for (int i = 0; i < 36; i++) if (inv[i].id == id) c += inv[i].n; return c; }
static void remove_item(u8 id, int n) {
    for (int i = 35; i >= 0 && n > 0; i--)
        if (inv[i].id == id) { int t = inv[i].n < n ? inv[i].n : n; inv[i].n -= t; n -= t; if (!inv[i].n) inv[i].id = 0; }
}
static void starter_kit(void) {
    static const u8 k[9] = {GRASS, DIRT, STONE, COBBLE, PLANKS, LOG, SAND, GLASS, TNT};
    memset(inv, 0, sizeof(inv));
    for (int i = 0; i < 9; i++) { inv[i].id = k[i]; inv[i].n = 64; }
}
/* st: 0 = her yerde, 1 = calisma masasi yakininda, 2 = ocak yakininda */
typedef struct { u8 a, an, b, bn, out, on, st; } Rec;
static const Rec REC[] = {
    {LOG,1,0,0,PLANKS,4,0}, {PLANKS,2,0,0,STICK,4,0}, {PLANKS,4,0,0,WORKBENCH,1,0},
    {INGOT_I,1,FLINT,1,FSTEEL,1,0}, {COBBLE,8,0,0,FURNACE,1,1},
    {IRON,1,COAL,1,INGOT_I,1,2}, {GOLD,1,COAL,1,INGOT_G,1,2}, {PORK,1,COAL,1,COOKED,1,2},
    {SAND,1,COAL,1,GLASS,1,2}, {COBBLE,1,COAL,1,STONE,1,2}, {PLANKS,3,STICK,2,TOOL0+0,1,1}, {COBBLE,3,STICK,2,TOOL0+1,1,1}, {INGOT_I,3,STICK,2,TOOL0+2,1,1}, {DIAMOND,3,STICK,2,TOOL0+3,1,1},
    {PLANKS,3,STICK,2,TOOL0+4,1,1}, {COBBLE,3,STICK,2,TOOL0+5,1,1}, {INGOT_I,3,STICK,2,TOOL0+6,1,1}, {DIAMOND,3,STICK,2,TOOL0+7,1,1},
    {PLANKS,1,STICK,2,TOOL0+8,1,1}, {COBBLE,1,STICK,2,TOOL0+9,1,1}, {INGOT_I,1,STICK,2,TOOL0+10,1,1}, {DIAMOND,1,STICK,2,TOOL0+11,1,1},
    {PLANKS,2,STICK,1,TOOL0+12,1,1}, {COBBLE,2,STICK,1,TOOL0+13,1,1}, {INGOT_I,2,STICK,1,TOOL0+14,1,1}, {DIAMOND,2,STICK,1,TOOL0+15,1,1},
    {STONE,4,0,0,BRICK,4,1}, {PLANKS,6,0,0,BOOKS,1,1}, {POWDER,3,SAND,2,TNT,1,1},
    {INGOT_G,1,COAL,1,GLOW,2,1}, {GRAVEL,2,0,0,COBBLE,2,0}
};
#define NREC ((int)(sizeof(REC) / sizeof(REC[0])))
static int near_block(u8 blk, int rad) {
    int cx = (int)floorf(pl.x), cy = (int)floorf(pl.y + 1), cz = (int)floorf(pl.z);
    for (int y = cy - rad; y <= cy + rad; y++) for (int z = cz - rad; z <= cz + rad; z++) for (int x = cx - rad; x <= cx + rad; x++)
        if (gb(x, y, z) == blk) return 1;
    return 0;
}
static int st_ok(int st) { return st == 0 || (st == 1 && near_block(WORKBENCH, 4)) || (st == 2 && near_block(FURNACE, 4)); }

/* ---- carpisma ---- */
static int box_hit(float x, float y, float z, float hw, float h) {
    int x0 = (int)floorf(x - hw), x1 = (int)floorf(x + hw);
    int y0 = (int)floorf(y), y1 = (int)floorf(y + h);
    int z0 = (int)floorf(z - hw), z1 = (int)floorf(z + hw);
    for (int j = y0; j <= y1; j++) for (int k = z0; k <= z1; k++) for (int i = x0; i <= x1; i++)
        if (solid_b(gb(i, j, k))) return 1;
    return 0;
}
static int phys_move(Ent *e, float hw, float h, float dt) {
    int blocked = 0;
    float nx = e->x + e->vx * dt;
    if (!box_hit(nx, e->y, e->z, hw, h)) e->x = nx; else { e->vx = 0; blocked = 1; }
    float nz = e->z + e->vz * dt;
    if (!box_hit(e->x, e->y, nz, hw, h)) e->z = nz; else { e->vz = 0; blocked = 1; }
    float ny = e->y + e->vy * dt;
    e->ground = 0;
    if (!box_hit(e->x, ny, e->z, hw, h)) e->y = ny;
    else {
        if (e->vy < 0) { e->ground = 1; e->impact = e->vy; e->y = floorf(e->y + 0.0001f); }
        e->vy = 0;
    }
    return blocked;
}

/* ---- esya / patlama ---- */
static void spawn_item(u8 id, float x, float y, float z, float vx, float vy, float vz, float delay) {
    for (int i = 0; i < MAXITEM; i++)
        if (!items[i].alive) {
            Item *it = &items[i];
            memset(it, 0, sizeof(*it));
            it->alive = 1; it->id = id; it->e.x = x; it->e.y = y; it->e.z = z;
            it->e.vx = vx; it->e.vy = vy; it->e.vz = vz; it->delay = delay;
            it->d = spawn_d; spawn_d = 0;
            return;
        }
}
static void hurt(float d) {
    if (mode == 0 || inv_t > 0 || state == ST_DEAD) return;
    hp -= d; inv_t = 0.5f;
    if (hp <= 0) { hp = 0; state = ST_DEAD; }
}
static void settle(int x, int y, int z) {
    while (y > 0) {
        u8 b = gb(x, y, z);
        if (b != SAND && b != GRAVEL) break;
        u8 u = gb(x, y - 1, z);
        if (u != AIR && u != WATER) break;
        sb(x, y - 1, z, b); sb(x, y, z, AIR); y--;
    }
}
static void settle_above(int x, int y, int z) {
    for (int yy = y + 1; yy < WY; yy++) {
        u8 b = gb(x, yy, z);
        if (b != SAND && b != GRAVEL) break;
        settle(x, yy, z);
    }
}
/* ---- ates / yanan TNT / parcaciklar ---- */
typedef struct { int on, x, y, z; float life, tick; } Fire;
typedef struct { int on; Ent e; float fuse; } PTnt;
typedef struct { int on; float x, y, z, vx, vy, vz, life; u8 id; } Part;
#define MAXFIRE 128
#define MAXPTNT 32
#define MAXPART 96
static Fire fires[MAXFIRE];
static PTnt ptnts[MAXPTNT];
static Part parts[MAXPART];
static int flammable(u8 b) { return b == LOG || b == PLANKS || b == LEAVES || b == BOOKS || b == WORKBENCH; }
static void set_fire(int x, int y, int z) {
    for (int i = 0; i < MAXFIRE; i++) if (!fires[i].on) {
        Fire *f = &fires[i];
        f->on = 1; f->x = x; f->y = y; f->z = z; f->life = 2.5f + rndf() * 2.5f; f->tick = 0.3f;
        sb(x, y, z, FIRE);
        return;
    }
}
static void add_fire(int x, int y, int z) { if (inb(x, y, z) && gb(x, y, z) == AIR) set_fire(x, y, z); }
static void prime_tnt(int x, int y, int z, float fuse) {
    sb(x, y, z, AIR);
    for (int i = 0; i < MAXPTNT; i++) if (!ptnts[i].on) {
        PTnt *p = &ptnts[i];
        memset(p, 0, sizeof(*p));
        p->on = 1; p->e.x = x + .5f; p->e.y = (float)y; p->e.z = z + .5f;
        p->e.vy = 3; p->e.vx = (rndf() - .5f) * 2; p->e.vz = (rndf() - .5f) * 2; p->fuse = fuse;
        return;
    }
}
static void spawn_particles(int x, int y, int z, u8 b) {
    if (b >= PORK || b == AIR) return;
    for (int n = 0; n < 7; n++) for (int i = 0; i < MAXPART; i++) if (!parts[i].on) {
        Part *p = &parts[i];
        p->on = 1; p->id = b; p->life = .5f + rndf() * .5f;
        p->x = x + .2f + rndf() * .6f; p->y = y + .2f + rndf() * .6f; p->z = z + .2f + rndf() * .6f;
        p->vx = (rndf() - .5f) * 4; p->vy = rndf() * 4 + 1; p->vz = (rndf() - .5f) * 4;
        break;
    }
}
static void explode(int cx, int cy, int cz, int r, int depth) {
    for (int dz = -r; dz <= r; dz++) for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
        if (dx * dx + dy * dy + dz * dz > r * r + 1) continue;
        int x = cx + dx, y = cy + dy, z = cz + dz;
        u8 b = gb(x, y, z);
        if (b == AIR || b == BEDROCK || !inb(x, y, z)) continue;
        sb(x, y, z, AIR);
        if (b == TNT) prime_tnt(x, y, z, 0.3f + rndf() * 0.9f);
        else if (mode == 1 && (rnd() & 7) == 0 && b != WATER && b != FIRE) spawn_item(b == STONE ? COBBLE : b, x + .5f, y + .5f, z + .5f, rndf() * 4 - 2, 4, rndf() * 4 - 2, 0.5f);
    }
    float px = pl.x - cx - .5f, py = pl.y + 1 - cy - .5f, pz = pl.z - cz - .5f;
    float d = sqrtf(px * px + py * py + pz * pz);
    if (d < 7 && d > 0.01f) { hurt((1 - d / 7) * 14); pl.vx += px / d * 12; pl.vy += 6; pl.vz += pz / d * 12; }
    for (int i = 0; i < MAXMOB; i++) if (mobs[i].alive) {
        float mx = mobs[i].e.x - cx, my = mobs[i].e.y - cy, mz = mobs[i].e.z - cz;
        if (mx * mx + my * my + mz * mz < 49) mobs[i].alive = 0;
    }
}

/* ---- oyuncu ---- */
static void respawn(void) {
    pl.x = spawnx; pl.y = spawny; pl.z = spawnz; pl.vx = pl.vy = pl.vz = 0;
    hp = 20; airt = 10; state = ST_PLAY; flying = 0;
    food = 20; sat = 5; exh = 0; starveT = 0; eatT = 0;
}
static void set_mode(int m) { mode = m; flying = 0; if (m == 0) { hp = 20; food = 20; sat = 5; exh = 0; } }

static void new_game(u32 sd) {
    gen_world(sd);
    memset(mobs, 0, sizeof(mobs)); memset(items, 0, sizeof(items));
    int x = WX / 2, z = WZ / 2;
    while (topy[x + WX * z] < SEA && x < WX - 8) x++;
    spawnx = x + .5f; spawnz = z + .5f; spawny = topy[x + WX * z] + 1;
    if (mode == 0) starter_kit(); else memset(inv, 0, sizeof(inv));
    respawn();
    for (int i = 0; i < 6; i++) {
        int px = (int)(spawnx + rndf() * 40 - 20), pz = (int)(spawnz + rndf() * 40 - 20);
        if (px < 2 || pz < 2 || px >= WX - 2 || pz >= WZ - 2) continue;
        int y = topy[px + WX * pz];
        if (y >= SEA && gb(px, y, pz) == GRASS && i < MAXMOB) {
            mobs[i].alive = 1; mobs[i].type = 0; mobs[i].hp = 10;
            mobs[i].e.x = px + .5f; mobs[i].e.y = y + 1; mobs[i].e.z = pz + .5f;
        }
    }
    rebuild_all();
}

/* ---- kaydet / yukle ---- */
typedef struct { char magic[8]; u32 seed; float t, px, py, pz, yaw, pitch, hp; int mode, hot; Slot inv[36]; } SaveHdr;
typedef struct { u8 id, n; } Slot1;
typedef struct { char magic[8]; u32 seed; float t, px, py, pz, yaw, pitch, hp; int mode, hot; Slot1 inv[36]; } SaveHdr1;
static u8 remap_v1(u8 id) { return id < 22 ? id : (id == 22 ? PORK : (id == 23 ? COOKED : (id == 24 ? STICK : TOOL0 + (id - 25)))); }
static const char *SDIR = "ms0:/PSP/GAME/MiniCraft";
static const char *SFILE = "ms0:/PSP/GAME/MiniCraft/world.sav";
#define NW (WX * WY * WZ)

static int save_world(void) {
    sceIoMkdir("ms0:/PSP/GAME", 0777);
    sceIoMkdir(SDIR, 0777);
    FILE *f = fopen(SFILE, "wb");
    if (!f) return 0;
    SaveHdr h; memset(&h, 0, sizeof(h));
    memcpy(h.magic, "MCPSP02", 8);
    h.seed = seed; h.t = dayT; h.px = pl.x; h.py = pl.y; h.pz = pl.z; h.yaw = yaw; h.pitch = pitch;
    h.hp = hp; h.mode = mode; h.hot = hot; memcpy(h.inv, inv, sizeof(inv));
    fwrite(&h, sizeof(h), 1, f);
    u8 buf[4096]; int bn = 0, i = 0;
    while (i < NW) {
        u8 v = world[i]; int run = 1;
        while (i + run < NW && run < 255 && world[i + run] == v) run++;
        buf[bn++] = run; buf[bn++] = v; i += run;
        if (bn >= 4094) { fwrite(buf, 1, bn, f); bn = 0; }
    }
    if (bn) fwrite(buf, 1, bn, f);
    fclose(f);
    return 1;
}
static int load_world(void) {
    FILE *f = fopen(SFILE, "rb");
    if (!f) return 0;
    SaveHdr h; memset(&h, 0, sizeof(h));
    char mg[8];
    if (fread(mg, 1, 8, f) != 8) { fclose(f); return 0; }
    fseek(f, 0, SEEK_SET);
    if (!memcmp(mg, "MCPSP02", 8)) {
        if (fread(&h, sizeof(h), 1, f) != 1) { fclose(f); return 0; }
    } else if (!memcmp(mg, "MCPSP01", 8)) {
        SaveHdr1 o;
        if (fread(&o, sizeof(o), 1, f) != 1) { fclose(f); return 0; }
        h.seed = o.seed; h.t = o.t; h.px = o.px; h.py = o.py; h.pz = o.pz; h.yaw = o.yaw; h.pitch = o.pitch;
        h.hp = o.hp; h.mode = o.mode; h.hot = o.hot;
        for (int q = 0; q < 36; q++) { h.inv[q].id = remap_v1(o.inv[q].id); h.inv[q].n = o.inv[q].n; }
    } else { fclose(f); return 0; }
    u8 buf[4096]; int i = 0, run = -1; size_t r;
    while (i < NW && (r = fread(buf, 1, sizeof(buf), f)) > 0)
        for (size_t k = 0; k < r && i < NW; k++) {
            if (run < 0) run = buf[k];
            else { int n = run < NW - i ? run : NW - i; memset(world + i, buf[k], n); i += n; run = -1; }
        }
    fclose(f);
    if (i < NW) return 0;
    seed = h.seed; dayT = h.t; pl.x = h.px; pl.y = h.py; pl.z = h.pz; yaw = h.yaw; pitch = h.pitch;
    hp = h.hp; mode = h.mode; hot = h.hot; memcpy(inv, h.inv, sizeof(inv));
    pl.vx = pl.vy = pl.vz = 0; flying = 0; food = 20; sat = 5; exh = 0;
    spawnx = pl.x; spawny = pl.y; spawnz = pl.z;
    memset(mobs, 0, sizeof(mobs)); memset(items, 0, sizeof(items));
    for (int z = 0; z < WZ; z++) for (int x = 0; x < WX; x++) update_top(x, z);
    rebuild_all();
    return 1;
}

/* ---- ray ---- */
typedef struct { int hit, x, y, z, px, py, pz; float dist; } Ray;
static void look_dir(float *dx, float *dy, float *dz) {
    *dx = -sinf(yaw) * cosf(pitch); *dy = sinf(pitch); *dz = -cosf(yaw) * cosf(pitch);
}
static Ray cast(float reach) {
    Ray r; memset(&r, 0, sizeof(r));
    float dx, dy, dz; look_dir(&dx, &dy, &dz);
    float ex = pl.x, ey = pl.y + eyeH, ez = pl.z;
    int lx = (int)floorf(ex), ly = (int)floorf(ey), lz = (int)floorf(ez);
    for (float t = 0; t < reach; t += 0.04f) {
        int bx = (int)floorf(ex + dx * t), by = (int)floorf(ey + dy * t), bz = (int)floorf(ez + dz * t);
        u8 b = gb(bx, by, bz);
        if (b != AIR && b != WATER) {
            r.hit = 1; r.x = bx; r.y = by; r.z = bz; r.px = lx; r.py = ly; r.pz = lz; r.dist = t;
            return r;
        }
        lx = bx; ly = by; lz = bz;
    }
    return r;
}
static int mob_ray(float reach, float *dist) {
    float dx, dy, dz; look_dir(&dx, &dy, &dz);
    float ex = pl.x, ey = pl.y + eyeH, ez = pl.z;
    for (float t = 0.3f; t < reach; t += 0.1f) {
        float x = ex + dx * t, y = ey + dy * t, z = ez + dz * t;
        for (int i = 0; i < MAXMOB; i++) if (mobs[i].alive) {
            float h = mobs[i].type ? 1.8f : 0.9f, w = mobs[i].type ? 0.35f : 0.45f;
            if (fabsf(x - mobs[i].e.x) < w && fabsf(z - mobs[i].e.z) < w && y > mobs[i].e.y && y < mobs[i].e.y + h) { *dist = t; return i; }
        }
    }
    return -1;
}

static int tool_kind(u8 id) { return (id >= TOOL0 && id <= TOOL_LAST) ? (id - TOOL0) / 4 : -1; }
static int tool_tier(u8 id) { return (id - TOOL0) % 4; }
static int best_kind(u8 b) {
    switch (b) {
    case STONE: case COBBLE: case COAL: case IRON: case GOLD: case DIAMOND: case BRICK: case GLOW: case FURNACE: return 0;
    case LOG: case PLANKS: case BOOKS: case WORKBENCH: return 1;
    case GRASS: case DIRT: case SAND: case GRAVEL: case SNOW: return 2;
    }
    return -1;
}
static int req_tier(u8 b) {
    switch (b) {
    case STONE: case COBBLE: case BRICK: case COAL: case FURNACE: return 0;
    case IRON: return 1;
    case GOLD: case DIAMOND: return 2;
    }
    return -1;
}
static int can_harvest(u8 b) {
    int rq = req_tier(b);
    if (rq < 0 || mode == 0) return 1;
    u8 h = inv[hot].id;
    return tool_kind(h) == 0 && tool_tier(h) >= rq;
}
static float break_time(u8 b) {
    float t = BI[b].h;
    u8 h = inv[hot].id;
    if (t <= 0) return 0;
    if (best_kind(b) >= 0 && tool_kind(h) == best_kind(b)) t /= (2 + 2 * tool_tier(h));
    if (!can_harvest(b)) t *= 3.0f;
    return t;
}
static float weapon_damage(void) {
    u8 h = inv[hot].id;
    int k = tool_kind(h);
    if (k < 0) return 2;
    float t = (float)tool_tier(h);
    return k == 3 ? 4 + t : (k == 1 ? 3 + t : 2 + t * 0.5f);
}
static int max_dur(u8 id) {
    static const int D[4] = {60, 130, 250, 1560};
    if (id == FSTEEL) return 64;
    return tool_kind(id) >= 0 ? D[tool_tier(id)] : 0;
}
static void wear(int a) {
    Slot *sl = &inv[hot];
    int mx = max_dur(sl->id);
    if (!mx || mode == 0) return;
    sl->d += a;
    if (sl->d >= mx) { sl->id = 0; sl->n = 0; sl->d = 0; say("ALETIN KIRILDI"); }
}
static u8 drop_of(u8 b) { return b == STONE ? COBBLE : (b == GRASS ? DIRT : b); }
static void do_break(int x, int y, int z) {
    u8 b = gb(x, y, z);
    if (b == AIR || b == BEDROCK) return;
    if (mode == 1) exh += 0.005f;
    if (b == FIRE) { sb(x, y, z, AIR); return; }
    if (mode == 1) {
        u8 d = drop_of(b);
        if (b == GRAVEL && rndf() < 0.10f) d = FLINT;
        if (b == LEAVES) { float q = rndf(); d = q < 0.06f ? APPLE : (q < 0.10f ? STICK : AIR); }
        if (d && can_harvest(b)) spawn_item(d, x + .5f, y + .5f, z + .5f, rndf() * 2 - 1, 3, rndf() * 2 - 1, 0.3f);
        if (tool_kind(inv[hot].id) >= 0) wear(tool_kind(inv[hot].id) == 3 ? 2 : 1);
    }
    spawn_particles(x, y, z, b);
    sb(x, y, z, AIR);
    settle_above(x, y, z);
}
static int vis_to_slot(int c) { return c < 27 ? 9 + c : c - 27; } /* gorsel sira -> slot */
static void drop_slot(int si) {
    Slot *s = &inv[si];
    if (!s->id || !s->n) return;
    float dx, dy, dz; look_dir(&dx, &dy, &dz);
    spawn_d = s->d;
    spawn_item(s->id, pl.x + dx * 0.6f, pl.y + 1.4f + dy * 0.6f, pl.z + dz * 0.6f, dx * 6, dy * 6 + 2, dz * 6, 1.2f);
    if (mode == 1 && --s->n == 0) { s->id = 0; s->d = 0; }
}
static void drop_hand(void) { drop_slot(hot); }

/* ---- guncelleme ---- */
static int night(void) { return dayf < 0.35f; }

static void update_player(float dt, const SceCtrlData *pad, u32 pr) {
    u32 B = pad->Buttons;
    float sx = (pad->Lx - 128) / 128.0f, sy = (pad->Ly - 128) / 128.0f;
    if (fabsf(sx) < 0.2f) sx = 0;
    if (fabsf(sy) < 0.2f) sy = 0;
    if (!(B & PSP_CTRL_SELECT)) {
        float ls = (1.4f + lookSens * 0.35f) * dt;
        if (B & PSP_CTRL_TRIANGLE) pitch += ls;
        if (B & PSP_CTRL_CROSS) pitch -= ls;
        if (B & PSP_CTRL_SQUARE) yaw += ls;
        if (B & PSP_CTRL_CIRCLE) yaw -= ls;
        pitch = clampf(pitch, -1.5f, 1.5f);
    }
    if (pr & PSP_CTRL_LEFT) { hot = (hot + 8) % 9; nameT = 1.5f; }
    if (pr & PSP_CTRL_RIGHT) { hot = (hot + 1) % 9; nameT = 1.5f; }
    if (pr & PSP_CTRL_UP) sprint = !sprint;
    if (pr & PSP_CTRL_START) {
        if (mode == 0 && gt - lastStart < 0.3f) { flying = !flying; pl.vy = 0; say(flying ? "UCUS ACIK" : "UCUS KAPALI"); }
        lastStart = gt;
    }
    sneak = (B & PSP_CTRL_DOWN) && !flying;
    eyeH += ((sneak ? 1.27f : 1.62f) - eyeH) * clampf(12 * dt, 0, 1);
    int inw = gb((int)floorf(pl.x), (int)floorf(pl.y + 0.3f), (int)floorf(pl.z)) == WATER;
    headW = gb((int)floorf(pl.x), (int)floorf(pl.y + eyeH), (int)floorf(pl.z)) == WATER;
    float mag = clampf(sqrtf(sx * sx + sy * sy), 0, 1);
    if (mag < 0.1f) sprint = 0;
    if (sneak || (mode == 1 && food <= 6)) sprint = 0;
    fovAdd += ((sprint ? 9.0f : 0.0f) - fovAdd) * clampf(8 * dt, 0, 1);
    float fx = -sinf(yaw), fz = -cosf(yaw), rx = cosf(yaw), rz = -sinf(yaw);
    float wx = fx * (-sy) + rx * sx, wz = fz * (-sy) + rz * sx;
    float wl = sqrtf(wx * wx + wz * wz);
    if (wl > 0) { wx /= wl; wz /= wl; }
    float speed = 4.3f;
    if (sprint) speed = 5.8f;
    if (sneak) speed = 1.3f;
    if (inw) speed = 2.6f;
    if (flying) speed = sprint ? 18.0f : 11.0f;
    float k = flying ? 8 : (pl.ground ? 14 : 4);
    float a = clampf(k * dt, 0, 1);
    pl.vx += (wx * speed * mag - pl.vx) * a;
    pl.vz += (wz * speed * mag - pl.vz) * a;
    if (sneak && pl.ground && !inw) { /* comelirken kenardan dusme */
        if (!box_hit(pl.x + pl.vx * dt, pl.y - 0.5f, pl.z, 0.3f, 0.0f)) pl.vx = 0;
        if (!box_hit(pl.x, pl.y - 0.5f, pl.z + pl.vz * dt, 0.3f, 0.0f)) pl.vz = 0;
    }
    int jump = (B & PSP_CTRL_START) != 0;
    if (flying) {
        pl.vy = jump ? 7 : ((B & PSP_CTRL_DOWN) ? -7 : 0);
    } else if (inw) {
        pl.vy -= 5 * dt;
        if (jump) pl.vy += 20 * dt;
        if (jump && !headW) pl.vy = fmaxf(pl.vy, 5.5f);
        pl.vy = clampf(pl.vy, -3, 5.5f);
    } else {
        pl.vy -= 28 * dt;
        if (pl.vy < -40) pl.vy = -40;
        if (jump && pl.ground) { pl.vy = 8.5f; exh += sprint ? 0.2f : 0.05f; }
    }
    int was = pl.ground;
    int gnd = 0; /* ground bayragi iki alt adimda da korunmali (zipla hatasi) */
    for (int i = 0; i < 2; i++) { phys_move(&pl, 0.3f, 1.8f, dt * 0.5f); gnd |= pl.ground; }
    pl.ground = gnd;
    if (pl.ground && !was && mode == 1 && !flying && !inw && pl.impact < -13) hurt((-pl.impact - 13) * 0.7f);
    if (pl.y < -20) { pl.x = spawnx; pl.y = spawny; pl.z = spawnz; hurt(4); }
    /* hayatta kalma */
    if (inv_t > 0) inv_t -= dt;
    if (mode == 1) {
        if (headW) { airt -= dt; if (airt < 0) { airt = 0; hurt(2 * dt * 2); } }
        else airt = fminf(10, airt + dt * 4);
        /* aclik */
        if (sprint && pl.ground && mag > 0.1f) exh += sqrtf(pl.vx * pl.vx + pl.vz * pl.vz) * dt * 0.1f;
        if (exh >= 4) { exh -= 4; if (sat > 0) sat = fmaxf(0, sat - 1); else food = fmaxf(0, food - 1); }
        if (food >= 18 && hp < 20) { hp = fminf(20, hp + 0.25f * dt); exh += 1.5f * dt; starveT = 0; }
        else if (food <= 0) { starveT += dt; if (starveT >= 4) { starveT = 0; if (hp > 1) hurt(1); } }
        else starveT = 0;
    } else { hp = 20; airt = 10; food = 20; exh = 0; }
    /* kir / saldir / koy */
    atkcd -= dt; brkcd -= dt; placecd -= dt;
    /* el animasyonu: vurus, esya degistirme, yururken sallanma */
    if ((B & PSP_CTRL_LTRIGGER) && swingP <= 0) swingP = 0.001f;
    if ((pr & PSP_CTRL_RTRIGGER) && swingP <= 0) swingP = 0.001f;
    if (swingP > 0) { swingP += dt / 0.30f; if (swingP >= 1) swingP = (B & PSP_CTRL_LTRIGGER) ? 0.001f : 0; }
    if (hot != lastHot || inv[hot].id != lastHotId) { lastHot = hot; lastHotId = inv[hot].id; equipT = 1; }
    equipT = fmaxf(0, equipT - dt * 4.5f);
    handSpd += ((pl.ground ? sqrtf(pl.vx * pl.vx + pl.vz * pl.vz) : 0) - handSpd) * clampf(10 * dt, 0, 1);
    bobT += handSpd * dt * 1.7f;
    Ray r = cast(5.0f);
    if (B & PSP_CTRL_LTRIGGER) {
        float md = 0; int mi = mob_ray(4.0f, &md);
        if (mi >= 0 && (!r.hit || md < r.dist)) {
            brk_p = 0;
            if (atkcd <= 0) {
                atkcd = 0.45f;
                Mob *m = &mobs[mi];
                m->hp -= mode == 0 ? 20 : weapon_damage(); m->hurt = 0.35f;
                float dx, dy, dz; look_dir(&dx, &dy, &dz);
                m->e.vx = dx * 7; m->e.vz = dz * 7; m->e.vy = 5;
                if (mode == 1) { exh += 0.1f; if (tool_kind(inv[hot].id) >= 0) wear(tool_kind(inv[hot].id) == 3 ? 1 : 2); }
                if (m->hp <= 0) {
                    m->alive = 0;
                    if (mode == 1) {
                        int cnt = 0; u8 dr = AIR;
                        if (m->type == 0) { dr = PORK; cnt = 1 + (int)(rndf() * 2); }
                        else if (m->type == 2) { dr = POWDER; cnt = (int)(rndf() * 3); }
                        else if (rndf() < 0.08f) { dr = INGOT_I; cnt = 1; }
                        for (int q = 0; q < cnt; q++) spawn_item(dr, m->e.x, m->e.y + 0.5f, m->e.z, rndf() * 2 - 1, 3, rndf() * 2 - 1, 0.3f);
                    }
                }
            }
        } else if (r.hit && gb(r.x, r.y, r.z) != BEDROCK) {
            if (r.x != brk_x || r.y != brk_y || r.z != brk_z) { brk_x = r.x; brk_y = r.y; brk_z = r.z; brk_p = 0; }
            if (mode == 0) { if (brkcd <= 0) { do_break(r.x, r.y, r.z); brkcd = 0.22f; } }
            else {
                brk_p += dt;
                if (brk_p >= break_time(gb(r.x, r.y, r.z))) { do_break(r.x, r.y, r.z); brk_p = 0; }
            }
        } else brk_p = 0;
    } else { brk_p = 0; brk_x = -1; }
    u8 hid = inv[hot].id;
    if (hid == FSTEEL && (pr & PSP_CTRL_RTRIGGER) && r.hit) { /* cakmak: TNT yak / ates cikar */
        if (gb(r.x, r.y, r.z) == TNT) { prime_tnt(r.x, r.y, r.z, 3.5f); wear(1); }
        else if (gb(r.px, r.py, r.pz) == AIR) { add_fire(r.px, r.py, r.pz); wear(1); }
    }
    if ((hid == PORK || hid == COOKED || hid == APPLE) && mode == 1 && food < 20 && (B & PSP_CTRL_RTRIGGER)) {
        eatT += dt;
        if (eatT >= 1.6f) {
            eatT = 0;
            float fv = hid == COOKED ? 8 : (hid == APPLE ? 4 : 3), sv = hid == COOKED ? 12.8f : (hid == APPLE ? 2.4f : 1.8f);
            food = fminf(20, food + fv); sat = fminf(food, sat + sv);
            if (--inv[hot].n == 0) inv[hot].id = 0;
        }
    } else eatT = 0;
    if ((B & PSP_CTRL_RTRIGGER) && placecd <= 0 && r.hit && hid < PORK) {
        placecd = 0.22f;
        u8 id = inv[hot].id;
        if (id && inv[hot].n > 0) {
            u8 pb = gb(r.px, r.py, r.pz);
            int ov = fabsf(pl.x - (r.px + .5f)) < 0.8f && fabsf(pl.z - (r.pz + .5f)) < 0.8f && pl.y < r.py + 1 && pl.y + 1.8f > r.py;
            if ((pb == AIR || pb == WATER) && !ov && inb(r.px, r.py, r.pz)) {
                sb(r.px, r.py, r.pz, id);
                if (id == FIRE) set_fire(r.px, r.py, r.pz);
                settle(r.px, r.py, r.pz);
                if (mode == 1 && --inv[hot].n == 0) inv[hot].id = 0;
            }
        }
    }
}

static void spawn_mob(int type, float x, float y, float z) {
    for (int i = 0; i < MAXMOB; i++) if (!mobs[i].alive) {
        memset(&mobs[i], 0, sizeof(Mob));
        mobs[i].alive = 1; mobs[i].type = type; mobs[i].hp = type ? 20 : 10;
        mobs[i].e.x = x; mobs[i].e.y = y; mobs[i].e.z = z;
        return;
    }
}
static void try_spawn(void) {
    int pigs = 0, cnt = 0;
    for (int i = 0; i < MAXMOB; i++) if (mobs[i].alive) { cnt++; if (!mobs[i].type) pigs++; }
    if (cnt >= MAXMOB) return;
    float a = rndf() * 2 * PI, d = 14 + rndf() * 22;
    int x = (int)(pl.x + cosf(a) * d), z = (int)(pl.z + sinf(a) * d);
    if (x < 2 || z < 2 || x >= WX - 2 || z >= WZ - 2) return;
    int y = topy[x + WX * z];
    if (y < 0 || gb(x, y + 1, z) != AIR || gb(x, y + 2, z) != AIR) return;
    if (mode == 1 && night() && rndf() < 0.7f) spawn_mob(rndf() < 0.3f ? 2 : 1, x + .5f, y + 1, z + .5f);
    else if (pigs < 8 && !night() && gb(x, y, z) == GRASS) spawn_mob(0, x + .5f, y + 1, z + .5f);
}
static void update_mobs(float dt) {
    spawnT -= dt;
    if (spawnT <= 0) { spawnT = 3; try_spawn(); }
    for (int i = 0; i < MAXMOB; i++) {
        Mob *m = &mobs[i];
        if (!m->alive) continue;
        float dx = pl.x - m->e.x, dz = pl.z - m->e.z, d = sqrtf(dx * dx + dz * dz) + 0.001f;
        if (d > 80) { m->alive = 0; continue; }
        if (m->hurt > 0) m->hurt -= dt;
        float wx = 0, wz = 0, sp = 1.1f;
        if (m->type == 2) { /* creeper */
            sp = 2.3f;
            if (d < 24) { wx = dx / d; wz = dz / d; }
            if (d < 3.0f && fabsf(pl.y - m->e.y) < 2.0f) {
                m->atk += dt; wx = wz = 0;
                if (m->atk >= 1.5f) {
                    m->alive = 0;
                    explode((int)floorf(m->e.x), (int)floorf(m->e.y + 0.5f), (int)floorf(m->e.z), 3, 0);
                    continue;
                }
            } else m->atk = 0;
        } else if (m->type == 1) {
            sp = 2.6f;
            if (d < 28) { wx = dx / d; wz = dz / d; }
            m->atk -= dt;
            if (d < 1.1f && fabsf(pl.y - m->e.y) < 1.6f && m->atk <= 0) {
                m->atk = 1; hurt(3);
                pl.vx += dx / d * 6; pl.vz += dz / d * 6; pl.vy = 4;
            }
            if (dayf > 0.6f && lit((int)m->e.x, (int)m->e.y + 1, (int)m->e.z)) { m->hp -= dt * 2; m->hurt = 0.1f; if (m->hp <= 0) m->alive = 0; }
        } else {
            m->timer -= dt;
            if (m->timer <= 0) {
                m->timer = 2 + rndf() * 4;
                if (rndf() < 0.6f) { float a = rndf() * 2 * PI; m->dx = cosf(a); m->dz = sinf(a); } else m->dx = m->dz = 0;
            }
            wx = m->dx; wz = m->dz;
        }
        if (wx || wz) m->yaw = atan2f(-wx, -wz);
        float kk = clampf(8 * dt, 0, 1);
        m->e.vx += (wx * sp - m->e.vx) * kk;
        m->e.vz += (wz * sp - m->e.vz) * kk;
        int inw = gb((int)floorf(m->e.x), (int)floorf(m->e.y + 0.2f), (int)floorf(m->e.z)) == WATER;
        m->e.vy -= (inw ? 4 : 26) * dt;
        if (inw) m->e.vy = fmaxf(m->e.vy, -1.5f) + 12 * dt;
        float hw = m->type ? 0.3f : 0.4f, h = m->type ? 1.8f : 0.9f;
        int bl = phys_move(&m->e, hw, h, dt);
        if (bl && m->e.ground && (wx || wz)) m->e.vy = 7.5f;
        if (m->e.y < -10) m->alive = 0;
    }
}
static void update_fire(float dt) {
    for (int i = 0; i < MAXFIRE; i++) {
        Fire *f = &fires[i];
        if (!f->on) continue;
        if (gb(f->x, f->y, f->z) != FIRE) { f->on = 0; continue; }
        f->life -= dt; f->tick -= dt;
        if (f->tick <= 0) {
            f->tick = 0.4f;
            float cx = f->x + .5f, cz = f->z + .5f;
            if (fabsf(pl.x - cx) < 0.8f && fabsf(pl.z - cz) < 0.8f && pl.y < f->y + 1 && pl.y + 1.8f > f->y) hurt(1.5f);
            for (int m = 0; m < MAXMOB; m++)
                if (mobs[m].alive && fabsf(mobs[m].e.x - cx) < 0.8f && fabsf(mobs[m].e.z - cz) < 0.8f && mobs[m].e.y < f->y + 1 && mobs[m].e.y + 1.5f > f->y) {
                    mobs[m].hp -= 3; mobs[m].hurt = 0.3f;
                    if (mobs[m].hp <= 0) mobs[m].alive = 0;
                }
            int fuel = 0, wet = 0;
            for (int d = 0; d < 6; d++) {
                int nx = f->x + FN[d][0], ny = f->y + FN[d][1], nz = f->z + FN[d][2];
                u8 nb = gb(nx, ny, nz);
                if (nb == WATER) wet = 1;
                else if (nb == TNT) prime_tnt(nx, ny, nz, 1.0f + rndf());
                else if (flammable(nb)) { fuel = 1; if (rndf() < 0.35f) set_fire(nx, ny, nz); }
            }
            if (fuel && f->life < 1.5f) f->life = 1.5f;
            if (wet) f->life = 0;
        }
        if (f->life <= 0) { sb(f->x, f->y, f->z, AIR); f->on = 0; }
    }
}
static void update_ptnt(float dt) {
    for (int i = 0; i < MAXPTNT; i++) {
        PTnt *p = &ptnts[i];
        if (!p->on) continue;
        p->e.vy -= 25 * dt;
        phys_move(&p->e, 0.4f, 0.9f, dt);
        if (p->e.ground) { p->e.vx *= 0.8f; p->e.vz *= 0.8f; }
        p->fuse -= dt;
        if (p->e.y < -10) { p->on = 0; continue; }
        if (p->fuse <= 0) { p->on = 0; explode((int)floorf(p->e.x), (int)floorf(p->e.y + 0.5f), (int)floorf(p->e.z), 4, 0); }
    }
}
static void update_particles(float dt) {
    for (int i = 0; i < MAXPART; i++) {
        Part *p = &parts[i];
        if (!p->on) continue;
        p->life -= dt;
        if (p->life <= 0) { p->on = 0; continue; }
        p->vy -= 18 * dt;
        p->x += p->vx * dt; p->y += p->vy * dt; p->z += p->vz * dt;
        if (solid_b(gb((int)floorf(p->x), (int)floorf(p->y - 0.05f), (int)floorf(p->z)))) { p->vy = 0; p->vx *= 0.6f; p->vz *= 0.6f; }
    }
}
static void update_items(float dt) {
    for (int i = 0; i < MAXITEM; i++) {
        Item *it = &items[i];
        if (!it->alive) continue;
        it->age += dt;
        if (it->age > 300) { it->alive = 0; continue; }
        it->e.vy -= 20 * dt;
        if (it->e.ground) { it->e.vx *= 0.8f; it->e.vz *= 0.8f; }
        phys_move(&it->e, 0.15f, 0.3f, dt);
        if (it->e.y < -10) { it->alive = 0; continue; }
        if (it->age > it->delay) {
            float dx = pl.x - it->e.x, dy = pl.y + 0.8f - it->e.y, dz = pl.z - it->e.z;
            if (dx * dx + dy * dy + dz * dz < 2.0f && (add_d = it->d, add_item(it->id, 1)) == 0) it->alive = 0;
        }
    }
}

static void update_time(float dt) {
    if (timeMode == 0) dayT += dt / 600.0f;
    else dayT = timeMode == 1 ? 0.25f : 0.75f;
    if (dayT >= 1) dayT -= 1;
    dayf = clampf(sinf(dayT * 2 * PI) * 2.0f + 0.4f, 0, 1);
}

/* ---------------- 3D cizim ---------------- */
static const int BQ[6][4] = {{1,3,7,5},{4,6,2,0},{2,3,7,6},{0,1,5,4},{4,5,7,6},{0,1,3,2}};
static const float BSH[6] = {0.78f, 0.78f, 1.0f, 0.5f, 0.88f, 0.88f};
static int box_id, box_tile = -1, box_alpha = 255;
static void box(float cx, float cy, float cz, float hx, float hy, float hz, float yw, int r, int g, int b) {
    V3 *v = sceGuGetMemory(36 * sizeof(V3));
    float s = sinf(yw), c = cosf(yw), px[8], py[8], pz[8];
    for (int i = 0; i < 8; i++) {
        float lx = (i & 1) ? hx : -hx, ly = (i & 2) ? hy : -hy, lz = (i & 4) ? hz : -hz;
        px[i] = cx + lx * c + lz * s; py[i] = cy + ly; pz[i] = cz - lx * s + lz * c;
    }
    static const int T[6] = {0, 1, 2, 0, 2, 3};
    int k = 0;
    for (int f = 0; f < 6; f++) {
        u8 col[3] = {(u8)r, (u8)g, (u8)b};
        u32 cc = mk(col, BSH[f], box_alpha);
        int tile = box_tile >= 0 ? box_tile : (box_id > 0 ? TILE[box_id][f == 2 ? 0 : (f == 3 ? 2 : 1)] : -1);
        for (int t = 0; t < 6; t++) {
            int i = BQ[f][T[t]];
            v[k].u = v[k].v = 0;
            if (tile >= 0) { int lp[3] = {i & 1, (i >> 1) & 1, (i >> 2) & 1}; face_uvp(f, lp, tile, &v[k].u, &v[k].v); }
            v[k].c = cc; v[k].x = px[i]; v[k].y = py[i]; v[k].z = pz[i]; k++;
        }
    }
    sceGumDrawArray(GU_TRIANGLES, VT3, 36, 0, v);
}
static void draw_mobs(void) {
    for (int i = 0; i < MAXMOB; i++) {
        Mob *m = &mobs[i];
        if (!m->alive) continue;
        float dx = m->e.x - pl.x, dz = m->e.z - pl.z;
        if (dx * dx + dz * dz > 3600) continue;
        int f = m->hurt > 0;
        float x = m->e.x, y = m->e.y, z = m->e.z, w = m->yaw;
        float fx = -sinf(w), fz = -cosf(w);
        if (m->type == 2) { /* patlayici bocek: orijinal mob */
            int fl = m->atk > 0 && (((int)(gt * 10)) & 1);
            int cr = (fl || f) ? 255 : 150, cg = fl ? 255 : (f ? 60 : 60), cb = fl ? 255 : (f ? 60 : 190);
            box(x, y + 0.35f, z, 0.45f, 0.3f, 0.45f, w, cr, cg, cb);
            box(x + fx * 0.5f, y + 0.5f, z + fz * 0.5f, 0.2f, 0.2f, 0.2f, w, cr * 3 / 4, cg * 3 / 4, cb * 3 / 4);
            box(x + fx * 0.6f, y + 0.85f, z + fz * 0.6f, 0.03f, 0.2f, 0.03f, w, 255, 200, 60);
            box(x, y + 0.72f, z, 0.3f, 0.12f, 0.3f, w, 255, 140, 40);
        } else if (m->type == 1) {
            box(x, y + 0.45f, z, 0.25f, 0.45f, 0.15f, w, f ? 255 : 40, f ? 60 : 60, f ? 60 : 150);
            box(x, y + 1.2f, z, 0.25f, 0.3f, 0.15f, w, f ? 255 : 0, f ? 60 : 160, f ? 60 : 160);
            box(x, y + 1.65f, z, 0.25f, 0.25f, 0.25f, w, f ? 255 : 80, f ? 60 : 140, f ? 60 : 70);
            box(x + fx * 0.3f, y + 1.3f, z + fz * 0.3f, 0.5f, 0.1f, 0.1f, w, f ? 255 : 80, f ? 60 : 140, f ? 60 : 70);
        } else {
            box(x, y + 0.5f, z, 0.3f, 0.25f, 0.5f, w, f ? 255 : 235, f ? 60 : 150, f ? 60 : 160);
            box(x + fx * 0.55f, y + 0.65f, z + fz * 0.55f, 0.22f, 0.22f, 0.2f, w, f ? 255 : 240, f ? 60 : 165, f ? 60 : 170);
            box(x, y + 0.12f, z, 0.25f, 0.12f, 0.4f, w, f ? 200 : 200, f ? 40 : 120, f ? 40 : 130);
        }
    }
}

static void item_sprite(float cx, float cy, float cz, float hs, float yw, int tile) {
    V3 *v = sceGuGetMemory(6 * sizeof(V3));
    float sn = sinf(yw), cs = cosf(yw), ax = cs * hs, az = -sn * hs;
    static const int T[6] = {0, 1, 2, 0, 2, 3};
    float px[4] = {cx - ax, cx + ax, cx + ax, cx - ax}, py[4] = {cy - hs, cy - hs, cy + hs, cy + hs};
    float pz[4] = {cz - az, cz + az, cz + az, cz - az}, uu[4] = {0, 1, 1, 0}, vv[4] = {1, 1, 0, 0};
    for (int t = 0; t < 6; t++) {
        int k = T[t];
        tile_uv(tile, uu[k], vv[k], &v[t].u, &v[t].v);
        v[t].c = 0xFFFFFFFF; v[t].x = px[k]; v[t].y = py[k]; v[t].z = pz[k];
    }
    sceGumDrawArray(GU_TRIANGLES, VT3, 6, 0, v);
}
static void draw_items(void) {
    sceGuEnable(GU_TEXTURE_2D);
    for (int i = 0; i < MAXITEM; i++) {
        Item *it = &items[i];
        if (!it->alive) continue;
        float bob = sinf(gt * 3 + i) * 0.05f;
        if (it->id < PORK) {
            box_id = it->id;
            box(it->e.x, it->e.y + 0.2f + bob, it->e.z, 0.14f, 0.14f, 0.14f, gt * 2, 255, 255, 255);
            box_id = 0;
        } else {
            item_sprite(it->e.x, it->e.y + 0.28f + bob, it->e.z, 0.22f, gt * 2, item_tile(it->id));
            item_sprite(it->e.x, it->e.y + 0.28f + bob, it->e.z, 0.22f, gt * 2 + 1.5708f, item_tile(it->id));
        }
    }
    sceGuDisable(GU_TEXTURE_2D);
}
static void draw_ptnt(void) {
    for (int i = 0; i < MAXPTNT; i++) {
        PTnt *p = &ptnts[i];
        if (!p->on) continue;
        int blink = ((int)(p->fuse * 5.0f)) & 1;
        float sc = 0.49f + (p->fuse < 1.0f ? (1.0f - p->fuse) * 0.04f : 0.0f);
        sceGuEnable(GU_TEXTURE_2D);
        box_id = TNT;
        box(p->e.x, p->e.y + 0.49f, p->e.z, sc, sc, sc, 0, 255, 255, 255);
        box_id = 0;
        sceGuDisable(GU_TEXTURE_2D);
        if (blink) {
            sceGuEnable(GU_BLEND); box_alpha = 170;
            box(p->e.x, p->e.y + 0.49f, p->e.z, sc + 0.01f, sc + 0.01f, sc + 0.01f, 0, 255, 255, 255);
            box_alpha = 255; sceGuDisable(GU_BLEND);
        }
    }
}
static void draw_particles(void) {
    sceGuEnable(GU_TEXTURE_2D);
    for (int i = 0; i < MAXPART; i++) {
        Part *p = &parts[i];
        if (!p->on) continue;
        box_id = p->id;
        box(p->x, p->y, p->z, 0.07f, 0.07f, 0.07f, p->life * 6, 255, 255, 255);
    }
    box_id = 0;
    sceGuDisable(GU_TEXTURE_2D);
}
static void draw_clouds(void) {
    int c = (int)(255 * (0.35f + 0.65f * dayf));
    for (int j = 0; j < 9; j++) for (int i = 0; i < 9; i++) {
        if (hf(i, 91, j) < 0.5f) continue;
        float x = -80.0f + fmodf(i * 32.0f + gt * 1.5f, 288.0f);
        float z = -80.0f + j * 32.0f;
        box(x, WY + 6.0f, z, 10 + hf(i, 92, j) * 6, 1.2f, 7 + hf(i, 93, j) * 6, 0, c, c, c);
    }
}
static void outline(int x, int y, int z) {
    V3 *v = sceGuGetMemory(24 * sizeof(V3));
    static const int E[12][2] = {{0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}};
    int k = 0;
    for (int e = 0; e < 12; e++) for (int j = 0; j < 2; j++) {
        int i = E[e][j];
        v[k].u = v[k].v = 0;
        v[k].c = RGBA(0, 0, 0, 255);
        v[k].x = x + ((i & 1) ? 1.003f : -0.003f);
        v[k].y = y + ((i & 2) ? 1.003f : -0.003f);
        v[k].z = z + ((i & 4) ? 1.003f : -0.003f);
        k++;
    }
    sceGumDrawArray(GU_LINES, VT3, 24, 0, v);
}
static void draw_vb(VB *b) {
    int n = b->ni, off = 0;
    while (n > 0) {
        int k = n > 59994 ? 59994 : n;
        sceGumDrawArray(GU_TRIANGLES, VT3 | GU_INDEX_16BIT, k, b->ix + off, b->v);
        off += k; n -= k;
    }
}

/* ---------------- HUD ---------------- */
static void tspr(int x, int y, int w, int h, int tile, u32 col) {
    flush2d();
    V2T *m = (V2T *)sceGuGetMemory(2 * sizeof(V2T));
    float u0, v0, u1, v1;
    tile_uv(tile, 0, 0, &u0, &v0); tile_uv(tile, 1, 1, &u1, &v1);
    m[0].u = u0; m[0].v = v0; m[0].c = col; m[0].x = x;     m[0].y = y;     m[0].z = 0; m[0].pad = 0;
    m[1].u = u1; m[1].v = v1; m[1].c = col; m[1].x = x + w; m[1].y = y + h; m[1].z = 0; m[1].pad = 0;
    sceGuEnable(GU_TEXTURE_2D);
    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, m);
    sceGuDisable(GU_TEXTURE_2D);
}
/* dondurulebilen dokulu dortgen (el animasyonu icin) */
static void tquad(float cx, float cy, float w, float h, float ang, int tile, u32 col) {
    flush2d();
    V2T *m = (V2T *)sceGuGetMemory(6 * sizeof(V2T));
    float cs = cosf(ang), sn = sinf(ang), hx = w * 0.5f, hy = h * 0.5f;
    static const float QX[4] = {-1, 1, 1, -1}, QY[4] = {-1, -1, 1, 1}, QU[4] = {0, 1, 1, 0}, QV[4] = {0, 0, 1, 1};
    static const int T[6] = {0, 1, 2, 0, 2, 3};
    for (int t = 0; t < 6; t++) {
        int k = T[t];
        float x = QX[k] * hx, y = QY[k] * hy;
        if (tile >= 0) tile_uv(tile, QU[k], QV[k], &m[t].u, &m[t].v); else m[t].u = m[t].v = 0;
        m[t].c = col; m[t].x = (short)(cx + x * cs - y * sn); m[t].y = (short)(cy + x * sn + y * cs); m[t].z = 0; m[t].pad = 0;
    }
    if (tile >= 0) sceGuEnable(GU_TEXTURE_2D);
    sceGuDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 6, 0, m);
    sceGuDisable(GU_TEXTURE_2D);
}
static void icon(int x, int y, int s, u8 id) {
    if (!id) return;
    if (id >= PORK) { tspr(x, y, s, s, item_tile(id), 0xFFFFFFFF); return; }
    int th = s * 3 / 10;
    tspr(x, y, s, th, TILE[id][0], 0xFFFFFFFF);
    tspr(x, y + th, s, s - th, TILE[id][1], RGBA(200, 200, 200, 255));
}
static void slot_draw(int x, int y, int s, Slot *sl, int selected, int held) {
    rect(x, y, s, s, RGBA(0, 0, 0, 150));
    if (selected) { rect(x - 2, y - 2, s + 4, 2, 0xFFFFFFFF); rect(x - 2, y + s, s + 4, 2, 0xFFFFFFFF);
                    rect(x - 2, y, 2, s, 0xFFFFFFFF); rect(x + s, y, 2, s, 0xFFFFFFFF); }
    if (held) rect(x, y, s, s, RGBA(255, 200, 0, 120));
    if (sl->id) {
        icon(x + 3, y + 3, s - 6, sl->id);
        int mxd = max_dur(sl->id);
        if (mxd && sl->d > 0) { /* dayaniklilik cubugu */
            float fr = (float)(mxd - sl->d) / mxd;
            u32 dc = fr > 0.5f ? RGBA(80, 230, 60, 255) : (fr > 0.25f ? RGBA(230, 200, 40, 255) : RGBA(230, 60, 40, 255));
            rect(x + 3, y + s - 5, s - 6, 2, RGBA(0, 0, 0, 255));
            rect(x + 3, y + s - 5, (int)((s - 6) * fr), 2, dc);
        }
        if (sl->n > 1 && !(mode == 0 && sl->n == 64)) {
            char b[8]; snprintf(b, sizeof(b), "%d", sl->n);
            ptext(x + s - 4 * (int)strlen(b) - 2, y + s - 7, 1, 0xFFFFFFFF, b);
        }
    }
}
static void draw_hud(float fps) {
    rect(SCR_W / 2 - 6, SCR_H / 2 - 1, 12, 2, RGBA(255, 255, 255, 200));
    rect(SCR_W / 2 - 1, SCR_H / 2 - 6, 2, 12, RGBA(255, 255, 255, 200));
    if (state == ST_PLAY) { /* eldeki esya / kol: vurma, esya degistirme ve yururken sallanma animasyonu */
        float sw = sinf(swingP * 3.14159f), eq = equipT * equipT, bs = clampf(handSpd / 5.0f, 0, 1);
        float bx = sinf(bobT) * 3.0f * bs, by = fabsf(cosf(bobT)) * 3.0f * bs;
        u8 hid = inv[hot].id;
        float px = SCR_W - 120 + bx - sw * 46, py = SCR_H - 22 + by + eq * 80 - sw * 10;   /* el (tutma noktasi) */
        float ang = -0.2f + sw * 1.15f, cs = cosf(ang), sn = sinf(ang);
        if (hid >= PORK) {
            float ox = 42, oy = -42;                                 /* karenin merkezi, tutma noktasina gore */
            tquad(px + ox * cs - oy * sn, py + ox * sn + oy * cs, 84, 84, ang, item_tile(hid), 0xFFFFFFFF);
        } else if (hid) {
            float cx = px + 8 - sw * 20, cy = py - 40 + sw * 12;
            tquad(cx, cy + 8, 56, 44, ang * 0.35f, TILE[hid][1], RGBA(205, 205, 205, 255));
            tquad(cx, cy - 20, 56, 22, ang * 0.35f, TILE[hid][0], 0xFFFFFFFF);
        } else {
            float ox = 14, oy = -50;
            tquad(px + ox * cs - oy * sn, py + ox * sn + oy * cs, 28, 100, ang * 0.7f, -1, RGBA(200, 150, 110, 255));
            tquad(px + ox * cs - (oy + 30) * sn, py + ox * sn + (oy + 30) * cs, 28, 40, ang * 0.7f, -1, RGBA(60, 140, 200, 255));
        }
    }
    int hx = (SCR_W - 9 * 24) / 2, hy = SCR_H - 28;
    for (int i = 0; i < 9; i++) slot_draw(hx + i * 24, hy, 22, &inv[i], i == hot, 0);
    if (nameT > 0 && inv[hot].id) ctext(SCR_W / 2, hy - 28 - (mode == 1 ? 12 : 0), 1, 0xFFFFFFFF, BI[inv[hot].id].n);
    if (mode == 1) {
        for (int i = 0; i < 10; i++) {
            int x = hx + i * 9, y = hy - 11;
            rect(x, y, 8, 7, RGBA(60, 0, 0, 220));
            if (hp >= i * 2 + 2) rect(x, y, 8, 7, RGBA(230, 30, 30, 255));
            else if (hp >= i * 2 + 1) rect(x, y, 4, 7, RGBA(230, 30, 30, 255));
        }
        for (int i = 0; i < 10; i++) { /* aclik */
            int x = hx + 216 - 9 - i * 9, y = hy - 11;
            rect(x, y, 8, 7, RGBA(50, 30, 10, 220));
            if (food >= i * 2 + 2) rect(x, y, 8, 7, RGBA(200, 120, 40, 255));
            else if (food >= i * 2 + 1) rect(x + 4, y, 4, 7, RGBA(200, 120, 40, 255));
        }
        if (headW || airt < 10)
            for (int i = 0; i < (int)ceilf(airt); i++) rect(hx + 216 - 9 - i * 9, hy - 20, 7, 7, RGBA(150, 200, 255, 230));
    }
    if (brk_p > 0 && brk_x >= 0) {
        float need = break_time(gb(brk_x, brk_y, brk_z));
        if (need > 0) { rect(SCR_W / 2 - 20, SCR_H / 2 + 14, 40, 3, RGBA(0, 0, 0, 160)); rect(SCR_W / 2 - 20, SCR_H / 2 + 14, (int)(40 * clampf(brk_p / need, 0, 1)), 3, 0xFF00FF00); }
    }
    if (eatT > 0) { rect(SCR_W / 2 - 20, SCR_H / 2 + 22, 40, 3, RGBA(0, 0, 0, 160)); rect(SCR_W / 2 - 20, SCR_H / 2 + 22, (int)(40 * clampf(eatT / 1.6f, 0, 1)), 3, 0xFF33AAFF); }
    char b[96];
    int hh = (int)(dayT * 24 + 6) % 24, mm = (int)(dayT * 24 * 60) % 60;
    snprintf(b, sizeof(b), "X%d Y%d Z%d  %02d:%02d  FPS %d", (int)pl.x, (int)pl.y, (int)pl.z, hh, mm, (int)fps);
    ptext(4, 4, 1, 0xFFFFFFFF, b);
    ptext(4, 12, 1, 0xFFAAFFAA, mode == 0 ? (flying ? "YARATICI - UCUYOR" : "YARATICI") : "HAYATTA KALMA");
    if (msgT > 0) ctext(SCR_W / 2, 40, 2, 0xFF55FFFF, msg);
}

static const char *MENU_N[10] = {"DEVAM ET", "OYUN MODU", "ZAMAN", "GORUS MESAFESI", "BAKIS HIZI",
                                 "KONTROLLER", "DUNYAYI KAYDET", "DUNYAYI YUKLE", "YENI DUNYA", "OYUNDAN CIK"};
static void draw_menu(void) {
    rect(0, 0, SCR_W, SCR_H, RGBA(0, 0, 0, 150));
    ctext(SCR_W / 2, 14, 3, 0xFF55FF55, "MINICRAFT PSP");
    for (int i = 0; i < 10; i++) {
        int y = 52 + i * 20;
        char b[64];
        const char *tt = MENU_N[i];
        if (i == 1) snprintf(b, sizeof(b), "%s: %s", tt, mode ? "HAYATTA KALMA" : "YARATICI");
        else if (i == 2) snprintf(b, sizeof(b), "%s: %s", tt, timeMode == 0 ? "DONGU" : (timeMode == 1 ? "GUNDUZ" : "GECE"));
        else if (i == 3) snprintf(b, sizeof(b), "%s: < %d >", tt, rdist);
        else if (i == 4) snprintf(b, sizeof(b), "%s: < %d >", tt, lookSens);
        else snprintf(b, sizeof(b), "%s", tt);
        if (i == menuSel) rect(SCR_W / 2 - 130, y - 3, 260, 17, RGBA(255, 255, 255, 60));
        ctext(SCR_W / 2, y, 2, i == menuSel ? 0xFF55FFFF : 0xFFFFFFFF, b);
    }
    ctext(SCR_W / 2, SCR_H - 10, 1, 0xFFAAAAAA, "D-PAD: SEC   X: ONAY   O: GERI");
}
static void draw_help(void) {
    rect(0, 0, SCR_W, SCR_H, RGBA(0, 0, 0, 200));
    ctext(SCR_W / 2, 10, 2, 0xFF55FF55, "KONTROLLER");
    static const char *L[] = {
        "ANALOG: YURU", "UCGEN / X: YUKARI / ASAGI BAK", "KARE / DAIRE: SOLA / SAGA BAK",
        "START: ZIPLA (CIFT BAS: UCUS)", "SELECT 2 KEZ: MENU", "SELECT + KARE: ESYA AT",
        "SELECT + UCGEN: ENVANTER", "L: KIR / SALDIR", "R: BLOK KOY / YE / CAKMAKLA YAK",
        "D-PAD SOL/SAG: HOTBAR", "D-PAD YUKARI: KOSU", "D-PAD ASAGI: COMEL (UCARKEN ALCAL)"
    };
    for (int i = 0; i < 12; i++) ptext(30, 34 + i * 16, 1, 0xFFFFFFFF, L[i]);
    ctext(SCR_W / 2, SCR_H - 14, 1, 0xFFAAAAAA, "GERI: HERHANGI BIR TUS");
}

static void draw_inv(void) {
    rect(0, 0, SCR_W, SCR_H, RGBA(0, 0, 0, 170));
    const char *t0 = mode == 0 ? "BLOKLAR" : "URETIM";
    rect(SCR_W / 2 - 110, 8, 105, 16, invTab == 0 ? RGBA(255,255,255,90) : RGBA(255,255,255,25));
    rect(SCR_W / 2 + 5, 8, 105, 16, invTab == 1 ? RGBA(255,255,255,90) : RGBA(255,255,255,25));
    ctext(SCR_W / 2 - 57, 12, 1, 0xFFFFFFFF, t0);
    ctext(SCR_W / 2 + 57, 12, 1, 0xFFFFFFFF, "ENVANTER");
    ctext(SCR_W / 2, 28, 1, 0xFFAAAAAA, "L / R: SEKME   X: SEC   O: KAPAT");
    if (invTab == 1) {
        for (int c = 0; c < 36; c++) {
            int row = c / 9, col = c % 9;
            int x = 100 + col * 31, y = 50 + row * 31 + (row == 3 ? 8 : 0);
            int s = vis_to_slot(c);
            slot_draw(x, y, 28, &inv[s], c == invCur, s == invHeld);
        }
        int s = vis_to_slot(invCur);
        if (inv[s].id) ctext(SCR_W / 2, 200, 2, 0xFFFFFF55, BI[inv[s].id].n);
    } else if (mode == 0) {
        for (int i = 0; i < NB - 1; i++) {
            int x = 60 + (i % 12) * 30, y = 46 + (i / 12) * 30;
            Slot sl = {(u8)(i + 1), 64};
            slot_draw(x, y, 28, &sl, i == palCur, 0);
        }
        ctext(SCR_W / 2, 200, 2, 0xFFFFFF55, BI[palCur + 1].n);
        for (int i = 0; i < 9; i++) slot_draw(132 + i * 24, 236, 22, &inv[i], i == hot, 0);
    } else {
        int r0 = recCur - 4;
        if (r0 > NREC - 8) r0 = NREC - 8;
        if (r0 < 0) r0 = 0;
        for (int i = r0; i < r0 + 8 && i < NREC; i++) {
            const Rec *r = &REC[i];
            int have = count_item(r->a) >= r->an && (!r->b || count_item(r->b) >= r->bn);
            int ok = have && st_ok(r->st);
            int y = 44 + (i - r0) * 22;
            if (i == recCur) rect(60, y - 3, 360, 20, RGBA(255, 255, 255, 60));
            char b[96];
            if (r->b) snprintf(b, sizeof(b), "%d %s + %d %s = %d %s", r->an, BI[r->a].n, r->bn, BI[r->b].n, r->on, BI[r->out].n);
            else snprintf(b, sizeof(b), "%d %s = %d %s", r->an, BI[r->a].n, r->on, BI[r->out].n);
            if (r->st) strncat(b, r->st == 1 ? " (MASA)" : " (OCAK)", sizeof(b) - strlen(b) - 1);
            ptext(70, y, 2, ok ? 0xFF55FF55 : (have ? 0xFF3399FF : 0xFF7777FF), b);
        }
    }
}

/* ---------------- ana dongu ---------------- */
static void frame(float dt, float fps) {
    /* gokyuzu */
    u8 dc[3] = {135, 206, 235}, nc[3] = {6, 8, 22};
    u8 sk[3];
    for (int i = 0; i < 3; i++) sk[i] = (u8)lerp(nc[i], dc[i], dayf);
    u32 skyc = RGBA(sk[0], sk[1], sk[2], 255);
    sceGuStart(GU_DIRECT, list);
    sceGuDisable(GU_TEXTURE_2D); setup_tex();
    sceGuClearColor(skyc);
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    sceGuDisable(GU_DEPTH_TEST); sceGuDisable(GU_FOG);
    sceGuEnable(GU_BLEND);
    if (dayf < 0.9f) {
        int sa = (int)((1 - dayf) * 255);
        for (int i = 0; i < 50; i++) rect((int)(hf(i, 1, 1) * 480), (int)(hf(i, 2, 2) * 150), 2, 2, RGBA(255, 255, 255, sa));
    }
    float a = dayT * 2 * PI;
    int sx = (int)(240 - cosf(a) * 190), sy = (int)(170 - sinf(a) * 150);
    if (sy < 170) { rect(sx - 14, sy - 14, 28, 28, RGBA(255, 220, 80, 255)); rect(sx - 9, sy - 9, 18, 18, RGBA(255, 250, 180, 255)); }
    int mx = (int)(240 + cosf(a) * 190), my = (int)(170 + sinf(a) * 150);
    if (my < 170) rect(mx - 10, my - 10, 20, 20, RGBA(225, 230, 240, 255));
    flush2d();

    /* 3D */
    sceGuEnable(GU_DEPTH_TEST); sceGuDisable(GU_BLEND);
    sceGuEnable(GU_FOG);
    float fogfar = headW ? 18.0f : rdist * CS - 4.0f;
    sceGuFog(fogfar * 0.45f, fogfar, headW ? RGBA(20, 60, 160, 255) : skyc);
    sceGumMatrixMode(GU_PROJECTION); sceGumLoadIdentity();
    sceGumPerspective(70.0f + fovAdd, 16.0f / 9.0f, 0.25f, 300.0f);
    sceGumMatrixMode(GU_VIEW); sceGumLoadIdentity();
    sceGumRotateX(-pitch); sceGumRotateY(-yaw);
    ScePspFVector3 tr = {-pl.x, -(pl.y + eyeH), -pl.z};
    sceGumTranslate(&tr);
    sceGumMatrixMode(GU_MODEL); sceGumLoadIdentity();

    int rebuilt = 0;
    for (int i = 0; i < NCX * NCZ && rebuilt < 4; i++)
        if (ch[i].dirty) { build_chunk(i % NCX, i / NCX); rebuilt++; }
    if (rebuilt) sceKernelDcacheWritebackAll();

    float fx = -sinf(yaw), fz = -cosf(yaw);
    int vischunks[NCX * NCZ], nv = 0;
    for (int cz = 0; cz < NCZ; cz++) for (int cx = 0; cx < NCX; cx++) {
        float dx = cx * CS + 8 - pl.x, dz = cz * CS + 8 - pl.z, d2 = dx * dx + dz * dz, R = rdist * CS + 12;
        if (d2 > R * R) continue;
        if (dx * fx + dz * fz < -12.0f) continue;
        vischunks[nv++] = cz * NCX + cx;
    }
    sceGuEnable(GU_TEXTURE_2D);
    for (int i = 0; i < nv; i++) draw_vb(&ch[vischunks[i]].op);
    sceGuDisable(GU_TEXTURE_2D);
    draw_clouds();
    draw_mobs();
    draw_items();
    draw_ptnt();
    draw_particles();
    if (state == ST_PLAY) {
        Ray r = cast(5.0f);
        if (r.hit) outline(r.x, r.y, r.z);
        if (mode == 1 && brk_p > 0 && brk_x >= 0) { /* kirilma catlaklari */
            float need = break_time(gb(brk_x, brk_y, brk_z));
            int st = need > 0 ? (int)(brk_p / need * 10) : 0;
            if (st > 9) st = 9;
            sceGuEnable(GU_BLEND); sceGuEnable(GU_TEXTURE_2D);
            box_tile = T_CRACK + st;
            box(brk_x + .5f, brk_y + .5f, brk_z + .5f, .505f, .505f, .505f, 0, 255, 255, 255);
            box_tile = -1;
            sceGuDisable(GU_TEXTURE_2D);
        }
    }
    sceGuEnable(GU_BLEND); sceGuDepthMask(GU_TRUE);
    sceGuEnable(GU_TEXTURE_2D);
    for (int i = 0; i < nv; i++) draw_vb(&ch[vischunks[i]].tr);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDepthMask(GU_FALSE);

    /* 2D */
    sceGuDisable(GU_DEPTH_TEST); sceGuDisable(GU_FOG);
    if (dayf < 0.95f) rect(0, 0, SCR_W, SCR_H, RGBA(5, 8, 40, (int)((1 - dayf) * 105)));
    if (headW) rect(0, 0, SCR_W, SCR_H, RGBA(20, 60, 160, 110));
    if (inv_t > 0.25f) rect(0, 0, SCR_W, SCR_H, RGBA(255, 0, 0, 70));
    draw_hud(fps);
    if (state == ST_MENU) draw_menu();
    else if (state == ST_INV) draw_inv();
    else if (state == ST_HELP) draw_help();
    else if (state == ST_DEAD) {
        rect(0, 0, SCR_W, SCR_H, RGBA(120, 0, 0, 160));
        ctext(SCR_W / 2, 100, 4, 0xFFFFFFFF, "OLDUN!");
        ctext(SCR_W / 2, 150, 2, 0xFFFFFFFF, "X: YENIDEN DOGUN");
    }
    flush2d();
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

static void menu_action(void) {
    switch (menuSel) {
    case 0: state = ST_PLAY; break;
    case 1: set_mode(!mode); break;
    case 2: timeMode = (timeMode + 1) % 3; break;
    case 5: state = ST_HELP; break;
    case 6: say(save_world() ? "DUNYA KAYDEDILDI" : "KAYIT BASARISIZ"); state = ST_PLAY; break;
    case 7: say(load_world() ? "DUNYA YUKLENDI" : "KAYIT BULUNAMADI"); state = ST_PLAY; break;
    case 8: new_game((u32)time(NULL)); state = ST_PLAY; say("YENI DUNYA"); break;
    case 9: sceKernelExitGame(); break;
    }
}

static void show_loading(void) {
    sceGuStart(GU_DIRECT, list);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuClearColor(RGBA(20, 20, 30, 255));
    sceGuClear(GU_COLOR_BUFFER_BIT);
    sceGuDisable(GU_DEPTH_TEST); sceGuEnable(GU_BLEND);
    ctext(SCR_W / 2, 120, 3, 0xFF55FF55, "MINICRAFT PSP");
    ctext(SCR_W / 2, 160, 2, 0xFFFFFFFF, "DUNYA OLUSTURULUYOR...");
    flush2d();
    sceGuFinish(); sceGuSync(0, 0); sceDisplayWaitVblankStart(); sceGuSwapBuffers();
}
static void title_frame(void) {
    static const char *TN[4] = {"HAYATTA KALMA", "YARATICI MOD", "KAYITLI DUNYAYI YUKLE", "CIKIS"};
    sceGuStart(GU_DIRECT, list);
    sceGuDisable(GU_TEXTURE_2D); setup_tex();
    sceGuClearColor(RGBA(120, 185, 235, 255));
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    sceGuDisable(GU_DEPTH_TEST); sceGuDisable(GU_FOG); sceGuEnable(GU_BLEND);
    rect(0, 120, SCR_W, 80, RGBA(170, 215, 245, 255));
    for (int i = 0; i < 15; i++) {
        tspr(i * 32, 206, 32, 32, T_GRASS_S, 0xFFFFFFFF);
        tspr(i * 32, 238, 32, 32, ((i * 7 + 3) % 5 == 0) ? T_COAL : T_DIRT, 0xFFFFFFFF);
    }
    tspr(30, 22, 56, 56, item_tile(TOOL(0, 3)), 0xFFFFFFFF);
    tspr(SCR_W - 86, 22, 56, 56, item_tile(TOOL(3, 3)), 0xFFFFFFFF);
    ctext(SCR_W / 2, 24, 6, 0xFF55DDFF, "MINICRAFT");
    ctext(SCR_W / 2, 70, 2, 0xFFFFFFFF, "PSP SURUMU");
    rect(110, 98, 260, 96, RGBA(0, 0, 0, 120));
    for (int i = 0; i < 4; i++) {
        int y = 104 + i * 22;
        if (i == titSel) rect(120, y - 3, 240, 18, RGBA(255, 255, 255, 70));
        ctext(SCR_W / 2, y, 2, i == titSel ? 0xFF55FFFF : 0xFFFFFFFF, TN[i]);
    }
    if (titMsg) ctext(SCR_W / 2, 198, 1, 0xFF5555FF, "KAYIT BULUNAMADI");
    ctext(SCR_W / 2, SCR_H - 10, 1, 0xFFFFFFFF, "D-PAD: SEC   X: ONAY");
    flush2d();
    sceGuFinish(); sceGuSync(0, 0); sceDisplayWaitVblankStart(); sceGuSwapBuffers();
}

int main(void) {
    setup_callbacks();
    init_gu();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    gen_tex();
    state = ST_TITLE; mode = 0;

    SceCtrlData pad; memset(&pad, 0, sizeof(pad));
    u32 prev = 0;
    u64 last = sceKernelGetSystemTimeWide();
    float fpsAcc = 0, fps = 60; int fpsN = 0;
    int selPrev = 0, selCombo = 0; float selHold = 0, lastTap = -9;

    while (1) {
        u64 now = sceKernelGetSystemTimeWide();
        float dt = (now - last) / 1000000.0f;
        last = now;
        if (dt > 0.05f) dt = 0.05f;
        if (dt < 0.001f) dt = 0.001f;
        gt += dt;
        fpsAcc += dt; fpsN++;
        if (fpsAcc >= 0.5f) { fps = fpsN / fpsAcc; fpsAcc = 0; fpsN = 0; }

        sceCtrlPeekBufferPositive(&pad, 1);
        u32 B = pad.Buttons, pr = B & ~prev;
        int selDown = (B & PSP_CTRL_SELECT) != 0;

        /* SELECT: cift tik = menu, basili + kare = at, basili + ucgen = envanter */
        if (selDown && !selPrev) { selHold = 0; selCombo = 0; }
        if (selDown) {
            selHold += dt;
            if (pr & PSP_CTRL_SQUARE) { if (state == ST_PLAY) drop_hand(); else if (state == ST_INV && invTab == 1) drop_slot(vis_to_slot(invCur)); selCombo = 1; }
            if (pr & PSP_CTRL_TRIANGLE) {
                if (state == ST_PLAY) { state = ST_INV; invHeld = -1; invTab = mode == 0 ? 0 : 1; }
                else if (state == ST_INV) state = ST_PLAY;
                selCombo = 1;
            }
        }
        if (!selDown && selPrev && !selCombo && selHold < 0.35f) {
            if (gt - lastTap < 0.4f) {
                if (state == ST_PLAY || state == ST_INV) { state = ST_MENU; menuSel = 0; }
                else if (state == ST_MENU || state == ST_HELP) state = ST_PLAY;
                lastTap = -9;
            } else lastTap = gt;
        }
        selPrev = selDown;
        if (selDown) pr &= ~(PSP_CTRL_SQUARE | PSP_CTRL_TRIANGLE);

        if (msgT > 0) msgT -= dt;
        if (nameT > 0) nameT -= dt;

        switch (state) {
        case ST_PLAY:
            update_time(dt);
            update_player(dt, &pad, pr);
            update_mobs(dt);
            update_items(dt);
            update_fire(dt); update_ptnt(dt); update_particles(dt);
            break;
        case ST_TITLE:
            if (pr & PSP_CTRL_UP) titSel = (titSel + 3) % 4;
            if (pr & PSP_CTRL_DOWN) titSel = (titSel + 1) % 4;
            if (pr & PSP_CTRL_CROSS) {
                titMsg = 0;
                if (titSel <= 1) {
                    mode = titSel == 0 ? 1 : 0;
                    show_loading(); new_game((u32)time(NULL)); state = ST_PLAY;
                    say(mode ? "HAYATTA KAL: AGAC KIR!" : "YARATICI MOD");
                } else if (titSel == 2) {
                    show_loading();
                    if (load_world()) { state = ST_PLAY; say("DUNYA YUKLENDI"); } else titMsg = 1;
                } else sceKernelExitGame();
            }
            break;
        case ST_MENU:
            if (pr & PSP_CTRL_UP) menuSel = (menuSel + 9) % 10;
            if (pr & PSP_CTRL_DOWN) menuSel = (menuSel + 1) % 10;
            if (pr & (PSP_CTRL_LEFT | PSP_CTRL_RIGHT)) {
                int d = (pr & PSP_CTRL_RIGHT) ? 1 : -1;
                if (menuSel == 1) set_mode(!mode);
                else if (menuSel == 2) timeMode = (timeMode + 3 + d) % 3;
                else if (menuSel == 3) { rdist += d; if (rdist < 3) rdist = 3; if (rdist > 7) rdist = 7; }
                else if (menuSel == 4) { lookSens += d; if (lookSens < 1) lookSens = 1; if (lookSens > 5) lookSens = 5; }
            }
            if (pr & PSP_CTRL_CROSS) menu_action();
            if (pr & PSP_CTRL_CIRCLE) state = ST_PLAY;
            break;
        case ST_HELP:
            if (pr & (PSP_CTRL_CROSS | PSP_CTRL_CIRCLE | PSP_CTRL_START)) state = ST_MENU;
            break;
        case ST_DEAD:
            if (pr & PSP_CTRL_CROSS) respawn();
            break;
        case ST_INV:
            if (pr & (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER)) invTab = !invTab;
            if (invTab == 1) {
                int r = invCur / 9, c = invCur % 9;
                if (pr & PSP_CTRL_UP) r = (r + 3) % 4;
                if (pr & PSP_CTRL_DOWN) r = (r + 1) % 4;
                if (pr & PSP_CTRL_LEFT) c = (c + 8) % 9;
                if (pr & PSP_CTRL_RIGHT) c = (c + 1) % 9;
                invCur = r * 9 + c;
                if (pr & PSP_CTRL_CROSS) {
                    int s = vis_to_slot(invCur);
                    if (invHeld < 0) { if (inv[s].id) invHeld = s; }
                    else { Slot t = inv[s]; inv[s] = inv[invHeld]; inv[invHeld] = t; invHeld = -1; }
                }
                if (pr & PSP_CTRL_CIRCLE) { if (invHeld >= 0) invHeld = -1; else state = ST_PLAY; }
            } else if (mode == 0) {
                int n = NB - 1;
                if (pr & PSP_CTRL_LEFT) palCur = (palCur + n - 1) % n;
                if (pr & PSP_CTRL_RIGHT) palCur = (palCur + 1) % n;
                if (pr & PSP_CTRL_UP) palCur = palCur >= 12 ? palCur - 12 : palCur;
                if (pr & PSP_CTRL_DOWN) palCur = palCur + 12 < n ? palCur + 12 : palCur;
                if (pr & PSP_CTRL_CROSS) { inv[hot].id = palCur + 1; inv[hot].n = 64; inv[hot].d = 0; }
                if (pr & PSP_CTRL_SQUARE) { hot = (hot + 1) % 9; }
                if (pr & PSP_CTRL_CIRCLE) state = ST_PLAY;
            } else {
                if (pr & PSP_CTRL_UP) recCur = (recCur + NREC - 1) % NREC;
                if (pr & PSP_CTRL_DOWN) recCur = (recCur + 1) % NREC;
                if (pr & PSP_CTRL_CROSS) {
                    const Rec *r = &REC[recCur];
                    if (!st_ok(r->st)) say(r->st == 1 ? "CALISMA MASASI GEREKLI" : "OCAK GEREKLI");
                    else if (count_item(r->a) >= r->an && (!r->b || count_item(r->b) >= r->bn)) {
                        remove_item(r->a, r->an);
                        if (r->b) remove_item(r->b, r->bn);
                        add_item(r->out, r->on);
                    } else say("MALZEME YETERSIZ");
                }
                if (pr & PSP_CTRL_CIRCLE) state = ST_PLAY;
            }
            break;
        }
        prev = B;
        if (state == ST_TITLE) title_frame(); else frame(dt, fps);
    }
    return 0;
}
