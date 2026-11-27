/* The frame of draws the game thread hands the renderer each tick; GL-free. */
#ifndef NUWORLD_GFX_PACKET_H
#define NUWORLD_GFX_PACKET_H

#include <stdbool.h>
#include <stdint.h>

#include "gfx/gem.h"
#include "gfx/mtl.h"
#include "gfx/tex.h"
#include "gfx/vif.h"
#include "gfx/vu.h"

struct numtl_s;

/* Made on the game thread, uploaded and freed on the render thread. */
typedef struct NwGpuMesh {
    NwVifMesh mesh;
    unsigned int vao, vbo, ibo;
} NwGpuMesh;

typedef struct NwGpuTex {
    NwTex tex;
    NwVuTexture gl;
} NwGpuTex;

typedef struct NwGpuSprites {
    NwVifSprites sprites;
    unsigned int vao, vbo, ibo;
} NwGpuSprites;

typedef struct NwGpuSkin {
    NwVifSkin skin;
    unsigned int vao, vbo, ibo;
    unsigned int deltas, delta_tex;
} NwGpuSkin;

/* The back or front buffer or a target's GS address (0: none), tw x th a power of two. */
#define NW_SURF_BACK (-1)
#define NW_SURF_FRONT (-3)

typedef struct NwSurf {
    int32_t addr;
    uint16_t tw, th;
    bool tcc;
} NwSurf;

typedef enum NwCmdOp {
    NW_CMD_DRAW,
    NW_CMD_DRAW_OT, /* some of its mesh's triangles, in their order in NwPacket.idx */
    NW_CMD_CLEAR, /* NuRndrClear: the scissor rectangle's colour and Z */
    NW_CMD_RECT,
    NW_CMD_SPRITES,
    NW_CMD_TARGET,
    NW_CMD_SKIN,
    NW_CMD_VERTS,   /* vertices made this frame, in NwPacket.verts */
    NW_CMD_PARTICLES,
    NW_CMD_VOLUMES, /* the back buffer's Z, squeezed, and the stencil 0x7F */
    NW_CMD_WAVES,   /* a water material's wave table, which the draws after it read */
} NwCmdOp;

/* nuwater's animateWater table at VU 0x1B2: per wave the camera-space texcoord offset (x, y), 1, the local y lift. */
typedef struct NwWaves {
    float wave[32][4];
} NwWaves;

/* An effect list as unpacked to VU 0x3C-0xBF; tri is 64 entries by age of corner offsets and colour. */
typedef struct NwPtlList {
    float head[4]; /* [1] the gravity, [3] the time */
    int32_t uv[4][2];
    uint32_t pad[4];
    uint32_t tri[64][8];
} NwPtlList;

/* As its set holds it; rate is how fast it ages. */
typedef struct NwPtl {
    float pos[3], birth;
    float mom[3], rate;
} NwPtl;


#define NW_ZCLUT_ZERO 0x100u
#define NW_ZCLUT_WRAP 0x200u

typedef struct NwZClut {
    uint32_t clut[256];
    uint32_t shift, base;
    uint32_t far, near;
    bool clear, spot;
    uint32_t clear_z[2];
    bool alpha;
    uint8_t ad;
} NwZClut;

typedef enum NwVertsMode {
    NW_VERTS_GS,   /* GS coordinates (x, y 12.4, z), s and t x4096, unlit: a DIRECT GIF packet's */
    NW_VERTS_TRIS, /* points under the local matrix, s and t x4096, through the command's program */
    NW_VERTS_SPRITE, /* points under the local matrix, each corner moved by tex.zw GS units before the divide */
} NwVertsMode;

/* NuRndrSetViewMtx's world-to-GS matrix, XYOFFSET and SCISSOR. */
typedef struct NwView {
    float vs[16];        /* row-major, row vectors (vpsmtx) */
    int32_t xyoffset[2]; /* GS 12.4 */
    int32_t scissor[4];  /* x0, y0, x1, y1 */
    float faceon[4];     /* VU 0x16 (FaceYDirStream): cos yaw, viewport width / 10240, -sin yaw, height / 3584 */
    float cam[16];       /* the camera vs was made from (global_camera's matrix and scale), for interpolation */
    float camscale[3];
    uint32_t key; /* its camera's (src/gfx/interp.h); 0 none */
} NwView;

/* An object's matrix and joints, for interpolation. */
typedef struct NwBody {
    float wm[16];
    uint32_t first, count; /* into NwPacket.bones and .parents */
} NwBody;

#define NW_JOINT_NONE UINT16_MAX

typedef struct NwMtlSnap {
    NwMtlState gs;
    float diffuse[4];
    float vu[8];    /* VU 0x14-0x15 (numtl_s.vuattr): [0] [1] NuMtlAnimate's UV offset */
    int32_t row[2]; /* the STROW row: the offset x4096, for geoms that unpack their texcoords under STMOD 1 */
    const NwGpuTex *tex;
    NwSurf surf; /* a texture in GS memory instead of tex */
    const void *key; /* the material, for interpolation */
    uint8_t wrap;    /* 1 u, 2 v: the offset scrolls, kept in 0-1 (NuMtlAnimate's mode 2) */
    NwGemInfo gem;
} NwMtlSnap;

#define NW_MTL_NONE UINT32_MAX   /* NwCmd.mtl of a command with no material */
#define NW_LIGHT_NONE UINT32_MAX /* NwCmd.light of an unlit draw */

typedef struct NwCmd {
    uint8_t op;
    uint16_t epoch;                 /* game thread only */
    uint32_t scope;                 /* game thread only */
    uint64_t key;                   /* the same draw in the frame before has it (src/gfx/interp.h); 0 none */
    uint32_t view;
    uint32_t mtl;                   /* into NwPacket.mtls, from material when the packet is published */
    uint32_t light;
    const void *mpg;                /* the microcode package (an NwVuProg) */
    const NwGpuTex *texture_override; /* an alternate texture for this draw */
    const struct numtl_s *material; /* game thread only */
    union {
        struct {
            union {
                const NwGpuMesh *mesh;
                const NwGpuSprites *sprites;
                const NwGpuSkin *skin;
            };
            float local[16];
            uint32_t first, count; /* into NwPacket.idx, joints, verts or ptls by op; WAVES: first into waves */
            uint32_t weights, nweights; /* SKIN: the blend-shape weights in NwPacket.weights */
            uint8_t mode;               /* VERTS: an NwVertsMode */
            uint16_t alpha;             /* VERTS: 0x100 | an ALPHA_1 written over the material's; 0 none */
            const NwGpuTex *clut;       /* VERTS: a CLUT over the material texture's, NULL none */
            uint32_t zclut;             /* VERTS: 1 + its depth CLUT in NwPacket.zcluts, 0 none */
            bool exact;                 /* VERTS: GS blend in the program for As past 0x80; no overlapping triangles */
            int8_t stencil;             /* VERTS: +1/-1 counted where Z passes; NW_STENCIL_SHADOWED: drawn at 0x80+ */
            uint32_t silhouette;
            float silhouette_scale;
            uint32_t ptllist;           /* PARTICLES: the effect list in NwPacket.ptllists */
            uint32_t body;              /* 1 + index into NwPacket.bodies, 0 none */
            uint16_t joint;             /* the body's joint local hangs on; NW_JOINT_NONE: its matrix */
        };
        struct {
            int32_t xy[4]; /* the corners in GS 12.4 */
            float st[4];   /* s, t at each corner (Q 1) */
            uint8_t rgba[4];
        } rect;
        struct {
            uint8_t rgba[4];
            uint32_t z;   /* GS Z */
            uint8_t mask; /* 1 colour, 2 Z */
        } clear;
        struct {
            int32_t width; /* the pixels the target's Z takes the back buffer's width in */
        } volumes;
        struct {
            NwSurf surf;          /* addr 0: the port does not draw it; tcc false: PSMCT24, whose alpha draws leave */
            int32_t width, height; /* the target's pixels (nurndr_pixel_width, height) */
            bool zback;            /* the Z buffer is the back buffer's (-2), shared when the sizes match */
        } target;
    };
} NwCmd;

#define NW_STENCIL_SHADOWED 2

typedef struct NwPacket {
    uint64_t seq;  /* 1 for the first published packet */
    int32_t vbcnt; /* nuvideo_global_vbcnt when it was published */
    uint64_t edge; /* the clock's edges then */
    bool snap;     /* a cut: drawn without blending from the packet before */
    NwCmd *cmds;
    uint32_t ncmds, cmdcap;
    NwView *views;
    uint32_t nviews, viewcap;
    NwMtlSnap *mtls;
    uint32_t nmtls, mtlcap;
    NwLights *lights;
    uint32_t nlights, lightcap;
    uint32_t *idx;
    uint32_t nidx, idxcap;
    float (*joints)[16]; /* the SKIN palettes, INV_WT times the joint matrices (row-major, row vectors) */
    uint32_t njoints, jointcap;
    float *weights;
    uint32_t nweights, weightcap;
    NwVuSpriteVertex *verts;
    uint32_t nverts, vertcap;
    NwPtlList *ptllists;
    uint32_t nptllists, ptllistcap;
    NwPtl *ptls;
    uint32_t nptls, ptlcap;
    NwZClut *zcluts;
    uint32_t nzcluts, zclutcap;
    NwWaves *waves;
    uint32_t nwaves, wavecap;
    NwBody *bodies;
    uint32_t nbodies, bodycap;
    float (*bones)[16]; /* model space */
    int8_t *parents;    /* within the body; -1 a root */
    uint32_t nbones, bonecap;
} NwPacket;

/* NULL on out of memory. */
NwCmd *nw_packet_cmd(NwPacket *p);
NwView *nw_packet_view(NwPacket *p, uint32_t *index);
NwMtlSnap *nw_packet_mtl(NwPacket *p, uint32_t *index);
NwLights *nw_packet_light(NwPacket *p, uint32_t *index);
uint32_t *nw_packet_idx(NwPacket *p, uint32_t n, uint32_t *first);
float (*nw_packet_joints(NwPacket *p, uint32_t n, uint32_t *first))[16];
float *nw_packet_weights(NwPacket *p, uint32_t n, uint32_t *first);
NwVuSpriteVertex *nw_packet_verts(NwPacket *p, uint32_t n, uint32_t *first);
NwPtlList *nw_packet_ptllist(NwPacket *p, uint32_t *index);
NwPtl *nw_packet_ptls(NwPacket *p, uint32_t n, uint32_t *first);
NwZClut *nw_packet_zclut(NwPacket *p, uint32_t *index);
NwWaves *nw_packet_waves(NwPacket *p, uint32_t *index);
/* The caller fills its n bones at .first and their parents at *parents. */
NwBody *nw_packet_body(NwPacket *p, uint32_t n, uint32_t *index, int8_t **parents);
/* Empties p, keeping its memory. */
void nw_packet_reset(NwPacket *p);

/* Game thread; publishing stamps it and makes it the newest for the renderer (returns its seq). */
NwPacket *nw_packet_building(void);
uint64_t nw_packet_publish(int32_t vbcnt);
/* Render thread: the newest published packet, else the last one taken; NULL before the first. */
const NwPacket *nw_packet_take(void);
/* The same, and the packet it took before that one at *prev (NULL: none). */
const NwPacket *nw_packet_take_pair(const NwPacket **prev);

/* The frees come last: nw_gpu_take fences every op from NW_GPU_FREE_MESH on. */
typedef enum NwGpuOp {
    NW_GPU_UPLOAD_MESH,
    NW_GPU_UPLOAD_TEX,
    NW_GPU_UPLOAD_SPRITES,
    NW_GPU_UPLOAD_SKIN,
    NW_GPU_FREE_MESH,
    NW_GPU_FREE_TEX,
    NW_GPU_FREE_SPRITES,
    NW_GPU_FREE_SKIN,
    NW_GPU_FREE_HOST, /* SDL_malloc'd memory the game thread's draws read until the fence */
} NwGpuOp;

typedef struct NwGpuJob {
    NwGpuOp op;
    void *obj;
    uint64_t fence; /* a free waits until a packet after this one is drawn */
} NwGpuJob;

/* Game thread: queues a job for the render thread, which owns the object after a free. */
void nw_gpu_queue(NwGpuOp op, void *obj);
/* Render thread, before drawing packet seq: moves the jobs due to *out (grown as needed) in queue order. */
uint32_t nw_gpu_take(uint64_t seq, NwGpuJob **out, uint32_t *cap);

#endif
