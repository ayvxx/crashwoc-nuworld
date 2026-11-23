#ifndef NU_PORT_H
#define NU_PORT_H

/* Store or read a T at p and step p past it; the EE spelling is gcc's cast-as-lvalue extension, which ISO C lacks. */
#ifdef NW_HOST
#define NW_PUT(T, p, v) (*(T *)(p) = (v), (p) = (void *)((T *)(p) + 1))
#define NW_GET(T, p) ((p) = (void *)((T *)(p) + 1), ((T *)(p))[-1])
#else
#define NW_PUT(T, p, v) *((T *)p)++ = v
#define NW_GET(T, p) *((T *)p)++
#endif

/* Ends a retail non-void function that falls off its end; empty on the EE, where callers ignore the value. */
#ifdef NW_HOST
#define NW_HOST_RETURN(v) return v
#else
#define NW_HOST_RETURN(v)
#endif

/* Alignment only the host needs (a header before 16-byte aligned data); the EE layout stays. */
#ifdef NW_HOST
#define NW_HOST_ALIGNED(n) __attribute__((aligned(n)))
#else
#define NW_HOST_ALIGNED(n)
#endif

#ifdef NW_HOST
#define NW_HOST_BITS(ee, host) host
#else
#define NW_HOST_BITS(ee, host) ee
#endif


#ifdef NW_HOST
void nw_ee_idle(void);
#define NW_HOST_IDLE() nw_ee_idle()
#else
#define NW_HOST_IDLE()
#endif

/* The cursor steps retail's n * ps2size; on the host x moves to zeroed twins beside the heap (src/port/heap.c). */
#ifdef NW_HOST
void *nw_heap_twin(void *at, __SIZE_TYPE__ charge, __SIZE_TYPE__ size, __SIZE_TYPE__ align);
#define NW_HEAP_TWIN(x, cursor, n, ps2size) \
    ((cursor) = (void *)((char *)(x) + (__SIZE_TYPE__)(n) * (ps2size)), \
     (x) = nw_heap_twin((x), (__SIZE_TYPE__)(n) * (ps2size), (__SIZE_TYPE__)(n) * sizeof(*(x)), __alignof__(*(x))))
#else
#define NW_HEAP_TWIN(x, cursor, n, ps2size) ((void)sizeof(char[sizeof(*(x)) == (ps2size) ? 1 : -1]))
#endif

#ifdef NW_HOST
#define NW_HEAP_REC(at, rec) ((__typeof__(at))(rec))
#else
#define NW_HEAP_REC(at, rec) at
#endif

#ifdef NW_HOST
#define NW_HEAP_AIM(p, at, rec) ((p) == (void *)(at) ? (void)((p) = (rec)) : (void)0)
#else
#define NW_HEAP_AIM(p, at, rec)
#endif

/* main.c's superbuffer: the loads inside it align to 0x80, so the host keeps retail's address modulo the alignment. */
#ifdef NW_HOST
void *nw_superbuffer_alloc(int size, char *file, int line);
#define NW_SUPERBUFFER_ALLOC(size, file, line) nw_superbuffer_alloc(size, file, line)
#else
#define NW_SUPERBUFFER_ALLOC(size, file, line) NuMemAllocFn(size, file, line)
#endif

/* NuFileLoadBuffer's buffer size */
#ifdef NW_HOST
int nw_load_room(const char *file, const void *mem, int size);
#define NW_LOAD_ROOM(file, mem, size) nw_load_room((file), (mem), (size))
#else
#define NW_LOAD_ROOM(file, mem, size) (size)
#endif

/* A load's buffer size where retail passes none (0x7FFFFFFF) */
#ifdef NW_HOST
#define NW_LOAD_SIZE(ps2, host) (host)
#else
#define NW_LOAD_SIZE(ps2, host) (ps2)
#endif

/* A faithful out-of-object read: ps2 reads retail's layout, host reads the same bytes from the host layout. */
#ifdef NW_HOST
#define NW_RETAIL(ps2, host) (host)
#else
#define NW_RETAIL(ps2, host) (ps2)
#endif

/* obj->member[i], possibly past the array into the next members; the host indexes from obj to stay inside obj. */
#ifdef NW_HOST
#define NW_PAST(obj, member, i) \
    (((__typeof__(&(obj)->member[0]))((char *)(obj) + __builtin_offsetof(__typeof__(*(obj)), member)))[i])
#else
#define NW_PAST(obj, member, i) ((obj)->member[i])
#endif

/* EE-float lerp: range 0 (two keys at one time) gives the largest float where IEEE gives inf or NaN. */
#ifdef NW_HOST
float nw_ee_lerp(float a, float dt, float range, float b);
#define NW_EE_LERP(a, dt, range, b) nw_ee_lerp(a, dt, range, b)
#else
#define NW_EE_LERP(a, dt, range, b) ((a) + (dt) / (range) * ((b) - (a)))
#endif

/* (int)x as the EE's cvt.w.s: saturating where C's conversion is undefined. */
#ifdef NW_HOST
int nw_ee_s32(float x);
#define NW_EE_S32(x) nw_ee_s32(x)
#else
#define NW_EE_S32(x) ((int)(x))
#endif

/* a / b as the EE's div.s: by zero gives the largest float with the operands' combined sign, not inf or NaN. */
#ifdef NW_HOST
float nw_ee_fdiv(float a, float b);
#define NW_EE_DIV(a, b) nw_ee_fdiv(a, b)
#else
#define NW_EE_DIV(a, b) ((a) / (b))
#endif

/* Skips -fsanitize=function for calls through retail's mismatched prototypes, which both ABIs accept. */
#ifdef NW_HOST
#define NW_HOST_LOOSE_CALLS __attribute__((no_sanitize("function")))
#else
#define NW_HOST_LOOSE_CALLS
#endif

/* A local retail reads after returning (a looping sound's position): the EE stack keeps it, the host makes it static. */
#ifdef NW_HOST
#define NW_HOST_KEPT static
#else
#define NW_HOST_KEPT
#endif

/* Host-only initializer for a variable retail reads before setting it: the value retail finds there. */
#ifdef NW_HOST
#define NW_HOST_INIT(v) = (v)
#else
#define NW_HOST_INIT(v)
#endif

/* Hor+ widescreen (src/port/wide.c): drawing widens by nw_wide.k (1 is retail), logic keeps retail's 4:3 tests. */
#ifdef NW_HOST
struct nuinstance_s;
struct numtx_s;
struct nw_wide_s {
    float k;       /* view width over retail's 4:3 width */
    float aspect;  /* h/w */
    int width;     /* GS frame width, 640 at 4:3 */
    float hud;     /* HUD x reach over retail's */
    float draw;    /* draw radii over retail's */
    float draw2;
    float distance; /* the draw distance setting alone, without the widening: the enemies' spawn radius */
    int pvs_all;
};
extern struct nw_wide_s nw_wide;
int nw_wide_anim(struct nuinstance_s *inst, int test);
void nw_wide_mark(void);
int nw_wide_seen(int drawn);
void nw_wide_vp(int paused);
int nw_wide_vp_x(void); /* 1/16 pixels */
void nw_wide_hud(struct numtx_s *m);
float nw_wide_ai_range2(void); /* ManageCreatures spawn radius squared at retails draw distance */
#define NW_WIDE(ps2, host) (host)
#define NW_WIDE_SCALE(x, f) ((x) * (f))
#define NW_WIDE_ONLY(stmt) stmt
#define NW_WIDE_PVS(test) ((test) || nw_wide.pvs_all)
#define NW_WIDE_ANIM(inst, test) nw_wide_anim(inst, test)
#define NW_WIDE_SEEN(drawn) (nw_wide_mark(), nw_wide_seen(drawn))
#define NW_WIDE_HUD(m) nw_wide_hud(m)
#else
#define NW_WIDE(ps2, host) (ps2)
#define NW_WIDE_SCALE(x, f) (x)
#define NW_WIDE_ONLY(stmt)
#define NW_WIDE_PVS(test) (test)
#define NW_WIDE_ANIM(inst, test) (test)
#define NW_WIDE_SEEN(drawn) (drawn)
#define NW_WIDE_HUD(m)
#endif

/* Display-rate interpolation (src/gfx/interp.c): keys stop a skipped draw from shifting later ones; SNAP is a cut. */
#ifdef NW_HOST
void nw_interp_key(const void *key);
void nw_interp_slot(const void *owner, unsigned long long place);
void nw_interp_snap(void);
#define NW_INTERP_KEY(p) nw_interp_key(p)
#define NW_INTERP_SLOT(p, i) nw_interp_slot(p, i)
#define NW_INTERP_SNAP() nw_interp_snap()
#else
#define NW_INTERP_KEY(p)
#define NW_INTERP_SLOT(p, i)
#define NW_INTERP_SNAP()
#endif

/* 50/60 Hz tick value (src/port/hz.c), PAL's on the PS2; NW_HZ_NTSC builds NTSC's for tools/ntsc_diff.py. */
#ifdef NW_HOST
extern int nw_hz;
#define NW_HZ(pal, ntsc) (nw_hz == 60 ? (ntsc) : (pal))
#elif defined(NW_HZ_NTSC)
#define NW_HZ(pal, ntsc) (ntsc)
#else
#define NW_HZ(pal, ntsc) (pal)
#endif

#ifdef NW_HOST
extern int nw_rev;
#define NW_REV(old, new) (nw_rev ? (new) : (old))
#define NW_REV_DIM(old, new) (new)
#elif defined(NW_REV_LATEST)
#define NW_REV(old, new) (new)
#define NW_REV_DIM(old, new) (new)
#else
#define NW_REV(old, new) (old)
#define NW_REV_DIM(old, new) (old)
#endif

/* NuWorld's longer names (src/port/names.h), scaled to fit letters wide; the game's own on the PS2. */
#ifdef NW_HOST
struct game_s;
char *nw_names_of(const struct game_s *g);
char *nw_names_time_text(const void *time);
float nw_names_scale(float scale, const char *text, __SIZE_TYPE__ fit);
#define NW_GAME_NAME(g) nw_names_of(g)
#define NW_TIME_NAME(t) nw_names_time_text(t)
#define NW_NAME_SCALE(scale, text, fit) nw_names_scale((scale), (text), (fit))
#else
#define NW_GAME_NAME(g) ((g)->name)
#define NW_TIME_NAME(t) (t)
#define NW_NAME_SCALE(scale, text, fit) (scale)
#endif

#ifdef NW_HOST
unsigned short nw_relic_spin(unsigned short still, unsigned short turning);
float nw_relic_glow(float scale, unsigned short turning);
#define NW_RELIC_SPIN(still, turning) nw_relic_spin((still), (turning))
#define NW_RELIC_GLOW(scale, turning) nw_relic_glow((scale), (turning))
#else
#define NW_RELIC_SPIN(still, turning) (still)
#define NW_RELIC_GLOW(scale, turning) (scale)
#endif

#endif /* NU_PORT_H */
