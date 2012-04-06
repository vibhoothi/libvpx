/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "mcomp.h"
#include "vpx_mem/vpx_mem.h"
#include "vpx_ports/config.h"
#include <stdio.h>
#include <limits.h>
#include <math.h>
#include "vp8/common/findnearmv.h"

#ifdef ENTROPY_STATS
static int mv_ref_ct [31] [4] [2];
static int mv_mode_cts [4] [2];
#endif

#if CONFIG_HIGH_PRECISION_MV
int vp8_mv_bit_cost(int_mv *mv, int_mv *ref, int *mvcost[2], int Weight, int ishp)
{
    // MV costing is based on the distribution of vectors in the previous frame and as such will tend to
    // over state the cost of vectors. In addition coding a new vector can have a knock on effect on the
    // cost of subsequent vectors and the quality of prediction from NEAR and NEAREST for subsequent blocks.
    // The "Weight" parameter allows, to a limited extent, for some account to be taken of these factors.
    return ((mvcost[0][(mv->as_mv.row - ref->as_mv.row) >> (ishp==0)] + mvcost[1][(mv->as_mv.col - ref->as_mv.col) >> (ishp==0)]) * Weight) >> 7;
}
#else
int vp8_mv_bit_cost(int_mv *mv, int_mv *ref, int *mvcost[2], int Weight)
{
    // MV costing is based on the distribution of vectors in the previous frame and as such will tend to
    // over state the cost of vectors. In addition coding a new vector can have a knock on effect on the
    // cost of subsequent vectors and the quality of prediction from NEAR and NEAREST for subsequent blocks.
    // The "Weight" parameter allows, to a limited extent, for some account to be taken of these factors.
    return ((mvcost[0][(mv->as_mv.row - ref->as_mv.row) >> 1] + mvcost[1][(mv->as_mv.col - ref->as_mv.col) >> 1]) * Weight) >> 7;
}
#endif

#if CONFIG_HIGH_PRECISION_MV
static int mv_err_cost(int_mv *mv, int_mv *ref, int *mvcost[2], int error_per_bit, int ishp)
{
    return ((mvcost[0][(mv->as_mv.row - ref->as_mv.row) >> (ishp==0)] +
        mvcost[1][(mv->as_mv.col - ref->as_mv.col) >> (ishp==0)])
        * error_per_bit + 128) >> 8;
}
#else
static int mv_err_cost(int_mv *mv, int_mv *ref, int *mvcost[2], int error_per_bit)
{
    return ((mvcost[0][(mv->as_mv.row - ref->as_mv.row) >> 1] +
        mvcost[1][(mv->as_mv.col - ref->as_mv.col) >> 1])
        * error_per_bit + 128) >> 8;
}
#endif


static int mvsad_err_cost(int_mv *mv, int_mv *ref, int *mvsadcost[2], int error_per_bit)
{
    /* Calculate sad error cost on full pixel basis. */
    return ((mvsadcost[0][(mv->as_mv.row - ref->as_mv.row)] +
        mvsadcost[1][(mv->as_mv.col - ref->as_mv.col)])
        * error_per_bit + 128) >> 8;
}


void vp8_init_dsmotion_compensation(MACROBLOCK *x, int stride)
{
    int Len;
    int search_site_count = 0;


    // Generate offsets for 4 search sites per step.
    Len = MAX_FIRST_STEP;
    x->ss[search_site_count].mv.col = 0;
    x->ss[search_site_count].mv.row = 0;
    x->ss[search_site_count].offset = 0;
    search_site_count++;

    while (Len > 0)
    {

        // Compute offsets for search sites.
        x->ss[search_site_count].mv.col = 0;
        x->ss[search_site_count].mv.row = -Len;
        x->ss[search_site_count].offset = -Len * stride;
        search_site_count++;

        // Compute offsets for search sites.
        x->ss[search_site_count].mv.col = 0;
        x->ss[search_site_count].mv.row = Len;
        x->ss[search_site_count].offset = Len * stride;
        search_site_count++;

        // Compute offsets for search sites.
        x->ss[search_site_count].mv.col = -Len;
        x->ss[search_site_count].mv.row = 0;
        x->ss[search_site_count].offset = -Len;
        search_site_count++;

        // Compute offsets for search sites.
        x->ss[search_site_count].mv.col = Len;
        x->ss[search_site_count].mv.row = 0;
        x->ss[search_site_count].offset = Len;
        search_site_count++;

        // Contract.
        Len /= 2;
    }

    x->ss_count = search_site_count;
    x->searches_per_step = 4;
}

void vp8_init3smotion_compensation(MACROBLOCK *x, int stride)
{
    int Len;
    int search_site_count = 0;

    // Generate offsets for 8 search sites per step.
    Len = MAX_FIRST_STEP;
    x->ss[search_site_count].mv.col = 0;
    x->ss[search_site_count].mv.row = 0;
    x->ss[search_site_count].offset = 0;
    search_site_count++;

    while (Len > 0)
    {

        // Compute offsets for search sites.
        x->ss[search_site_count].mv.col = 0;
        x->ss[search_site_count].mv.row = -Len;
        x->ss[search_site_count].offset = -Len * stride;
        search_site_count++;

        // Compute offsets for search sites.
        x->ss[search_site_count].mv.col = 0;
        x->ss[search_site_count].mv.row = Len;
        x->ss[search_site_count].offset = Len * stride;
        search_site_count++;

        // Compute offsets for search sites.
        x->ss[search_site_count].mv.col = -Len;
        x->ss[search_site_count].mv.row = 0;
        x->ss[search_site_count].offset = -Len;
        search_site_count++;

        // Compute offsets for search sites.
        x->ss[search_site_count].mv.col = Len;
        x->ss[search_site_count].mv.row = 0;
        x->ss[search_site_count].offset = Len;
        search_site_count++;

        // Compute offsets for search sites.
        x->ss[search_site_count].mv.col = -Len;
        x->ss[search_site_count].mv.row = -Len;
        x->ss[search_site_count].offset = -Len * stride - Len;
        search_site_count++;

        // Compute offsets for search sites.
        x->ss[search_site_count].mv.col = Len;
        x->ss[search_site_count].mv.row = -Len;
        x->ss[search_site_count].offset = -Len * stride + Len;
        search_site_count++;

        // Compute offsets for search sites.
        x->ss[search_site_count].mv.col = -Len;
        x->ss[search_site_count].mv.row = Len;
        x->ss[search_site_count].offset = Len * stride - Len;
        search_site_count++;

        // Compute offsets for search sites.
        x->ss[search_site_count].mv.col = Len;
        x->ss[search_site_count].mv.row = Len;
        x->ss[search_site_count].offset = Len * stride + Len;
        search_site_count++;


        // Contract.
        Len /= 2;
    }

    x->ss_count = search_site_count;
    x->searches_per_step = 8;
}

/*
 * To avoid the penalty for crossing cache-line read, preload the reference
 * area in a small buffer, which is aligned to make sure there won't be crossing
 * cache-line read while reading from this buffer. This reduced the cpu
 * cycles spent on reading ref data in sub-pixel filter functions.
 * TODO: Currently, since sub-pixel search range here is -3 ~ 3, copy 22 rows x
 * 32 cols area that is enough for 16x16 macroblock. Later, for SPLITMV, we
 * could reduce the area.
 */

#define PRE(r,c) (y + (((r)>>2) * y_stride + ((c)>>2) -(offset))) // pointer to predictor base of a motionvector
#if CONFIG_SIXTEENTH_SUBPEL_UV
#define SP(x) (((x)&3)<<2) // convert motion vector component to offset for svf calc
#else
#define SP(x) (((x)&3)<<1) // convert motion vector component to offset for svf calc
#endif  /* CONFIG_SIXTEENTH_SUBPEL_UV */
#define MVC(r,c) (((mvcost[0][(r)-rr] + mvcost[1][(c)-rc]) * error_per_bit + 128 )>>8 ) // estimated cost of a motion vector (r,c)
#define DIST(r,c) vfp->svf( PRE(r,c), y_stride, SP(c),SP(r), z,b->src_stride,&sse) // returns subpixel variance error function.
#define ERR(r,c) (MVC(r,c)+DIST(r,c)) // returns distortion + motion vector cost
#define IFMVCV(r,c,s,e) if ( c >= minc && c <= maxc && r >= minr && r <= maxr) s else e;

#if CONFIG_HIGH_PRECISION_MV
#define PREHP(r,c) (y + (((r)>>3) * y_stride + ((c)>>3) -(offset))) // pointer to predictor base of a motionvector
#if CONFIG_SIXTEENTH_SUBPEL_UV
#define SPHP(x) (((x)&7)<<1) // convert motion vector component to offset for svf calc
#else /* CONFIG_SIXTEENTH_SUBPEL_UV */
#define SPHP(x) ((x)&7) // convert motion vector component to offset for svf calc
#endif  /* CONFIG_SIXTEENTH_SUBPEL_UV */
#define DISTHP(r,c) vfp->svf( PREHP(r,c), y_stride, SPHP(c),SPHP(r), z,b->src_stride,&sse) // returns subpixel variance error function.
#define ERRHP(r,c) (MVC(r,c)+DISTHP(r,c)) // returns distortion + motion vector cost
#define CHECK_BETTER(v,r,c) IFMVCV(r,c,{thismse = ((xd->allow_high_precision_mv)?DISTHP(r,c):DIST(r,c)); if((v = (MVC(r,c)+thismse)) < besterr) { besterr = v; br=r; bc=c; *distortion = thismse; *sse1 = sse; }}, v=INT_MAX;)// checks if (r,c) has better score than previous best
#else
#define CHECK_BETTER(v,r,c) IFMVCV(r,c,{thismse = DIST(r,c); if((v = (MVC(r,c)+thismse)) < besterr) { besterr = v; br=r; bc=c; *distortion = thismse; *sse1 = sse; }}, v=INT_MAX;)// checks if (r,c) has better score than previous best
#endif /* CONFIG_HIGH_PRECISION_MV */

#define MIN(x,y) (((x)<(y))?(x):(y))
#define MAX(x,y) (((x)>(y))?(x):(y))

int vp8_find_best_sub_pixel_step_iteratively(MACROBLOCK *x, BLOCK *b, BLOCKD *d,
                                             int_mv *bestmv, int_mv *ref_mv,
                                             int error_per_bit,
                                             const vp8_variance_fn_ptr_t *vfp,
                                             int *mvcost[2], int *distortion,
                                             unsigned int *sse1)
{
    unsigned char *z = (*(b->base_src) + b->src);
    MACROBLOCKD *xd = &x->e_mbd;

    int rr, rc, br, bc, hstep;
    int tr, tc;
    unsigned int besterr = INT_MAX;
    unsigned int left, right, up, down, diag;
    unsigned int sse;
    unsigned int whichdir;
    unsigned int halfiters = 4;
    unsigned int quarteriters = 4;
#if CONFIG_HIGH_PRECISION_MV
    unsigned int eighthiters = 4;
#endif
    int thismse;
    int maxc, minc, maxr, minr;
    int y_stride;
    int offset;

#if ARCH_X86 || ARCH_X86_64
    unsigned char *y0 = *(d->base_pre) + d->pre + (bestmv->as_mv.row) * d->pre_stride + bestmv->as_mv.col;
    unsigned char *y;
    int buf_r1, buf_r2, buf_c1, buf_c2;

    // Clamping to avoid out-of-range data access
    buf_r1 = ((bestmv->as_mv.row - INTERP_EXTEND) < x->mv_row_min)?(bestmv->as_mv.row - x->mv_row_min):INTERP_EXTEND;
    buf_r2 = ((bestmv->as_mv.row + INTERP_EXTEND) > x->mv_row_max)?(x->mv_row_max - bestmv->as_mv.row):INTERP_EXTEND;
    buf_c1 = ((bestmv->as_mv.col - INTERP_EXTEND) < x->mv_col_min)?(bestmv->as_mv.col - x->mv_col_min):INTERP_EXTEND;
    buf_c2 = ((bestmv->as_mv.col + INTERP_EXTEND) > x->mv_col_max)?(x->mv_col_max - bestmv->as_mv.col):INTERP_EXTEND;
    y_stride = 32;

    /* Copy to intermediate buffer before searching. */
    vfp->copymem(y0 - buf_c1 - d->pre_stride*buf_r1, d->pre_stride, xd->y_buf, y_stride, 16+buf_r1+buf_r2);
    y = xd->y_buf + y_stride*buf_r1 +buf_c1;
#else
    unsigned char *y = *(d->base_pre) + d->pre + (bestmv->as_mv.row) * d->pre_stride + bestmv->as_mv.col;
    y_stride = d->pre_stride;
#endif


#if CONFIG_HIGH_PRECISION_MV
    if (xd->allow_high_precision_mv)
    {
        rr = ref_mv->as_mv.row; rc = ref_mv->as_mv.col;
        br = bestmv->as_mv.row << 3; bc = bestmv->as_mv.col << 3;
        hstep = 4;
        minc = MAX(x->mv_col_min << 3, (ref_mv->as_mv.col) - ((1 << mvlong_width_hp) - 1));
        maxc = MIN(x->mv_col_max << 3, (ref_mv->as_mv.col) + ((1 << mvlong_width_hp) - 1));
        minr = MAX(x->mv_row_min << 3, (ref_mv->as_mv.row) - ((1 << mvlong_width_hp) - 1));
        maxr = MIN(x->mv_row_max << 3, (ref_mv->as_mv.row) + ((1 << mvlong_width_hp) - 1));
    }
    else
#endif
    {
        rr = ref_mv->as_mv.row >> 1; rc = ref_mv->as_mv.col >> 1;
        br = bestmv->as_mv.row << 2; bc = bestmv->as_mv.col << 2;
        hstep = 2;
        minc = MAX(x->mv_col_min << 2, (ref_mv->as_mv.col >> 1) - ((1 << mvlong_width) - 1));
        maxc = MIN(x->mv_col_max << 2, (ref_mv->as_mv.col >> 1) + ((1 << mvlong_width) - 1));
        minr = MAX(x->mv_row_min << 2, (ref_mv->as_mv.row >> 1) - ((1 << mvlong_width) - 1));
        maxr = MIN(x->mv_row_max << 2, (ref_mv->as_mv.row >> 1) + ((1 << mvlong_width) - 1));
    }

    tr = br;
    tc = bc;


    offset = (bestmv->as_mv.row) * y_stride + bestmv->as_mv.col;

    // central mv
    bestmv->as_mv.row <<= 3;
    bestmv->as_mv.col <<= 3;

    // calculate central point error
    besterr = vfp->vf(y, y_stride, z, b->src_stride, sse1);
    *distortion = besterr;
#if CONFIG_HIGH_PRECISION_MV
    besterr += mv_err_cost(bestmv, ref_mv, mvcost, error_per_bit, xd->allow_high_precision_mv);
#else
    besterr += mv_err_cost(bestmv, ref_mv, mvcost, error_per_bit);
#endif

    // TODO: Each subsequent iteration checks at least one point in common with the last iteration could be 2 ( if diag selected)
    while (--halfiters)
    {
        // 1/2 pel
        CHECK_BETTER(left, tr, tc - hstep);
        CHECK_BETTER(right, tr, tc + hstep);
        CHECK_BETTER(up, tr - hstep, tc);
        CHECK_BETTER(down, tr + hstep, tc);

        whichdir = (left < right ? 0 : 1) + (up < down ? 0 : 2);

        switch (whichdir)
        {
        case 0:
            CHECK_BETTER(diag, tr - hstep, tc - hstep);
            break;
        case 1:
            CHECK_BETTER(diag, tr - hstep, tc + hstep);
            break;
        case 2:
            CHECK_BETTER(diag, tr + hstep, tc - hstep);
            break;
        case 3:
            CHECK_BETTER(diag, tr + hstep, tc + hstep);
            break;
        }

        // no reason to check the same one again.
        if (tr == br && tc == bc)
            break;

        tr = br;
        tc = bc;
    }

    // TODO: Each subsequent iteration checks at least one point in common with the last iteration could be 2 ( if diag selected)
    // 1/4 pel
    hstep >>= 1;
    while (--quarteriters)
    {
        CHECK_BETTER(left, tr, tc - hstep);
        CHECK_BETTER(right, tr, tc + hstep);
        CHECK_BETTER(up, tr - hstep, tc);
        CHECK_BETTER(down, tr + hstep, tc);

        whichdir = (left < right ? 0 : 1) + (up < down ? 0 : 2);

        switch (whichdir)
        {
        case 0:
            CHECK_BETTER(diag, tr - hstep, tc - hstep);
            break;
        case 1:
            CHECK_BETTER(diag, tr - hstep, tc + hstep);
            break;
        case 2:
            CHECK_BETTER(diag, tr + hstep, tc - hstep);
            break;
        case 3:
            CHECK_BETTER(diag, tr + hstep, tc + hstep);
            break;
        }

        // no reason to check the same one again.
        if (tr == br && tc == bc)
            break;

        tr = br;
        tc = bc;
    }

#if CONFIG_HIGH_PRECISION_MV
    if (x->e_mbd.allow_high_precision_mv)
    {
        hstep >>= 1;
        while (--eighthiters)
        {
            CHECK_BETTER(left, tr, tc - hstep);
            CHECK_BETTER(right, tr, tc + hstep);
            CHECK_BETTER(up, tr - hstep, tc);
            CHECK_BETTER(down, tr + hstep, tc);

            whichdir = (left < right ? 0 : 1) + (up < down ? 0 : 2);

            switch (whichdir)
            {
            case 0:
                CHECK_BETTER(diag, tr - hstep, tc - hstep);
                break;
            case 1:
                CHECK_BETTER(diag, tr - hstep, tc + hstep);
                break;
            case 2:
                CHECK_BETTER(diag, tr + hstep, tc - hstep);
                break;
            case 3:
                CHECK_BETTER(diag, tr + hstep, tc + hstep);
                break;
            }

            // no reason to check the same one again.
            if (tr == br && tc == bc)
                break;

            tr = br;
            tc = bc;
        }
    }
#endif
#if CONFIG_HIGH_PRECISION_MV
    if (x->e_mbd.allow_high_precision_mv)
    {
        bestmv->as_mv.row = br;
        bestmv->as_mv.col = bc;
    }
    else
#endif  /* CONFIG_HIGH_PRECISION_MV */
    {
        bestmv->as_mv.row = br << 1;
        bestmv->as_mv.col = bc << 1;
    }

    if ((abs(bestmv->as_mv.col - ref_mv->as_mv.col) > (MAX_FULL_PEL_VAL<<3)) ||
        (abs(bestmv->as_mv.row - ref_mv->as_mv.row) > (MAX_FULL_PEL_VAL<<3)))
        return INT_MAX;

    return besterr;
}
#undef MVC
#undef PRE
#undef SP
#undef DIST
#undef IFMVCV
#undef ERR
#undef CHECK_BETTER
#undef MIN
#undef MAX

#if CONFIG_HIGH_PRECISION_MV
#undef PREHP
#undef DPHP
#undef DISTHP
#undef ERRHP
#endif

#if CONFIG_SIXTEENTH_SUBPEL_UV
#define SP(x) (((x)&7)<<1) // convert motion vector component to offset for svf calc
#else
#define SP(x) ((x)&7) // convert motion vector component to offset for svf calc
#endif  /* CONFIG_SIXTEENTH_SUBPEL_UV */
int vp8_find_best_sub_pixel_step(MACROBLOCK *x, BLOCK *b, BLOCKD *d,
                                 int_mv *bestmv, int_mv *ref_mv,
                                 int error_per_bit,
                                 const vp8_variance_fn_ptr_t *vfp,
                                 int *mvcost[2], int *distortion,
                                 unsigned int *sse1)
{
    int bestmse = INT_MAX;
    int_mv startmv;
    int_mv this_mv;
#if CONFIG_HIGH_PRECISION_MV
    int_mv orig_mv;
    int yrow_movedback=0, ycol_movedback=0;
#endif
    unsigned char *z = (*(b->base_src) + b->src);
    int left, right, up, down, diag;
    unsigned int sse;
    int whichdir ;
    int thismse;
    int y_stride;
    MACROBLOCKD *xd = &x->e_mbd;

#if ARCH_X86 || ARCH_X86_64
    unsigned char *y0 = *(d->base_pre) + d->pre + (bestmv->as_mv.row) * d->pre_stride + bestmv->as_mv.col;
    unsigned char *y;

    y_stride = 32;
    /* Copy 18 rows x 32 cols area to intermediate buffer before searching. */
     vfp->copymem(y0 - 1 - d->pre_stride, d->pre_stride, xd->y_buf, y_stride, 18);
     y = xd->y_buf + y_stride + 1;
#else
     unsigned char *y = *(d->base_pre) + d->pre + (bestmv->as_mv.row) * d->pre_stride + bestmv->as_mv.col;
     y_stride = d->pre_stride;
#endif

    // central mv
    bestmv->as_mv.row <<= 3;
    bestmv->as_mv.col <<= 3;
    startmv = *bestmv;
#if CONFIG_HIGH_PRECISION_MV
    orig_mv = *bestmv;
#endif

    // calculate central point error
    bestmse = vfp->vf(y, y_stride, z, b->src_stride, sse1);
    *distortion = bestmse;
#if CONFIG_HIGH_PRECISION_MV
    bestmse += mv_err_cost(bestmv, ref_mv, mvcost, error_per_bit, xd->allow_high_precision_mv);
#else
    bestmse += mv_err_cost(bestmv, ref_mv, mvcost, error_per_bit);
#endif

    // go left then right and check error
    this_mv.as_mv.row = startmv.as_mv.row;
    this_mv.as_mv.col = ((startmv.as_mv.col - 8) | 4);
    thismse = vfp->svf_halfpix_h(y - 1, y_stride, z, b->src_stride, &sse);
#if CONFIG_HIGH_PRECISION_MV
    left = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit, xd->allow_high_precision_mv);
#else
    left = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit);
#endif

    if (left < bestmse)
    {
        *bestmv = this_mv;
        bestmse = left;
        *distortion = thismse;
        *sse1 = sse;
    }

    this_mv.as_mv.col += 8;
    thismse = vfp->svf_halfpix_h(y, y_stride, z, b->src_stride, &sse);
#if CONFIG_HIGH_PRECISION_MV
    right = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit, xd->allow_high_precision_mv);
#else
    right = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit);
#endif

    if (right < bestmse)
    {
        *bestmv = this_mv;
        bestmse = right;
        *distortion = thismse;
        *sse1 = sse;
    }

    // go up then down and check error
    this_mv.as_mv.col = startmv.as_mv.col;
    this_mv.as_mv.row = ((startmv.as_mv.row - 8) | 4);
    thismse =  vfp->svf_halfpix_v(y - y_stride, y_stride, z, b->src_stride, &sse);
#if CONFIG_HIGH_PRECISION_MV
    up = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit, xd->allow_high_precision_mv);
#else
    up = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit);
#endif

    if (up < bestmse)
    {
        *bestmv = this_mv;
        bestmse = up;
        *distortion = thismse;
        *sse1 = sse;
    }

    this_mv.as_mv.row += 8;
    thismse = vfp->svf_halfpix_v(y, y_stride, z, b->src_stride, &sse);
#if CONFIG_HIGH_PRECISION_MV
    down = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit, xd->allow_high_precision_mv);
#else
    down = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit);
#endif

    if (down < bestmse)
    {
        *bestmv = this_mv;
        bestmse = down;
        *distortion = thismse;
        *sse1 = sse;
    }


    // now check 1 more diagonal
    whichdir = (left < right ? 0 : 1) + (up < down ? 0 : 2);
    //for(whichdir =0;whichdir<4;whichdir++)
    //{
    this_mv = startmv;

    switch (whichdir)
    {
    case 0:
        this_mv.as_mv.col = (this_mv.as_mv.col - 8) | 4;
        this_mv.as_mv.row = (this_mv.as_mv.row - 8) | 4;
        thismse = vfp->svf_halfpix_hv(y - 1 - y_stride, y_stride, z, b->src_stride, &sse);
        break;
    case 1:
        this_mv.as_mv.col += 4;
        this_mv.as_mv.row = (this_mv.as_mv.row - 8) | 4;
        thismse = vfp->svf_halfpix_hv(y - y_stride, y_stride, z, b->src_stride, &sse);
        break;
    case 2:
        this_mv.as_mv.col = (this_mv.as_mv.col - 8) | 4;
        this_mv.as_mv.row += 4;
        thismse = vfp->svf_halfpix_hv(y - 1, y_stride, z, b->src_stride, &sse);
        break;
    case 3:
    default:
        this_mv.as_mv.col += 4;
        this_mv.as_mv.row += 4;
        thismse = vfp->svf_halfpix_hv(y, y_stride, z, b->src_stride, &sse);
        break;
    }

#if CONFIG_HIGH_PRECISION_MV
    diag = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit, xd->allow_high_precision_mv);
#else
    diag = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit);
#endif

    if (diag < bestmse)
    {
        *bestmv = this_mv;
        bestmse = diag;
        *distortion = thismse;
        *sse1 = sse;
    }

//  }


    // time to check quarter pels.
    if (bestmv->as_mv.row < startmv.as_mv.row)
    {
        y -= y_stride;
#if CONFIG_HIGH_PRECISION_MV
        yrow_movedback = 1;
#endif
    }

    if (bestmv->as_mv.col < startmv.as_mv.col)
    {
        y--;
#if CONFIG_HIGH_PRECISION_MV
        ycol_movedback = 1;
#endif
    }

    startmv = *bestmv;



    // go left then right and check error
    this_mv.as_mv.row = startmv.as_mv.row;

    if (startmv.as_mv.col & 7)
    {
        this_mv.as_mv.col = startmv.as_mv.col - 2;
        thismse = vfp->svf(y, y_stride, SP(this_mv.as_mv.col), SP(this_mv.as_mv.row), z, b->src_stride, &sse);
    }
    else
    {
        this_mv.as_mv.col = (startmv.as_mv.col - 8) | 6;
        thismse = vfp->svf(y - 1, y_stride, SP(6), SP(this_mv.as_mv.row), z, b->src_stride, &sse);
    }

#if CONFIG_HIGH_PRECISION_MV
    left = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit, xd->allow_high_precision_mv);
#else
    left = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit);
#endif

    if (left < bestmse)
    {
        *bestmv = this_mv;
        bestmse = left;
        *distortion = thismse;
        *sse1 = sse;
    }

    this_mv.as_mv.col += 4;
    thismse = vfp->svf(y, y_stride, SP(this_mv.as_mv.col), SP(this_mv.as_mv.row), z, b->src_stride, &sse);
#if CONFIG_HIGH_PRECISION_MV
    right = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit, xd->allow_high_precision_mv);
#else
    right = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit);
#endif

    if (right < bestmse)
    {
        *bestmv = this_mv;
        bestmse = right;
        *distortion = thismse;
        *sse1 = sse;
    }

    // go up then down and check error
    this_mv.as_mv.col = startmv.as_mv.col;

    if (startmv.as_mv.row & 7)
    {
        this_mv.as_mv.row = startmv.as_mv.row - 2;
        thismse = vfp->svf(y, y_stride, SP(this_mv.as_mv.col), SP(this_mv.as_mv.row), z, b->src_stride, &sse);
    }
    else
    {
        this_mv.as_mv.row = (startmv.as_mv.row - 8) | 6;
        thismse = vfp->svf(y - y_stride, y_stride, SP(this_mv.as_mv.col), SP(6), z, b->src_stride, &sse);
    }

#if CONFIG_HIGH_PRECISION_MV
    up = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit, xd->allow_high_precision_mv);
#else
    up = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit);
#endif

    if (up < bestmse)
    {
        *bestmv = this_mv;
        bestmse = up;
        *distortion = thismse;
        *sse1 = sse;
    }

    this_mv.as_mv.row += 4;
    thismse = vfp->svf(y, y_stride, SP(this_mv.as_mv.col), SP(this_mv.as_mv.row), z, b->src_stride, &sse);
#if CONFIG_HIGH_PRECISION_MV
    down = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit, xd->allow_high_precision_mv);
#else
    down = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit);
#endif

    if (down < bestmse)
    {
        *bestmv = this_mv;
        bestmse = down;
        *distortion = thismse;
        *sse1 = sse;
    }


    // now check 1 more diagonal
    whichdir = (left < right ? 0 : 1) + (up < down ? 0 : 2);

//  for(whichdir=0;whichdir<4;whichdir++)
//  {
    this_mv = startmv;

    switch (whichdir)
    {
    case 0:

        if (startmv.as_mv.row & 7)
        {
            this_mv.as_mv.row -= 2;

            if (startmv.as_mv.col & 7)
            {
                this_mv.as_mv.col -= 2;
                thismse = vfp->svf(y, y_stride, SP(this_mv.as_mv.col), SP(this_mv.as_mv.row), z, b->src_stride, &sse);
            }
            else
            {
                this_mv.as_mv.col = (startmv.as_mv.col - 8) | 6;
                thismse = vfp->svf(y - 1, y_stride, SP(6), SP(this_mv.as_mv.row), z, b->src_stride, &sse);;
            }
        }
        else
        {
            this_mv.as_mv.row = (startmv.as_mv.row - 8) | 6;

            if (startmv.as_mv.col & 7)
            {
                this_mv.as_mv.col -= 2;
                thismse = vfp->svf(y - y_stride, y_stride, SP(this_mv.as_mv.col), SP(6), z, b->src_stride, &sse);
            }
            else
            {
                this_mv.as_mv.col = (startmv.as_mv.col - 8) | 6;
                thismse = vfp->svf(y - y_stride - 1, y_stride, SP(6), SP(6), z, b->src_stride, &sse);
            }
        }

        break;
    case 1:
        this_mv.as_mv.col += 2;

        if (startmv.as_mv.row & 7)
        {
            this_mv.as_mv.row -= 2;
            thismse = vfp->svf(y, y_stride, SP(this_mv.as_mv.col), SP(this_mv.as_mv.row), z, b->src_stride, &sse);
        }
        else
        {
            this_mv.as_mv.row = (startmv.as_mv.row - 8) | 6;
            thismse = vfp->svf(y - y_stride, y_stride, SP(this_mv.as_mv.col), SP(6), z, b->src_stride, &sse);
        }

        break;
    case 2:
        this_mv.as_mv.row += 2;

        if (startmv.as_mv.col & 7)
        {
            this_mv.as_mv.col -= 2;
            thismse = vfp->svf(y, y_stride, SP(this_mv.as_mv.col), SP(this_mv.as_mv.row), z, b->src_stride, &sse);
        }
        else
        {
            this_mv.as_mv.col = (startmv.as_mv.col - 8) | 6;
            thismse = vfp->svf(y - 1, y_stride, SP(6), SP(this_mv.as_mv.row), z, b->src_stride, &sse);
        }

        break;
    case 3:
        this_mv.as_mv.col += 2;
        this_mv.as_mv.row += 2;
        thismse = vfp->svf(y, y_stride,  SP(this_mv.as_mv.col), SP(this_mv.as_mv.row), z, b->src_stride, &sse);
        break;
    }

#if CONFIG_HIGH_PRECISION_MV
    diag = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit, xd->allow_high_precision_mv);
#else
    diag = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit);
#endif

    if (diag < bestmse)
    {
        *bestmv = this_mv;
        bestmse = diag;
        *distortion = thismse;
        *sse1 = sse;
    }

#if CONFIG_HIGH_PRECISION_MV
    if (!x->e_mbd.allow_high_precision_mv)
        return bestmse;

    /* Now do 1/8th pixel */
    if (bestmv->as_mv.row < orig_mv.as_mv.row && !yrow_movedback)
    {
        y -= y_stride;
        yrow_movedback = 1;
    }

    if (bestmv->as_mv.col < orig_mv.as_mv.col && !ycol_movedback)
    {
        y--;
        ycol_movedback = 1;
    }

    startmv = *bestmv;

    // go left then right and check error
    this_mv.as_mv.row = startmv.as_mv.row;

    if (startmv.as_mv.col & 7)
    {
        this_mv.as_mv.col = startmv.as_mv.col - 1;
        thismse = vfp->svf(y, y_stride, SP(this_mv.as_mv.col), SP(this_mv.as_mv.row), z, b->src_stride, &sse);
    }
    else
    {
        this_mv.as_mv.col = (startmv.as_mv.col - 8) | 7;
        thismse = vfp->svf(y - 1, y_stride, SP(7), SP(this_mv.as_mv.row), z, b->src_stride, &sse);
    }

    left = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit, xd->allow_high_precision_mv);

    if (left < bestmse)
    {
        *bestmv = this_mv;
        bestmse = left;
        *distortion = thismse;
        *sse1 = sse;
    }

    this_mv.as_mv.col += 2;
    thismse = vfp->svf(y, y_stride, SP(this_mv.as_mv.col), SP(this_mv.as_mv.row), z, b->src_stride, &sse);
    right = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit, xd->allow_high_precision_mv);

    if (right < bestmse)
    {
        *bestmv = this_mv;
        bestmse = right;
        *distortion = thismse;
        *sse1 = sse;
    }

    // go up then down and check error
    this_mv.as_mv.col = startmv.as_mv.col;

    if (startmv.as_mv.row & 7)
    {
        this_mv.as_mv.row = startmv.as_mv.row - 1;
        thismse = vfp->svf(y, y_stride, SP(this_mv.as_mv.col), SP(this_mv.as_mv.row), z, b->src_stride, &sse);
    }
    else
    {
        this_mv.as_mv.row = (startmv.as_mv.row - 8) | 7;
        thismse = vfp->svf(y - y_stride, y_stride, SP(this_mv.as_mv.col), SP(7), z, b->src_stride, &sse);
    }

    up = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit, xd->allow_high_precision_mv);

    if (up < bestmse)
    {
        *bestmv = this_mv;
        bestmse = up;
        *distortion = thismse;
        *sse1 = sse;
    }

    this_mv.as_mv.row += 2;
    thismse = vfp->svf(y, y_stride, SP(this_mv.as_mv.col), SP(this_mv.as_mv.row), z, b->src_stride, &sse);
    down = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit, xd->allow_high_precision_mv);

    if (down < bestmse)
    {
        *bestmv = this_mv;
        bestmse = down;
        *distortion = thismse;
        *sse1 = sse;
    }


    // now check 1 more diagonal
    whichdir = (left < right ? 0 : 1) + (up < down ? 0 : 2);

//  for(whichdir=0;whichdir<4;whichdir++)
//  {
    this_mv = startmv;

    switch (whichdir)
    {
    case 0:

        if (startmv.as_mv.row & 7)
        {
            this_mv.as_mv.row -= 1;

            if (startmv.as_mv.col & 7)
            {
                this_mv.as_mv.col -= 1;
                thismse = vfp->svf(y, y_stride, SP(this_mv.as_mv.col), SP(this_mv.as_mv.row), z, b->src_stride, &sse);
            }
            else
            {
                this_mv.as_mv.col = (startmv.as_mv.col - 8) | 7;
                thismse = vfp->svf(y - 1, y_stride, SP(7), SP(this_mv.as_mv.row), z, b->src_stride, &sse);;
            }
        }
        else
        {
            this_mv.as_mv.row = (startmv.as_mv.row - 8) | 7;

            if (startmv.as_mv.col & 7)
            {
                this_mv.as_mv.col -= 1;
                thismse = vfp->svf(y - y_stride, y_stride, SP(this_mv.as_mv.col), SP(7), z, b->src_stride, &sse);
            }
            else
            {
                this_mv.as_mv.col = (startmv.as_mv.col - 8) | 7;
                thismse = vfp->svf(y - y_stride - 1, y_stride, SP(7), SP(7), z, b->src_stride, &sse);
            }
        }

        break;
    case 1:
        this_mv.as_mv.col += 1;

        if (startmv.as_mv.row & 7)
        {
            this_mv.as_mv.row -= 1;
            thismse = vfp->svf(y, y_stride, SP(this_mv.as_mv.col), SP(this_mv.as_mv.row), z, b->src_stride, &sse);
        }
        else
        {
            this_mv.as_mv.row = (startmv.as_mv.row - 8) | 7;
            thismse = vfp->svf(y - y_stride, y_stride, SP(this_mv.as_mv.col), SP(7), z, b->src_stride, &sse);
        }

        break;
    case 2:
        this_mv.as_mv.row += 1;

        if (startmv.as_mv.col & 7)
        {
            this_mv.as_mv.col -= 1;
            thismse = vfp->svf(y, y_stride, SP(this_mv.as_mv.col), SP(this_mv.as_mv.row), z, b->src_stride, &sse);
        }
        else
        {
            this_mv.as_mv.col = (startmv.as_mv.col - 8) | 7;
            thismse = vfp->svf(y - 1, y_stride, SP(7), SP(this_mv.as_mv.row), z, b->src_stride, &sse);
        }

        break;
    case 3:
        this_mv.as_mv.col += 1;
        this_mv.as_mv.row += 1;
        thismse = vfp->svf(y, y_stride,  SP(this_mv.as_mv.col), SP(this_mv.as_mv.row), z, b->src_stride, &sse);
        break;
    }

    diag = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit, xd->allow_high_precision_mv);

    if (diag < bestmse)
    {
        *bestmv = this_mv;
        bestmse = diag;
        *distortion = thismse;
        *sse1 = sse;
    }

#endif  /* CONFIG_HIGH_PRECISION_MV */

    return bestmse;
}

#undef SP

int vp8_find_best_half_pixel_step(MACROBLOCK *x, BLOCK *b, BLOCKD *d,
                                  int_mv *bestmv, int_mv *ref_mv,
                                  int error_per_bit,
                                  const vp8_variance_fn_ptr_t *vfp,
                                  int *mvcost[2], int *distortion,
                                  unsigned int *sse1)
{
    int bestmse = INT_MAX;
    int_mv startmv;
    int_mv this_mv;
    unsigned char *z = (*(b->base_src) + b->src);
    int left, right, up, down, diag;
    unsigned int sse;
    int whichdir ;
    int thismse;
    int y_stride;
    MACROBLOCKD *xd = &x->e_mbd;

#if ARCH_X86 || ARCH_X86_64
    unsigned char *y0 = *(d->base_pre) + d->pre + (bestmv->as_mv.row) * d->pre_stride + bestmv->as_mv.col;
    unsigned char *y;

    y_stride = 32;
    /* Copy 18 rows x 32 cols area to intermediate buffer before searching. */
    vfp->copymem(y0 - 1 - d->pre_stride, d->pre_stride, xd->y_buf, y_stride, 18);
    y = xd->y_buf + y_stride + 1;
#else
    unsigned char *y = *(d->base_pre) + d->pre + (bestmv->as_mv.row) * d->pre_stride + bestmv->as_mv.col;
    y_stride = d->pre_stride;
#endif

    // central mv
    bestmv->as_mv.row <<= 3;
    bestmv->as_mv.col <<= 3;
    startmv = *bestmv;

    // calculate central point error
    bestmse = vfp->vf(y, y_stride, z, b->src_stride, sse1);
    *distortion = bestmse;
#if CONFIG_HIGH_PRECISION_MV
    bestmse += mv_err_cost(bestmv, ref_mv, mvcost, error_per_bit, xd->allow_high_precision_mv);
#else
    bestmse += mv_err_cost(bestmv, ref_mv, mvcost, error_per_bit);
#endif

    // go left then right and check error
    this_mv.as_mv.row = startmv.as_mv.row;
    this_mv.as_mv.col = ((startmv.as_mv.col - 8) | 4);
    thismse = vfp->svf_halfpix_h(y - 1, y_stride, z, b->src_stride, &sse);
#if CONFIG_HIGH_PRECISION_MV
    left = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit, xd->allow_high_precision_mv);
#else
    left = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit);
#endif

    if (left < bestmse)
    {
        *bestmv = this_mv;
        bestmse = left;
        *distortion = thismse;
        *sse1 = sse;
    }

    this_mv.as_mv.col += 8;
    thismse = vfp->svf_halfpix_h(y, y_stride, z, b->src_stride, &sse);
#if CONFIG_HIGH_PRECISION_MV
    right = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit, xd->allow_high_precision_mv);
#else
    right = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit);
#endif

    if (right < bestmse)
    {
        *bestmv = this_mv;
        bestmse = right;
        *distortion = thismse;
        *sse1 = sse;
    }

    // go up then down and check error
    this_mv.as_mv.col = startmv.as_mv.col;
    this_mv.as_mv.row = ((startmv.as_mv.row - 8) | 4);
    thismse = vfp->svf_halfpix_v(y - y_stride, y_stride, z, b->src_stride, &sse);
#if CONFIG_HIGH_PRECISION_MV
    up = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit, xd->allow_high_precision_mv);
#else
    up = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit);
#endif

    if (up < bestmse)
    {
        *bestmv = this_mv;
        bestmse = up;
        *distortion = thismse;
        *sse1 = sse;
    }

    this_mv.as_mv.row += 8;
    thismse = vfp->svf_halfpix_v(y, y_stride, z, b->src_stride, &sse);
#if CONFIG_HIGH_PRECISION_MV
    down = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit, xd->allow_high_precision_mv);
#else
    down = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit);
#endif

    if (down < bestmse)
    {
        *bestmv = this_mv;
        bestmse = down;
        *distortion = thismse;
        *sse1 = sse;
    }

    // now check 1 more diagonal -
    whichdir = (left < right ? 0 : 1) + (up < down ? 0 : 2);
    this_mv = startmv;

    switch (whichdir)
    {
    case 0:
        this_mv.as_mv.col = (this_mv.as_mv.col - 8) | 4;
        this_mv.as_mv.row = (this_mv.as_mv.row - 8) | 4;
        thismse = vfp->svf_halfpix_hv(y - 1 - y_stride, y_stride, z, b->src_stride, &sse);
        break;
    case 1:
        this_mv.as_mv.col += 4;
        this_mv.as_mv.row = (this_mv.as_mv.row - 8) | 4;
        thismse = vfp->svf_halfpix_hv(y - y_stride, y_stride, z, b->src_stride, &sse);
        break;
    case 2:
        this_mv.as_mv.col = (this_mv.as_mv.col - 8) | 4;
        this_mv.as_mv.row += 4;
        thismse = vfp->svf_halfpix_hv(y - 1, y_stride, z, b->src_stride, &sse);
        break;
    case 3:
    default:
        this_mv.as_mv.col += 4;
        this_mv.as_mv.row += 4;
        thismse = vfp->svf_halfpix_hv(y, y_stride, z, b->src_stride, &sse);
        break;
    }

#if CONFIG_HIGH_PRECISION_MV
    diag = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit, xd->allow_high_precision_mv);
#else
    diag = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit);
#endif

    if (diag < bestmse)
    {
        *bestmv = this_mv;
        bestmse = diag;
        *distortion = thismse;
        *sse1 = sse;
    }

    return bestmse;
}

#define CHECK_BOUNDS(range) \
{\
    all_in = 1;\
    all_in &= ((br-range) >= x->mv_row_min);\
    all_in &= ((br+range) <= x->mv_row_max);\
    all_in &= ((bc-range) >= x->mv_col_min);\
    all_in &= ((bc+range) <= x->mv_col_max);\
}

#define CHECK_POINT \
{\
    if (this_mv.as_mv.col < x->mv_col_min) continue;\
    if (this_mv.as_mv.col > x->mv_col_max) continue;\
    if (this_mv.as_mv.row < x->mv_row_min) continue;\
    if (this_mv.as_mv.row > x->mv_row_max) continue;\
}

#define CHECK_BETTER \
{\
    if (thissad < bestsad)\
    {\
        thissad += mvsad_err_cost(&this_mv, &fcenter_mv, mvsadcost, sad_per_bit);\
        if (thissad < bestsad)\
        {\
            bestsad = thissad;\
            best_site = i;\
        }\
    }\
}

static const MV next_chkpts[6][3] =
{
    {{ -2, 0}, { -1, -2}, {1, -2}},
    {{ -1, -2}, {1, -2}, {2, 0}},
    {{1, -2}, {2, 0}, {1, 2}},
    {{2, 0}, {1, 2}, { -1, 2}},
    {{1, 2}, { -1, 2}, { -2, 0}},
    {{ -1, 2}, { -2, 0}, { -1, -2}}
};

int vp8_hex_search
(
    MACROBLOCK *x,
    BLOCK *b,
    BLOCKD *d,
    int_mv *ref_mv,
    int_mv *best_mv,
    int search_param,
    int sad_per_bit,
    const vp8_variance_fn_ptr_t *vfp,
    int *mvsadcost[2],
    int *mvcost[2],
    int_mv *center_mv
)
{
    MV hex[6] = { { -1, -2}, {1, -2}, {2, 0}, {1, 2}, { -1, 2}, { -2, 0} } ;
    MV neighbors[4] = {{0, -1}, { -1, 0}, {1, 0}, {0, 1}} ;
    int i, j;

    unsigned char *what = (*(b->base_src) + b->src);
    int what_stride = b->src_stride;
    int in_what_stride = d->pre_stride;
    int br, bc;
    int_mv this_mv;
    unsigned int bestsad = 0x7fffffff;
    unsigned int thissad;
    unsigned char *base_offset;
    unsigned char *this_offset;
    int k = -1;
    int all_in;
    int best_site = -1;

    int_mv fcenter_mv;
    fcenter_mv.as_mv.row = center_mv->as_mv.row >> 3;
    fcenter_mv.as_mv.col = center_mv->as_mv.col >> 3;

    // adjust ref_mv to make sure it is within MV range
    vp8_clamp_mv(ref_mv, x->mv_col_min, x->mv_col_max, x->mv_row_min, x->mv_row_max);
    br = ref_mv->as_mv.row;
    bc = ref_mv->as_mv.col;

    // Work out the start point for the search
    base_offset = (unsigned char *)(*(d->base_pre) + d->pre);
    this_offset = base_offset + (br * (d->pre_stride)) + bc;
    this_mv.as_mv.row = br;
    this_mv.as_mv.col = bc;
    bestsad = vfp->sdf( what, what_stride, this_offset,
                        in_what_stride, 0x7fffffff)
            + mvsad_err_cost(&this_mv, &fcenter_mv, mvsadcost, sad_per_bit);

    // hex search
    //j=0
    CHECK_BOUNDS(2)

    if(all_in)
    {
        for (i = 0; i < 6; i++)
        {
            this_mv.as_mv.row = br + hex[i].row;
            this_mv.as_mv.col = bc + hex[i].col;
            this_offset = base_offset + (this_mv.as_mv.row * in_what_stride) + this_mv.as_mv.col;
            thissad=vfp->sdf( what, what_stride, this_offset, in_what_stride, bestsad);
            CHECK_BETTER
        }
    }else
    {
        for (i = 0; i < 6; i++)
        {
            this_mv.as_mv.row = br + hex[i].row;
            this_mv.as_mv.col = bc + hex[i].col;
            CHECK_POINT
            this_offset = base_offset + (this_mv.as_mv.row * in_what_stride) + this_mv.as_mv.col;
            thissad=vfp->sdf( what, what_stride, this_offset, in_what_stride, bestsad);
            CHECK_BETTER
        }
    }

    if (best_site == -1)
        goto cal_neighbors;
    else
    {
        br += hex[best_site].row;
        bc += hex[best_site].col;
        k = best_site;
    }

    for (j = 1; j < 127; j++)
    {
        best_site = -1;
        CHECK_BOUNDS(2)

        if(all_in)
        {
            for (i = 0; i < 3; i++)
            {
                this_mv.as_mv.row = br + next_chkpts[k][i].row;
                this_mv.as_mv.col = bc + next_chkpts[k][i].col;
                this_offset = base_offset + (this_mv.as_mv.row * (in_what_stride)) + this_mv.as_mv.col;
                thissad = vfp->sdf( what, what_stride, this_offset, in_what_stride, bestsad);
                CHECK_BETTER
            }
        }else
        {
            for (i = 0; i < 3; i++)
            {
                this_mv.as_mv.row = br + next_chkpts[k][i].row;
                this_mv.as_mv.col = bc + next_chkpts[k][i].col;
                CHECK_POINT
                this_offset = base_offset + (this_mv.as_mv.row * (in_what_stride)) + this_mv.as_mv.col;
                thissad = vfp->sdf( what, what_stride, this_offset, in_what_stride, bestsad);
                CHECK_BETTER
            }
        }

        if (best_site == -1)
            break;
        else
        {
            br += next_chkpts[k][best_site].row;
            bc += next_chkpts[k][best_site].col;
            k += 5 + best_site;
            if (k >= 12) k -= 12;
            else if (k >= 6) k -= 6;
        }
    }

    // check 4 1-away neighbors
cal_neighbors:
    for (j = 0; j < 32; j++)
    {
        best_site = -1;
        CHECK_BOUNDS(1)

        if(all_in)
        {
            for (i = 0; i < 4; i++)
            {
                this_mv.as_mv.row = br + neighbors[i].row;
                this_mv.as_mv.col = bc + neighbors[i].col;
                this_offset = base_offset + (this_mv.as_mv.row * (in_what_stride)) + this_mv.as_mv.col;
                thissad = vfp->sdf( what, what_stride, this_offset, in_what_stride, bestsad);
                CHECK_BETTER
            }
        }else
        {
            for (i = 0; i < 4; i++)
            {
                this_mv.as_mv.row = br + neighbors[i].row;
                this_mv.as_mv.col = bc + neighbors[i].col;
                CHECK_POINT
                this_offset = base_offset + (this_mv.as_mv.row * (in_what_stride)) + this_mv.as_mv.col;
                thissad = vfp->sdf( what, what_stride, this_offset, in_what_stride, bestsad);
                CHECK_BETTER
            }
        }

        if (best_site == -1)
            break;
        else
        {
            br += neighbors[best_site].row;
            bc += neighbors[best_site].col;
        }
    }

    best_mv->as_mv.row = br;
    best_mv->as_mv.col = bc;

    return bestsad;
}
#undef CHECK_BOUNDS
#undef CHECK_POINT
#undef CHECK_BETTER

int vp8_diamond_search_sad
(
    MACROBLOCK *x,
    BLOCK *b,
    BLOCKD *d,
    int_mv *ref_mv,
    int_mv *best_mv,
    int search_param,
    int sad_per_bit,
    int *num00,
    vp8_variance_fn_ptr_t *fn_ptr,
    int *mvcost[2],
    int_mv *center_mv
)
{
    int i, j, step;

    unsigned char *what = (*(b->base_src) + b->src);
    int what_stride = b->src_stride;
    unsigned char *in_what;
    int in_what_stride = d->pre_stride;
    unsigned char *best_address;

    int tot_steps;
    int_mv this_mv;

    int bestsad = INT_MAX;
    int best_site = 0;
    int last_site = 0;

    int ref_row;
    int ref_col;
    int this_row_offset;
    int this_col_offset;
    search_site *ss;

    unsigned char *check_here;
    int thissad;
    MACROBLOCKD *xd = &x->e_mbd;

    int *mvsadcost[2] = {x->mvsadcost[0], x->mvsadcost[1]};
    int_mv fcenter_mv;

#if CONFIG_HIGH_PRECISION_MV
    if (xd->allow_high_precision_mv)
    {
        mvsadcost[0] = x->mvsadcost_hp[0];
        mvsadcost[1] = x->mvsadcost_hp[1];
    }
#endif
    fcenter_mv.as_mv.row = center_mv->as_mv.row >> 3;
    fcenter_mv.as_mv.col = center_mv->as_mv.col >> 3;

    vp8_clamp_mv(ref_mv, x->mv_col_min, x->mv_col_max, x->mv_row_min, x->mv_row_max);
    ref_row = ref_mv->as_mv.row;
    ref_col = ref_mv->as_mv.col;
    *num00 = 0;
    best_mv->as_mv.row = ref_row;
    best_mv->as_mv.col = ref_col;

    // Work out the start point for the search
    in_what = (unsigned char *)(*(d->base_pre) + d->pre + (ref_row * (d->pre_stride)) + ref_col);
    best_address = in_what;

    // Check the starting position
    bestsad = fn_ptr->sdf(what, what_stride, in_what,
                          in_what_stride, 0x7fffffff)
              + mvsad_err_cost(best_mv, &fcenter_mv, mvsadcost, sad_per_bit);

    // search_param determines the length of the initial step and hence the number of iterations
    // 0 = initial step (MAX_FIRST_STEP) pel : 1 = (MAX_FIRST_STEP/2) pel, 2 = (MAX_FIRST_STEP/4) pel... etc.
    ss = &x->ss[search_param * x->searches_per_step];
    tot_steps = (x->ss_count / x->searches_per_step) - search_param;

    i = 1;

    for (step = 0; step < tot_steps ; step++)
    {
        for (j = 0 ; j < x->searches_per_step ; j++)
        {
            // Trap illegal vectors
            this_row_offset = best_mv->as_mv.row + ss[i].mv.row;
            this_col_offset = best_mv->as_mv.col + ss[i].mv.col;

            if ((this_col_offset > x->mv_col_min) && (this_col_offset < x->mv_col_max) &&
            (this_row_offset > x->mv_row_min) && (this_row_offset < x->mv_row_max))

            {
                check_here = ss[i].offset + best_address;
                thissad = fn_ptr->sdf(what, what_stride, check_here , in_what_stride, bestsad);

                if (thissad < bestsad)
                {
                    this_mv.as_mv.row = this_row_offset;
                    this_mv.as_mv.col = this_col_offset;
                    thissad += mvsad_err_cost(&this_mv, &fcenter_mv,
                                                mvsadcost, sad_per_bit);

                    if (thissad < bestsad)
                    {
                        bestsad = thissad;
                        best_site = i;
                    }
                }
            }

            i++;
        }

        if (best_site != last_site)
        {
            best_mv->as_mv.row += ss[best_site].mv.row;
            best_mv->as_mv.col += ss[best_site].mv.col;
            best_address += ss[best_site].offset;
            last_site = best_site;
        }
        else if (best_address == in_what)
            (*num00)++;
    }

    this_mv.as_mv.row = best_mv->as_mv.row << 3;
    this_mv.as_mv.col = best_mv->as_mv.col << 3;

    if (bestsad == INT_MAX)
        return INT_MAX;

    return fn_ptr->vf(what, what_stride, best_address, in_what_stride, (unsigned int *)(&thissad))
#if CONFIG_HIGH_PRECISION_MV
        + mv_err_cost(&this_mv, center_mv, mvcost, x->errorperbit, xd->allow_high_precision_mv);
#else
        + mv_err_cost(&this_mv, center_mv, mvcost, x->errorperbit);
#endif
}

int vp8_diamond_search_sadx4
(
    MACROBLOCK *x,
    BLOCK *b,
    BLOCKD *d,
    int_mv *ref_mv,
    int_mv *best_mv,
    int search_param,
    int sad_per_bit,
    int *num00,
    vp8_variance_fn_ptr_t *fn_ptr,
    int *mvcost[2],
    int_mv *center_mv
)
{
    int i, j, step;

    unsigned char *what = (*(b->base_src) + b->src);
    int what_stride = b->src_stride;
    unsigned char *in_what;
    int in_what_stride = d->pre_stride;
    unsigned char *best_address;

    int tot_steps;
    int_mv this_mv;

    int bestsad = INT_MAX;
    int best_site = 0;
    int last_site = 0;

    int ref_row;
    int ref_col;
    int this_row_offset;
    int this_col_offset;
    search_site *ss;

    unsigned char *check_here;
    unsigned int thissad;
    MACROBLOCKD *xd = &x->e_mbd;

    int *mvsadcost[2] = {x->mvsadcost[0], x->mvsadcost[1]};
    int_mv fcenter_mv;

#if CONFIG_HIGH_PRECISION_MV
    if (xd->allow_high_precision_mv)
    {
        mvsadcost[0] = x->mvsadcost_hp[0];
        mvsadcost[1] = x->mvsadcost_hp[1];
    }
#endif
    fcenter_mv.as_mv.row = center_mv->as_mv.row >> 3;
    fcenter_mv.as_mv.col = center_mv->as_mv.col >> 3;

    vp8_clamp_mv(ref_mv, x->mv_col_min, x->mv_col_max, x->mv_row_min, x->mv_row_max);
    ref_row = ref_mv->as_mv.row;
    ref_col = ref_mv->as_mv.col;
    *num00 = 0;
    best_mv->as_mv.row = ref_row;
    best_mv->as_mv.col = ref_col;

    // Work out the start point for the search
    in_what = (unsigned char *)(*(d->base_pre) + d->pre + (ref_row * (d->pre_stride)) + ref_col);
    best_address = in_what;

    // Check the starting position
    bestsad = fn_ptr->sdf(what, what_stride,
                          in_what, in_what_stride, 0x7fffffff)
              + mvsad_err_cost(best_mv, &fcenter_mv, mvsadcost, sad_per_bit);

    // search_param determines the length of the initial step and hence the number of iterations
    // 0 = initial step (MAX_FIRST_STEP) pel : 1 = (MAX_FIRST_STEP/2) pel, 2 = (MAX_FIRST_STEP/4) pel... etc.
    ss = &x->ss[search_param * x->searches_per_step];
    tot_steps = (x->ss_count / x->searches_per_step) - search_param;

    i = 1;

    for (step = 0; step < tot_steps ; step++)
    {
        int all_in = 1, t;

        // To know if all neighbor points are within the bounds, 4 bounds checking are enough instead of
        // checking 4 bounds for each points.
        all_in &= ((best_mv->as_mv.row + ss[i].mv.row)> x->mv_row_min);
        all_in &= ((best_mv->as_mv.row + ss[i+1].mv.row) < x->mv_row_max);
        all_in &= ((best_mv->as_mv.col + ss[i+2].mv.col) > x->mv_col_min);
        all_in &= ((best_mv->as_mv.col + ss[i+3].mv.col) < x->mv_col_max);

        if (all_in)
        {
            unsigned int sad_array[4];

            for (j = 0 ; j < x->searches_per_step ; j += 4)
            {
                unsigned char *block_offset[4];

                for (t = 0; t < 4; t++)
                    block_offset[t] = ss[i+t].offset + best_address;

                fn_ptr->sdx4df(what, what_stride, block_offset, in_what_stride, sad_array);

                for (t = 0; t < 4; t++, i++)
                {
                    if (sad_array[t] < bestsad)
                    {
                        this_mv.as_mv.row = best_mv->as_mv.row + ss[i].mv.row;
                        this_mv.as_mv.col = best_mv->as_mv.col + ss[i].mv.col;
                        sad_array[t] += mvsad_err_cost(&this_mv, &fcenter_mv,
                                                       mvsadcost, sad_per_bit);

                        if (sad_array[t] < bestsad)
                        {
                            bestsad = sad_array[t];
                            best_site = i;
                        }
                    }
                }
            }
        }
        else
        {
            for (j = 0 ; j < x->searches_per_step ; j++)
            {
                // Trap illegal vectors
                this_row_offset = best_mv->as_mv.row + ss[i].mv.row;
                this_col_offset = best_mv->as_mv.col + ss[i].mv.col;

                if ((this_col_offset > x->mv_col_min) && (this_col_offset < x->mv_col_max) &&
                (this_row_offset > x->mv_row_min) && (this_row_offset < x->mv_row_max))
                {
                    check_here = ss[i].offset + best_address;
                    thissad = fn_ptr->sdf(what, what_stride, check_here , in_what_stride, bestsad);

                    if (thissad < bestsad)
                    {
                        this_mv.as_mv.row = this_row_offset;
                        this_mv.as_mv.col = this_col_offset;
                        thissad += mvsad_err_cost(&this_mv, &fcenter_mv,
                                                   mvsadcost, sad_per_bit);

                        if (thissad < bestsad)
                        {
                            bestsad = thissad;
                            best_site = i;
                        }
                    }
                }
                i++;
            }
        }

        if (best_site != last_site)
        {
            best_mv->as_mv.row += ss[best_site].mv.row;
            best_mv->as_mv.col += ss[best_site].mv.col;
            best_address += ss[best_site].offset;
            last_site = best_site;
        }
        else if (best_address == in_what)
            (*num00)++;
    }

    this_mv.as_mv.row = best_mv->as_mv.row << 3;
    this_mv.as_mv.col = best_mv->as_mv.col << 3;

    if (bestsad == INT_MAX)
        return INT_MAX;

    return fn_ptr->vf(what, what_stride, best_address, in_what_stride, (unsigned int *)(&thissad))
#if CONFIG_HIGH_PRECISION_MV
        + mv_err_cost(&this_mv, center_mv, mvcost, x->errorperbit, xd->allow_high_precision_mv);
#else
        + mv_err_cost(&this_mv, center_mv, mvcost, x->errorperbit);
#endif
}

int vp8_full_search_sad(MACROBLOCK *x, BLOCK *b, BLOCKD *d, int_mv *ref_mv,
                        int sad_per_bit, int distance,
                        vp8_variance_fn_ptr_t *fn_ptr, int *mvcost[2],
                        int_mv *center_mv)
{
    unsigned char *what = (*(b->base_src) + b->src);
    int what_stride = b->src_stride;
    unsigned char *in_what;
    int in_what_stride = d->pre_stride;
    int mv_stride = d->pre_stride;
    unsigned char *bestaddress;
    int_mv *best_mv = &d->bmi.mv;
    int_mv this_mv;
    int bestsad = INT_MAX;
    int r, c;

    unsigned char *check_here;
    int thissad;
    MACROBLOCKD *xd = &x->e_mbd;

    int ref_row = ref_mv->as_mv.row;
    int ref_col = ref_mv->as_mv.col;

    int row_min = ref_row - distance;
    int row_max = ref_row + distance;
    int col_min = ref_col - distance;
    int col_max = ref_col + distance;

    int *mvsadcost[2] = {x->mvsadcost[0], x->mvsadcost[1]};
    int_mv fcenter_mv;

#if CONFIG_HIGH_PRECISION_MV
    if (xd->allow_high_precision_mv)
    {
        mvsadcost[0] = x->mvsadcost_hp[0];
        mvsadcost[1] = x->mvsadcost_hp[1];
    }
#endif
    fcenter_mv.as_mv.row = center_mv->as_mv.row >> 3;
    fcenter_mv.as_mv.col = center_mv->as_mv.col >> 3;

    // Work out the mid point for the search
    in_what = *(d->base_pre) + d->pre;
    bestaddress = in_what + (ref_row * d->pre_stride) + ref_col;

    best_mv->as_mv.row = ref_row;
    best_mv->as_mv.col = ref_col;

    // Baseline value at the centre
    bestsad = fn_ptr->sdf(what, what_stride, bestaddress,
                          in_what_stride, 0x7fffffff)
              + mvsad_err_cost(best_mv, &fcenter_mv, mvsadcost, sad_per_bit);

    // Apply further limits to prevent us looking using vectors that stretch beyiond the UMV border
    if (col_min < x->mv_col_min)
        col_min = x->mv_col_min;

    if (col_max > x->mv_col_max)
        col_max = x->mv_col_max;

    if (row_min < x->mv_row_min)
        row_min = x->mv_row_min;

    if (row_max > x->mv_row_max)
        row_max = x->mv_row_max;

    for (r = row_min; r < row_max ; r++)
    {
        this_mv.as_mv.row = r;
        check_here = r * mv_stride + in_what + col_min;

        for (c = col_min; c < col_max; c++)
        {
            thissad = fn_ptr->sdf(what, what_stride, check_here , in_what_stride, bestsad);

            this_mv.as_mv.col = c;
            thissad  += mvsad_err_cost(&this_mv, &fcenter_mv,
                        mvsadcost, sad_per_bit);

            if (thissad < bestsad)
            {
                bestsad = thissad;
                best_mv->as_mv.row = r;
                best_mv->as_mv.col = c;
                bestaddress = check_here;
            }

            check_here++;
        }
    }

    this_mv.as_mv.row = best_mv->as_mv.row << 3;
    this_mv.as_mv.col = best_mv->as_mv.col << 3;

    if (bestsad < INT_MAX)
        return fn_ptr->vf(what, what_stride, bestaddress, in_what_stride, (unsigned int *)(&thissad))
#if CONFIG_HIGH_PRECISION_MV
        + mv_err_cost(&this_mv, center_mv, mvcost, x->errorperbit, xd->allow_high_precision_mv);
#else
        + mv_err_cost(&this_mv, center_mv, mvcost, x->errorperbit);
#endif
    else
        return INT_MAX;
}

int vp8_full_search_sadx3(MACROBLOCK *x, BLOCK *b, BLOCKD *d, int_mv *ref_mv,
                          int sad_per_bit, int distance,
                          vp8_variance_fn_ptr_t *fn_ptr, int *mvcost[2],
                          int_mv *center_mv)
{
    unsigned char *what = (*(b->base_src) + b->src);
    int what_stride = b->src_stride;
    unsigned char *in_what;
    int in_what_stride = d->pre_stride;
    int mv_stride = d->pre_stride;
    unsigned char *bestaddress;
    int_mv *best_mv = &d->bmi.mv;
    int_mv this_mv;
    int bestsad = INT_MAX;
    int r, c;

    unsigned char *check_here;
    unsigned int thissad;
    MACROBLOCKD *xd = &x->e_mbd;

    int ref_row = ref_mv->as_mv.row;
    int ref_col = ref_mv->as_mv.col;

    int row_min = ref_row - distance;
    int row_max = ref_row + distance;
    int col_min = ref_col - distance;
    int col_max = ref_col + distance;

    unsigned int sad_array[3];

    int *mvsadcost[2] = {x->mvsadcost[0], x->mvsadcost[1]};
    int_mv fcenter_mv;

#if CONFIG_HIGH_PRECISION_MV
    if (xd->allow_high_precision_mv)
    {
        mvsadcost[0] = x->mvsadcost_hp[0];
        mvsadcost[1] = x->mvsadcost_hp[1];
    }
#endif
    fcenter_mv.as_mv.row = center_mv->as_mv.row >> 3;
    fcenter_mv.as_mv.col = center_mv->as_mv.col >> 3;

    // Work out the mid point for the search
    in_what = *(d->base_pre) + d->pre;
    bestaddress = in_what + (ref_row * d->pre_stride) + ref_col;

    best_mv->as_mv.row = ref_row;
    best_mv->as_mv.col = ref_col;

    // Baseline value at the centre
    bestsad = fn_ptr->sdf(what, what_stride,
                          bestaddress, in_what_stride, 0x7fffffff)
              + mvsad_err_cost(best_mv, &fcenter_mv, mvsadcost, sad_per_bit);

    // Apply further limits to prevent us looking using vectors that stretch beyiond the UMV border
    if (col_min < x->mv_col_min)
        col_min = x->mv_col_min;

    if (col_max > x->mv_col_max)
        col_max = x->mv_col_max;

    if (row_min < x->mv_row_min)
        row_min = x->mv_row_min;

    if (row_max > x->mv_row_max)
        row_max = x->mv_row_max;

    for (r = row_min; r < row_max ; r++)
    {
        this_mv.as_mv.row = r;
        check_here = r * mv_stride + in_what + col_min;
        c = col_min;

        while ((c + 2) < col_max)
        {
            int i;

            fn_ptr->sdx3f(what, what_stride, check_here , in_what_stride, sad_array);

            for (i = 0; i < 3; i++)
            {
                thissad = sad_array[i];

                if (thissad < bestsad)
                {
                    this_mv.as_mv.col = c;
                    thissad  += mvsad_err_cost(&this_mv, &fcenter_mv,
                                                mvsadcost, sad_per_bit);

                    if (thissad < bestsad)
                    {
                        bestsad = thissad;
                        best_mv->as_mv.row = r;
                        best_mv->as_mv.col = c;
                        bestaddress = check_here;
                    }
                }

                check_here++;
                c++;
            }
        }

        while (c < col_max)
        {
            thissad = fn_ptr->sdf(what, what_stride, check_here , in_what_stride, bestsad);

            if (thissad < bestsad)
            {
                this_mv.as_mv.col = c;
                thissad  += mvsad_err_cost(&this_mv, &fcenter_mv,
                                            mvsadcost, sad_per_bit);

                if (thissad < bestsad)
                {
                    bestsad = thissad;
                    best_mv->as_mv.row = r;
                    best_mv->as_mv.col = c;
                    bestaddress = check_here;
                }
            }

            check_here ++;
            c ++;
        }

    }

    this_mv.as_mv.row = best_mv->as_mv.row << 3;
    this_mv.as_mv.col = best_mv->as_mv.col << 3;

    if (bestsad < INT_MAX)
        return fn_ptr->vf(what, what_stride, bestaddress, in_what_stride, (unsigned int *)(&thissad))
#if CONFIG_HIGH_PRECISION_MV
        + mv_err_cost(&this_mv, center_mv, mvcost, x->errorperbit, xd->allow_high_precision_mv);
#else
        + mv_err_cost(&this_mv, center_mv, mvcost, x->errorperbit);
#endif
    else
        return INT_MAX;
}

int vp8_full_search_sadx8(MACROBLOCK *x, BLOCK *b, BLOCKD *d, int_mv *ref_mv,
                          int sad_per_bit, int distance,
                          vp8_variance_fn_ptr_t *fn_ptr, int *mvcost[2],
                          int_mv *center_mv)
{
    unsigned char *what = (*(b->base_src) + b->src);
    int what_stride = b->src_stride;
    unsigned char *in_what;
    int in_what_stride = d->pre_stride;
    int mv_stride = d->pre_stride;
    unsigned char *bestaddress;
    int_mv *best_mv = &d->bmi.mv;
    int_mv this_mv;
    int bestsad = INT_MAX;
    int r, c;

    unsigned char *check_here;
    unsigned int thissad;
    MACROBLOCKD *xd = &x->e_mbd;

    int ref_row = ref_mv->as_mv.row;
    int ref_col = ref_mv->as_mv.col;

    int row_min = ref_row - distance;
    int row_max = ref_row + distance;
    int col_min = ref_col - distance;
    int col_max = ref_col + distance;

    DECLARE_ALIGNED_ARRAY(16, unsigned short, sad_array8, 8);
    unsigned int sad_array[3];

    int *mvsadcost[2] = {x->mvsadcost[0], x->mvsadcost[1]};
    int_mv fcenter_mv;

#if CONFIG_HIGH_PRECISION_MV
    if (xd->allow_high_precision_mv)
    {
        mvsadcost[0] = x->mvsadcost_hp[0];
        mvsadcost[1] = x->mvsadcost_hp[1];
    }
#endif
    fcenter_mv.as_mv.row = center_mv->as_mv.row >> 3;
    fcenter_mv.as_mv.col = center_mv->as_mv.col >> 3;

    // Work out the mid point for the search
    in_what = *(d->base_pre) + d->pre;
    bestaddress = in_what + (ref_row * d->pre_stride) + ref_col;

    best_mv->as_mv.row = ref_row;
    best_mv->as_mv.col = ref_col;

    // Baseline value at the centre
    bestsad = fn_ptr->sdf(what, what_stride,
                          bestaddress, in_what_stride, 0x7fffffff)
              + mvsad_err_cost(best_mv, &fcenter_mv, mvsadcost, sad_per_bit);

    // Apply further limits to prevent us looking using vectors that stretch beyiond the UMV border
    if (col_min < x->mv_col_min)
        col_min = x->mv_col_min;

    if (col_max > x->mv_col_max)
        col_max = x->mv_col_max;

    if (row_min < x->mv_row_min)
        row_min = x->mv_row_min;

    if (row_max > x->mv_row_max)
        row_max = x->mv_row_max;

    for (r = row_min; r < row_max ; r++)
    {
        this_mv.as_mv.row = r;
        check_here = r * mv_stride + in_what + col_min;
        c = col_min;

        while ((c + 7) < col_max)
        {
            int i;

            fn_ptr->sdx8f(what, what_stride, check_here , in_what_stride, sad_array8);

            for (i = 0; i < 8; i++)
            {
                thissad = (unsigned int)sad_array8[i];

                if (thissad < bestsad)
                {
                    this_mv.as_mv.col = c;
                    thissad  += mvsad_err_cost(&this_mv, &fcenter_mv,
                                                mvsadcost, sad_per_bit);

                    if (thissad < bestsad)
                    {
                        bestsad = thissad;
                        best_mv->as_mv.row = r;
                        best_mv->as_mv.col = c;
                        bestaddress = check_here;
                    }
                }

                check_here++;
                c++;
            }
        }

        while ((c + 2) < col_max)
        {
            int i;

            fn_ptr->sdx3f(what, what_stride, check_here , in_what_stride, sad_array);

            for (i = 0; i < 3; i++)
            {
                thissad = sad_array[i];

                if (thissad < bestsad)
                {
                    this_mv.as_mv.col = c;
                    thissad  += mvsad_err_cost(&this_mv, &fcenter_mv,
                        mvsadcost, sad_per_bit);

                    if (thissad < bestsad)
                    {
                        bestsad = thissad;
                        best_mv->as_mv.row = r;
                        best_mv->as_mv.col = c;
                        bestaddress = check_here;
                    }
                }

                check_here++;
                c++;
            }
        }

        while (c < col_max)
        {
            thissad = fn_ptr->sdf(what, what_stride, check_here , in_what_stride, bestsad);

            if (thissad < bestsad)
            {
                this_mv.as_mv.col = c;
                thissad  += mvsad_err_cost(&this_mv, &fcenter_mv,
                    mvsadcost, sad_per_bit);

                if (thissad < bestsad)
                {
                    bestsad = thissad;
                    best_mv->as_mv.row = r;
                    best_mv->as_mv.col = c;
                    bestaddress = check_here;
                }
            }

            check_here ++;
            c ++;
        }
    }

    this_mv.as_mv.row = best_mv->as_mv.row << 3;
    this_mv.as_mv.col = best_mv->as_mv.col << 3;

    if (bestsad < INT_MAX)
        return fn_ptr->vf(what, what_stride, bestaddress, in_what_stride, (unsigned int *)(&thissad))
#if CONFIG_HIGH_PRECISION_MV
        + mv_err_cost(&this_mv, center_mv, mvcost, x->errorperbit, xd->allow_high_precision_mv);
#else
        + mv_err_cost(&this_mv, center_mv, mvcost, x->errorperbit);
#endif
    else
        return INT_MAX;
}

int vp8_refining_search_sad(MACROBLOCK *x, BLOCK *b, BLOCKD *d, int_mv *ref_mv,
                            int error_per_bit, int search_range,
                            vp8_variance_fn_ptr_t *fn_ptr, int *mvcost[2],
                            int_mv *center_mv)
{
    MV neighbors[4] = {{-1, 0}, {0, -1}, {0, 1}, {1, 0}};
    int i, j;
    short this_row_offset, this_col_offset;

    int what_stride = b->src_stride;
    int in_what_stride = d->pre_stride;
    unsigned char *what = (*(b->base_src) + b->src);
    unsigned char *best_address = (unsigned char *)(*(d->base_pre) + d->pre +
        (ref_mv->as_mv.row * (d->pre_stride)) + ref_mv->as_mv.col);
    unsigned char *check_here;
    unsigned int thissad;
    int_mv this_mv;
    unsigned int bestsad = INT_MAX;
    MACROBLOCKD *xd = &x->e_mbd;

    int *mvsadcost[2] = {x->mvsadcost[0], x->mvsadcost[1]};
    int_mv fcenter_mv;

#if CONFIG_HIGH_PRECISION_MV
    if (xd->allow_high_precision_mv)
    {
        mvsadcost[0] = x->mvsadcost_hp[0];
        mvsadcost[1] = x->mvsadcost_hp[1];
    }
#endif

    fcenter_mv.as_mv.row = center_mv->as_mv.row >> 3;
    fcenter_mv.as_mv.col = center_mv->as_mv.col >> 3;

    bestsad = fn_ptr->sdf(what, what_stride, best_address, in_what_stride, 0x7fffffff) + mvsad_err_cost(ref_mv, &fcenter_mv, mvsadcost, error_per_bit);

    for (i=0; i<search_range; i++)
    {
        int best_site = -1;

        for (j = 0 ; j < 4 ; j++)
        {
            this_row_offset = ref_mv->as_mv.row + neighbors[j].row;
            this_col_offset = ref_mv->as_mv.col + neighbors[j].col;

            if ((this_col_offset > x->mv_col_min) && (this_col_offset < x->mv_col_max) &&
            (this_row_offset > x->mv_row_min) && (this_row_offset < x->mv_row_max))
            {
                check_here = (neighbors[j].row)*in_what_stride + neighbors[j].col + best_address;
                thissad = fn_ptr->sdf(what, what_stride, check_here , in_what_stride, bestsad);

                if (thissad < bestsad)
                {
                    this_mv.as_mv.row = this_row_offset;
                    this_mv.as_mv.col = this_col_offset;
                    thissad += mvsad_err_cost(&this_mv, &fcenter_mv, mvsadcost, error_per_bit);

                    if (thissad < bestsad)
                    {
                        bestsad = thissad;
                        best_site = j;
                    }
                }
            }
        }

        if (best_site == -1)
            break;
        else
        {
            ref_mv->as_mv.row += neighbors[best_site].row;
            ref_mv->as_mv.col += neighbors[best_site].col;
            best_address += (neighbors[best_site].row)*in_what_stride + neighbors[best_site].col;
        }
    }

    this_mv.as_mv.row = ref_mv->as_mv.row << 3;
    this_mv.as_mv.col = ref_mv->as_mv.col << 3;

    if (bestsad < INT_MAX)
        return fn_ptr->vf(what, what_stride, best_address, in_what_stride, (unsigned int *)(&thissad))
#if CONFIG_HIGH_PRECISION_MV
        + mv_err_cost(&this_mv, center_mv, mvcost, x->errorperbit, xd->allow_high_precision_mv);
#else
        + mv_err_cost(&this_mv, center_mv, mvcost, x->errorperbit);
#endif
    else
        return INT_MAX;
}

int vp8_refining_search_sadx4(MACROBLOCK *x, BLOCK *b, BLOCKD *d,
                              int_mv *ref_mv, int error_per_bit,
                              int search_range, vp8_variance_fn_ptr_t *fn_ptr,
                              int *mvcost[2], int_mv *center_mv)
{
    MV neighbors[4] = {{-1, 0}, {0, -1}, {0, 1}, {1, 0}};
    int i, j;
    short this_row_offset, this_col_offset;

    int what_stride = b->src_stride;
    int in_what_stride = d->pre_stride;
    unsigned char *what = (*(b->base_src) + b->src);
    unsigned char *best_address = (unsigned char *)(*(d->base_pre) + d->pre +
        (ref_mv->as_mv.row * (d->pre_stride)) + ref_mv->as_mv.col);
    unsigned char *check_here;
    unsigned int thissad;
    int_mv this_mv;
    unsigned int bestsad = INT_MAX;
    MACROBLOCKD *xd = &x->e_mbd;

    int *mvsadcost[2] = {x->mvsadcost[0], x->mvsadcost[1]};
    int_mv fcenter_mv;

#if CONFIG_HIGH_PRECISION_MV
    if (xd->allow_high_precision_mv)
    {
        mvsadcost[0] = x->mvsadcost_hp[0];
        mvsadcost[1] = x->mvsadcost_hp[1];
    }
#endif

    fcenter_mv.as_mv.row = center_mv->as_mv.row >> 3;
    fcenter_mv.as_mv.col = center_mv->as_mv.col >> 3;

    bestsad = fn_ptr->sdf(what, what_stride, best_address, in_what_stride, 0x7fffffff) + mvsad_err_cost(ref_mv, &fcenter_mv, mvsadcost, error_per_bit);

    for (i=0; i<search_range; i++)
    {
        int best_site = -1;
        int all_in = 1;

        all_in &= ((ref_mv->as_mv.row - 1) > x->mv_row_min);
        all_in &= ((ref_mv->as_mv.row + 1) < x->mv_row_max);
        all_in &= ((ref_mv->as_mv.col - 1) > x->mv_col_min);
        all_in &= ((ref_mv->as_mv.col + 1) < x->mv_col_max);

        if(all_in)
        {
            unsigned int sad_array[4];
            unsigned char *block_offset[4];
            block_offset[0] = best_address - in_what_stride;
            block_offset[1] = best_address - 1;
            block_offset[2] = best_address + 1;
            block_offset[3] = best_address + in_what_stride;

            fn_ptr->sdx4df(what, what_stride, block_offset, in_what_stride, sad_array);

            for (j = 0; j < 4; j++)
            {
                if (sad_array[j] < bestsad)
                {
                    this_mv.as_mv.row = ref_mv->as_mv.row + neighbors[j].row;
                    this_mv.as_mv.col = ref_mv->as_mv.col + neighbors[j].col;
                    sad_array[j] += mvsad_err_cost(&this_mv, &fcenter_mv, mvsadcost, error_per_bit);

                    if (sad_array[j] < bestsad)
                    {
                        bestsad = sad_array[j];
                        best_site = j;
                    }
                }
            }
        }
        else
        {
            for (j = 0 ; j < 4 ; j++)
            {
                this_row_offset = ref_mv->as_mv.row + neighbors[j].row;
                this_col_offset = ref_mv->as_mv.col + neighbors[j].col;

                if ((this_col_offset > x->mv_col_min) && (this_col_offset < x->mv_col_max) &&
                (this_row_offset > x->mv_row_min) && (this_row_offset < x->mv_row_max))
                {
                    check_here = (neighbors[j].row)*in_what_stride + neighbors[j].col + best_address;
                    thissad = fn_ptr->sdf(what, what_stride, check_here , in_what_stride, bestsad);

                    if (thissad < bestsad)
                    {
                        this_mv.as_mv.row = this_row_offset;
                        this_mv.as_mv.col = this_col_offset;
                        thissad += mvsad_err_cost(&this_mv, &fcenter_mv, mvsadcost, error_per_bit);

                        if (thissad < bestsad)
                        {
                            bestsad = thissad;
                            best_site = j;
                        }
                    }
                }
            }
        }

        if (best_site == -1)
            break;
        else
        {
            ref_mv->as_mv.row += neighbors[best_site].row;
            ref_mv->as_mv.col += neighbors[best_site].col;
            best_address += (neighbors[best_site].row)*in_what_stride + neighbors[best_site].col;
        }
    }

    this_mv.as_mv.row = ref_mv->as_mv.row << 3;
    this_mv.as_mv.col = ref_mv->as_mv.col << 3;

    if (bestsad < INT_MAX)
        return fn_ptr->vf(what, what_stride, best_address, in_what_stride, (unsigned int *)(&thissad))
#if CONFIG_HIGH_PRECISION_MV
        + mv_err_cost(&this_mv, center_mv, mvcost, x->errorperbit, xd->allow_high_precision_mv);
#else
        + mv_err_cost(&this_mv, center_mv, mvcost, x->errorperbit);
#endif
    else
        return INT_MAX;
}



#ifdef ENTROPY_STATS
void print_mode_context(void)
{
    FILE *f = fopen("modecont.c", "a");
    int i, j;

    fprintf(f, "#include \"entropy.h\"\n");
    fprintf(f, "const int vp8_mode_contexts[6][4] =");
    fprintf(f, "{\n");
    for (j = 0; j < 6; j++)
    {
        fprintf(f, "  {/* %d */ ", j);
        fprintf(f, "    ");
        for (i = 0; i < 4; i++)
        {
            int this_prob;
            int count;

            // context probs
            count = mv_ref_ct[j][i][0] + mv_ref_ct[j][i][1];
            if (count)
                this_prob = 256 * mv_ref_ct[j][i][0] / count;
            else
                this_prob = 128;

            if (this_prob == 0)
                this_prob = 1;
            fprintf(f, "%5d, ", this_prob);
        }
        fprintf(f, "  },\n");
    }

    fprintf(f, "};\n");
    fclose(f);
}

/* MV ref count ENTROPY_STATS stats code */
void init_mv_ref_counts()
{
    vpx_memset(mv_ref_ct, 0, sizeof(mv_ref_ct));
    vpx_memset(mv_mode_cts, 0, sizeof(mv_mode_cts));
}

void accum_mv_refs(MB_PREDICTION_MODE m, const int ct[4])
{
    if (m == ZEROMV)
    {
        ++mv_ref_ct [ct[0]] [0] [0];
        ++mv_mode_cts[0][0];
    }
    else
    {
        ++mv_ref_ct [ct[0]] [0] [1];
        ++mv_mode_cts[0][1];

        if (m == NEARESTMV)
        {
            ++mv_ref_ct [ct[1]] [1] [0];
            ++mv_mode_cts[1][0];
        }
        else
        {
            ++mv_ref_ct [ct[1]] [1] [1];
            ++mv_mode_cts[1][1];

            if (m == NEARMV)
            {
                ++mv_ref_ct [ct[2]] [2] [0];
                ++mv_mode_cts[2][0];
            }
            else
            {
                ++mv_ref_ct [ct[2]] [2] [1];
                ++mv_mode_cts[2][1];

                if (m == NEWMV)
                {
                    ++mv_ref_ct [ct[3]] [3] [0];
                    ++mv_mode_cts[3][0];
                }
                else
                {
                    ++mv_ref_ct [ct[3]] [3] [1];
                    ++mv_mode_cts[3][1];
                }
            }
        }
    }
}

#endif/* END MV ref count ENTROPY_STATS stats code */
