/* Overridden whole (port/units.toml); unported NuRndr* are stubs whose hits are the renderer's worklist. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <SDL3/SDL.h>

#include "app.h"
#include "data/image_tables.h"
#include "game/creature.h"
#include "game/gametypes.h"
#include "gfx/interp.h"
#include "gfx/rndr.h"
#include "nu3d/nucamera.h"
#include "nu3d/nugobj.h"
#include "nu3d/nugscn.h"
#include "nu3d/nuhgobj.h"
#include "nu3d/numtl.h"
#include "nu3d/nurndr.h"
#include "nu3d/nustream.h"
#include "gamelib/debris.h"
#include "nu3d/nutex.h"
#include "nu3d/nutexanm.h"
#include "nu3d/nuvport.h"
#include "numath/numtx.h"
#include "platform/log.h"
#include "numath/nutrig.h"
#include "numath/nuvec.h"
#include "nups2/nups2types.h"
#include "port/wide.h"

int NuPs2ScreenSwap(int vsync);
void NuCameraSet(struct nucamera_s *cam);
void NuRndrSetRTarget(struct rndrstream_s *stream, int target);
void NuRndrSetZBuffer(struct rndrstream_s *stream, int zbuffer);
void NuPs2DisableAutoScreenSwap(void);
void NuFntInit(void);
int Ps2TxValToPowerOfTwo(int value);

float XFormVuTriZ(float *depth, void *tri, struct numtx_s *mtx, int count);
void NuMtxMulVU0(struct numtx_s *dst, struct numtx_s *a, struct numtx_s *b);
void NuMtxMulArrayVU0(struct numtx_s *dst, struct numtx_s *a, struct numtx_s *b, int count);
struct NUJOINTANIM_s;
void NuHGobjEval(struct NUHGOBJ_s *hobj, int njanims, struct NUJOINTANIM_s *janim, struct numtx_s *mtx_array);

extern int nurndr_tritot_this_frame;
extern int nurndr_pixel_width;
extern int nurndr_pixel_height;
extern int nurndr_pixel_p2w;
extern int nurndr_pixel_p2h;
extern int PS2_REZ_W;
extern int PS2_REZ_H;
extern int PS2_VCNTR_X;
extern int PS2_VCNTR_Y;
extern struct numtl_s *numtl_defaultmtl2d;
extern struct rndrstream_s *rndrstream_2d;
extern struct rndrstream_s *rndrstream_3d;
extern struct numtl_s *nurndr_forced_mtl;
extern struct numtl_s **nurndr_forced_mtl_table;
extern struct numtl_s *NuLightAddMat;
extern struct numtl_s *NuLightAddMat2;
extern struct numtl_s *NuLgtGreyNZMat;
extern struct numtl_s *NuLgtSurf[18];
extern int NuLgtHSurf[18];
extern u32 curlightcol;
extern struct nuvec_s NuRndrShadBox[2];
extern int txupload_on;
extern struct nucamera_s global_camera;
extern struct nutexslot_s *tinfo;
#define TexList tinfo

extern struct rndrstream_s *rndrstream_system;
extern struct rndrstream_s *rndrstream_ot_sort;
extern struct rndrstream_s *rndrstream_fx;
extern struct rndrstream_s *rndrstream_ptl;
extern u8 *rndrstream_free;
extern struct numtx_s numtx_identity;
extern NuRndrShadType NuRndrShadPolDat[];
extern int NuRndrShadowCnt;
extern struct nurndrwaterrip_s NuRndrWaterRipDat[];
extern int NuRndrWaterRipCnt;
extern struct nurndrfoot_s NuRndrFootData[];
extern int shadstorecnt;
extern struct nuvec_s curlight;
extern float NuTrigTable[];
extern float NuRndrWaterLevel;
extern u32 NuRndrWaterTint;
extern struct numtl_s *NuLgtBackBuff;
extern struct rndrstream_s *rndrstream_3d_u;

void NuVec4MtxTransformVU0(struct nuvec4_s *dst, struct nuvec4_s *src, struct numtx_s *mtx);
void NuVec4ScaleVU0(struct nuvec4_s *dst, struct nuvec4_s *src, float scale);
void *vpDmaTag_Next(void *packet, uint32_t addr);
void NuRndrStreamLink(struct rndrstream_s *stream);
void NuRndrShadowDirCol(struct nuvec_s *dir, u32 col);
void NuRndrShadowStack(struct nuvec_s *light);
struct nurndrshadface_s;
void NuRndrShadowCylinderCap(struct numtx_s *mtx, struct NUCYLINDER_s *cyl, int count, struct nuvec_s *light);
void NuRndrShadowEllipse(struct numtx_s *mtx, struct NUELLIPSOID_s *ellipse, int count, struct nuvec_s *light);
void NuRndrShadowPols(struct numtx_s *mtx, struct nurndrshadface_s *face, struct nurndrshadface_s *vtx,
                      struct nuvec_s *light);
void RotateCyl(struct nuvec_s *v, float xang, float yang);
void DeRotateVectorAngles(struct nuvec_s *dir, float *xang, float *yang);

#define NW_OT_BINS 0x1000
#define NW_OT_ITEMS 0x400
#define NW_OT_TRIS 0x800 /* a batch's most, the per-triangle buckets' slots */

extern u32 testfootcol; /* a debug colour over every footprint's; 0 in retail */
extern int testflipold;
extern struct nuvec_s lightfan[9];
extern struct nurndrshadstore_s shadstore[256];
static struct nuvec_s default_light_dir = { 0.0f, -0.75f, -0.75f }; /* retail's data */
/* an effect list over every particle group's; NULL in retail. The decomp's nurndrptlhead_s view of a debdma_s. */
extern struct debdma_s *testforcepart;
extern float testmultl; /* the volume shadows' light's length over NuRndrShadowStencil's */
extern float waveampx;  /* NuRndrWater's cell corners' sway, 1/16 pixels */
extern float waveampy;

static const float otscale = 2.0f;
static NwRecord *ot_bins[NW_OT_BINS];
static int ot_batch_free;

void NuRndrOTInit(void)
{
    ot_batch_free = NW_OT_ITEMS;
    memset(ot_bins, 0, sizeof(ot_bins));
}

/* NuRndrOTInsertItem: bin depth * otscale, the latest first in its bin. */
void nw_rndr_ot_insert(NwRecord *item, float depth)
{
    int index = (int)(depth * otscale);
    index = index < 0 ? 0 : index > NW_OT_BINS - 1 ? NW_OT_BINS - 1 : index;
    item->next = ot_bins[index];
    ot_bins[index] = item;
    ot_batch_free--;
}

/* Back to front: bin 0xFFF first, each bin from its latest item, under the view and lights of the flush. */
void NuRndrOTFlush(void)
{
    if (ot_batch_free == NW_OT_ITEMS) {
        return;
    }
    for (int bin = NW_OT_BINS - 1; bin >= 0; bin--) {
        NwRecord *item = ot_bins[bin];
        while (item) {
            NwRecord *next = item->next;
            nw_rndr_latch(item);
            nw_rndr_push(rndrstream_ot_sort, item);
            item = next;
        }
        ot_bins[bin] = NULL;
    }
    ot_batch_free = NW_OT_ITEMS;
}

/* Buckets by view z (XFormVuTriZ), far to near; retail drops a batch over 0x800 triangles or with no item left. */
void nw_rndr_ot_batch(struct numtx_s *mtx, struct numtl_s *mtl, const NwGpuMesh *mesh, struct nutristream_s *batch)
{
    static float depth[NW_OT_TRIS];
    static uint16_t bucket[NW_OT_TRIS];
    static uint16_t first[NW_OT_BINS], next[NW_OT_TRIS];
    int count = *(int *)batch;
    if (count <= 0 || count > NW_OT_TRIS || !ot_batch_free) {
        return;
    }
    struct numtx_s caminv, local;
    NuMtxInv(&caminv, &global_camera.mtx);
    NuMtxMulH(&local, mtx, &caminv);
    uint8_t *tris = (uint8_t *)batch + 16;
    float batchz = XFormVuTriZ(&depth[count - 1], tris + (size_t)(count - 1) * 0xA0, &local, count);
    int highest = 0;
    memset(first, 0xFF, sizeof(first));
    for (int t = count - 1; t >= 0; t--) {
        int z = (int)depth[t];
        z = z < 0 ? 0 : z > NW_OT_BINS - 1 ? NW_OT_BINS - 1 : z;
        bucket[t] = (uint16_t)z;
        next[t] = first[z];
        first[z] = (uint16_t)t;
        highest = z > highest ? z : highest;
    }
    NwRecord *item = nw_rndr_record(NW_CMD_DRAW_OT, mtl, mtl->rs.mpg, 1);
    uint32_t *idx = nw_rndr_alloc((size_t)count * 3 * sizeof(*idx));
    uint32_t n = 0;
    for (int z = highest; z >= 0; z--) {
        for (uint16_t t = first[z]; t != 0xFFFF; t = next[t]) {
            for (int k = 0; k < 3; k++) {
                idx[n++] = mesh->mesh.tris[t * 3 + k];
            }
        }
    }
    item->cmd.mesh = mesh;
    item->cmd.count = n;
    item->idx = idx;
    nw_rndr_ot_insert(item, batchz);
}

/* A NULL weights array stands for retail's default blend-deformer weights. */
void NuRndrInitEx(int streamsize)
{
    NuRndrStreamInit(streamsize);
    NuFntInit();
    NuRndrOTInit();
    NuRndrShadowDirCol(&default_light_dir, 0x10808080);
    NuRndrShadowCnt = 0;
    for (int i = 0; i < 32; i++) {
        NuRndrWaterRipDat[i].timer = 0;
    }
}

u32 *NuRndrShadowLightFan(void)
{
    struct nuvec_s angles;
    DeRotateVectorAngles(&curlight, &angles.x, &angles.y);
    for (int i = 0; i < 8; i++) {
        lightfan[i].x = NuTrigTable[(i * 0x2000 + 0x4000) & 0xFFFF];
        lightfan[i].y = -NuTrigTable[(i * 0x2000) & 0xFFFF];
        lightfan[i].z = 0.0f;
        RotateCyl(&lightfan[i], angles.x, angles.y);
    }
    return NULL;
}

/* Retail counts in NuLgtHSurf[2]'s colour from 0x7F; here the stencil counts and the mask is grey at 0x80 or more. */
void NuRndrShadow(int mode)
{
    struct nuvec_s light = curlight;
    NwVuSpriteVertex *v;
    (void)mode;
    if ((testflipold & 4) || shadstorecnt == 0) {
        return;
    }
    NuRndrStreamLink(rndrstream_system);
    NuRndrSetRTarget(rndrstream_system, NuLgtHSurf[2]);
    NuRndrSetZBuffer(rndrstream_system, NuLgtHSurf[1]);
    rndrstream_free = vpDmaTag_Next(rndrstream_system->ptr.u8, 0);
    NuRndrSetXYOffset((0x800 - (nurndr_pixel_width >> 1)) << 4, (0x800 - (nurndr_pixel_height >> 1)) << 4);
    NuRndrSetScissor(0, 0, nurndr_pixel_width - 1, nurndr_pixel_height - 1);
    txupload_on = 0;
    NuRndrStreamLink(rndrstream_fx);
    NuRndrStreamSetMisc(rndrstream_fx);
    NuRndrStreamAddMtl(rndrstream_fx, NuLgtGreyNZMat);
    nw_rndr_cmd(rndrstream_fx, NW_CMD_VOLUMES)->volumes.width = (int)ceilf((float)PS2_REZ_W * 0.4f);
    rndrstream_free = vpDmaTag_Next(rndrstream_fx->ptr.u8, 0);
    light.x *= testmultl;
    light.y *= testmultl;
    light.z *= testmultl;
    NuRndrShadowStack(&light);
    NuRndrStreamLink(rndrstream_fx);
    NuRndrStreamSetMisc(rndrstream_fx);
    NuRndrStreamAddMtl(rndrstream_fx, NuLgtGreyNZMat);
    static const float none[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    for (int shadowed = 0; shadowed < 2; shadowed++) {
        const int cols = ((int)ceilf((float)PS2_REZ_W * 0.4f) + 63) >> 6;
        NwCmd *c = nw_rndr_verts(rndrstream_fx, NW_VERTS_GS, (uint32_t)cols * 6, &v);
        c->stencil = (int8_t)(shadowed ? NW_STENCIL_SHADOWED : 0);
        for (int i = 0; i < cols; i++) {
            v = nw_rndr_rect3d_quad(v, (i << 10) - 8, -8, 0x400, PS2_REZ_H << 4, none,
                                    shadowed ? (int)0x80808080 : 0, 0xFFFFFF);
        }
    }
    rndrstream_free = vpDmaTag_Next(rndrstream_fx->ptr.u8, 0);
    NuRndrStreamFlush();

    NuRndrStreamLink(rndrstream_system);
    NuRndrSetRTarget(rndrstream_system, -1);
    NuRndrSetZBuffer(rndrstream_system, -2);
    rndrstream_free = vpDmaTag_Next(rndrstream_system->ptr.u8, 0);
    NuRndrSetXYOffset((0x800 - (nurndr_pixel_width >> 1)) << 4, (0x800 - (nurndr_pixel_height >> 1)) << 4);
    NuRndrSetScissor(0, 0, nurndr_pixel_width - 1, nurndr_pixel_height - 1);
    NuRndrStreamLink(rndrstream_3d);
    NuRndrStreamSetMisc(rndrstream_3d);
    NuRndrStreamAddMtl(rndrstream_3d, NuLgtSurf[2]);
    for (int i = (int)NuRndrShadBox[0].x >> 10; i < ((int)NuRndrShadBox[1].x + 0x3FF) >> 10; i++) {
        int x[2] = { i << 10, (i + 1) << 10 };
        if ((float)x[0] < NuRndrShadBox[0].x) {
            x[0] = (int)NuRndrShadBox[0].x;
        }
        if (NuRndrShadBox[1].x < (float)x[1]) {
            x[1] = (int)NuRndrShadBox[1].x;
        }
        int y0 = (int)NuRndrShadBox[0].y, y1 = (int)NuRndrShadBox[1].y;
        const float uv[4] = { (float)x[0] * 0.0009765625f * 25.6f / NuPower2(PS2_REZ_W),
                              NuRndrShadBox[0].y / (float)(NuPower2(PS2_REZ_H) << 4),
                              (float)x[1] * 0.0009765625f * 25.6f / NuPower2(PS2_REZ_W),
                              NuRndrShadBox[1].y / (float)(NuPower2(PS2_REZ_H) << 4) };
        nw_rndr_rect3d(rndrstream_3d, x[0] - 8, y0 - 8, x[1] - x[0], y1 - y0, uv, (int)curlightcol, 0);
    }
    rndrstream_free = vpDmaTag_Next(rndrstream_3d->ptr.u8, 0);
    NuRndrStreamFlush();
}

/* The port's GS Z stops at 2^23 - 1. */
static void nw_water_vertex(NwVuSpriteVertex *v, int x, int y, int z, float s, float t, u32 colour)
{
    memset(v, 0, sizeof(*v));
    v->pos[0] = (float)x;
    v->pos[1] = (float)y;
    v->pos[2] = (float)(z > 0x7FFFFF ? 0x7FFFFF : z);
    v->pos[3] = 1.0f;
    v->tex[0] = s * 4096.0f;
    v->tex[1] = t * 4096.0f;
    memcpy(v->col, &colour, 4);
}

static void nw_water_stream(struct numtl_s *mtl)
{
    rndrstream_3d = rndrstream_3d_u;
    rndrstream_3d->mtl = 0;
    rndrstream_3d->tid = 0;
    NuRndrStreamLink(rndrstream_3d);
    NuRndrStreamSetMisc(rndrstream_3d);
    NuRndrStreamAddMtl(rndrstream_3d, mtl);
}

static void nw_water_target(int target)
{
    NuRndrStreamLink(rndrstream_system);
    NuRndrSetRTarget(rndrstream_system, target);
    NuRndrSetZBuffer(rndrstream_system, -2);
    rndrstream_free = vpDmaTag_Next(rndrstream_system->ptr.u8, 0);
    NuRndrSetXYOffset((0x800 - (nurndr_pixel_width >> 1)) << 4, (0x800 - (nurndr_pixel_height >> 1)) << 4);
    NuRndrSetScissor(0, 0, nurndr_pixel_width - 1, nurndr_pixel_height - 1);
}

/* Only the uncalled NuRndrWaterLevelTint sets NuRndrWaterLevel, so this runs at most once, at a start below 0.5. */
void NuRndrWater(void)
{
    struct nuvec4_s nearpnt, farpnt;
    NwVuSpriteVertex q[4], *v;

    if (NuRndrWaterLevel > -10000.0f && NuRndrWaterLevel > global_camera.mtx._31 - 0.5f) {
        struct numtx_s *pcs = NuCameraGetPCSMtx();
        int ang = NuAtan2D(global_camera.mtx._11, global_camera.mtx._21);
        nearpnt.x = 0.0f;
        nearpnt.y = ((NuRndrWaterLevel - global_camera.mtx._31) -
                     global_camera.nearclip * NuTrigTable[(ang + 0x4000) & 0xFFFF]) /
                    NuTrigTable[ang & 0xFFFF];
        nearpnt.z = global_camera.nearclip;
        nearpnt.w = 1.0f;
        NuVec4MtxTransformVU0(&nearpnt, &nearpnt, pcs);
        NuVec4ScaleVU0(&nearpnt, &nearpnt, 1.0f / nearpnt.w);
        if (nearpnt.y < 34944.0f) {
            farpnt.x = 0.0f;
            farpnt.y = ((NuRndrWaterLevel - global_camera.mtx._31) -
                        (global_camera.nearclip + 0.1f) * NuTrigTable[(ang + 0x4000) & 0xFFFF]) /
                       NuTrigTable[ang & 0xFFFF];
            farpnt.z = global_camera.nearclip + 0.1f;
            farpnt.w = 1.0f;
            NuVec4MtxTransformVU0(&farpnt, &farpnt, pcs);
            NuVec4ScaleVU0(&farpnt, &farpnt, 1.0f / farpnt.w);
            if (farpnt.y < nearpnt.y) {
                farpnt.x = nearpnt.y;
                nearpnt.y = farpnt.y;
                farpnt.y = farpnt.x;
            }
            if (farpnt.y > 30720.0f) {
                nw_water_target(-1);
                nw_water_stream(NuLightAddMat);
                bool ramp = farpnt.y > nearpnt.y;
                nw_rndr_verts(rndrstream_3d, NW_VERTS_GS, ramp ? 12 : 6, &v);
                const int ys[3] = { (int)nearpnt.y, (int)farpnt.y, 0x8800 };
                for (int k = ramp ? 0 : 1; k < 2; k++) {
                    for (int c = 0; c < 4; c++) {
                        nw_water_vertex(&q[c], c & 1 ? ((0x800 + (PS2_REZ_W >> 1)) << 4) + 8
                                                     : ((0x800 - (PS2_REZ_W >> 1)) << 4) - 8,
                                        ys[k + (c >> 1)], 0xFFFFFF, 0.0f, 0.0f,
                                        k + (c >> 1) ? 0x80000000u : 0);
                    }
                    v = nw_rndr_quad(v, q);
                }
                rndrstream_free = vpDmaTag_Next(rndrstream_3d->ptr.u8, 0);
                rndrstream_3d = rndrstream_3d_u;
                rndrstream_3d->mtl = 0;
                rndrstream_3d->tid = 0;
                NuRndrStreamFlush();
            }
            nw_water_target(NuLgtHSurf[4]);
            NuRndrStreamLink(rndrstream_3d);
            NuRndrStreamSetMisc(rndrstream_3d);
            NuRndrStreamAddMtl(rndrstream_3d, NuLgtBackBuff);
            const int xoffset = (PS2_VCNTR_X - (nurndr_pixel_width >> 1)) << 4;
            const int yoffset = (PS2_VCNTR_Y - (nurndr_pixel_height >> 1)) << 4;
            const int cols = (PS2_REZ_W + 63) >> 6;
            const float p2w = (float)NuPower2(PS2_REZ_W);
            nw_rndr_verts(rndrstream_3d, NW_VERTS_GS, (uint32_t)cols * 14 * 6, &v);
            for (int i = 0; i < cols; i++) {
                for (int j = 0; j < 14; j++) {
                    for (int k = 0; k < 4; k++) {
                        const int col = (i + (k & 1)) % 40;
                        const struct nurndrscreenvtx_s *w = &watertab[j + (k >> 1)][col <= 20 ? col : 40 - col];
                        int dx = (int)(NuTrigTable[w->x & 0xFFFF] * waveampx) - 8;
                        int dy = (int)(NuTrigTable[(w->y + 0x4000) & 0xFFFF] * waveampy) - 8;
                        nw_water_vertex(&q[k], ((i + (k & 1)) << 10) + dx + xoffset,
                                        ((j + (k >> 1)) << 8) + dy + yoffset, 0xFFFFFF,
                                        (float)(i + (k & 1)) * 64.0f / p2w,
                                        (float)((j + (k >> 1)) << 4) / 256.0f, 0x80808080u);
                    }
                    v = nw_rndr_quad(v, q);
                }
            }
            rndrstream_free = vpDmaTag_Next(rndrstream_3d->ptr.u8, 0);
            NuRndrStreamFlush();
            for (int i = 0; i < 21; i++) {
                for (int j = 0; j < 15; j++) {
                    watertab[j][i].x += watertab[j][i].u;
                    watertab[j][i].y += watertab[j][i].v;
                }
            }
            nw_water_target(-1);
            NuRndrStreamLink(rndrstream_3d);
            NuRndrStreamSetMisc(rndrstream_3d);
            NuRndrStreamAddMtl(rndrstream_3d, farpnt.y > 30720.0f ? NuLgtSurf[12] : NuLgtSurf[15]);
            const float uv[4] = { 4.0f / NuPower2(PS2_REZ_W), 0.015625f,
                                  (float)(PS2_REZ_W - 4) / NuPower2(PS2_REZ_W), 0.859375f };
            nw_rndr_rect3d(rndrstream_3d, -8, -8, PS2_REZ_W << 4, nurndr_pixel_height << 4, uv, (int)NuRndrWaterTint,
                           0x1FFFFFF);
            rndrstream_free = vpDmaTag_Next(rndrstream_3d->ptr.u8, 0);
            NuRndrStreamFlush();
        }
        NuRndrWaterLevel = -10000.0f;
    }
}

/* In a viewport 0.4 of the frame's width (4096 / 16 pixels at 640), which squeezes the volumes; empties the store. */
void NuRndrShadowStack(struct nuvec_s *light)
{
    NuVpSetSize((float)(PS2_REZ_W << 4) * 0.4f, (float)(PS2_REZ_H << 4));
    NuCameraSet(&global_camera);
    NuRndrStreamLink(rndrstream_fx);
    NuRndrStreamSetMisc(rndrstream_fx);
    NuRndrStreamAddMtl(rndrstream_fx, NuLightAddMat2);
    NuRndrSetLocalMtx(&numtx_identity, 2);
    NuRndrStreamSetViewMtxScissorClip(rndrstream_fx);
    NuRndrStreamSetLocalMtx(rndrstream_fx);
    NuRndrStreamFxMPG(1);
    for (int i = 0; i < shadstorecnt; i++) {
        struct nurndrshadstore_s *store = &shadstore[i];
        if (store->shadow.ncylinders) {
            NuRndrShadowCylinderCap(&store->mtx, store->shadow.cylinders, store->shadow.ncylinders, light);
        }
        if (store->shadow.nellipsoids) {
            NuRndrShadowEllipse(&store->mtx, store->shadow.ellipsoids, store->shadow.nellipsoids, light);
        }
        struct NUSHADOWMESH_s *mesh = store->shadow.shadow_meshes;
        if (mesh) {
            NuRndrShadowPols(&store->mtx, (struct nurndrshadface_s *)mesh->normals,
                             (struct nurndrshadface_s *)mesh->verts, light);
        }
    }
    NuRndrStreamFxMPG(0);
    rndrstream_free = vpDmaTag_Next(rndrstream_fx->ptr.u8, 0);
    NuVpSetSize((float)(PS2_REZ_W << 4), (float)(PS2_REZ_H << 4));
    NuCameraSet(&global_camera);
    shadstorecnt = 0;
}

static NwCmd *nw_target;    /* the TARGET record NuRndrBeginSceneEx's NuRndrSetZBuffer completes */
static int nw_zbuffer = -2;

/* -2 and -4 put the Z buffer behind the frame, which the port does not draw, so their draws go nowhere. */
void NuRndrSetRTarget(struct rndrstream_s *stream, int target)
{
    NwSurf surf = { 0, 0, 0, false };
    int width = PS2_REZ_W, height = PS2_REZ_H;
    if (target > 0) {
        const struct nutexslot_s *e = &TexList[target - 1];
        if (!e->valid) {
            return;
        }
        surf = nw_ps2tx_surf(target);
        width = e->width;
        height = e->height;
    } else if (target == -2 || target == -4) {
        static bool reported;
        if (!reported) {
            reported = true;
            nw_log_warn(NW_LOG_GFX, "the Z buffer as a target is not drawn");
        }
    } else {
        surf = nw_ps2tx_surf(target == -3 ? NW_SURF_FRONT : NW_SURF_BACK);
    }
    nurndr_pixel_width = width;
    nurndr_pixel_height = height;
    nurndr_pixel_p2w = Ps2TxValToPowerOfTwo(width);
    nurndr_pixel_p2h = Ps2TxValToPowerOfTwo(height);
    NwCmd *c = nw_target = nw_rndr_cmd(stream, NW_CMD_TARGET);
    c->target.surf = surf;
    c->target.width = width;
    c->target.height = height;
    c->target.zback = nw_zbuffer == -2;
}

/* A render target as Z buffer (zbuffer > 0) is only ever the volume shadows' NuLgtHSurf[1] (NW_CMD_VOLUMES). */
void NuRndrSetZBuffer(struct rndrstream_s *stream, int zbuffer)
{
    (void)stream;
    nw_zbuffer = zbuffer;
    if (nw_target) {
        nw_target->target.zback = zbuffer == -2;
        nw_target = NULL;
    }
}

/* halfoffset adds nurndr_half_offset, which is 0 in retail. */
int NuRndrBeginSceneEx(int target, int zbuffer, int halfoffset)
{
    (void)halfoffset;
    NuRndrSetRTarget(rndrstream_system, target);
    NuRndrSetZBuffer(rndrstream_system, zbuffer);
    NuRndrSetXYOffset((0x800 - (nurndr_pixel_width >> 1)) << 4, (0x800 - (nurndr_pixel_height >> 1)) << 4);
    NuRndrSetScissor(0, 0, nurndr_pixel_width - 1, nurndr_pixel_height - 1);
    return 1;
}

int NuRndrBeginScene(int target)
{
    return NuRndrBeginSceneEx(target, -2, target == -1);
}

void NuRndrFlush(int flags)
{
    NuRndrEndSceneEx(flags);
}

/* CLEAR sorts first in the system stream: colour (flags & 3 = 1), Z (2) or both; flags & 4 only flushes. */
void NuRndrClear(int flags, int colour, float depth)
{
    if (flags & 4) {
        NuRndrStreamFlush();
    } else {
        NuMtlBuildRenderList();
        NuRndrOTFlush();
        NuRndrStreamFlush();
    }
    NwCmd *c = nw_rndr_cmd(rndrstream_system, NW_CMD_CLEAR);
    memcpy(c->clear.rgba, &colour, 4);
    c->clear.z = (uint32_t)(int)((1.0f - depth) * 8388607.0f);
    c->clear.mask = (flags & 3) == 1 || (flags & 3) == 2 ? flags & 3 : 3;
}

/* (x, y) in 12.4 pixels of a width x height screen, centred on PS2_VCNTR as the GS sees it. */
static void nw_rndr_rect(int x, int y, int width, int height, int colour, struct numtl_s *mtl, const float st[4],
                         int screen_w, int screen_h)
{
    x += (PS2_VCNTR_X - (screen_w >> 1)) << 4;
    y += (PS2_VCNTR_Y - (screen_h >> 1)) << 4;
    NuRndrStreamAddMtl(rndrstream_2d, mtl ? mtl : numtl_defaultmtl2d);
    NwCmd *c = nw_rndr_cmd(rndrstream_2d, NW_CMD_RECT);
    c->rect.xy[0] = x;
    c->rect.xy[1] = y;
    c->rect.xy[2] = x + width;
    c->rect.xy[3] = y + height;
    memcpy(c->rect.st, st, sizeof(c->rect.st));
    memcpy(c->rect.rgba, &colour, 4);
}

/* A rect short of the screen's width is laid out on retail's 640-wide screen, at a wider one's centre. */
void NuRndrRect2di(int x, int y, int width, int height, int colour, struct numtl_s *mtl)
{
    static const float st[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
    nw_rndr_rect(x, y, width, height, colour, mtl, st, x <= 0 && x + width >= PS2_REZ_W << 4 ? PS2_REZ_W : 640,
                 PS2_REZ_H);
}

/* Centred on the render target, where NuRndrRect2di centres on the screen. */
void NuRndrRectUV2di(int x, int y, int width, int height, float u0, float v0, float u1, float v1, int colour,
                     struct numtl_s *mtl)
{
    const float st[4] = { u0, v0, u1, v1 };
    nw_rndr_rect(x, y, width, height, colour, mtl, st, nurndr_pixel_width, nurndr_pixel_height);
}

NwVuSpriteVertex *nw_rndr_rect3d_quad(NwVuSpriteVertex *v, int x, int y, int width, int height, const float uv[4],
                                      int colour, int z)
{
    NwVuSpriteVertex q[4];
    x += (PS2_VCNTR_X - (nurndr_pixel_width >> 1)) << 4;
    y += (PS2_VCNTR_Y - (nurndr_pixel_height >> 1)) << 4;
    z = z < 0 ? 0 : z > 0x7FFFFF ? 0x7FFFFF : z;
    memset(q, 0, sizeof(q));
    for (int k = 0; k < 4; k++) {
        q[k].pos[0] = (float)(k & 1 ? x + width : x);
        q[k].pos[1] = (float)(k & 2 ? y + height : y);
        q[k].pos[2] = (float)z;
        q[k].pos[3] = 1.0f;
        q[k].tex[0] = uv[k & 1 ? 2 : 0] * 4096.0f;
        q[k].tex[1] = uv[k & 2 ? 3 : 1] * 4096.0f;
        memcpy(q[k].col, &colour, 4);
    }
    return nw_rndr_quad(v, q);
}

NwCmd *nw_rndr_rect3d(struct rndrstream_s *stream, int x, int y, int width, int height, const float uv[4],
                      int colour, int z)
{
    NwVuSpriteVertex *v;
    NwCmd *c = nw_rndr_verts(stream, NW_VERTS_GS, 6, &v);
    nw_rndr_rect3d_quad(v, x, y, width, height, uv, colour, z);
    return c;
}

void NuRndrRectUV3di(int x, int y, int width, int height, float u0, float v0, float u1, float v1, int colour, int z)
{
    const float uv[4] = { u0, v0, u1, v1 };
    nw_rndr_rect3d(rndrstream_3d, x, y, width, height, uv, colour, z);
}

void NuRndrRectUVStream(struct rndrstream_s *stream, int x, int y, int width, int height, float u0, float v0, float u1,
                        float v1, int colour, int z)
{
    const float uv[4] = { u0, v0, u1, v1 };
    nw_rndr_rect3d(stream, x, y, width, height, uv, colour, z);
}

/* Target -3 swaps without counting a frame; only the frame counter main.c reads is kept. */
int NuRndrSwapScreen(int target)
{
    NuPs2DisableAutoScreenSwap();
    int swapped = NuPs2ScreenSwap(target != -3);
    NuRndrVu1Draw();
    NuRndrSwapStreamBuffers();
    nurndr_tritot_this_frame = 0;
    if (target != -3) {
        nw_wide_poll();
    }
    return swapped;
}

static float nw_rndr_radius(const struct nuvec_s *min, const struct nuvec_s *max)
{
    float x = max->x - min->x, y = max->y - min->y, z = max->z - min->z;
    return 0.5f * sqrtf(x * x + y * y + z * z);
}

/* Returns retail's clip state: 0 out of view, 1 in view, 2 partly. */
static int nw_render_scene_object(struct nugobj_s *gobj, struct numtx_s *wm,
                                  int eye, const NwGpuTex *lid)
{
    struct numtx_s mtx;
    struct numtx_s offset;
    int clip = -1;
    int n = gobj->ngobjs;
    int instanced = n > 1;
    NwInterpScope scope = nw_interp_begin(wm, gobj, wm, nw_rndr_radius(&gobj->min, &gobj->max));
    while (n-- != 0) {
        struct numtx_s *m = wm;
        if (instanced) {
            NuMtxSetIdentity(&offset);
            offset._30 = gobj->origin.x;
            offset._31 = gobj->origin.y;
            offset._32 = gobj->origin.z;
            NuMtxMul(&mtx, &offset, wm);
            m = &mtx;
        }
        int vis = NuCameraClipTestExtents(&gobj->min, &gobj->max, m);
        if (clip == -1) {
            clip = vis;
        } else if ((clip == 1 && vis != 1) || (clip == 0 && vis != 0)) {
            clip = 2;
        }
        if (vis) {
            nurndr_tritot_this_frame += gobj->tricount;
            NuRndrSetLocalMtx(m, vis);
            uint32_t g = 0;
            for (struct nugeom_s *geom = gobj->geom; geom; geom = geom->next, g++) {
                struct numtl_s *mtl = nurndr_forced_mtl ? nurndr_forced_mtl : geom->mtl;
                if (nurndr_forced_mtl_table && mtl->special_id && nurndr_forced_mtl_table[mtl->special_id]) {
                    mtl = nurndr_forced_mtl_table[mtl->special_id];
                }
                for (uint32_t pass = 0; mtl; mtl = mtl->next, pass++) {
                    nw_interp_at((uint32_t)n, g, pass);
                    rndrstream_3d = &mtl->rs;
                    if (geom->tristream) {
                        nw_rndr_ot_batch(m, mtl, geom->dmastream, geom->tristream);
                    } else if (geom->dmastream) {
                        NwCmd *c = nw_rndr_cmd(rndrstream_3d, NW_CMD_DRAW);
                        c->mesh = geom->dmastream;
                        if (lid && mtl->tid == eye) c->texture_override = lid;
                    }
                }
            }
            g = 0x10000;
            for (struct nufaceongeom_s *fgeom = gobj->faceon_geom; fgeom; fgeom = fgeom->next, g++) {
                nw_interp_at((uint32_t)n, g, 0);
                rndrstream_3d = &fgeom->mtl->rs;
                if (fgeom->dmastream) {
                    NwCmd *c = nw_rndr_cmd(rndrstream_3d, NW_CMD_SPRITES);
                    c->sprites = fgeom->dmastream;
                }
            }
        }
        gobj++;
    }
    nw_interp_end(scope);
    return clip;
}

int NuRndrGScnObj(struct nugobj_s *gobj, struct numtx_s *wm)
{
    return nw_render_scene_object(gobj, wm, 0, NULL);
}


/* As NuRndrGScnObj but along next_gobj, never through the ordering table, no forced material table under one. */
static const struct nutexanim_s *nw_body_eye_tracks[2];
static const NwGpuTex *nw_body_eye_textures[2];

static const NwGpuTex *nw_coco_body_eye(const struct numtl_s *material)
{
    for (int i = 0; i < 2; ++i) {
        if (nw_body_eye_tracks[i] && material == nw_body_eye_tracks[i]->mtl) return nw_body_eye_textures[i];
    }
    return NULL;
}

int NuRndrGobj(struct nugobj_s *gobj, struct numtx_s *wm)
{
    struct numtx_s mtx;
    struct numtx_s offset;
    int clip = -1;
    int instanced = gobj->next_gobj != 0;
    NwInterpScope scope = nw_interp_begin(wm, gobj, wm, nw_rndr_radius(&gobj->min, &gobj->max));
    for (uint32_t link = 0; gobj; gobj = gobj->next_gobj, link++) {
        struct numtx_s *m = wm;
        if (instanced) {
            NuMtxSetIdentity(&offset);
            offset._30 = gobj->origin.x;
            offset._31 = gobj->origin.y;
            offset._32 = gobj->origin.z;
            NuMtxMul(&mtx, &offset, wm);
            m = &mtx;
        }
        int vis = NuCameraClipTestExtents(&gobj->min, &gobj->max, m);
        if (clip == -1) {
            clip = vis;
        } else if ((clip == 1 && vis != 1) || (clip == 0 && vis != 0)) {
            clip = 2;
        }
        if (!vis) {
            continue;
        }
        NuRndrSetLocalMtx(m, vis);
        uint32_t g = 0;
        for (struct nugeom_s *geom = gobj->geom; geom; geom = geom->next, g++) {
            struct numtl_s *mtl = geom->mtl;
            if (mtl && nurndr_forced_mtl) {
                mtl = nurndr_forced_mtl;
            } else if (mtl && nurndr_forced_mtl_table && mtl->special_id && nurndr_forced_mtl_table[mtl->special_id]) {
                mtl = nurndr_forced_mtl_table[mtl->special_id];
            }
            for (uint32_t pass = 0; mtl && geom->dmastream; mtl = mtl->next, pass++) {
                nw_interp_at(link, g, pass);
                rndrstream_3d = &mtl->rs;
                NwCmd *draw = nw_rndr_cmd(rndrstream_3d, NW_CMD_DRAW);
                draw->mesh = geom->dmastream;
                draw->texture_override = nw_coco_body_eye(mtl);
            }
        }
        g = 0x10000;
        for (struct nufaceongeom_s *fgeom = gobj->faceon_geom; fgeom; fgeom = fgeom->next, g++) {
            nw_interp_at(link, g, 0);
            rndrstream_3d = &fgeom->mtl->rs;
            if (fgeom->dmastream) {
                nw_rndr_cmd(rndrstream_3d, NW_CMD_SPRITES)->sprites = fgeom->dmastream;
            }
        }
    }
    nw_interp_end(scope);
    return clip;
}

/* Like retail it skips next_gobj and ignores njoints and joints; skinskipix is -1 in retail and nothing sets it. */
int NuRndrGobjSkin(struct nugobj_s *gobj, struct numtx_s *wm, int njoints, struct numtx_s *joints, u32 *weights,
                   int vis)
{
    (void)njoints;
    (void)joints;
    if (!vis) {
        return vis;
    }
    NuRndrSetLocalMtx(wm, vis);
    nw_rndr_blend(weights);
    NwInterpScope scope = nw_interp_begin(wm, gobj, wm, nw_rndr_radius(&gobj->min, &gobj->max));
    uint32_t g = 0;
    for (struct nugeom_s *geom = gobj->geom; geom; geom = geom->next, g++) {
        struct numtl_s *mtl = nurndr_forced_mtl ? nurndr_forced_mtl : geom->mtl;
        if (mtl && nurndr_forced_mtl_table && mtl->special_id && nurndr_forced_mtl_table[mtl->special_id]) {
            mtl = nurndr_forced_mtl_table[mtl->special_id];
        }
        for (uint32_t pass = 0; mtl && geom->skindma; mtl = mtl->next, pass++) {
            nw_interp_at(g, pass, 0);
            rndrstream_3d = &mtl->rs;
            NwCmd *draw = nw_rndr_cmd(rndrstream_3d, NW_CMD_SKIN);
            draw->skin = (const NwGpuSkin *)geom->skindma;
            draw->texture_override = nw_coco_body_eye(mtl);
        }
    }
    nw_interp_end(scope);
    return vis;
}

/* Skin palette is INV_WT x mtx_array, dwa[ix] the layer's weights; hgobj_enabled is 1 in retail, nothing clears it. */
bool nw_coco_swim_render(struct NUHGOBJ_s *model, struct numtx_s *world, int *drawn);
struct numtx_s *nw_coco_render_pose(struct NUHGOBJ_s *body, struct numtx_s *pose,
                                    struct numtx_s *adjusted, struct numtx_s *before_pump,
                                    bool *has_before_pump);

struct NwCocoVertex {
    float pos[3], uv[2], weight[3];
    uint8_t joint[3], padding;
};

enum { NW_COCO_ATLAS_SIZE = 512, NW_COCO_POSES = 4, NW_COCO_JOINTS = 55 };
static const struct NwCocoVertex *nw_coco_vertices;
static const uint32_t *nw_coco_head;
static uint32_t nw_coco_vertex_count, nw_coco_head_count;
static struct numtl_s *nw_coco_materials[NW_COCO_POSES + 1];
static struct nuvec_s nw_coco_head_min, nw_coco_head_max;

static bool nw_load_coco_model(void)
{
    static bool attempted;
    if (attempted) return nw_coco_vertices != NULL;
    attempted = true;
    const char *directory = SDL_GetBasePath();
    char *path = NULL;
    if (!directory || SDL_asprintf(&path, "%sCocoModel.bin", directory) < 0) return false;
    size_t size;
    uint32_t *data = SDL_LoadFile(path, &size);
    SDL_free(path);
    if (!data) return false;

    const size_t atlas_bytes = NW_COCO_ATLAS_SIZE * NW_COCO_ATLAS_SIZE * 3;
    const uint32_t count = 2115;
    const size_t mesh_bytes = count * sizeof(struct NwCocoVertex);
    if (size < 16 || memcmp(data, "COC1", 4) || data[1] != count ||
        data[2] == 0 || data[2] >= count || data[2] % 3 || data[3] != NW_COCO_JOINTS ||
        size != 16 + mesh_bytes + data[2] * sizeof(uint32_t) + NW_COCO_POSES * atlas_bytes) {
        SDL_free(data);
        return false;
    }
    const struct NwCocoVertex *vertices = (const void *)(data + 4);
    const uint32_t *head = (const void *)(vertices + count);
    for (uint32_t i = 0; i < count; ++i) {
        for (int j = 0; j < 3; ++j) {
            if (vertices[i].weight[j] && vertices[i].joint[j] >= NW_COCO_JOINTS) {
                SDL_free(data);
                return false;
            }
        }
    }
    nw_coco_head_min = (struct nuvec_s){INFINITY, INFINITY, INFINITY};
    nw_coco_head_max = (struct nuvec_s){-INFINITY, -INFINITY, -INFINITY};
    for (uint32_t i = 0; i < data[2]; ++i) {
        if (head[i] >= count) {
            SDL_free(data);
            return false;
        }
        const float *p = vertices[head[i]].pos;
        nw_coco_head_min.x = fminf(nw_coco_head_min.x, p[0]);
        nw_coco_head_min.y = fminf(nw_coco_head_min.y, p[1]);
        nw_coco_head_min.z = fminf(nw_coco_head_min.z, p[2]);
        nw_coco_head_max.x = fmaxf(nw_coco_head_max.x, p[0]);
        nw_coco_head_max.y = fmaxf(nw_coco_head_max.y, p[1]);
        nw_coco_head_max.z = fmaxf(nw_coco_head_max.z, p[2]);
    }
    const size_t head_count = data[2];
    nw_coco_vertex_count = count;
    nw_coco_head_count = (uint32_t)head_count;
    nw_coco_vertices = vertices;
    nw_coco_head = head;
    return true;
}

void nw_coco_reset_materials(void)
{
    /* NuTexInit and NuMtlRelease reclaim the old level's allocations. */
    memset(nw_coco_materials, 0, sizeof(nw_coco_materials));
}

static void nw_coco_create_materials(void)
{
    if (nw_coco_materials[0]) return;
    uint8_t *texture = (void *)(nw_coco_head + nw_coco_head_count);
    const size_t atlas_bytes = NW_COCO_ATLAS_SIZE * NW_COCO_ATLAS_SIZE * 3;
    uint8_t *aim_texture = SDL_malloc(atlas_bytes);
    if (aim_texture) {
        memcpy(aim_texture, texture, atlas_bytes);
        const uint8_t *closed = texture + (NW_COCO_POSES - 1) * atlas_bytes;
        /* The atlas packs each left-eye triangle into a separate 16-pixel tile. */
        static const uint16_t tiles[][2] = {
            {272,224}, {288,224}, {304,224},
            {208,272}, {224,272}, {240,272}, {256,272},
            {272,272}, {288,272}, {304,272},
            {224,288}, {240,288}, {256,288},
        };
        for (size_t i = 0; i < SDL_arraysize(tiles); ++i) {
            for (unsigned y = 0; y < 16; ++y) {
                const size_t offset = ((tiles[i][1] + y) * NW_COCO_ATLAS_SIZE + tiles[i][0]) * 3;
                memcpy(aim_texture + offset, closed + offset, 16 * 3);
            }
        }
    }
    for (int i = 0; i < NW_COCO_POSES + (aim_texture != NULL); ++i) {
        if (i == NW_COCO_POSES) texture = aim_texture;
        struct nutex_s spec = {2, NW_COCO_ATLAS_SIZE, NW_COCO_ATLAS_SIZE, 1, texture, NULL};
        struct numtl_s *mtl = NuMtlCreate(1);
        mtl->diffuse.r = mtl->diffuse.g = mtl->diffuse.b = 1.0f;
        mtl->alpha = 1.0f;
        mtl->attrib.cull = 2;
        mtl->tid = NuTexCreate(&spec);
        /* Draw the opaque rider before translucent vehicle glass. */
        NuMtlRemove(mtl);
        mtl->sort_pri = 0;
        NuMtlInsert(mtl);
        NuMtlUpdate(mtl);
        nw_coco_materials[i] = mtl;
        if (i < NW_COCO_POSES) texture += atlas_bytes;
    }
    SDL_free(aim_texture);
}

bool nw_coco_player_eyes_closed(void);
bool nw_coco_player_dying(void);
bool nw_coco_body_eye_control(void);
bool nw_coco_body_aiming(void);

static unsigned nw_coco_blink_pose(bool closed)
{
    if (nw_coco_player_dying()) return closed ? 3 : 0;
    extern struct gtimer_s GameTimer;
    /* Two simulation ticks per Vita animation frame. Share the pose with the portrait. */
    const uint32_t frame = GameTimer.frame / 2;
    uint32_t pattern = (frame / 120 + 1) * 0x9e3779b9u;
    pattern ^= pattern >> 16;
    pattern *= 0x7feb352du;
    pattern ^= pattern >> 15;
    const unsigned start = 54 + pattern % 13;
    unsigned phase = frame % 120 - start;
    if ((pattern >> 8) % 3 == 0 && phase >= 12) phase -= 12;
    const unsigned blink = phase < 2 ? phase + 1 : phase < 4 ? 3 : phase < 6 ? 6 - phase : 0;
    return blink;
}

static struct numtl_s *nw_coco_blink_material(bool closed, bool aiming)
{
    nw_coco_create_materials();
    if (!closed && aiming) {
        return nw_coco_materials[NW_COCO_POSES] ? nw_coco_materials[NW_COCO_POSES] : nw_coco_materials[0];
    }
    return nw_coco_materials[nw_coco_blink_pose(closed)];
}

static NwVuSpriteVertex *nw_coco_model_stream(struct numtx_s *world, uint32_t count, bool closed, bool aiming)
{
    static const NwVuProg textured = {.name = "vu_load_tricu_mpg"};
    struct numtl_s *mtl = nw_coco_blink_material(closed, aiming);
    NwVuSpriteVertex *out;
    NuRndrSetLocalMtx(world, 2);
    NuRndrStreamAddMtl(&mtl->rs, mtl);
    nw_rndr_verts(&mtl->rs, NW_VERTS_TRIS, count, &out)->mpg = &textured;
    return out;
}

static NwVuSpriteVertex nw_coco_model_vertex(const struct NwCocoVertex *in, struct nuvec_s p)
{
    return (NwVuSpriteVertex){
        .pos = {p.x, p.y, p.z, 0},
        .tex = {in->uv[0] * 4096, in->uv[1] * 4096, 0, 0},
        .col = {128, 128, 128, 128},
    };
}

static bool nw_draw_coco_model(struct NUHGOBJ_s *body, struct numtx_s *world, struct numtx_s *pose,
                               struct numtx_s *before_pump)
{
    if (!pose || CRemap[1] < 0 || CModel[CRemap[1]].obj != body ||
        body->num_joints != NW_COCO_JOINTS || !nw_load_coco_model()) return false;

    struct numtx_s palette[NW_COCO_JOINTS];
    NuMtxMulArrayVU0(palette, body->INV_WT, pose, NW_COCO_JOINTS);
    struct numtx_s before_pump_palette[NW_COCO_JOINTS];
    if (before_pump) NuMtxMulArrayVU0(before_pump_palette, body->INV_WT, before_pump, NW_COCO_JOINTS);
    NwVuSpriteVertex *out = nw_coco_model_stream(world, nw_coco_vertex_count,
        nw_coco_body_eye_control() && nw_coco_player_eyes_closed(), nw_coco_body_aiming());
    for (uint32_t i = 0; i < nw_coco_vertex_count; ++i) {
        const struct NwCocoVertex *in = &nw_coco_vertices[i];
        struct nuvec_s src = {in->pos[0], in->pos[1], in->pos[2]};
        struct nuvec_s pos = {0};
        /* All copies of the arm-weighted vertices in the two side curls. */
        const bool curl = i == 16 || i == 18 || i == 20 || i == 21 || i == 22 ||
            i == 25 || i == 29 || i == 35 || i == 37 || i == 39 ||
            i == 41 || i == 42 || i == 43 || i == 46 || i == 50;
        struct numtx_s *skin = before_pump && curl ? before_pump_palette : palette;
        for (int j = 0; j < 3; ++j) {
            if (!in->weight[j]) continue;
            struct nuvec_s point;
            NuVecMtxTransform(&point, &src, &skin[in->joint[j]]);
            pos.x += point.x * in->weight[j];
            pos.y += point.y * in->weight[j];
            pos.z += point.z * in->weight[j];
        }
        out[i] = nw_coco_model_vertex(in, pos);
    }
    return true;
}

/* Both base eye tracks share the same closed artwork. Remove its baked glint
 * without changing the alpha, crease, or shadows on the eyelid. */
static const NwGpuTex *nw_coco_native_eye(const struct nutexanim_s *eye, unsigned phase)
{
    const NwGpuTex *texture = nw_rndr_texture(eye->tids[phase]);
    enum { size = 32 };
    if (!texture || phase != 3 ||
        texture->tex.levels[0].width != size || texture->tex.levels[0].height != size) return texture;
    static int matte_tid;
    if (!nw_rndr_texture(matte_tid)) {
        uint8_t pixels[size * size * 4], row[size * 4];
        nw_tex_rgba(&texture->tex, 16, row);
        const uint8_t green = row[15 * 4 + 1], blue = row[15 * 4 + 2];
        for (int y = 0; y < size; ++y) {
            nw_tex_rgba(&texture->tex, y, row);
            for (int x = 0; x < size; ++x) {
                const uint8_t *in = &row[4 * x];
                uint8_t *out = &pixels[4 * (y * size + x)];
                const bool glint = x >= 17 && x <= 28 && y >= 9 && y <= 21;
                out[0] = in[3];
                out[1] = glint ? SDL_min(in[2], blue) : in[2];
                out[2] = glint ? SDL_min(in[1], green) : in[1];
                out[3] = in[0];
            }
        }
        struct nutex_s spec = {3, size, size, 1, pixels, NULL};
        matte_tid = NuTexCreate(&spec);
    }
    const NwGpuTex *matte = nw_rndr_texture(matte_tid);
    return matte ? matte : texture;
}

static const struct nutexanim_s *nw_coco_eye_track(const struct NUHGOBJ_s *body, const char *name)
{
    if (!body) return NULL;
    for (int i = 0; i < body->numtexanims; ++i) {
        const struct nutexanim_s *eye = &body->texanims[i];
        if (eye->numtids == 5 && eye->scriptname &&
            SDL_strcasecmp(eye->scriptname, name) == 0) return eye;
    }
    return NULL;
}

int nw_draw_coco_lives_head(struct nugscn_s *scene, struct nugobj_s *stock, struct numtx_s *world)
{
    if (!nw_load_coco_model()) {
        const unsigned phase = nw_coco_blink_pose(nw_coco_player_eyes_closed());
        const struct nutexanim_s *eye = phase && CRemap[1] >= 0 ?
            nw_coco_eye_track(CModel[CRemap[1]].obj, "leftEyeC") : NULL;
        const NwGpuTex *lid = eye ? nw_coco_native_eye(eye, phase) : NULL;
        const int eye_slot = 9; /* WUMPA.GSC portrait white-eye texture */
        if (lid && scene->numtid > eye_slot) {
            return nw_render_scene_object(stock, world, scene->tids[eye_slot], lid);
        }
        return NuRndrGScnObj(stock, world);
    }
    const struct nuvec_s min = nw_coco_head_min, max = nw_coco_head_max;
    const float scale = (stock->max.y - stock->min.y) / (max.y - min.y);
    const struct nuvec_s shift = {
        (stock->min.x + stock->max.x - (min.x + max.x) * scale) * 0.5f,
        (stock->min.y + stock->max.y - (min.y + max.y) * scale) * 0.5f,
        (stock->min.z + stock->max.z - (min.z + max.z) * scale) * 0.5f,
    };
    NwVuSpriteVertex *out = nw_coco_model_stream(world, nw_coco_head_count, nw_coco_player_eyes_closed(), false);
    for (uint32_t i = 0; i < nw_coco_head_count; ++i) {
        const struct NwCocoVertex *in = &nw_coco_vertices[nw_coco_head[i]];
        out[i] = nw_coco_model_vertex(in, (struct nuvec_s){
            in->pos[0] * scale + shift.x,
            in->pos[1] * scale + shift.y,
            in->pos[2] * scale + shift.z,
        });
    }
    return 1;
}


int NuHGobjRndrMtxDwa(struct NUHGOBJ_s *, struct numtx_s *, int, short *,
                     struct numtx_s *, float **);
static struct NUHGOBJ_s *nw_coco_fin_gear;

static bool nw_is_coco_fin(const struct NUHGOBJ_s *gear, int joint)
{
    const char *name = gear->joints[joint].name;
    return name && (SDL_strcasecmp(name, "leftFlipper") == 0 ||
                    SDL_strcasecmp(name, "rightFlipper") == 0);
}

int nw_draw_coco_gear(struct NUHGOBJ_s *gear, struct numtx_s *world, struct numtx_s *pose)
{
    struct NUHGOBJ_s *previous = nw_coco_fin_gear;
    nw_coco_fin_gear = gear;
    const int drawn = NuHGobjRndrMtxDwa(gear, world, 1, NULL, pose, NULL);
    nw_coco_fin_gear = previous;
    return drawn;
}

int NuHGobjRndrMtxDwa(struct NUHGOBJ_s *hobj, struct numtx_s *wm, int nlayers, short *layer, struct numtx_s *mtx_array,
                      float **dwa)
{
    int coco_drawn;
    if (nw_coco_swim_render(hobj, wm, &coco_drawn)) {
        return coco_drawn;
    }
    struct numtx_s adjusted[256];
    struct numtx_s before_pump[256];
    bool has_before_pump;
    mtx_array = nw_coco_render_pose(hobj, mtx_array, adjusted, before_pump, &has_before_pump);
    struct numtx_s mtx;
    int rv = 0;
    NwInterpScope scope = nw_interp_begin(wm, hobj, wm, nw_rndr_radius(&hobj->bbox_min, &hobj->bbox_max));
    nw_rndr_model_begin(wm);
    int vis = NuCameraClipTestExtents(&hobj->bbox_min, &hobj->bbox_max, wm);
    if (vis == 0) {
        nw_rndr_model_end();
        nw_interp_end(scope);
        return 0;
    }
    if (hobj->joints) {
        nw_interp_joints(mtx_array, &hobj->joints[0].parent_ix, sizeof(hobj->joints[0]), hobj->num_joints);
    }
    if (hobj->shadow && (u8)hobj->pad81[0] == 0) {
        for (struct NUSHADOWDATA_s *shadow = hobj->shadow; shadow->joint != 0xFF; shadow++) {
            if (shadstorecnt < 256) {
                struct nurndrshadstore_s *s = &shadstore[shadstorecnt++];
                NuMtxMulVU0(&s->mtx, &mtx_array[shadow->joint], wm);
                s->shadow.ellipsoids = shadow->ellipsoids;
                s->shadow.cylinders = shadow->cylinders;
                s->shadow.shadow_meshes = shadow->nshadow_meshes ? shadow->shadow_meshes : NULL;
                s->shadow.nellipsoids = shadow->nellipsoids;
                s->shadow.ncylinders = shadow->ncylinders;
                s->shadow.nshadow_meshes = shadow->nshadow_meshes;
            }
        }
    }
    if (nw_draw_coco_model(hobj, wm, mtx_array, has_before_pump ? before_pump : NULL)) {
        nw_rndr_model_end();
        nw_interp_end(scope);
        return vis;
    }
    if (nw_coco_body_eye_control() && CRemap[1] >= 0 && CModel[CRemap[1]].obj == hobj) {
        const unsigned phase = nw_coco_blink_pose(nw_coco_player_eyes_closed());
        const char *names[] = {"leftEyeC", "rghtEyeC"};
        for (int i = 0; i < 2; ++i) {
            const struct nutexanim_s *eye = nw_coco_eye_track(hobj, names[i]);
            if (eye) {
                nw_body_eye_tracks[i] = eye;
                nw_body_eye_textures[i] = nw_coco_native_eye(eye, phase);
            }
        }
    }
    for (int i = 0; i < nlayers; i++) {
        int ix = 0;
        if (layer) {
            ix = layer[i];
        } else {
            nlayers = 1;
        }
        if (ix < 0 || ix >= hobj->num_layers) {
            continue;
        }
        float *weights = dwa ? dwa[ix] : 0;
        struct NULAYERDATA_s *l = &hobj->layers[ix];
        for (int set = 0; set < 2; set++) {
            struct nugobj_s **gobjs = set ? l->blend_gobjs : l->gobjs;
            for (int j = 0; gobjs && j < hobj->num_joints; j++) {
                if (gobjs[j]) {
                    nw_interp_at((uint32_t)ix, (uint32_t)set, (uint32_t)j);
                    nw_interp_joint((uint32_t)j);
                    NuMtxMulVU0(&mtx, &mtx_array[j], wm);
                    if (hobj != nw_coco_fin_gear || !nw_is_coco_fin(hobj, j)) {
                        rv |= NuRndrGobj(gobjs[j], &mtx);
                    }
                }
            }
        }
        nw_interp_joint(NW_JOINT_NONE);
        if (l->skin_gobj || l->blend_skin_gobj) {
            NuMtxMulArrayVU0(nw_rndr_joints(hobj->num_joints), hobj->INV_WT, mtx_array, hobj->num_joints);
            if (l->skin_gobj) {
                nw_interp_at((uint32_t)ix, 2, 0);
                rv |= NuRndrGobjSkin(l->skin_gobj, wm, hobj->num_joints, mtx_array, (u32 *)weights, vis);
            }
            if (l->blend_skin_gobj) {
                nw_interp_at((uint32_t)ix, 3, 0);
                rv |= NuRndrGobjSkin(l->blend_skin_gobj, wm, hobj->num_joints, mtx_array, (u32 *)weights, vis);
            }
        }
    }
    memset(nw_body_eye_tracks, 0, sizeof(nw_body_eye_tracks));
    nw_rndr_model_end();
    nw_interp_end(scope);
    return rv;
}

void NuHGobjRndr(void *hobj, struct numtx_s *mtx, int nlayers, short *layer)
{
    static struct numtx_s joints[256]; /* hgobj_joint_mtx */
    NuHGobjEval(hobj, 0, 0, joints);
    NuHGobjRndrMtxDwa(hobj, mtx, nlayers, layer, joints, 0);
}

/* s and t x4096 truncated as retail sends them. */
static NwVuSpriteVertex *nw_rndr_tc1_quad(NwVuSpriteVertex *v, const struct nuvtx_tc1_s q[4])
{
    NwVuSpriteVertex c[4];
    memset(c, 0, sizeof(c));
    for (int k = 0; k < 4; k++) {
        c[k].pos[0] = q[k].pnt.x;
        c[k].pos[1] = q[k].pnt.y;
        c[k].pos[2] = q[k].pnt.z;
        c[k].tex[0] = (float)(int)(q[k].tc[0] * 4096.0f);
        c[k].tex[1] = (float)(int)(q[k].tc[1] * 4096.0f);
        memcpy(c[k].col, &q[k].diffuse, 4);
    }
    return nw_rndr_quad(v, c);
}

/* Retail loops forever on a count that is not a multiple of four. */
static NwCmd *nw_rndr_quads(struct rndrstream_s *stream, const struct nuvtx_tc1_s *verts, int count)
{
    NwVuSpriteVertex *v;
    NwCmd *c = nw_rndr_verts(stream, NW_VERTS_TRIS, (uint32_t)(count / 4) * 6, &v);
    for (int i = 0; i + 4 <= count; i += 4) {
        v = nw_rndr_tc1_quad(v, &verts[i]);
    }
    return c;
}

int NuRndrStreamQuad3dClip(struct nuvtx_tc1_s *verts, int count, void *mtx, struct rndrstream_s *stream)
{
    (void)mtx;
    nw_rndr_quads(stream, verts, count);
    return 1;
}

/* Retail sends batches of 42 that each restart the strip; the normal goes x in w, y and z x4096 after s and t. */
NwCmd *nw_rndr_strip(struct nuvtx_tc1_s *verts, int count, struct numtx_s *mtx, struct rndrstream_s *stream)
{
    NwVuSpriteVertex *v;
    uint32_t n = 0;
    for (int b = 0; b < count; b += 42) {
        int batch = count - b < 42 ? count - b : 42;
        n += batch >= 3 ? (uint32_t)(batch - 2) * 3 : 0;
    }
    NuRndrSetLocalMtx(mtx ? mtx : &numtx_identity, 2);
    NwInterpScope scope = mtx ? nw_interp_begin(mtx, stream->mtl, mtx, 0.0f) : (NwInterpScope){ 0 };
    NwCmd *c = nw_rndr_verts(stream, NW_VERTS_TRIS, n, &v);
    if (mtx) {
        nw_interp_end(scope);
    }
    for (int b = 0; b < count; b += 42) {
        for (int i = b + 2; i < b + 42 && i < count; i++) {
            for (int k = i - 2; k <= i; k++, v++) {
                const struct nuvtx_tc1_s *s = &verts[k];
                memset(v, 0, sizeof(*v));
                v->pos[0] = s->pnt.x;
                v->pos[1] = s->pnt.y;
                v->pos[2] = s->pnt.z;
                v->pos[3] = s->nrm.x;
                v->tex[0] = (float)(int)(s->tc[0] * 4096.0f);
                v->tex[1] = (float)(int)(s->tc[1] * 4096.0f);
                v->tex[2] = (float)(int)(s->nrm.y * 4096.0f);
                v->tex[3] = (float)(int)(s->nrm.z * 4096.0f);
                v->col[0] = (uint8_t)s->rgba.r;
                v->col[1] = (uint8_t)s->rgba.g;
                v->col[2] = (uint8_t)s->rgba.b;
                v->col[3] = (uint8_t)s->rgba.a;
            }
        }
    }
    return c;
}

int NuRndrStreamTriStrip3dClip(struct nuvtx_tc1_s *verts, int count, struct numtx_s *mtx, struct rndrstream_s *stream)
{
    nw_rndr_strip(verts, count, mtx, stream);
    return 1;
}

/* NuRndrShadPolys' quads: each shadow's square turned by y, z then x at its point, black with the shade as alpha. */
int NuRndrStreamQuadShad(struct rndrstream_s *stream)
{
    struct nuvtx_tc1_s q[4];
    struct numtx_s m;
    NwVuSpriteVertex *v;
    memset(q, 0, sizeof(q));
    nw_rndr_verts(stream, NW_VERTS_TRIS, (uint32_t)NuRndrShadowCnt * 6, &v);
    for (int i = 0; i < NuRndrShadowCnt; i++) {
        NuRndrShadType *s = &NuRndrShadPolDat[i];
        NuMtxSetRotationY(&m, s->yrot);
        NuMtxRotateZ(&m, s->zrot);
        NuMtxRotateX(&m, s->xrot);
        for (int k = 0; k < 4; k++) {
            struct nuvec_s offset = { k & 1 ? s->size : -s->size, 0.0f, k & 2 ? s->size : -s->size };
            NuVecMtxRotate(&q[k].pnt, &offset, &m);
            q[k].pnt.x += s->pos.x;
            q[k].pnt.y += s->pos.y;
            q[k].pnt.z += s->pos.z;
            q[k].tc[0] = (float)(k & 1);
            q[k].tc[1] = (float)(k >> 1);
            q[k].diffuse = (int)((u32)s->shade << 24);
        }
        v = nw_rndr_tc1_quad(v, q);
    }
    return 1;
}

int NuRndrStreamQuadWRip(struct rndrstream_s *stream, short *visible)
{
    struct nuvtx_tc1_s q[4];
    NwVuSpriteVertex *v;
    memset(q, 0, sizeof(q));
    nw_rndr_verts(stream, NW_VERTS_TRIS, (uint32_t)NuRndrWaterRipCnt * 6, &v);
    for (int i = 0; i < NuRndrWaterRipCnt; i++) {
        const struct nurndrwaterrip_s *r = &NuRndrWaterRipDat[visible[i]];
        for (int k = 0; k < 4; k++) {
            q[k].pnt.x = k & 1 ? r->pos.x + r->size : r->pos.x - r->size;
            q[k].pnt.y = r->pos.y;
            q[k].pnt.z = k & 2 ? r->pos.z + r->size : r->pos.z - r->size;
            q[k].tc[0] = (k & 1 ? 0xFF0 : 0xC00) / 4096.0f;
            q[k].tc[1] = (k & 2 ? 0x7F0 : 0x400) / 4096.0f;
            q[k].diffuse = (int)r->curcolour;
        }
        v = nw_rndr_tc1_quad(v, q);
    }
    return 1;
}

/* Retail writes ALPHA_1 0x44, (Cs - Cd) * As + Cd, into the fx stream over the material's before the quads. */
void NuRndrFootPrints(struct numtl_s *mtl, float *u, float *v)
{
    struct nuvtx_tc1_s vtx[64 * 4];
    struct nuvtx_tc1_s *vp = vtx;
    for (int i = 0; i < 64; i++) {
        struct nurndrfoot_s *foot = &NuRndrFootData[i];
        if (foot->timer <= 0) {
            continue;
        }
        if (testfootcol != 0) {
            foot->colour = testfootcol;
        }
        if (foot->timer < 16) {
            foot->timer--;
        }
        int gfx = foot->gfx;
        u32 colour = ((u32)((foot->brightness * foot->timer) >> 4) << 24) + foot->colour;
        for (int k = 0; k < 4; k++, vp++) {
            vp->pnt = foot->pnts[k];
            vp->tc[0] = u[gfx + k];
            vp->tc[1] = v[gfx + k];
            vp->diffuse = (int)colour;
        }
    }
    if (vp == vtx) {
        return;
    }
    NuRndrStreamLink(rndrstream_fx);
    NuRndrStreamSetMisc(rndrstream_fx);
    NuRndrStreamAddMtl(rndrstream_fx, mtl);
    NuRndrSetLocalMtx(&numtx_identity, 2);
    NuRndrStreamSetViewMtxScissorClip(rndrstream_fx);
    NuRndrStreamSetLocalMtx(rndrstream_fx);
    NuRndrStreamFxMPG(1);
    nw_rndr_quads(rndrstream_fx, vtx, (int)(vp - vtx))->alpha = 0x100 | 0x44;
    rndrstream_free = vpDmaTag_Next(rndrstream_fx->ptr.u8, 0);
}

/* nurndr.c's; vehicle.c's trail_s and jeep.c's jeeptrail_s have its layout. */
struct nurndrtrail_s {
    struct nuvec_s pos1;
    struct nuvec_s pos2;
    int intensity;
    int RealIntensity;
};

/* Retail's trail quads (ALPHA_1 0x44): two ribbons per segment, white fading inwards; the 0xFFFFF is retail's. */
void NuRndrTrail(int trailpt, struct nurndrtrail_s *trail, int trailcount)
{
    static struct nuvtx_tc1_s vtx[2048];
    int cnt = 0, last = -1, start = trailpt + (1 - trailcount);
    int col = 0, lastcol, begin = 0;
    memset(vtx, 0, sizeof(vtx));
    for (int loop = trailpt; loop > start; loop--) {
        int pt = loop & (trailcount - 1);
        if (trail[pt].pos1.x == -10000.0f) {
            last = -1;
            continue;
        }
        if (last == -1) {
            last = pt;
            col = 0;
            begin = 1;
            continue;
        }
        int next = (loop - 1) & (trailcount - 1);
        int next2 = (loop - 2) & (trailcount - 1);
        lastcol = col;
        if (trail[next].pos1.x == -10000.0f || loop - 1 <= start) {
            col = 0;
        } else if (trail[next2].pos1.x == -10000.0f || begin) {
            col = ((trail[pt].intensity >> 24) / 2) << 24;
        } else {
            col = trail[pt].intensity;
        }
        begin = 0;
        for (int side = 0; side < 2; side++, cnt += 4) {
            const struct nuvec_s *a0 = side ? &trail[last].pos2 : &trail[last].pos1;
            const struct nuvec_s *a1 = side ? &trail[last].pos1 : &trail[last].pos2;
            const struct nuvec_s *b0 = side ? &trail[pt].pos2 : &trail[pt].pos1;
            const struct nuvec_s *b1 = side ? &trail[pt].pos1 : &trail[pt].pos2;
            vtx[cnt].pnt = *a0;
            vtx[cnt + 1].pnt = *b0;
            vtx[cnt + 2].pnt.x = (a1->x - a0->x) * 0.25f + a0->x;
            vtx[cnt + 2].pnt.y = (a1->y - a0->y) * 0.25f + a0->y;
            vtx[cnt + 2].pnt.z = (a1->z - a0->z) * 0.25f + a0->z;
            vtx[cnt + 3].pnt.x = (b1->x - b0->x) * 0.25f + b0->x;
            vtx[cnt + 3].pnt.y = (b1->y - b0->y) * 0.25f + b0->y;
            vtx[cnt + 3].pnt.z = (b1->z - b0->z) * 0.25f + b0->z;
            if (side == 0) {
                vtx[cnt].diffuse = lastcol < 0 ? -lastcol : lastcol + 0xFFFFFF;
                vtx[cnt + 2].diffuse = lastcol < 0 ? 0 : 0xFFFFFF;
                vtx[cnt + 1].diffuse = col < 0 ? -col : col + 0xFFFFFF;
                vtx[cnt + 3].diffuse = col < 0 ? 0 : 0xFFFFFF;
            } else {
                vtx[cnt].diffuse = lastcol < 0 ? 0xFFFFF - lastcol : lastcol;
                vtx[cnt + 2].diffuse = lastcol < 0 ? 0xFFFFFF : 0;
                vtx[cnt + 1].diffuse = col < 0 ? 0xFFFFFF - col : col;
                vtx[cnt + 3].diffuse = col < 0 ? 0xFFFFFF : 0;
            }
        }
        last = pt;
    }
    if (cnt == 0) {
        return;
    }
    NuRndrStreamLink(rndrstream_fx);
    NuRndrStreamSetMisc(rndrstream_fx);
    NuRndrStreamAddMtl(rndrstream_fx, NuLightAddMat);
    NuRndrSetLocalMtx(&numtx_identity, 2);
    NuRndrStreamSetViewMtxScissorClip(rndrstream_fx);
    NuRndrStreamSetLocalMtx(rndrstream_fx);
    NuRndrStreamFxMPG(1);
    nw_rndr_quads(rndrstream_fx, vtx, cnt)->alpha = 0x100 | 0x44;
    rndrstream_free = vpDmaTag_Next(rndrstream_fx->ptr.u8, 0);
}

_Static_assert(sizeof(NwPtl) == sizeof(struct debpart_s), "debpart_s");
_Static_assert(sizeof(((NwPtlList *)0)->tri) == sizeof(((struct debdma_s *)0)->tri), "debdma_s.tri");

/* Sets are linked by NEXT tags (LinkDmaParticalSets' offset), RET on the last; mode's screen clamp is moot here. */
void NuRndrParticleGroup(struct debchunk_s *chunk, struct debdma_s *list, struct numtl_s *mtl, struct numtx_s *mtx,
                         int mode, float time)
{
    NwPtlList *l;
    NwPtl *ptls;
    (void)mode;
    if (testforcepart) {
        list = testforcepart;
    }
    list->time = time;
    struct debchunk_s *sets[256];
    uint32_t n = 0;
    for (struct debchunk_s *set = chunk; set && n < 256;) {
        const struct DmaTag *tag = (const struct DmaTag *)set;
        sets[n++] = set;
        set = (tag->ctrl >> 28 & 7) == 2 ? (struct debchunk_s *)((u8 *)set + (s32)tag->addr) : NULL;
    }
    NuRndrSetLocalMtx(mtx, 1);
    NuRndrStreamAddMtl(rndrstream_ptl, mtl);
    NwInterpScope scope = nw_interp_begin(chunk, list, mtx, 0.0f);
    nw_rndr_particles(rndrstream_ptl, n * 32, &l, &ptls);
    nw_interp_end(scope);
    memset(l, 0, sizeof(*l));
    l->head[1] = list->grav;
    l->head[3] = list->time;
    const s32 uv[8] = { list->u0, list->v0, list->u1, list->v1, list->u2, list->v2, list->u3, list->v3 };
    memcpy(l->uv, uv, sizeof(uv));
    memcpy(l->tri, list->tri, sizeof(l->tri));
    for (uint32_t i = 0; i < n; i++) {
        memcpy(&ptls[i * 32], sets[i]->parts, sizeof(sets[i]->parts));
    }
}

/* Retail's packet: RET tag, S-32 unpack to VU 0x63, count at u32 [5]; NuHGobjEvalDwa* write weights from [6] on. */
u32 *NuRndrCreateBlendShapeDeformerWeightsArray(u32 count)
{
    u32 *base = NuRndrStreamAlloc((int)((count + 10) >> 2));
    base[4] = ((count + 1) << 16) | 0x60000063;
    base[5] = count;
    return base;
}
