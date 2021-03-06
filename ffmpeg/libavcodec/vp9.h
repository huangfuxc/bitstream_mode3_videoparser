/*
 * VP9 compatible video decoder
 *
 * Copyright (C) 2013 Ronald S. Bultje <rsbultje gmail com>
 * Copyright (C) 2013 Clément Bœsch <u pkh me>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef AVCODEC_VP9_H
#define AVCODEC_VP9_H

#include <stdint.h>

#include "thread.h"
#include "vp56.h"
#include "vp9common.h"
#include "vp9dsp.h"
#include "vp9data.h"
#include "videodsp.h"

struct VP9mvrefPair {
    VP56mv mv[2];
    int8_t ref[2];
};

typedef struct VP9Frame {
    ThreadFrame tf;
    AVBufferRef *extradata;
    uint8_t *segmentation_map;
    struct VP9mvrefPair *mv;
    int uses_2pass;

    AVBufferRef *hwaccel_priv_buf;
    void *hwaccel_picture_private;
} VP9Frame;

typedef struct VP9BitstreamHeader {
    // bitstream header
    uint8_t profile;
    uint8_t keyframe;
    uint8_t invisible;
    uint8_t errorres;
    uint8_t intraonly;
    uint8_t resetctx;
    uint8_t refreshrefmask;
    uint8_t highprecisionmvs;
    enum FilterMode filtermode;
    uint8_t allowcompinter;
    uint8_t refreshctx;
    uint8_t parallelmode;
    uint8_t framectxid;
    uint8_t use_last_frame_mvs;
    uint8_t refidx[3];
    uint8_t signbias[3];
    uint8_t fixcompref;
    uint8_t varcompref[2];
    struct {
        uint8_t level;
        int8_t sharpness;
    } filter;
    struct {
        uint8_t enabled;
        uint8_t updated;
        int8_t mode[2];
        int8_t ref[4];
    } lf_delta;
    uint8_t yac_qi;
    int8_t ydc_qdelta, uvdc_qdelta, uvac_qdelta;
    uint8_t lossless;
#define MAX_SEGMENT 8
    struct {
        uint8_t enabled;
        uint8_t temporal;
        uint8_t absolute_vals;
        uint8_t update_map;
        uint8_t prob[7];
        uint8_t pred_prob[3];
        struct {
            uint8_t q_enabled;
            uint8_t lf_enabled;
            uint8_t ref_enabled;
            uint8_t skip_enabled;
            uint8_t ref_val;
            int16_t q_val;
            int8_t lf_val;
            int16_t qmul[2][2];
            uint8_t lflvl[4][2];
        } feat[MAX_SEGMENT];
    } segmentation;
    enum TxfmMode txfmmode;
    enum CompPredMode comppredmode;
    struct {
        unsigned log2_tile_cols, log2_tile_rows;
        unsigned tile_cols, tile_rows;
    } tiling;

    int uncompressed_header_size;
    int compressed_header_size;
} VP9BitstreamHeader;

typedef struct VP9SharedContext {
    VP9BitstreamHeader h;

    ThreadFrame refs[8];
#define CUR_FRAME 0
#define REF_FRAME_MVPAIR 1
#define REF_FRAME_SEGMAP 2
    VP9Frame frames[3];
} VP9SharedContext;

struct VP9Filter {
    uint8_t level[8 * 8];
    uint8_t /* bit=col */ mask[2 /* 0=y, 1=uv */][2 /* 0=col, 1=row */]
                              [8 /* rows */][4 /* 0=16, 1=8, 2=4, 3=inner4 */];
};

typedef struct VP9Block {
    uint8_t seg_id, intra, comp, ref[2], mode[4], uvmode, skip;
    enum FilterMode filter;
    VP56mv mv [4 /* b_idx */][2 /* ref */];
    VP56mv mvd[4 /* b_idx */][2 /* ref */];   //  P.L.
    int    CodedMv[4] ;   //  P.L.
    enum BlockSize bs;
    enum TxfmMode tx, uvtx;
    enum BlockLevel bl;
    enum BlockPartition bp;
} VP9Block;

typedef struct VP9Context {
    VP9SharedContext s;

    VP9DSPContext dsp;
    VideoDSPContext vdsp;
    GetBitContext gb;
    VP56RangeCoder c;
    VP56RangeCoder *c_b;
    unsigned c_b_size;
    VP9Block *b_base, *b;
    int pass;
    int row, row7, col, col7;
    uint8_t *dst[3];
    ptrdiff_t y_stride, uv_stride;

    uint8_t ss_h, ss_v;
    uint8_t last_bpp, bpp, bpp_index, bytesperpixel;
    uint8_t last_keyframe;
    // sb_cols/rows, rows/cols and last_fmt are used for allocating all internal
    // arrays, and are thus per-thread. w/h and gf_fmt are synced between threads
    // and are therefore per-stream. pix_fmt represents the value in the header
    // of the currently processed frame.
    int w, h;
    enum AVPixelFormat pix_fmt, last_fmt, gf_fmt;
    unsigned sb_cols, sb_rows, rows, cols;
    ThreadFrame next_refs[8];

    struct {
        uint8_t lim_lut[64];
        uint8_t mblim_lut[64];
    } filter_lut;
    unsigned tile_row_start, tile_row_end, tile_col_start, tile_col_end;
    struct {
        prob_context p;
        uint8_t coef[4][2][2][6][6][3];
    } prob_ctx[4];
    struct {
        prob_context p;
        uint8_t coef[4][2][2][6][6][11];
    } prob;
    struct {
        unsigned y_mode[4][10];
        unsigned uv_mode[10][10];
        unsigned filter[4][3];
        unsigned mv_mode[7][4];
        unsigned intra[4][2];
        unsigned comp[5][2];
        unsigned single_ref[5][2][2];
        unsigned comp_ref[5][2];
        unsigned tx32p[2][4];
        unsigned tx16p[2][3];
        unsigned tx8p[2][2];
        unsigned skip[3][2];
        unsigned mv_joint[4];
        struct {
            unsigned sign[2];
            unsigned classes[11];
            unsigned class0[2];
            unsigned bits[10][2];
            unsigned class0_fp[2][4];
            unsigned fp[4];
            unsigned class0_hp[2];
            unsigned hp[2];
        } mv_comp[2];
        unsigned partition[4][4][4];
        unsigned coef[4][2][2][6][6][3];
        unsigned eob[4][2][2][6][6][2];
    } counts;

    // contextual (left/above) cache
    DECLARE_ALIGNED(16, uint8_t, left_y_nnz_ctx)[16];
    DECLARE_ALIGNED(16, uint8_t, left_mode_ctx)[16];
    DECLARE_ALIGNED(16, VP56mv, left_mv_ctx)[16][2];
    DECLARE_ALIGNED(16, uint8_t, left_uv_nnz_ctx)[2][16];
    DECLARE_ALIGNED(8, uint8_t, left_partition_ctx)[8];
    DECLARE_ALIGNED(8, uint8_t, left_skip_ctx)[8];
    DECLARE_ALIGNED(8, uint8_t, left_txfm_ctx)[8];
    DECLARE_ALIGNED(8, uint8_t, left_segpred_ctx)[8];
    DECLARE_ALIGNED(8, uint8_t, left_intra_ctx)[8];
    DECLARE_ALIGNED(8, uint8_t, left_comp_ctx)[8];
    DECLARE_ALIGNED(8, uint8_t, left_ref_ctx)[8];
    DECLARE_ALIGNED(8, uint8_t, left_filter_ctx)[8];
    uint8_t *above_partition_ctx;
    uint8_t *above_mode_ctx;
    // FIXME maybe merge some of the below in a flags field?
    uint8_t *above_y_nnz_ctx;
    uint8_t *above_uv_nnz_ctx[2];
    uint8_t *above_skip_ctx; // 1bit
    uint8_t *above_txfm_ctx; // 2bit
    uint8_t *above_segpred_ctx; // 1bit
    uint8_t *above_intra_ctx; // 1bit
    uint8_t *above_comp_ctx; // 1bit
    uint8_t *above_ref_ctx; // 2bit
    uint8_t *above_filter_ctx;
    VP56mv (*above_mv_ctx)[2];

    // whole-frame cache
    uint8_t *intra_pred_data[3];
    struct VP9Filter *lflvl;
    DECLARE_ALIGNED(32, uint8_t, edge_emu_buffer)[135 * 144 * 2];

    // block reconstruction intermediates
    int block_alloc_using_2pass;
    int16_t *block_base, *block, *uvblock_base[2], *uvblock[2];
    uint8_t *eob_base, *uveob_base[2], *eob, *uveob[2];
    struct { int x, y; } min_mv, max_mv;
    DECLARE_ALIGNED(32, uint8_t, tmp_y)[64 * 64 * 2];
    DECLARE_ALIGNED(32, uint8_t, tmp_uv)[2][64 * 64 * 2];
    uint16_t mvscale[3][2];
    uint8_t mvstep[3][2];
    int32_t*  BlackLine ;           // P.L.
} VP9Context;

#endif /* AVCODEC_VP9_H */
