/******************************************************************************
 * x86_emulate.c
 * 
 * Generic x86 (32-bit and 64-bit) instruction decoder and emulator.
 * 
 * Copyright (c) 2005-2007 Keir Fraser
 * Copyright (c) 2005-2007 XenSource Inc.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; If not, see <http://www.gnu.org/licenses/>.
 */

/* Operand sizes: 8-bit operands or specified/overridden size. */
#define ByteOp      (1<<0) /* 8-bit operands. */
/* Destination operand type. */
#define DstNone     (0<<1) /* No destination operand. */
#define DstImplicit (0<<1) /* Destination operand is implicit in the opcode. */
#define DstBitBase  (1<<1) /* Memory operand, bit string. */
#define DstReg      (2<<1) /* Register operand. */
#define DstEax      DstReg /* Register EAX (aka DstReg with no ModRM) */
#define DstMem      (3<<1) /* Memory operand. */
#define DstMask     (3<<1)
/* Source operand type. */
#define SrcNone     (0<<3) /* No source operand. */
#define SrcImplicit (0<<3) /* Source operand is implicit in the opcode. */
#define SrcReg      (1<<3) /* Register operand. */
#define SrcEax      SrcReg /* Register EAX (aka SrcReg with no ModRM) */
#define SrcMem      (2<<3) /* Memory operand. */
#define SrcMem16    (3<<3) /* Memory operand (16-bit). */
#define SrcImm      (4<<3) /* Immediate operand. */
#define SrcImmByte  (5<<3) /* 8-bit sign-extended immediate operand. */
#define SrcImm16    (6<<3) /* 16-bit zero-extended immediate operand. */
#define SrcMask     (7<<3)
/* Generic ModRM decode. */
#define ModRM       (1<<6)
/* Destination is only written; never read. */
#define Mov         (1<<7)
/* All operands are implicit in the opcode. */
#define ImplicitOps (DstImplicit|SrcImplicit)

typedef uint8_t opcode_desc_t;

static const opcode_desc_t opcode_table[256] = {
    /* 0x00 - 0x07 */
    ByteOp|DstMem|SrcReg|ModRM, DstMem|SrcReg|ModRM,
    ByteOp|DstReg|SrcMem|ModRM, DstReg|SrcMem|ModRM,
    ByteOp|DstEax|SrcImm, DstEax|SrcImm, ImplicitOps|Mov, ImplicitOps|Mov,
    /* 0x08 - 0x0F */
    ByteOp|DstMem|SrcReg|ModRM, DstMem|SrcReg|ModRM,
    ByteOp|DstReg|SrcMem|ModRM, DstReg|SrcMem|ModRM,
    ByteOp|DstEax|SrcImm, DstEax|SrcImm, ImplicitOps|Mov, 0,
    /* 0x10 - 0x17 */
    ByteOp|DstMem|SrcReg|ModRM, DstMem|SrcReg|ModRM,
    ByteOp|DstReg|SrcMem|ModRM, DstReg|SrcMem|ModRM,
    ByteOp|DstEax|SrcImm, DstEax|SrcImm, ImplicitOps|Mov, ImplicitOps|Mov,
    /* 0x18 - 0x1F */
    ByteOp|DstMem|SrcReg|ModRM, DstMem|SrcReg|ModRM,
    ByteOp|DstReg|SrcMem|ModRM, DstReg|SrcMem|ModRM,
    ByteOp|DstEax|SrcImm, DstEax|SrcImm, ImplicitOps|Mov, ImplicitOps|Mov,
    /* 0x20 - 0x27 */
    ByteOp|DstMem|SrcReg|ModRM, DstMem|SrcReg|ModRM,
    ByteOp|DstReg|SrcMem|ModRM, DstReg|SrcMem|ModRM,
    ByteOp|DstEax|SrcImm, DstEax|SrcImm, 0, ImplicitOps,
    /* 0x28 - 0x2F */
    ByteOp|DstMem|SrcReg|ModRM, DstMem|SrcReg|ModRM,
    ByteOp|DstReg|SrcMem|ModRM, DstReg|SrcMem|ModRM,
    ByteOp|DstEax|SrcImm, DstEax|SrcImm, 0, ImplicitOps,
    /* 0x30 - 0x37 */
    ByteOp|DstMem|SrcReg|ModRM, DstMem|SrcReg|ModRM,
    ByteOp|DstReg|SrcMem|ModRM, DstReg|SrcMem|ModRM,
    ByteOp|DstEax|SrcImm, DstEax|SrcImm, 0, ImplicitOps,
    /* 0x38 - 0x3F */
    ByteOp|DstMem|SrcReg|ModRM, DstMem|SrcReg|ModRM,
    ByteOp|DstReg|SrcMem|ModRM, DstReg|SrcMem|ModRM,
    ByteOp|DstEax|SrcImm, DstEax|SrcImm, 0, ImplicitOps,
    /* 0x40 - 0x4F */
    ImplicitOps, ImplicitOps, ImplicitOps, ImplicitOps,
    ImplicitOps, ImplicitOps, ImplicitOps, ImplicitOps,
    ImplicitOps, ImplicitOps, ImplicitOps, ImplicitOps,
    ImplicitOps, ImplicitOps, ImplicitOps, ImplicitOps,
    /* 0x50 - 0x5F */
    ImplicitOps|Mov, ImplicitOps|Mov, ImplicitOps|Mov, ImplicitOps|Mov,
    ImplicitOps|Mov, ImplicitOps|Mov, ImplicitOps|Mov, ImplicitOps|Mov,
    ImplicitOps|Mov, ImplicitOps|Mov, ImplicitOps|Mov, ImplicitOps|Mov,
    ImplicitOps|Mov, ImplicitOps|Mov, ImplicitOps|Mov, ImplicitOps|Mov,
    /* 0x60 - 0x67 */
    ImplicitOps, ImplicitOps, DstReg|SrcMem|ModRM, DstReg|SrcNone|ModRM|Mov,
    0, 0, 0, 0,
    /* 0x68 - 0x6F */
    DstImplicit|SrcImm|Mov, DstReg|SrcImm|ModRM|Mov,
    DstImplicit|SrcImmByte|Mov, DstReg|SrcImmByte|ModRM|Mov,
    ImplicitOps|Mov, ImplicitOps|Mov, ImplicitOps|Mov, ImplicitOps|Mov,
    /* 0x70 - 0x77 */
    DstImplicit|SrcImmByte, DstImplicit|SrcImmByte,
    DstImplicit|SrcImmByte, DstImplicit|SrcImmByte,
    DstImplicit|SrcImmByte, DstImplicit|SrcImmByte,
    DstImplicit|SrcImmByte, DstImplicit|SrcImmByte,
    /* 0x78 - 0x7F */
    DstImplicit|SrcImmByte, DstImplicit|SrcImmByte,
    DstImplicit|SrcImmByte, DstImplicit|SrcImmByte,
    DstImplicit|SrcImmByte, DstImplicit|SrcImmByte,
    DstImplicit|SrcImmByte, DstImplicit|SrcImmByte,
    /* 0x80 - 0x87 */
    ByteOp|DstMem|SrcImm|ModRM, DstMem|SrcImm|ModRM,
    ByteOp|DstMem|SrcImm|ModRM, DstMem|SrcImmByte|ModRM,
    ByteOp|DstReg|SrcMem|ModRM, DstReg|SrcMem|ModRM,
    ByteOp|DstMem|SrcReg|ModRM, DstMem|SrcReg|ModRM,
    /* 0x88 - 0x8F */
    ByteOp|DstMem|SrcReg|ModRM|Mov, DstMem|SrcReg|ModRM|Mov,
    ByteOp|DstReg|SrcMem|ModRM|Mov, DstReg|SrcMem|ModRM|Mov,
    DstMem|SrcReg|ModRM|Mov, DstReg|SrcNone|ModRM,
    DstReg|SrcMem16|ModRM|Mov, DstMem|SrcNone|ModRM|Mov,
    /* 0x90 - 0x97 */
    DstImplicit|SrcEax, DstImplicit|SrcEax,
    DstImplicit|SrcEax, DstImplicit|SrcEax,
    DstImplicit|SrcEax, DstImplicit|SrcEax,
    DstImplicit|SrcEax, DstImplicit|SrcEax,
    /* 0x98 - 0x9F */
    ImplicitOps, ImplicitOps, ImplicitOps, ImplicitOps,
    ImplicitOps|Mov, ImplicitOps|Mov, ImplicitOps, ImplicitOps,
    /* 0xA0 - 0xA7 */
    ByteOp|DstEax|SrcMem|Mov, DstEax|SrcMem|Mov,
    ByteOp|DstMem|SrcEax|Mov, DstMem|SrcEax|Mov,
    ByteOp|ImplicitOps|Mov, ImplicitOps|Mov,
    ByteOp|ImplicitOps, ImplicitOps,
    /* 0xA8 - 0xAF */
    ByteOp|DstEax|SrcImm, DstEax|SrcImm,
    ByteOp|DstImplicit|SrcEax|Mov, DstImplicit|SrcEax|Mov,
    ByteOp|DstEax|SrcImplicit|Mov, DstEax|SrcImplicit|Mov,
    ByteOp|DstImplicit|SrcEax, DstImplicit|SrcEax,
    /* 0xB0 - 0xB7 */
    ByteOp|DstReg|SrcImm|Mov, ByteOp|DstReg|SrcImm|Mov,
    ByteOp|DstReg|SrcImm|Mov, ByteOp|DstReg|SrcImm|Mov,
    ByteOp|DstReg|SrcImm|Mov, ByteOp|DstReg|SrcImm|Mov,
    ByteOp|DstReg|SrcImm|Mov, ByteOp|DstReg|SrcImm|Mov,
    /* 0xB8 - 0xBF */
    DstReg|SrcImm|Mov, DstReg|SrcImm|Mov, DstReg|SrcImm|Mov, DstReg|SrcImm|Mov,
    DstReg|SrcImm|Mov, DstReg|SrcImm|Mov, DstReg|SrcImm|Mov, DstReg|SrcImm|Mov,
    /* 0xC0 - 0xC7 */
    ByteOp|DstMem|SrcImm|ModRM, DstMem|SrcImmByte|ModRM,
    DstImplicit|SrcImm16, ImplicitOps,
    DstReg|SrcMem|ModRM|Mov, DstReg|SrcMem|ModRM|Mov,
    ByteOp|DstMem|SrcImm|ModRM|Mov, DstMem|SrcImm|ModRM|Mov,
    /* 0xC8 - 0xCF */
    DstImplicit|SrcImm16, ImplicitOps, DstImplicit|SrcImm16, ImplicitOps,
    ImplicitOps, DstImplicit|SrcImmByte, ImplicitOps, ImplicitOps,
    /* 0xD0 - 0xD7 */
    ByteOp|DstMem|SrcImplicit|ModRM, DstMem|SrcImplicit|ModRM,
    ByteOp|DstMem|SrcImplicit|ModRM, DstMem|SrcImplicit|ModRM,
    DstImplicit|SrcImmByte, DstImplicit|SrcImmByte, ImplicitOps, ImplicitOps,
    /* 0xD8 - 0xDF */
    ImplicitOps|ModRM, ImplicitOps|ModRM|Mov,
    ImplicitOps|ModRM, ImplicitOps|ModRM|Mov,
    ImplicitOps|ModRM, ImplicitOps|ModRM|Mov,
    DstImplicit|SrcMem16|ModRM, ImplicitOps|ModRM|Mov,
    /* 0xE0 - 0xE7 */
    DstImplicit|SrcImmByte, DstImplicit|SrcImmByte,
    DstImplicit|SrcImmByte, DstImplicit|SrcImmByte,
    DstEax|SrcImmByte, DstEax|SrcImmByte,
    DstImplicit|SrcImmByte, DstImplicit|SrcImmByte,
    /* 0xE8 - 0xEF */
    DstImplicit|SrcImm|Mov, DstImplicit|SrcImm,
    ImplicitOps, DstImplicit|SrcImmByte,
    DstEax|SrcImplicit, DstEax|SrcImplicit, ImplicitOps, ImplicitOps,
    /* 0xF0 - 0xF7 */
    0, ImplicitOps, 0, 0,
    ImplicitOps, ImplicitOps, ByteOp|ModRM, ModRM,
    /* 0xF8 - 0xFF */
    ImplicitOps, ImplicitOps, ImplicitOps, ImplicitOps,
    ImplicitOps, ImplicitOps, ByteOp|DstMem|SrcNone|ModRM, DstMem|SrcNone|ModRM
};

static const opcode_desc_t twobyte_table[256] = {
    /* 0x00 - 0x07 */
    ModRM, ImplicitOps|ModRM, DstReg|SrcMem16|ModRM, DstReg|SrcMem16|ModRM,
    0, ImplicitOps, ImplicitOps, ImplicitOps,
    /* 0x08 - 0x0F */
    ImplicitOps, ImplicitOps, 0, ImplicitOps,
    0, ImplicitOps|ModRM, ImplicitOps, ModRM|SrcImmByte,
    /* 0x10 - 0x17 */
    ImplicitOps|ModRM, ImplicitOps|ModRM, ImplicitOps|ModRM, ImplicitOps|ModRM,
    ImplicitOps|ModRM, ImplicitOps|ModRM, ImplicitOps|ModRM, ImplicitOps|ModRM,
    /* 0x18 - 0x1F */
    ImplicitOps|ModRM, ImplicitOps|ModRM, ImplicitOps|ModRM, ImplicitOps|ModRM,
    ImplicitOps|ModRM, ImplicitOps|ModRM, ImplicitOps|ModRM, ImplicitOps|ModRM,
    /* 0x20 - 0x27 */
    DstMem|SrcImplicit|ModRM, DstMem|SrcImplicit|ModRM,
    DstImplicit|SrcMem|ModRM, DstImplicit|SrcMem|ModRM,
    0, 0, 0, 0,
    /* 0x28 - 0x2F */
    ImplicitOps|ModRM, ImplicitOps|ModRM, ImplicitOps|ModRM, ImplicitOps|ModRM,
    ImplicitOps|ModRM, ImplicitOps|ModRM, ImplicitOps|ModRM, ImplicitOps|ModRM,
    /* 0x30 - 0x37 */
    ImplicitOps, ImplicitOps, ImplicitOps, ImplicitOps,
    ImplicitOps, ImplicitOps, 0, ImplicitOps,
    /* 0x38 - 0x3F */
    DstReg|SrcMem|ModRM, 0, DstReg|SrcImmByte|ModRM, 0, 0, 0, 0, 0,
    /* 0x40 - 0x47 */
    DstReg|SrcMem|ModRM|Mov, DstReg|SrcMem|ModRM|Mov,
    DstReg|SrcMem|ModRM|Mov, DstReg|SrcMem|ModRM|Mov,
    DstReg|SrcMem|ModRM|Mov, DstReg|SrcMem|ModRM|Mov,
    DstReg|SrcMem|ModRM|Mov, DstReg|SrcMem|ModRM|Mov,
    /* 0x48 - 0x4F */
    DstReg|SrcMem|ModRM|Mov, DstReg|SrcMem|ModRM|Mov,
    DstReg|SrcMem|ModRM|Mov, DstReg|SrcMem|ModRM|Mov,
    DstReg|SrcMem|ModRM|Mov, DstReg|SrcMem|ModRM|Mov,
    DstReg|SrcMem|ModRM|Mov, DstReg|SrcMem|ModRM|Mov,
    /* 0x50 - 0x5F */
    ModRM, ModRM, ModRM, ModRM, ModRM, ModRM, ModRM, ModRM,
    ModRM, ModRM, ModRM, ModRM, ModRM, ModRM, ModRM, ModRM,
    /* 0x60 - 0x6F */
    ModRM, ModRM, ModRM, ModRM, ModRM, ModRM, ModRM, ModRM,
    ModRM, ModRM, ModRM, ModRM, ModRM, ModRM, ModRM, ImplicitOps|ModRM,
    /* 0x70 - 0x7F */
    SrcImmByte|ModRM, SrcImmByte|ModRM, SrcImmByte|ModRM, SrcImmByte|ModRM,
    ModRM, ModRM, ModRM, ImplicitOps,
    ModRM, ModRM, 0, 0, ModRM, ModRM, ImplicitOps|ModRM, ImplicitOps|ModRM,
    /* 0x80 - 0x87 */
    DstImplicit|SrcImm, DstImplicit|SrcImm,
    DstImplicit|SrcImm, DstImplicit|SrcImm,
    DstImplicit|SrcImm, DstImplicit|SrcImm,
    DstImplicit|SrcImm, DstImplicit|SrcImm,
    /* 0x88 - 0x8F */
    DstImplicit|SrcImm, DstImplicit|SrcImm,
    DstImplicit|SrcImm, DstImplicit|SrcImm,
    DstImplicit|SrcImm, DstImplicit|SrcImm,
    DstImplicit|SrcImm, DstImplicit|SrcImm,
    /* 0x90 - 0x97 */
    ByteOp|DstMem|SrcNone|ModRM|Mov, ByteOp|DstMem|SrcNone|ModRM|Mov,
    ByteOp|DstMem|SrcNone|ModRM|Mov, ByteOp|DstMem|SrcNone|ModRM|Mov,
    ByteOp|DstMem|SrcNone|ModRM|Mov, ByteOp|DstMem|SrcNone|ModRM|Mov,
    ByteOp|DstMem|SrcNone|ModRM|Mov, ByteOp|DstMem|SrcNone|ModRM|Mov,
    /* 0x98 - 0x9F */
    ByteOp|DstMem|SrcNone|ModRM|Mov, ByteOp|DstMem|SrcNone|ModRM|Mov,
    ByteOp|DstMem|SrcNone|ModRM|Mov, ByteOp|DstMem|SrcNone|ModRM|Mov,
    ByteOp|DstMem|SrcNone|ModRM|Mov, ByteOp|DstMem|SrcNone|ModRM|Mov,
    ByteOp|DstMem|SrcNone|ModRM|Mov, ByteOp|DstMem|SrcNone|ModRM|Mov,
    /* 0xA0 - 0xA7 */
    ImplicitOps|Mov, ImplicitOps|Mov, ImplicitOps, DstBitBase|SrcReg|ModRM,
    DstMem|SrcImmByte|ModRM, DstMem|SrcReg|ModRM, ModRM, ModRM,
    /* 0xA8 - 0xAF */
    ImplicitOps|Mov, ImplicitOps|Mov, ImplicitOps, DstBitBase|SrcReg|ModRM,
    DstMem|SrcImmByte|ModRM, DstMem|SrcReg|ModRM,
    ImplicitOps|ModRM, DstReg|SrcMem|ModRM,
    /* 0xB0 - 0xB7 */
    ByteOp|DstMem|SrcReg|ModRM, DstMem|SrcReg|ModRM,
    DstReg|SrcMem|ModRM|Mov, DstBitBase|SrcReg|ModRM,
    DstReg|SrcMem|ModRM|Mov, DstReg|SrcMem|ModRM|Mov,
    ByteOp|DstReg|SrcMem|ModRM|Mov, DstReg|SrcMem16|ModRM|Mov,
    /* 0xB8 - 0xBF */
    DstReg|SrcMem|ModRM, ModRM,
    DstBitBase|SrcImmByte|ModRM, DstBitBase|SrcReg|ModRM,
    DstReg|SrcMem|ModRM, DstReg|SrcMem|ModRM,
    ByteOp|DstReg|SrcMem|ModRM|Mov, DstReg|SrcMem16|ModRM|Mov,
    /* 0xC0 - 0xC7 */
    ByteOp|DstMem|SrcReg|ModRM, DstMem|SrcReg|ModRM,
    SrcImmByte|ModRM, DstMem|SrcReg|ModRM|Mov,
    SrcImmByte|ModRM, SrcImmByte|ModRM, SrcImmByte|ModRM, ImplicitOps|ModRM,
    /* 0xC8 - 0xCF */
    ImplicitOps, ImplicitOps, ImplicitOps, ImplicitOps,
    ImplicitOps, ImplicitOps, ImplicitOps, ImplicitOps,
    /* 0xD0 - 0xDF */
    ModRM, ModRM, ModRM, ModRM, ModRM, ModRM, ImplicitOps|ModRM, ModRM,
    ModRM, ModRM, ModRM, ModRM, ModRM, ModRM, ModRM, ModRM,
    /* 0xE0 - 0xEF */
    ModRM, ModRM, ModRM, ModRM, ModRM, ModRM, ModRM, ImplicitOps|ModRM,
    ModRM, ModRM, ModRM, ModRM, ModRM, ModRM, ModRM, ModRM,
    /* 0xF0 - 0xFF */
    ModRM, ModRM, ModRM, ModRM, ModRM, ModRM, ModRM, ModRM,
    ModRM, ModRM, ModRM, ModRM, ModRM, ModRM, ModRM, ModRM
};

static const opcode_desc_t xop_table[] = {
    DstReg|SrcImmByte|ModRM,
    DstReg|SrcMem|ModRM,
    DstReg|SrcImm|ModRM,
};

#define REX_PREFIX 0x40
#define REX_B 0x01
#define REX_X 0x02
#define REX_R 0x04
#define REX_W 0x08

#define vex_none 0

enum vex_opcx {
    vex_0f = vex_none + 1,
    vex_0f38,
    vex_0f3a,
};

enum vex_pfx {
    vex_66 = vex_none + 1,
    vex_f3,
    vex_f2
};

#define VEX_PREFIX_DOUBLE_MASK 0x1
#define VEX_PREFIX_SCALAR_MASK 0x2

static const uint8_t sse_prefix[] = { 0x66, 0xf3, 0xf2 };

#define SET_SSE_PREFIX(dst, vex_pfx) do { \
    if ( vex_pfx ) \
        (dst) = sse_prefix[(vex_pfx) - 1]; \
} while (0)

union vex {
    uint8_t raw[2];
    struct {
        uint8_t opcx:5;
        uint8_t b:1;
        uint8_t x:1;
        uint8_t r:1;
        uint8_t pfx:2;
        uint8_t l:1;
        uint8_t reg:4;
        uint8_t w:1;
    };
};

#define copy_REX_VEX(ptr, rex, vex) do { \
    if ( (vex).opcx != vex_none ) \
    { \
        if ( !mode_64bit() ) \
            vex.reg |= 8; \
        ptr[0] = 0xc4, ptr[1] = (vex).raw[0], ptr[2] = (vex).raw[1]; \
    } \
    else if ( mode_64bit() ) \
        ptr[1] = rex | REX_PREFIX; \
} while (0)

union evex {
    uint8_t raw[3];
    struct {
        uint8_t opcx:2;
        uint8_t :2;
        uint8_t R:1;
        uint8_t b:1;
        uint8_t x:1;
        uint8_t r:1;
        uint8_t pfx:2;
        uint8_t evex:1;
        uint8_t reg:4;
        uint8_t w:1;
        uint8_t opmsk:3;
        uint8_t RX:1;
        uint8_t bcst:1;
        uint8_t lr:2;
        uint8_t z:1;
    };
};

#define rep_prefix()   (vex.pfx >= vex_f3)
#define repe_prefix()  (vex.pfx == vex_f3)
#define repne_prefix() (vex.pfx == vex_f2)

/* Type, address-of, and value of an instruction's operand. */
struct operand {
    enum { OP_REG, OP_MEM, OP_IMM, OP_NONE } type;
    unsigned int bytes;

    /* Operand value. */
    unsigned long val;

    /* Original operand value. */
    unsigned long orig_val;

    /* OP_REG: Pointer to register field. */
    unsigned long *reg;

    /* OP_MEM: Segment and offset. */
    struct {
        enum x86_segment seg;
        unsigned long    off;
    } mem;
};
#ifdef __x86_64__
#define PTR_POISON ((void *)0x8086000000008086UL) /* non-canonical */
#else
#define PTR_POISON NULL /* 32-bit builds are for user-space, so NULL is OK. */
#endif

typedef union {
    uint64_t mmx;
    uint64_t __attribute__ ((aligned(16))) xmm[2];
    uint64_t __attribute__ ((aligned(32))) ymm[4];
} mmval_t;

/*
 * While proper alignment gets specified above, this doesn't get honored by
 * the compiler for automatic variables. Use this helper to instantiate a
 * suitably aligned variable, producing a pointer to access it.
 */
#define DECLARE_ALIGNED(type, var)                                   \
    long __##var[sizeof(type) + __alignof(type) - __alignof(long)];  \
    type *const var##p =                                             \
        (void *)((long)(__##var + __alignof(type) - __alignof(long)) \
                 & -__alignof(type))

#ifdef __GCC_ASM_FLAG_OUTPUTS__
# define ASM_FLAG_OUT(yes, no) yes
#else
# define ASM_FLAG_OUT(yes, no) no
#endif

/* MSRs. */
#define MSR_TSC          0x00000010
#define MSR_SYSENTER_CS  0x00000174
#define MSR_SYSENTER_ESP 0x00000175
#define MSR_SYSENTER_EIP 0x00000176
#define MSR_DEBUGCTL     0x000001d9
#define DEBUGCTL_BTF     (1 << 1)
#define MSR_BNDCFGS      0x00000d90
#define BNDCFG_ENABLE    (1 << 0)
#define BNDCFG_PRESERVE  (1 << 1)
#define MSR_EFER         0xc0000080
#define MSR_STAR         0xc0000081
#define MSR_LSTAR        0xc0000082
#define MSR_CSTAR        0xc0000083
#define MSR_FMASK        0xc0000084
#define MSR_TSC_AUX      0xc0000103

/* Control register flags. */
#define CR0_PE    (1<<0)
#define CR0_MP    (1<<1)
#define CR0_EM    (1<<2)
#define CR0_TS    (1<<3)

#define CR4_VME        (1<<0)
#define CR4_PVI        (1<<1)
#define CR4_TSD        (1<<2)
#define CR4_OSFXSR     (1<<9)
#define CR4_OSXMMEXCPT (1<<10)
#define CR4_UMIP       (1<<11)
#define CR4_FSGSBASE   (1<<16)
#define CR4_OSXSAVE    (1<<18)

/* EFLAGS bit definitions. */
#define EFLG_ID   (1<<21)
#define EFLG_VIP  (1<<20)
#define EFLG_VIF  (1<<19)
#define EFLG_AC   (1<<18)
#define EFLG_VM   (1<<17)
#define EFLG_RF   (1<<16)
#define EFLG_NT   (1<<14)
#define EFLG_IOPL (3<<12)
#define EFLG_OF   (1<<11)
#define EFLG_DF   (1<<10)
#define EFLG_IF   (1<<9)
#define EFLG_TF   (1<<8)
#define EFLG_SF   (1<<7)
#define EFLG_ZF   (1<<6)
#define EFLG_AF   (1<<4)
#define EFLG_PF   (1<<2)
#define EFLG_MBS  (1<<1)
#define EFLG_CF   (1<<0)

/* Floating point status word definitions. */
#define FSW_ES    (1U << 7)

/* MXCSR bit definitions. */
#define MXCSR_MM  (1U << 17)

/* Exception definitions. */
#define EXC_DE  0
#define EXC_DB  1
#define EXC_BP  3
#define EXC_OF  4
#define EXC_BR  5
#define EXC_UD  6
#define EXC_NM  7
#define EXC_DF  8
#define EXC_TS 10
#define EXC_NP 11
#define EXC_SS 12
#define EXC_GP 13
#define EXC_PF 14
#define EXC_MF 16
#define EXC_AC 17
#define EXC_XM 19

#define EXC_HAS_EC                                                      \
    ((1u << EXC_DF) | (1u << EXC_TS) | (1u << EXC_NP) |                 \
     (1u << EXC_SS) | (1u << EXC_GP) | (1u << EXC_PF) | (1u << EXC_AC))

/* Segment selector error code bits. */
#define ECODE_EXT (1 << 0)
#define ECODE_IDT (1 << 1)
#define ECODE_TI  (1 << 2)

/*
 * Instruction emulation:
 * Most instructions are emulated directly via a fragment of inline assembly
 * code. This allows us to save/restore EFLAGS and thus very easily pick up
 * any modified flags.
 */

#if defined(__x86_64__)
#define _LO32 "k"          /* force 32-bit operand */
#define _STK  "%%rsp"      /* stack pointer */
#define _BYTES_PER_LONG "8"
#elif defined(__i386__)
#define _LO32 ""           /* force 32-bit operand */
#define _STK  "%%esp"      /* stack pointer */
#define _BYTES_PER_LONG "4"
#endif

/*
 * These EFLAGS bits are restored from saved value during emulation, and
 * any changes are written back to the saved value after emulation.
 */
#define EFLAGS_MASK (EFLG_OF|EFLG_SF|EFLG_ZF|EFLG_AF|EFLG_PF|EFLG_CF)

/*
 * These EFLAGS bits are modifiable (by POPF and IRET), possibly subject
 * to further CPL and IOPL constraints.
 */
#define EFLAGS_MODIFIABLE (EFLG_ID|EFLG_AC|EFLG_RF|EFLG_NT|EFLG_IOPL| \
                           EFLG_DF|EFLG_IF|EFLG_TF|EFLAGS_MASK)

/* Before executing instruction: restore necessary bits in EFLAGS. */
#define _PRE_EFLAGS(_sav, _msk, _tmp)                           \
/* EFLAGS = (_sav & _msk) | (EFLAGS & ~_msk); _sav &= ~_msk; */ \
"movl %"_LO32 _sav",%"_LO32 _tmp"; "                            \
"push %"_tmp"; "                                                \
"push %"_tmp"; "                                                \
"movl %"_msk",%"_LO32 _tmp"; "                                  \
"andl %"_LO32 _tmp",("_STK"); "                                 \
"pushf; "                                                       \
"notl %"_LO32 _tmp"; "                                          \
"andl %"_LO32 _tmp",("_STK"); "                                 \
"andl %"_LO32 _tmp",2*"_BYTES_PER_LONG"("_STK"); "              \
"pop  %"_tmp"; "                                                \
"orl  %"_LO32 _tmp",("_STK"); "                                 \
"popf; "                                                        \
"pop  %"_tmp"; "                                                \
"movl %"_LO32 _tmp",%"_LO32 _sav"; "

/* After executing instruction: write-back necessary bits in EFLAGS. */
#define _POST_EFLAGS(_sav, _msk, _tmp)          \
/* _sav |= EFLAGS & _msk; */                    \
"pushf; "                                       \
"pop  %"_tmp"; "                                \
"andl %"_msk",%"_LO32 _tmp"; "                  \
"orl  %"_LO32 _tmp",%"_LO32 _sav"; "

/* Raw emulation: instruction has two explicit operands. */
#define __emulate_2op_nobyte(_op,_src,_dst,_eflags, wsx,wsy,wdx,wdy,       \
                             lsx,lsy,ldx,ldy, qsx,qsy,qdx,qdy)             \
do{ unsigned long _tmp;                                                    \
    switch ( (_dst).bytes )                                                \
    {                                                                      \
    case 2:                                                                \
        asm volatile (                                                     \
            _PRE_EFLAGS("0","4","2")                                       \
            _op"w %"wsx"3,%"wdx"1; "                                       \
            _POST_EFLAGS("0","4","2")                                      \
            : "+g" (_eflags), "+" wdy ((_dst).val), "=&r" (_tmp)           \
            : wsy ((_src).val), "i" (EFLAGS_MASK) );                       \
        break;                                                             \
    case 4:                                                                \
        asm volatile (                                                     \
            _PRE_EFLAGS("0","4","2")                                       \
            _op"l %"lsx"3,%"ldx"1; "                                       \
            _POST_EFLAGS("0","4","2")                                      \
            : "+g" (_eflags), "+" ldy ((_dst).val), "=&r" (_tmp)           \
            : lsy ((_src).val), "i" (EFLAGS_MASK) );                       \
        break;                                                             \
    case 8:                                                                \
        __emulate_2op_8byte(_op, _src, _dst, _eflags, qsx, qsy, qdx, qdy); \
        break;                                                             \
    }                                                                      \
} while (0)
#define __emulate_2op(_op,_src,_dst,_eflags,_bx,_by,_wx,_wy,_lx,_ly,_qx,_qy)\
do{ unsigned long _tmp;                                                    \
    switch ( (_dst).bytes )                                                \
    {                                                                      \
    case 1:                                                                \
        asm volatile (                                                     \
            _PRE_EFLAGS("0","4","2")                                       \
            _op"b %"_bx"3,%1; "                                            \
            _POST_EFLAGS("0","4","2")                                      \
            : "+g" (_eflags), "+m" ((_dst).val), "=&r" (_tmp)              \
            : _by ((_src).val), "i" (EFLAGS_MASK) );                       \
        break;                                                             \
    default:                                                               \
        __emulate_2op_nobyte(_op,_src,_dst,_eflags, _wx,_wy,"","m",        \
                             _lx,_ly,"","m", _qx,_qy,"","m");              \
        break;                                                             \
    }                                                                      \
} while (0)
/* Source operand is byte-sized and may be restricted to just %cl. */
#define emulate_2op_SrcB(_op, _src, _dst, _eflags)                         \
    __emulate_2op(_op, _src, _dst, _eflags,                                \
                  "b", "c", "b", "c", "b", "c", "b", "c")
/* Source operand is byte, word, long or quad sized. */
#define emulate_2op_SrcV(_op, _src, _dst, _eflags)                         \
    __emulate_2op(_op, _src, _dst, _eflags,                                \
                  "b", "q", "w", "r", _LO32, "r", "", "r")
/* Source operand is word, long or quad sized. */
#define emulate_2op_SrcV_nobyte(_op, _src, _dst, _eflags)                  \
    __emulate_2op_nobyte(_op, _src, _dst, _eflags, "w", "r", "", "m",      \
                         _LO32, "r", "", "m", "", "r", "", "m")
/* Operands are word, long or quad sized and source may be in memory. */
#define emulate_2op_SrcV_srcmem(_op, _src, _dst, _eflags)                  \
    __emulate_2op_nobyte(_op, _src, _dst, _eflags, "", "m", "w", "r",      \
                         "", "m", _LO32, "r", "", "m", "", "r")

/* Instruction has only one explicit operand (no source operand). */
#define emulate_1op(_op,_dst,_eflags)                                      \
do{ unsigned long _tmp;                                                    \
    switch ( (_dst).bytes )                                                \
    {                                                                      \
    case 1:                                                                \
        asm volatile (                                                     \
            _PRE_EFLAGS("0","3","2")                                       \
            _op"b %1; "                                                    \
            _POST_EFLAGS("0","3","2")                                      \
            : "+g" (_eflags), "+m" ((_dst).val), "=&r" (_tmp)              \
            : "i" (EFLAGS_MASK) );                                         \
        break;                                                             \
    case 2:                                                                \
        asm volatile (                                                     \
            _PRE_EFLAGS("0","3","2")                                       \
            _op"w %1; "                                                    \
            _POST_EFLAGS("0","3","2")                                      \
            : "+g" (_eflags), "+m" ((_dst).val), "=&r" (_tmp)              \
            : "i" (EFLAGS_MASK) );                                         \
        break;                                                             \
    case 4:                                                                \
        asm volatile (                                                     \
            _PRE_EFLAGS("0","3","2")                                       \
            _op"l %1; "                                                    \
            _POST_EFLAGS("0","3","2")                                      \
            : "+g" (_eflags), "+m" ((_dst).val), "=&r" (_tmp)              \
            : "i" (EFLAGS_MASK) );                                         \
        break;                                                             \
    case 8:                                                                \
        __emulate_1op_8byte(_op, _dst, _eflags);                           \
        break;                                                             \
    }                                                                      \
} while (0)

/* Emulate an instruction with quadword operands (x86/64 only). */
#if defined(__x86_64__)
#define __emulate_2op_8byte(_op, _src, _dst, _eflags, qsx, qsy, qdx, qdy) \
do{ asm volatile (                                                      \
        _PRE_EFLAGS("0","4","2")                                        \
        _op"q %"qsx"3,%"qdx"1; "                                        \
        _POST_EFLAGS("0","4","2")                                       \
        : "+g" (_eflags), "+" qdy ((_dst).val), "=&r" (_tmp)            \
        : qsy ((_src).val), "i" (EFLAGS_MASK) );                        \
} while (0)
#define __emulate_1op_8byte(_op, _dst, _eflags)                         \
do{ asm volatile (                                                      \
        _PRE_EFLAGS("0","3","2")                                        \
        _op"q %1; "                                                     \
        _POST_EFLAGS("0","3","2")                                       \
        : "+g" (_eflags), "+m" ((_dst).val), "=&r" (_tmp)               \
        : "i" (EFLAGS_MASK) );                                          \
} while (0)
#elif defined(__i386__)
#define __emulate_2op_8byte(_op, _src, _dst, _eflags, qsx, qsy, qdx, qdy)
#define __emulate_1op_8byte(_op, _dst, _eflags)
#endif /* __i386__ */

#define emulate_stub(dst, src...) do {                                  \
    unsigned long tmp;                                                  \
    asm volatile ( _PRE_EFLAGS("[efl]", "[msk]", "[tmp]")               \
                   "call *%[stub];"                                     \
                   _POST_EFLAGS("[efl]", "[msk]", "[tmp]")              \
                   : dst, [tmp] "=&r" (tmp), [efl] "+g" (_regs._eflags) \
                   : [stub] "r" (stub.func),                            \
                     [msk] "i" (EFLAGS_MASK), ## src );                 \
} while (0)

/* Fetch next part of the instruction being emulated. */
#define insn_fetch_bytes(_size)                                         \
({ unsigned long _x = 0, _ip = state->ip;                               \
   state->ip += (_size); /* real hardware doesn't truncate */           \
   generate_exception_if((uint8_t)(state->ip -                          \
                                   ctxt->regs->r(ip)) > MAX_INST_LEN,   \
                         EXC_GP, 0);                                    \
   rc = ops->insn_fetch(x86_seg_cs, _ip, &_x, (_size), ctxt);           \
   if ( rc ) goto done;                                                 \
   _x;                                                                  \
})
#define insn_fetch_type(_type) ((_type)insn_fetch_bytes(sizeof(_type)))

#define truncate_word(ea, byte_width)           \
({  unsigned long __ea = (ea);                  \
    unsigned int _width = (byte_width);         \
    ((_width == sizeof(unsigned long)) ? __ea : \
     (__ea & ((1UL << (_width << 3)) - 1)));    \
})
#define truncate_ea(ea) truncate_word((ea), ad_bytes)

#ifdef __x86_64__
# define mode_64bit() (ctxt->addr_size == 64)
#else
# define mode_64bit() false
#endif

#define fail_if(p)                                      \
do {                                                    \
    rc = (p) ? X86EMUL_UNHANDLEABLE : X86EMUL_OKAY;     \
    if ( rc ) goto done;                                \
} while (0)

static inline int mkec(uint8_t e, int32_t ec, ...)
{
    return (e < 32 && ((1u << e) & EXC_HAS_EC)) ? ec : X86_EVENT_NO_EC;
}

#define generate_exception_if(p, e, ec...)                                \
({  if ( (p) ) {                                                          \
        x86_emul_hw_exception(e, mkec(e, ##ec, 0), ctxt);                 \
        rc = X86EMUL_EXCEPTION;                                           \
        goto done;                                                        \
    }                                                                     \
})

#define generate_exception(e, ec...) generate_exception_if(true, e, ##ec)

/*
 * Given byte has even parity (even number of 1s)? SDM Vol. 1 Sec. 3.4.3.1,
 * "Status Flags": EFLAGS.PF reflects parity of least-sig. byte of result only.
 */
static bool even_parity(uint8_t v)
{
    asm ( "test %1,%1" ASM_FLAG_OUT(, "; setp %0")
          : ASM_FLAG_OUT("=@ccp", "=qm") (v) : "q" (v) );

    return v;
}

/* Update address held in a register, based on addressing mode. */
#define _register_address_increment(reg, inc, byte_width)               \
do {                                                                    \
    int _inc = (inc); /* signed type ensures sign extension to long */  \
    unsigned int _width = (byte_width);                                 \
    if ( _width == sizeof(unsigned long) )                              \
        (reg) += _inc;                                                  \
    else if ( mode_64bit() )                                            \
        (reg) = ((reg) + _inc) & ((1UL << (_width << 3)) - 1);          \
    else                                                                \
        (reg) = ((reg) & ~((1UL << (_width << 3)) - 1)) |               \
                (((reg) + _inc) & ((1UL << (_width << 3)) - 1));        \
} while (0)
#define register_address_adjust(reg, adj)                               \
    _register_address_increment(reg,                                    \
                                _regs._eflags & EFLG_DF ? -(adj) : (adj), \
                                ad_bytes)

#define sp_pre_dec(dec) ({                                              \
    _register_address_increment(_regs.r(sp), -(dec), ctxt->sp_size/8);  \
    truncate_word(_regs.r(sp), ctxt->sp_size/8);                        \
})
#define sp_post_inc(inc) ({                                             \
    unsigned long sp = truncate_word(_regs.r(sp), ctxt->sp_size/8);     \
    _register_address_increment(_regs.r(sp), (inc), ctxt->sp_size/8);   \
    sp;                                                                 \
})

#define jmp_rel(rel)                                                    \
do {                                                                    \
    unsigned long ip = _regs.r(ip) + (int)(rel);                        \
    if ( op_bytes == 2 )                                                \
        ip = (uint16_t)ip;                                              \
    else if ( !mode_64bit() )                                           \
        ip = (uint32_t)ip;                                              \
    rc = ops->insn_fetch(x86_seg_cs, ip, NULL, 0, ctxt);                \
    if ( rc ) goto done;                                                \
    _regs.r(ip) = ip;                                                   \
    singlestep = _regs._eflags & EFLG_TF;                               \
} while (0)

#define validate_far_branch(cs, ip) ({                                  \
    if ( sizeof(ip) <= 4 ) {                                            \
        ASSERT(in_longmode(ctxt, ops) <= 0);                            \
        generate_exception_if((ip) > (cs)->limit, EXC_GP, 0);           \
    } else                                                              \
        generate_exception_if(in_longmode(ctxt, ops) &&                 \
                              (cs)->attr.fields.l                       \
                              ? !is_canonical_address(ip)               \
                              : (ip) > (cs)->limit, EXC_GP, 0);         \
})

#define commit_far_branch(cs, newip) ({                                 \
    validate_far_branch(cs, newip);                                     \
    _regs.r(ip) = (newip);                                              \
    singlestep = _regs._eflags & EFLG_TF;                               \
    ops->write_segment(x86_seg_cs, cs, ctxt);                           \
})

struct fpu_insn_ctxt {
    uint8_t insn_bytes;
    int8_t exn_raised;
};

static void fpu_handle_exception(void *_fic, struct cpu_user_regs *regs)
{
    struct fpu_insn_ctxt *fic = _fic;
    ASSERT(regs->entry_vector < 0x20);
    fic->exn_raised = regs->entry_vector;
    regs->r(ip) += fic->insn_bytes;
}

static int _get_fpu(
    enum x86_emulate_fpu_type type,
    struct fpu_insn_ctxt *fic,
    struct x86_emulate_ctxt *ctxt,
    const struct x86_emulate_ops *ops)
{
    int rc;

    fic->exn_raised = -1;

    fail_if(!ops->get_fpu);
    rc = ops->get_fpu(fpu_handle_exception, fic, type, ctxt);

    if ( rc == X86EMUL_OKAY )
    {
        unsigned long cr0;

        fail_if(!ops->read_cr);
        if ( type >= X86EMUL_FPU_xmm )
        {
            unsigned long cr4;

            rc = ops->read_cr(4, &cr4, ctxt);
            if ( rc != X86EMUL_OKAY )
                return rc;
            generate_exception_if(!(cr4 & ((type == X86EMUL_FPU_xmm)
                                           ? CR4_OSFXSR : CR4_OSXSAVE)),
                                  EXC_UD);
        }

        rc = ops->read_cr(0, &cr0, ctxt);
        if ( rc != X86EMUL_OKAY )
            return rc;
        if ( type >= X86EMUL_FPU_ymm )
        {
            /* Should be unreachable if VEX decoding is working correctly. */
            ASSERT((cr0 & CR0_PE) && !(ctxt->regs->_eflags & EFLG_VM));
        }
        if ( cr0 & CR0_EM )
        {
            generate_exception_if(type == X86EMUL_FPU_fpu, EXC_NM);
            generate_exception_if(type == X86EMUL_FPU_mmx, EXC_UD);
            generate_exception_if(type == X86EMUL_FPU_xmm, EXC_UD);
        }
        generate_exception_if((cr0 & CR0_TS) &&
                              (type != X86EMUL_FPU_wait || (cr0 & CR0_MP)),
                              EXC_NM);
    }

 done:
    return rc;
}

#define get_fpu(_type, _fic)                                    \
do {                                                            \
    rc = _get_fpu(_type, _fic, ctxt, ops);                      \
    if ( rc ) goto done;                                        \
} while (0)
#define _put_fpu()                                              \
do {                                                            \
    if ( ops->put_fpu != NULL )                                 \
        (ops->put_fpu)(ctxt);                                   \
} while (0)
#define put_fpu(_fic)                                           \
do {                                                            \
    _put_fpu();                                                 \
    if ( (_fic)->exn_raised == EXC_XM && ops->read_cr &&        \
         ops->read_cr(4, &cr4, ctxt) == X86EMUL_OKAY &&         \
         !(cr4 & CR4_OSXMMEXCPT) )                              \
        (_fic)->exn_raised = EXC_UD;                            \
    generate_exception_if((_fic)->exn_raised >= 0,              \
                          (_fic)->exn_raised);                  \
} while (0)

static inline bool fpu_check_write(void)
{
    uint16_t fsw;

    asm ( "fnstsw %0" : "=am" (fsw) );

    return !(fsw & FSW_ES);
}

#define emulate_fpu_insn(_op)                           \
    asm volatile (                                      \
        "movb $2f-1f,%0 \n"                             \
        "1: " _op "     \n"                             \
        "2:             \n"                             \
        : "=m" (fic.insn_bytes) : : "memory" )

#define emulate_fpu_insn_memdst(_op, _arg)              \
    asm volatile (                                      \
        "movb $2f-1f,%0 \n"                             \
        "1: " _op " %1  \n"                             \
        "2:             \n"                             \
        : "=m" (fic.insn_bytes), "=m" (_arg)            \
        : : "memory" )

#define emulate_fpu_insn_memsrc(_op, _arg)              \
    asm volatile (                                      \
        "movb $2f-1f,%0 \n"                             \
        "1: " _op " %1  \n"                             \
        "2:             \n"                             \
        : "=m" (fic.insn_bytes)                         \
        : "m" (_arg) : "memory" )

#define emulate_fpu_insn_stub(_bytes...)                                \
do {                                                                    \
    uint8_t *buf = get_stub(stub);                                      \
    unsigned int _nr = sizeof((uint8_t[]){ _bytes });                   \
    fic.insn_bytes = _nr;                                               \
    memcpy(buf, ((uint8_t[]){ _bytes, 0xc3 }), _nr + 1);                \
    stub.func();                                                        \
    put_stub(stub);                                                     \
} while (0)

#define emulate_fpu_insn_stub_eflags(bytes...)                          \
do {                                                                    \
    unsigned int nr_ = sizeof((uint8_t[]){ bytes });                    \
    unsigned long tmp_;                                                 \
    fic.insn_bytes = nr_;                                               \
    memcpy(get_stub(stub), ((uint8_t[]){ bytes, 0xc3 }), nr_ + 1);      \
    asm volatile ( _PRE_EFLAGS("[eflags]", "[mask]", "[tmp]")           \
                   "call *%[func];"                                     \
                   _POST_EFLAGS("[eflags]", "[mask]", "[tmp]")          \
                   : [eflags] "+g" (_regs._eflags),                     \
                     [tmp] "=&r" (tmp_)                                 \
                   : [func] "rm" (stub.func),                           \
                     [mask] "i" (EFLG_ZF|EFLG_PF|EFLG_CF) );            \
    put_stub(stub);                                                     \
} while (0)

static inline unsigned long get_loop_count(
    const struct cpu_user_regs *regs,
    int ad_bytes)
{
    return (ad_bytes > 4) ? regs->r(cx)
                          : (ad_bytes < 4) ? regs->cx : regs->_ecx;
}

static inline void put_loop_count(
    struct cpu_user_regs *regs,
    int ad_bytes,
    unsigned long count)
{
    if ( ad_bytes == 2 )
        regs->cx = count;
    else
        regs->r(cx) = ad_bytes == 4 ? (uint32_t)count : count;
}

#define get_rep_prefix(using_si, using_di) ({                           \
    unsigned long max_reps = 1;                                         \
    if ( rep_prefix() )                                                 \
        max_reps = get_loop_count(&_regs, ad_bytes);                    \
    if ( max_reps == 0 )                                                \
    {                                                                   \
        /*                                                              \
         * Skip the instruction if no repetitions are required, but     \
         * zero extend involved registers first when using 32-bit       \
         * addressing in 64-bit mode.                                   \
         */                                                             \
        if ( mode_64bit() && ad_bytes == 4 )                            \
        {                                                               \
            _regs.r(cx) = 0;                                            \
            if ( using_si ) _regs.r(si) = _regs._esi;                   \
            if ( using_di ) _regs.r(di) = _regs._edi;                   \
        }                                                               \
        goto complete_insn;                                             \
    }                                                                   \
    if ( max_reps > 1 && (_regs._eflags & EFLG_TF) &&                   \
         !is_branch_step(ctxt, ops) )                                   \
        max_reps = 1;                                                   \
    max_reps;                                                           \
})

static void __put_rep_prefix(
    struct cpu_user_regs *int_regs,
    struct cpu_user_regs *ext_regs,
    int ad_bytes,
    unsigned long reps_completed)
{
    unsigned long ecx = get_loop_count(int_regs, ad_bytes);

    /* Reduce counter appropriately, and repeat instruction if non-zero. */
    ecx -= reps_completed;
    if ( ecx != 0 )
        int_regs->r(ip) = ext_regs->r(ip);

    put_loop_count(int_regs, ad_bytes, ecx);
}

#define put_rep_prefix(reps_completed) ({                               \
    if ( rep_prefix() )                                                 \
    {                                                                   \
        __put_rep_prefix(&_regs, ctxt->regs, ad_bytes, reps_completed); \
        if ( unlikely(rc == X86EMUL_EXCEPTION) )                        \
            goto complete_insn;                                         \
    }                                                                   \
})

/* Clip maximum repetitions so that the index register at most just wraps. */
#define truncate_ea_and_reps(ea, reps, bytes_per_rep) ({                  \
    unsigned long todo__, ea__ = truncate_word(ea, ad_bytes);             \
    if ( !(_regs._eflags & EFLG_DF) )                                     \
        todo__ = truncate_word(-(ea), ad_bytes) / (bytes_per_rep);        \
    else if ( truncate_word((ea) + (bytes_per_rep) - 1, ad_bytes) < ea__ )\
        todo__ = 1;                                                       \
    else                                                                  \
        todo__ = ea__ / (bytes_per_rep) + 1;                              \
    if ( !todo__ )                                                        \
        (reps) = 1;                                                       \
    else if ( todo__ < (reps) )                                           \
        (reps) = todo__;                                                  \
    ea__;                                                                 \
})

/* Compatibility function: read guest memory, zero-extend result to a ulong. */
static int read_ulong(
        enum x86_segment seg,
        unsigned long offset,
        unsigned long *val,
        unsigned int bytes,
        struct x86_emulate_ctxt *ctxt,
        const struct x86_emulate_ops *ops)
{
    *val = 0;
    return ops->read(seg, offset, val, bytes, ctxt);
}

/*
 * Unsigned multiplication with double-word result.
 * IN:  Multiplicand=m[0], Multiplier=m[1]
 * OUT: Return CF/OF (overflow status); Result=m[1]:m[0]
 */
static bool mul_dbl(unsigned long m[2])
{
    bool rc;

    asm ( "mul %1" ASM_FLAG_OUT(, "; seto %2")
          : "+a" (m[0]), "+d" (m[1]), ASM_FLAG_OUT("=@cco", "=qm") (rc) );

    return rc;
}

/*
 * Signed multiplication with double-word result.
 * IN:  Multiplicand=m[0], Multiplier=m[1]
 * OUT: Return CF/OF (overflow status); Result=m[1]:m[0]
 */
static bool imul_dbl(unsigned long m[2])
{
    bool rc;

    asm ( "imul %1" ASM_FLAG_OUT(, "; seto %2")
          : "+a" (m[0]), "+d" (m[1]), ASM_FLAG_OUT("=@cco", "=qm") (rc) );

    return rc;
}

/*
 * Unsigned division of double-word dividend.
 * IN:  Dividend=u[1]:u[0], Divisor=v
 * OUT: Return 1: #DE
 *      Return 0: Quotient=u[0], Remainder=u[1]
 */
static bool div_dbl(unsigned long u[2], unsigned long v)
{
    if ( (v == 0) || (u[1] >= v) )
        return 1;
    asm ( "div"__OS" %2" : "+a" (u[0]), "+d" (u[1]) : "rm" (v) );
    return 0;
}

/*
 * Signed division of double-word dividend.
 * IN:  Dividend=u[1]:u[0], Divisor=v
 * OUT: Return 1: #DE
 *      Return 0: Quotient=u[0], Remainder=u[1]
 * NB. We don't use idiv directly as it's moderately hard to work out
 *     ahead of time whether it will #DE, which we cannot allow to happen.
 */
static bool idiv_dbl(unsigned long u[2], long v)
{
    bool negu = (long)u[1] < 0, negv = v < 0;

    /* u = abs(u) */
    if ( negu )
    {
        u[1] = ~u[1];
        if ( (u[0] = -u[0]) == 0 )
            u[1]++;
    }

    /* abs(u) / abs(v) */
    if ( div_dbl(u, negv ? -v : v) )
        return 1;

    /* Remainder has same sign as dividend. It cannot overflow. */
    if ( negu )
        u[1] = -u[1];

    /* Quotient is overflowed if sign bit is set. */
    if ( negu ^ negv )
    {
        if ( (long)u[0] >= 0 )
            u[0] = -u[0];
        else if ( (u[0] << 1) != 0 ) /* == 0x80...0 is okay */
            return 1;
    }
    else if ( (long)u[0] < 0 )
        return 1;

    return 0;
}

static bool
test_cc(
    unsigned int condition, unsigned int flags)
{
    int rc = 0;

    switch ( (condition & 15) >> 1 )
    {
    case 0: /* o */
        rc |= (flags & EFLG_OF);
        break;
    case 1: /* b/c/nae */
        rc |= (flags & EFLG_CF);
        break;
    case 2: /* z/e */
        rc |= (flags & EFLG_ZF);
        break;
    case 3: /* be/na */
        rc |= (flags & (EFLG_CF|EFLG_ZF));
        break;
    case 4: /* s */
        rc |= (flags & EFLG_SF);
        break;
    case 5: /* p/pe */
        rc |= (flags & EFLG_PF);
        break;
    case 7: /* le/ng */
        rc |= (flags & EFLG_ZF);
        /* fall through */
    case 6: /* l/nge */
        rc |= (!(flags & EFLG_SF) != !(flags & EFLG_OF));
        break;
    }

    /* Odd condition identifiers (lsb == 1) have inverted sense. */
    return (!!rc ^ (condition & 1));
}

static int
get_cpl(
    struct x86_emulate_ctxt *ctxt,
    const struct x86_emulate_ops  *ops)
{
    struct segment_register reg;

    if ( ctxt->regs->_eflags & EFLG_VM )
        return 3;

    if ( (ops->read_segment == NULL) ||
         ops->read_segment(x86_seg_ss, &reg, ctxt) )
        return -1;

    return reg.attr.fields.dpl;
}

static int
_mode_iopl(
    struct x86_emulate_ctxt *ctxt,
    const struct x86_emulate_ops  *ops)
{
    int cpl = get_cpl(ctxt, ops);
    if ( cpl == -1 )
        return -1;
    return cpl <= MASK_EXTR(ctxt->regs->_eflags, EFLG_IOPL);
}

#define mode_ring0() ({                         \
    int _cpl = get_cpl(ctxt, ops);              \
    fail_if(_cpl < 0);                          \
    (_cpl == 0);                                \
})
#define mode_iopl() ({                          \
    int _iopl = _mode_iopl(ctxt, ops);          \
    fail_if(_iopl < 0);                         \
    _iopl;                                      \
})
#define mode_vif() ({                                        \
    cr4 = 0;                                                 \
    if ( ops->read_cr && get_cpl(ctxt, ops) == 3 )           \
    {                                                        \
        rc = ops->read_cr(4, &cr4, ctxt);                    \
        if ( rc != X86EMUL_OKAY ) goto done;                 \
    }                                                        \
    !!(cr4 & (_regs._eflags & EFLG_VM ? CR4_VME : CR4_PVI)); \
})

static int ioport_access_check(
    unsigned int first_port,
    unsigned int bytes,
    struct x86_emulate_ctxt *ctxt,
    const struct x86_emulate_ops *ops)
{
    unsigned long iobmp;
    struct segment_register tr;
    int rc = X86EMUL_OKAY;

    if ( !(ctxt->regs->_eflags & EFLG_VM) && mode_iopl() )
        return X86EMUL_OKAY;

    fail_if(ops->read_segment == NULL);
    /*
     * X86EMUL_DONE coming back here may be used to defer the port
     * permission check to the respective ioport hook.
     */
    if ( (rc = ops->read_segment(x86_seg_tr, &tr, ctxt)) != 0 )
        return rc == X86EMUL_DONE ? X86EMUL_OKAY : rc;

    /* Ensure the TSS has an io-bitmap-offset field. */
    generate_exception_if(tr.attr.fields.type != 0xb, EXC_GP, 0);

    switch ( rc = read_ulong(x86_seg_tr, 0x66, &iobmp, 2, ctxt, ops) )
    {
    case X86EMUL_OKAY:
        break;

    case X86EMUL_EXCEPTION:
        generate_exception_if(!ctxt->event_pending, EXC_GP, 0);
        /* fallthrough */

    default:
        return rc;
    }

    /* Read two bytes including byte containing first port. */
    switch ( rc = read_ulong(x86_seg_tr, iobmp + first_port / 8,
                             &iobmp, 2, ctxt, ops) )
    {
    case X86EMUL_OKAY:
        break;

    case X86EMUL_EXCEPTION:
        generate_exception_if(!ctxt->event_pending, EXC_GP, 0);
        /* fallthrough */

    default:
        return rc;
    }

    generate_exception_if(iobmp & (((1 << bytes) - 1) << (first_port & 7)),
                          EXC_GP, 0);

 done:
    return rc;
}

static bool
in_realmode(
    struct x86_emulate_ctxt *ctxt,
    const struct x86_emulate_ops  *ops)
{
    unsigned long cr0;
    int rc;

    if ( ops->read_cr == NULL )
        return 0;

    rc = ops->read_cr(0, &cr0, ctxt);
    return (!rc && !(cr0 & CR0_PE));
}

static bool
in_protmode(
    struct x86_emulate_ctxt *ctxt,
    const struct x86_emulate_ops  *ops)
{
    return !(in_realmode(ctxt, ops) || (ctxt->regs->_eflags & EFLG_VM));
}

#define EAX 0
#define ECX 1
#define EDX 2
#define EBX 3

static bool vcpu_has(
    unsigned int eax,
    unsigned int reg,
    unsigned int bit,
    struct x86_emulate_ctxt *ctxt,
    const struct x86_emulate_ops *ops)
{
    struct cpuid_leaf res;
    int rc = X86EMUL_OKAY;

    fail_if(!ops->cpuid);
    rc = ops->cpuid(eax, 0, &res, ctxt);
    if ( rc == X86EMUL_OKAY )
    {
        switch ( reg )
        {
        case EAX: reg = res.a; break;
        case EBX: reg = res.b; break;
        case ECX: reg = res.c; break;
        case EDX: reg = res.d; break;
        default: BUG();
        }
        if ( !(reg & (1U << bit)) )
            rc = ~X86EMUL_OKAY;
    }

 done:
    return rc == X86EMUL_OKAY;
}

#define vcpu_has_fpu()         vcpu_has(         1, EDX,  0, ctxt, ops)
#define vcpu_has_sep()         vcpu_has(         1, EDX, 11, ctxt, ops)
#define vcpu_has_cx8()         vcpu_has(         1, EDX,  8, ctxt, ops)
#define vcpu_has_cmov()        vcpu_has(         1, EDX, 15, ctxt, ops)
#define vcpu_has_clflush()     vcpu_has(         1, EDX, 19, ctxt, ops)
#define vcpu_has_mmx()         vcpu_has(         1, EDX, 23, ctxt, ops)
#define vcpu_has_sse()         vcpu_has(         1, EDX, 25, ctxt, ops)
#define vcpu_has_sse2()        vcpu_has(         1, EDX, 26, ctxt, ops)
#define vcpu_has_sse3()        vcpu_has(         1, ECX,  0, ctxt, ops)
#define vcpu_has_cx16()        vcpu_has(         1, ECX, 13, ctxt, ops)
#define vcpu_has_sse4_2()      vcpu_has(         1, ECX, 20, ctxt, ops)
#define vcpu_has_movbe()       vcpu_has(         1, ECX, 22, ctxt, ops)
#define vcpu_has_popcnt()      vcpu_has(         1, ECX, 23, ctxt, ops)
#define vcpu_has_avx()         vcpu_has(         1, ECX, 28, ctxt, ops)
#define vcpu_has_rdrand()      vcpu_has(         1, ECX, 30, ctxt, ops)
#define vcpu_has_lahf_lm()     vcpu_has(0x80000001, ECX,  0, ctxt, ops)
#define vcpu_has_cr8_legacy()  vcpu_has(0x80000001, ECX,  4, ctxt, ops)
#define vcpu_has_lzcnt()       vcpu_has(0x80000001, ECX,  5, ctxt, ops)
#define vcpu_has_misalignsse() vcpu_has(0x80000001, ECX,  7, ctxt, ops)
#define vcpu_has_tbm()         vcpu_has(0x80000001, ECX, 21, ctxt, ops)
#define vcpu_has_bmi1()        vcpu_has(         7, EBX,  3, ctxt, ops)
#define vcpu_has_hle()         vcpu_has(         7, EBX,  4, ctxt, ops)
#define vcpu_has_bmi2()        vcpu_has(         7, EBX,  8, ctxt, ops)
#define vcpu_has_rtm()         vcpu_has(         7, EBX, 11, ctxt, ops)
#define vcpu_has_mpx()         vcpu_has(         7, EBX, 14, ctxt, ops)
#define vcpu_has_rdseed()      vcpu_has(         7, EBX, 18, ctxt, ops)
#define vcpu_has_adx()         vcpu_has(         7, EBX, 19, ctxt, ops)
#define vcpu_has_smap()        vcpu_has(         7, EBX, 20, ctxt, ops)
#define vcpu_has_clflushopt()  vcpu_has(         7, EBX, 23, ctxt, ops)
#define vcpu_has_clwb()        vcpu_has(         7, EBX, 24, ctxt, ops)
#define vcpu_has_rdpid()       vcpu_has(         7, ECX, 22, ctxt, ops)

#define vcpu_must_have(feat) \
    generate_exception_if(!vcpu_has_##feat(), EXC_UD)

#ifdef __XEN__
/*
 * Note the difference between vcpu_must_have(<feature>) and
 * host_and_vcpu_must_have(<feature>): The latter needs to be used when
 * emulation code is using the same instruction class for carrying out
 * the actual operation.
 */
#define host_and_vcpu_must_have(feat) ({ \
    generate_exception_if(!cpu_has_##feat, EXC_UD); \
    vcpu_must_have(feat); \
})
#else
/*
 * For the test harness both are fine to be used interchangeably, i.e.
 * features known to always be available (e.g. SSE/SSE2) to (64-bit) Xen
 * may be checked for by just vcpu_must_have().
 */
#define host_and_vcpu_must_have(feat) vcpu_must_have(feat)
#endif

static int
in_longmode(
    struct x86_emulate_ctxt *ctxt,
    const struct x86_emulate_ops *ops)
{
    uint64_t efer;

    if ( !ops->read_msr ||
         unlikely(ops->read_msr(MSR_EFER, &efer, ctxt) != X86EMUL_OKAY) )
        return -1;

    return !!(efer & EFER_LMA);
}

static int
realmode_load_seg(
    enum x86_segment seg,
    uint16_t sel,
    struct segment_register *sreg,
    struct x86_emulate_ctxt *ctxt,
    const struct x86_emulate_ops *ops)
{
    int rc;

    if ( !ops->read_segment )
        return X86EMUL_UNHANDLEABLE;

    if ( (rc = ops->read_segment(seg, sreg, ctxt)) == X86EMUL_OKAY )
    {
        sreg->sel  = sel;
        sreg->base = (uint32_t)sel << 4;
    }

    return rc;
}

/*
 * Passing in x86_seg_none means
 * - suppress any exceptions other than #PF,
 * - don't commit any state.
 */
static int
protmode_load_seg(
    enum x86_segment seg,
    uint16_t sel, bool is_ret,
    struct segment_register *sreg,
    struct x86_emulate_ctxt *ctxt,
    const struct x86_emulate_ops *ops)
{
    enum x86_segment sel_seg = (sel & 4) ? x86_seg_ldtr : x86_seg_gdtr;
    struct { uint32_t a, b; } desc, desc_hi = {};
    uint8_t dpl, rpl;
    int cpl = get_cpl(ctxt, ops);
    uint32_t a_flag = 0x100;
    int rc, fault_type = EXC_GP;

    if ( cpl < 0 )
        return X86EMUL_UNHANDLEABLE;

    /* NULL selector? */
    if ( (sel & 0xfffc) == 0 )
    {
        switch ( seg )
        {
        case x86_seg_ss:
            if ( mode_64bit() && (cpl != 3) && (cpl == sel) )
        default:
                break;
            /* fall through */
        case x86_seg_cs:
        case x86_seg_tr:
            goto raise_exn;
        }
        if ( ctxt->vendor != X86_VENDOR_AMD || !ops->read_segment ||
             ops->read_segment(seg, sreg, ctxt) != X86EMUL_OKAY )
            memset(sreg, 0, sizeof(*sreg));
        else
            sreg->attr.bytes = 0;
        sreg->sel = sel;

        /* Since CPL == SS.DPL, we need to put back DPL. */
        if ( seg == x86_seg_ss )
            sreg->attr.fields.dpl = sel;

        return X86EMUL_OKAY;
    }

    /* System segment descriptors must reside in the GDT. */
    if ( is_x86_system_segment(seg) && (sel & 4) )
        goto raise_exn;

    switch ( rc = ops->read(sel_seg, sel & 0xfff8, &desc, sizeof(desc), ctxt) )
    {
    case X86EMUL_OKAY:
        break;

    case X86EMUL_EXCEPTION:
        if ( !ctxt->event_pending )
            goto raise_exn;
        /* fallthrough */

    default:
        return rc;
    }

    /* System segments must have S flag == 0. */
    if ( is_x86_system_segment(seg) && (desc.b & (1u << 12)) )
        goto raise_exn;
    /* User segments must have S flag == 1. */
    if ( is_x86_user_segment(seg) && !(desc.b & (1u << 12)) )
        goto raise_exn;

    dpl = (desc.b >> 13) & 3;
    rpl = sel & 3;

    switch ( seg )
    {
    case x86_seg_cs:
        /* Code segment? */
        if ( !(desc.b & (1u<<11)) )
            goto raise_exn;
        if ( is_ret
             ? /*
                * Really rpl < cpl, but our sole caller doesn't handle
                * privilege level changes.
                */
               rpl != cpl || (desc.b & (1 << 10) ? dpl > rpl : dpl != rpl)
             : desc.b & (1 << 10)
               /* Conforming segment: check DPL against CPL. */
               ? dpl > cpl
               /* Non-conforming segment: check RPL and DPL against CPL. */
               : rpl > cpl || dpl != cpl )
            goto raise_exn;
        /*
         * 64-bit code segments (L bit set) must have D bit clear.
         * Experimentally in long mode, the L and D bits are checked before
         * the Present bit.
         */
        if ( in_longmode(ctxt, ops) &&
             (desc.b & (1 << 21)) && (desc.b & (1 << 22)) )
            goto raise_exn;
        sel = (sel ^ rpl) | cpl;
        break;
    case x86_seg_ss:
        /* Writable data segment? */
        if ( (desc.b & (5u<<9)) != (1u<<9) )
            goto raise_exn;
        if ( (dpl != cpl) || (dpl != rpl) )
            goto raise_exn;
        break;
    case x86_seg_ldtr:
        /* LDT system segment? */
        if ( (desc.b & (15u<<8)) != (2u<<8) )
            goto raise_exn;
        a_flag = 0;
        break;
    case x86_seg_tr:
        /* Available TSS system segment? */
        if ( (desc.b & (15u<<8)) != (9u<<8) )
            goto raise_exn;
        a_flag = 0x200; /* busy flag */
        break;
    default:
        /* Readable code or data segment? */
        if ( (desc.b & (5u<<9)) == (4u<<9) )
            goto raise_exn;
        /* Non-conforming segment: check DPL against RPL and CPL. */
        if ( ((desc.b & (6u<<9)) != (6u<<9)) &&
             ((dpl < cpl) || (dpl < rpl)) )
            goto raise_exn;
        break;
    case x86_seg_none:
        /* Non-conforming segment: check DPL against RPL and CPL. */
        if ( ((desc.b & (0x1c << 8)) != (0x1c << 8)) &&
             ((dpl < cpl) || (dpl < rpl)) )
            return X86EMUL_EXCEPTION;
        a_flag = 0;
        break;
    }

    /* Segment present in memory? */
    if ( !(desc.b & (1 << 15)) && seg != x86_seg_none )
    {
        fault_type = seg != x86_seg_ss ? EXC_NP : EXC_SS;
        goto raise_exn;
    }

    if ( !is_x86_user_segment(seg) )
    {
        int lm = (desc.b & (1u << 12)) ? 0 : in_longmode(ctxt, ops);

        if ( lm < 0 )
            return X86EMUL_UNHANDLEABLE;
        if ( lm )
        {
            switch ( rc = ops->read(sel_seg, (sel & 0xfff8) + 8,
                                    &desc_hi, sizeof(desc_hi), ctxt) )
            {
            case X86EMUL_OKAY:
                break;

            case X86EMUL_EXCEPTION:
                if ( !ctxt->event_pending )
                    goto raise_exn;
                /* fall through */
            default:
                return rc;
            }
            if ( (desc_hi.b & 0x00001f00) ||
                 (seg != x86_seg_none &&
                  !is_canonical_address((uint64_t)desc_hi.a << 32)) )
                goto raise_exn;
        }
    }

    /* Ensure Accessed flag is set. */
    if ( a_flag && !(desc.b & a_flag) )
    {
        uint32_t new_desc_b = desc.b | a_flag;

        fail_if(!ops->cmpxchg);
        switch ( (rc = ops->cmpxchg(sel_seg, (sel & 0xfff8) + 4, &desc.b,
                                    &new_desc_b, sizeof(desc.b), ctxt)) )
        {
        case X86EMUL_OKAY:
            break;

        case X86EMUL_EXCEPTION:
            if ( !ctxt->event_pending )
                goto raise_exn;
            /* fallthrough */

        default:
            return rc;
        }

        /* Force the Accessed flag in our local copy. */
        desc.b = new_desc_b;
    }

    sreg->base = (((uint64_t)desc_hi.a << 32) |
                  ((desc.b <<  0) & 0xff000000u) |
                  ((desc.b << 16) & 0x00ff0000u) |
                  ((desc.a >> 16) & 0x0000ffffu));
    sreg->attr.bytes = (((desc.b >>  8) & 0x00ffu) |
                        ((desc.b >> 12) & 0x0f00u));
    sreg->limit = (desc.b & 0x000f0000u) | (desc.a & 0x0000ffffu);
    if ( sreg->attr.fields.g )
        sreg->limit = (sreg->limit << 12) | 0xfffu;
    sreg->sel = sel;
    return X86EMUL_OKAY;

 raise_exn:
    generate_exception_if(seg != x86_seg_none, fault_type, sel & 0xfffc);
    rc = X86EMUL_EXCEPTION;
 done:
    return rc;
}

static int
load_seg(
    enum x86_segment seg,
    uint16_t sel, bool is_ret,
    struct segment_register *sreg,
    struct x86_emulate_ctxt *ctxt,
    const struct x86_emulate_ops *ops)
{
    struct segment_register reg;
    int rc;

    if ( !ops->write_segment )
        return X86EMUL_UNHANDLEABLE;

    if ( !sreg )
        sreg = &reg;

    if ( in_protmode(ctxt, ops) )
        rc = protmode_load_seg(seg, sel, is_ret, sreg, ctxt, ops);
    else
        rc = realmode_load_seg(seg, sel, sreg, ctxt, ops);

    if ( !rc && sreg == &reg )
        rc = ops->write_segment(seg, sreg, ctxt);

    return rc;
}

void *
decode_register(
    uint8_t modrm_reg, struct cpu_user_regs *regs, int highbyte_regs)
{
    void *p;

    switch ( modrm_reg )
    {
    case  0: p = &regs->r(ax); break;
    case  1: p = &regs->r(cx); break;
    case  2: p = &regs->r(dx); break;
    case  3: p = &regs->r(bx); break;
    case  4: p = (highbyte_regs ? &regs->ah : (void *)&regs->r(sp)); break;
    case  5: p = (highbyte_regs ? &regs->ch : (void *)&regs->r(bp)); break;
    case  6: p = (highbyte_regs ? &regs->dh : (void *)&regs->r(si)); break;
    case  7: p = (highbyte_regs ? &regs->bh : (void *)&regs->r(di)); break;
#if defined(__x86_64__)
    case  8: p = &regs->r8;  break;
    case  9: p = &regs->r9;  break;
    case 10: p = &regs->r10; break;
    case 11: p = &regs->r11; break;
    case 12: mark_regs_dirty(regs); p = &regs->r12; break;
    case 13: mark_regs_dirty(regs); p = &regs->r13; break;
    case 14: mark_regs_dirty(regs); p = &regs->r14; break;
    case 15: mark_regs_dirty(regs); p = &regs->r15; break;
#endif
    default: BUG(); p = NULL; break;
    }

    return p;
}

static void *decode_vex_gpr(unsigned int vex_reg, struct cpu_user_regs *regs,
                            const struct x86_emulate_ctxt *ctxt)
{
    return decode_register(~vex_reg & (mode_64bit() ? 0xf : 7), regs, 0);
}

static bool is_aligned(enum x86_segment seg, unsigned long offs,
                       unsigned int size, struct x86_emulate_ctxt *ctxt,
                       const struct x86_emulate_ops *ops)
{
    struct segment_register reg;

    /* Expecting powers of two only. */
    ASSERT(!(size & (size - 1)));

    if ( mode_64bit() && seg < x86_seg_fs )
        memset(&reg, 0, sizeof(reg));
    else
    {
        /* No alignment checking when we have no way to read segment data. */
        if ( !ops->read_segment )
            return true;

        if ( ops->read_segment(seg, &reg, ctxt) != X86EMUL_OKAY )
            return false;
    }

    return !((reg.base + offs) & (size - 1));
}

static bool is_branch_step(struct x86_emulate_ctxt *ctxt,
                           const struct x86_emulate_ops *ops)
{
    uint64_t debugctl;

    return ops->read_msr &&
           ops->read_msr(MSR_DEBUGCTL, &debugctl, ctxt) == X86EMUL_OKAY &&
           (debugctl & DEBUGCTL_BTF);
}

static bool umip_active(struct x86_emulate_ctxt *ctxt,
                        const struct x86_emulate_ops *ops)
{
    unsigned long cr4;

    /* Intentionally not using mode_ring0() here to avoid its fail_if(). */
    return get_cpl(ctxt, ops) > 0 &&
           ops->read_cr && ops->read_cr(4, &cr4, ctxt) == X86EMUL_OKAY &&
           (cr4 & CR4_UMIP);
}

/* Inject a software interrupt/exception, emulating if needed. */
static int inject_swint(enum x86_swint_type type,
                        uint8_t vector, uint8_t insn_len,
                        struct x86_emulate_ctxt *ctxt,
                        const struct x86_emulate_ops *ops)
{
    int rc, error_code, fault_type = EXC_GP;

    /*
     * Without hardware support, injecting software interrupts/exceptions is
     * problematic.
     *
     * All software methods of generating exceptions (other than BOUND) yield
     * traps, so eip in the exception frame needs to point after the
     * instruction, not at it.
     *
     * However, if injecting it as a hardware exception causes a fault during
     * delivery, our adjustment of eip will cause the fault to be reported
     * after the faulting instruction, not pointing to it.
     *
     * Therefore, eip can only safely be wound forwards if we are certain that
     * injecting an equivalent hardware exception won't fault, which means
     * emulating everything the processor would do on a control transfer.
     *
     * However, emulation of complete control transfers is very complicated.
     * All we care about is that guest userspace cannot avoid the descriptor
     * DPL check by using the Xen emulator, and successfully invoke DPL=0
     * descriptors.
     *
     * Any OS which would further fault during injection is going to receive a
     * double fault anyway, and won't be in a position to care that the
     * faulting eip is incorrect.
     */

    if ( (ctxt->swint_emulate == x86_swint_emulate_all) ||
         ((ctxt->swint_emulate == x86_swint_emulate_icebp) &&
          (type == x86_swint_icebp)) )
    {
        if ( !in_realmode(ctxt, ops) )
        {
            unsigned int idte_size, idte_offset;
            struct { uint32_t a, b, c, d; } idte = {};
            int lm = in_longmode(ctxt, ops);

            if ( lm < 0 )
                return X86EMUL_UNHANDLEABLE;

            idte_size = lm ? 16 : 8;
            idte_offset = vector * idte_size;

            /* icebp sets the External Event bit despite being an instruction. */
            error_code = (vector << 3) | ECODE_IDT |
                (type == x86_swint_icebp ? ECODE_EXT : 0);

            /*
             * TODO - this does not cover the v8086 mode with CR4.VME case
             * correctly, but falls on the safe side from the point of view of
             * a 32bit OS.  Someone with many TUITs can see about reading the
             * TSS Software Interrupt Redirection bitmap.
             */
            if ( (ctxt->regs->_eflags & EFLG_VM) &&
                 ((ctxt->regs->_eflags & EFLG_IOPL) != EFLG_IOPL) )
                goto raise_exn;

            /*
             * Read all 8/16 bytes so the idtr limit check is applied properly
             * to this entry, even though we only end up looking at the 2nd
             * word.
             */
            switch ( rc = ops->read(x86_seg_idtr, idte_offset,
                                    &idte, idte_size, ctxt) )
            {
            case X86EMUL_OKAY:
                break;

            case X86EMUL_EXCEPTION:
                if ( !ctxt->event_pending )
                    goto raise_exn;
                /* fallthrough */

            default:
                return rc;
            }

            /* This must be an interrupt, trap, or task gate. */
#ifdef __XEN__
            switch ( (idte.b >> 8) & 0x1f )
            {
            case SYS_DESC_irq_gate:
            case SYS_DESC_trap_gate:
                break;
            case SYS_DESC_irq_gate16:
            case SYS_DESC_trap_gate16:
            case SYS_DESC_task_gate:
                if ( !lm )
                    break;
                /* fall through */
            default:
                goto raise_exn;
            }
#endif

            /* The 64-bit high half's type must be zero. */
            if ( idte.d & 0x1f00 )
                goto raise_exn;

            /* icebp counts as a hardware event, and bypasses the dpl check. */
            if ( type != x86_swint_icebp )
            {
                int cpl = get_cpl(ctxt, ops);

                fail_if(cpl < 0);

                if ( cpl > ((idte.b >> 13) & 3) )
                    goto raise_exn;
            }

            /* Is this entry present? */
            if ( !(idte.b & (1u << 15)) )
            {
                fault_type = EXC_NP;
                goto raise_exn;
            }
        }
    }

    x86_emul_software_event(type, vector, insn_len, ctxt);
    rc = X86EMUL_OKAY;

 done:
    return rc;

 raise_exn:
    generate_exception(fault_type, error_code);
}

static void adjust_bnd(struct x86_emulate_ctxt *ctxt,
                       const struct x86_emulate_ops *ops, enum vex_pfx pfx)
{
    uint64_t bndcfg;
    int rc;

    if ( pfx == vex_f2 || !cpu_has_mpx || !vcpu_has_mpx() )
        return;

    if ( !mode_ring0() )
        bndcfg = read_bndcfgu();
    else if ( !ops->read_msr ||
              ops->read_msr(MSR_BNDCFGS, &bndcfg, ctxt) != X86EMUL_OKAY )
        return;
    if ( (bndcfg & BNDCFG_ENABLE) && !(bndcfg & BNDCFG_PRESERVE) )
    {
        /*
         * Using BNDMK or any other MPX instruction here is pointless, as
         * we run with MPX disabled ourselves, and hence they're all no-ops.
         * Therefore we have two ways to clear BNDn: Enable MPX temporarily
         * (in which case executing any suitable non-prefixed branch
         * instruction would do), or use XRSTOR.
         */
        xstate_set_init(XSTATE_BNDREGS);
    }
 done:;
}

int x86emul_unhandleable_rw(
    enum x86_segment seg,
    unsigned long offset,
    void *p_data,
    unsigned int bytes,
    struct x86_emulate_ctxt *ctxt)
{
    return X86EMUL_UNHANDLEABLE;
}

struct x86_emulate_state {
    unsigned int op_bytes, ad_bytes;

    enum {
        ext_none = vex_none,
        ext_0f   = vex_0f,
        ext_0f38 = vex_0f38,
        ext_0f3a = vex_0f3a,
        /*
         * For XOP use values such that the respective instruction field
         * can be used without adjustment.
         */
        ext_8f08 = 8,
        ext_8f09,
        ext_8f0a,
    } ext;
    uint8_t modrm, modrm_mod, modrm_reg, modrm_rm;
    uint8_t rex_prefix;
    bool lock_prefix;
    bool not_64bit; /* Instruction not available in 64bit. */
    opcode_desc_t desc;
    union vex vex;
    union evex evex;

    /*
     * Data operand effective address (usually computed from ModRM).
     * Default is a memory operand relative to segment DS.
     */
    struct operand ea;

    /* Immediate operand values, if any. Use otherwise unused fields. */
#define imm1 ea.val
#define imm2 ea.orig_val

    unsigned long ip;
    struct cpu_user_regs *regs;

#ifndef NDEBUG
    /*
     * Track caller of x86_decode_insn() to spot missing as well as
     * premature calls to x86_emulate_free_state().
     */
    void *caller;
#endif
};

/* Helper definitions. */
#define op_bytes (state->op_bytes)
#define ad_bytes (state->ad_bytes)
#define ext (state->ext)
#define modrm (state->modrm)
#define modrm_mod (state->modrm_mod)
#define modrm_reg (state->modrm_reg)
#define modrm_rm (state->modrm_rm)
#define rex_prefix (state->rex_prefix)
#define lock_prefix (state->lock_prefix)
#define vex (state->vex)
#define evex (state->evex)
#define ea (state->ea)

static int
x86_decode_onebyte(
    struct x86_emulate_state *state,
    struct x86_emulate_ctxt *ctxt,
    const struct x86_emulate_ops *ops)
{
    int rc = X86EMUL_OKAY;

    switch ( ctxt->opcode )
    {
    case 0x06: /* push %%es */
    case 0x07: /* pop %%es */
    case 0x0e: /* push %%cs */
    case 0x16: /* push %%ss */
    case 0x17: /* pop %%ss */
    case 0x1e: /* push %%ds */
    case 0x1f: /* pop %%ds */
    case 0x27: /* daa */
    case 0x2f: /* das */
    case 0x37: /* aaa */
    case 0x3f: /* aas */
    case 0x60: /* pusha */
    case 0x61: /* popa */
    case 0x62: /* bound */
    case 0x82: /* Grp1 (x86/32 only) */
    case 0xc4: /* les */
    case 0xc5: /* lds */
    case 0xce: /* into */
    case 0xd4: /* aam */
    case 0xd5: /* aad */
    case 0xd6: /* salc */
        state->not_64bit = true;
        break;

    case 0x90: /* nop / pause */
        if ( repe_prefix() )
            ctxt->opcode |= X86EMUL_OPC_F3(0, 0);
        break;

    case 0x9a: /* call (far, absolute) */
    case 0xea: /* jmp (far, absolute) */
        generate_exception_if(mode_64bit(), EXC_UD);

        imm1 = insn_fetch_bytes(op_bytes);
        imm2 = insn_fetch_type(uint16_t);
        break;

    case 0xa0: case 0xa1: /* mov mem.offs,{%al,%ax,%eax,%rax} */
    case 0xa2: case 0xa3: /* mov {%al,%ax,%eax,%rax},mem.offs */
        /* Source EA is not encoded via ModRM. */
        ea.type = OP_MEM;
        ea.mem.off = insn_fetch_bytes(ad_bytes);
        break;

    case 0xb8 ... 0xbf: /* mov imm{16,32,64},r{16,32,64} */
        if ( op_bytes == 8 ) /* Fetch more bytes to obtain imm64. */
            imm1 = ((uint32_t)imm1 |
                    ((uint64_t)insn_fetch_type(uint32_t) << 32));
        break;

    case 0xc8: /* enter imm16,imm8 */
        imm2 = insn_fetch_type(uint8_t);
        break;

    case 0xff: /* Grp5 */
        switch ( modrm_reg & 7 )
        {
        case 2: /* call (near) */
        case 4: /* jmp (near) */
        case 6: /* push */
            if ( mode_64bit() && op_bytes == 4 )
                op_bytes = 8;
            /* fall through */
        case 3: /* call (far, absolute indirect) */
        case 5: /* jmp (far, absolute indirect) */
            state->desc = DstNone | SrcMem | ModRM | Mov;
            break;
        }
        break;
    }

 done:
    return rc;
}

static int
x86_decode_twobyte(
    struct x86_emulate_state *state,
    struct x86_emulate_ctxt *ctxt,
    const struct x86_emulate_ops *ops)
{
    int rc = X86EMUL_OKAY;

    switch ( ctxt->opcode & X86EMUL_OPC_MASK )
    {
    case 0x00: /* Grp6 */
        switch ( modrm_reg & 6 )
        {
        case 0:
            state->desc |= DstMem | SrcImplicit | Mov;
            break;
        case 2: case 4:
            state->desc |= SrcMem16;
            break;
        }
        break;

    case 0x78:
        switch ( vex.pfx )
        {
        case vex_66: /* extrq $imm8, $imm8, xmm */
        case vex_f2: /* insertq $imm8, $imm8, xmm, xmm */
            imm1 = insn_fetch_type(uint8_t);
            imm2 = insn_fetch_type(uint8_t);
            break;
        }
        /* fall through */
    case 0x10 ... 0x18:
    case 0x28 ... 0x2f:
    case 0x50 ... 0x77:
    case 0x79 ... 0x7f:
    case 0xae:
    case 0xc2 ... 0xc6:
    case 0xd0 ... 0xfe:
        ctxt->opcode |= MASK_INSR(vex.pfx, X86EMUL_OPC_PFX_MASK);
        break;

    case 0x20: case 0x22: /* mov to/from cr */
        if ( lock_prefix && vcpu_has_cr8_legacy() )
        {
            modrm_reg += 8;
            lock_prefix = false;
        }
        /* fall through */
    case 0x21: case 0x23: /* mov to/from dr */
        generate_exception_if(lock_prefix || ea.type != OP_REG, EXC_UD);
        op_bytes = mode_64bit() ? 8 : 4;
        break;

    case 0xb8: /* jmpe / popcnt */
        if ( rep_prefix() )
            ctxt->opcode |= MASK_INSR(vex.pfx, X86EMUL_OPC_PFX_MASK);
        break;

        /* Intentionally not handling here despite being modified by F3:
    case 0xbc: bsf / tzcnt
    case 0xbd: bsr / lzcnt
         * They're being dealt with in the execution phase (if at all).
         */
    }

 done:
    return rc;
}

static int
x86_decode_0f38(
    struct x86_emulate_state *state,
    struct x86_emulate_ctxt *ctxt,
    const struct x86_emulate_ops *ops)
{
    switch ( ctxt->opcode & X86EMUL_OPC_MASK )
    {
    case 0x00 ... 0xef:
    case 0xf2 ... 0xff:
        ctxt->opcode |= MASK_INSR(vex.pfx, X86EMUL_OPC_PFX_MASK);
        break;

    case 0xf0: /* movbe / crc32 */
        state->desc |= repne_prefix() ? ByteOp : Mov;
        if ( rep_prefix() )
            ctxt->opcode |= MASK_INSR(vex.pfx, X86EMUL_OPC_PFX_MASK);
        break;

    case 0xf1: /* movbe / crc32 */
        if ( !repne_prefix() )
            state->desc = (state->desc & ~(DstMask | SrcMask)) | DstMem | SrcReg | Mov;
        if ( rep_prefix() )
            ctxt->opcode |= MASK_INSR(vex.pfx, X86EMUL_OPC_PFX_MASK);
        break;
    }

    return X86EMUL_OKAY;
}

static int
x86_decode(
    struct x86_emulate_state *state,
    struct x86_emulate_ctxt *ctxt,
    const struct x86_emulate_ops  *ops)
{
    uint8_t b, d, sib, sib_index, sib_base;
    unsigned int def_op_bytes, def_ad_bytes, opcode;
    enum x86_segment override_seg = x86_seg_none;
    bool pc_rel = false;
    int rc = X86EMUL_OKAY;

    ASSERT(ops->insn_fetch);

    memset(state, 0, sizeof(*state));
    ea.type = OP_NONE;
    ea.mem.seg = x86_seg_ds;
    ea.reg = PTR_POISON;
    state->regs = ctxt->regs;
    state->ip = ctxt->regs->r(ip);

    /* Initialise output state in x86_emulate_ctxt */
    ctxt->retire.raw = 0;
    x86_emul_reset_event(ctxt);

    op_bytes = def_op_bytes = ad_bytes = def_ad_bytes = ctxt->addr_size/8;
    if ( op_bytes == 8 )
    {
        op_bytes = def_op_bytes = 4;
#ifndef __x86_64__
        return X86EMUL_UNHANDLEABLE;
#endif
    }

    /* Prefix bytes. */
    for ( ; ; )
    {
        switch ( b = insn_fetch_type(uint8_t) )
        {
        case 0x66: /* operand-size override */
            op_bytes = def_op_bytes ^ 6;
            if ( !vex.pfx )
                vex.pfx = vex_66;
            break;
        case 0x67: /* address-size override */
            ad_bytes = def_ad_bytes ^ (mode_64bit() ? 12 : 6);
            break;
        case 0x2e: /* CS override */
            override_seg = x86_seg_cs;
            break;
        case 0x3e: /* DS override */
            override_seg = x86_seg_ds;
            break;
        case 0x26: /* ES override */
            override_seg = x86_seg_es;
            break;
        case 0x64: /* FS override */
            override_seg = x86_seg_fs;
            break;
        case 0x65: /* GS override */
            override_seg = x86_seg_gs;
            break;
        case 0x36: /* SS override */
            override_seg = x86_seg_ss;
            break;
        case 0xf0: /* LOCK */
            lock_prefix = 1;
            break;
        case 0xf2: /* REPNE/REPNZ */
            vex.pfx = vex_f2;
            break;
        case 0xf3: /* REP/REPE/REPZ */
            vex.pfx = vex_f3;
            break;
        case 0x40 ... 0x4f: /* REX */
            if ( !mode_64bit() )
                goto done_prefixes;
            rex_prefix = b;
            continue;
        default:
            goto done_prefixes;
        }

        /* Any legacy prefix after a REX prefix nullifies its effect. */
        rex_prefix = 0;
    }
 done_prefixes:

    if ( rex_prefix & REX_W )
        op_bytes = 8;

    /* Opcode byte(s). */
    d = opcode_table[b];
    if ( d == 0 && b == 0x0f )
    {
        /* Two-byte opcode. */
        b = insn_fetch_type(uint8_t);
        d = twobyte_table[b];
        switch ( b )
        {
        default:
            opcode = b | MASK_INSR(0x0f, X86EMUL_OPC_EXT_MASK);
            ext = ext_0f;
            break;
        case 0x38:
            b = insn_fetch_type(uint8_t);
            opcode = b | MASK_INSR(0x0f38, X86EMUL_OPC_EXT_MASK);
            ext = ext_0f38;
            break;
        case 0x3a:
            b = insn_fetch_type(uint8_t);
            opcode = b | MASK_INSR(0x0f3a, X86EMUL_OPC_EXT_MASK);
            ext = ext_0f3a;
            break;
        }
    }
    else
        opcode = b;

    /* ModRM and SIB bytes. */
    if ( d & ModRM )
    {
        modrm = insn_fetch_type(uint8_t);
        modrm_mod = (modrm & 0xc0) >> 6;

        if ( !ext && ((b & ~1) == 0xc4 || (b == 0x8f && (modrm & 0x18)) ||
                      b == 0x62) )
            switch ( def_ad_bytes )
            {
            default:
                BUG(); /* Shouldn't be possible. */
            case 2:
                if ( in_realmode(ctxt, ops) || (state->regs->_eflags & EFLG_VM) )
                    break;
                /* fall through */
            case 4:
                if ( modrm_mod != 3 )
                    break;
                /* fall through */
            case 8:
                /* VEX / XOP / EVEX */
                generate_exception_if(rex_prefix || vex.pfx, EXC_UD);

                vex.raw[0] = modrm;
                if ( b == 0xc5 )
                {
                    opcode = X86EMUL_OPC_VEX_;
                    vex.raw[1] = modrm;
                    vex.opcx = vex_0f;
                    vex.x = 1;
                    vex.b = 1;
                    vex.w = 0;
                }
                else
                {
                    vex.raw[1] = insn_fetch_type(uint8_t);
                    if ( mode_64bit() )
                    {
                        if ( !vex.b )
                            rex_prefix |= REX_B;
                        if ( !vex.x )
                            rex_prefix |= REX_X;
                        if ( vex.w )
                        {
                            rex_prefix |= REX_W;
                            op_bytes = 8;
                        }
                    }
                    else
                    {
                        ASSERT(op_bytes == 4);
                        vex.b = 1;
                    }
                    switch ( b )
                    {
                    case 0x62:
                        opcode = X86EMUL_OPC_EVEX_;
                        evex.raw[0] = vex.raw[0];
                        evex.raw[1] = vex.raw[1];
                        evex.raw[2] = insn_fetch_type(uint8_t);

                        vex.opcx = evex.opcx;
                        break;
                    case 0xc4:
                        opcode = X86EMUL_OPC_VEX_;
                        break;
                    default:
                        opcode = 0;
                        break;
                    }
                }
                if ( !vex.r )
                    rex_prefix |= REX_R;

                ext = vex.opcx;
                if ( b != 0x8f )
                {
                    b = insn_fetch_type(uint8_t);
                    switch ( ext )
                    {
                    case vex_0f:
                        opcode |= MASK_INSR(0x0f, X86EMUL_OPC_EXT_MASK);
                        d = twobyte_table[b];
                        break;
                    case vex_0f38:
                        opcode |= MASK_INSR(0x0f38, X86EMUL_OPC_EXT_MASK);
                        d = twobyte_table[0x38];
                        break;
                    case vex_0f3a:
                        opcode |= MASK_INSR(0x0f3a, X86EMUL_OPC_EXT_MASK);
                        d = twobyte_table[0x3a];
                        break;
                    default:
                        rc = X86EMUL_UNHANDLEABLE;
                        goto done;
                    }
                }
                else if ( ext < ext_8f08 +
                                sizeof(xop_table) / sizeof(*xop_table) )
                {
                    b = insn_fetch_type(uint8_t);
                    opcode |= MASK_INSR(0x8f08 + ext - ext_8f08,
                                        X86EMUL_OPC_EXT_MASK);
                    d = xop_table[ext - ext_8f08];
                }
                else
                {
                    rc = X86EMUL_UNHANDLEABLE;
                    goto done;
                }

                opcode |= b | MASK_INSR(vex.pfx, X86EMUL_OPC_PFX_MASK);

                modrm = insn_fetch_type(uint8_t);
                modrm_mod = (modrm & 0xc0) >> 6;

                break;
            }

        modrm_reg = ((rex_prefix & 4) << 1) | ((modrm & 0x38) >> 3);
        modrm_rm  = modrm & 0x07;

        /*
         * Early operand adjustments. Only ones affecting further processing
         * prior to the x86_decode_*() calls really belong here. That would
         * normally be only addition/removal of SrcImm/SrcImm16, so their
         * fetching can be taken care of by the common code below.
         */
        if ( ext == ext_none )
        {
            switch ( b )
            {
            case 0xf6 ... 0xf7: /* Grp3 */
                switch ( modrm_reg & 7 )
                {
                case 0 ... 1: /* test */
                    d |= DstMem | SrcImm;
                    break;
                case 2: /* not */
                case 3: /* neg */
                    d |= DstMem;
                    break;
                case 4: /* mul */
                case 5: /* imul */
                case 6: /* div */
                case 7: /* idiv */
                    /*
                     * DstEax isn't really precise for all cases; updates to
                     * rDX get handled in an open coded manner.
                     */
                    d |= DstEax | SrcMem;
                    break;
                }
                break;
            }
        }

        if ( modrm_mod == 3 )
        {
            modrm_rm |= (rex_prefix & 1) << 3;
            ea.type = OP_REG;
        }
        else if ( ad_bytes == 2 )
        {
            /* 16-bit ModR/M decode. */
            ea.type = OP_MEM;
            switch ( modrm_rm )
            {
            case 0:
                ea.mem.off = state->regs->bx + state->regs->si;
                break;
            case 1:
                ea.mem.off = state->regs->bx + state->regs->di;
                break;
            case 2:
                ea.mem.seg = x86_seg_ss;
                ea.mem.off = state->regs->bp + state->regs->si;
                break;
            case 3:
                ea.mem.seg = x86_seg_ss;
                ea.mem.off = state->regs->bp + state->regs->di;
                break;
            case 4:
                ea.mem.off = state->regs->si;
                break;
            case 5:
                ea.mem.off = state->regs->di;
                break;
            case 6:
                if ( modrm_mod == 0 )
                    break;
                ea.mem.seg = x86_seg_ss;
                ea.mem.off = state->regs->bp;
                break;
            case 7:
                ea.mem.off = state->regs->bx;
                break;
            }
            switch ( modrm_mod )
            {
            case 0:
                if ( modrm_rm == 6 )
                    ea.mem.off = insn_fetch_type(int16_t);
                break;
            case 1:
                ea.mem.off += insn_fetch_type(int8_t);
                break;
            case 2:
                ea.mem.off += insn_fetch_type(int16_t);
                break;
            }
        }
        else
        {
            /* 32/64-bit ModR/M decode. */
            ea.type = OP_MEM;
            if ( modrm_rm == 4 )
            {
                sib = insn_fetch_type(uint8_t);
                sib_index = ((sib >> 3) & 7) | ((rex_prefix << 2) & 8);
                sib_base  = (sib & 7) | ((rex_prefix << 3) & 8);
                if ( sib_index != 4 )
                    ea.mem.off = *(long *)decode_register(sib_index,
                                                          state->regs, 0);
                ea.mem.off <<= (sib >> 6) & 3;
                if ( (modrm_mod == 0) && ((sib_base & 7) == 5) )
                    ea.mem.off += insn_fetch_type(int32_t);
                else if ( sib_base == 4 )
                {
                    ea.mem.seg  = x86_seg_ss;
                    ea.mem.off += state->regs->r(sp);
                    if ( !ext && (b == 0x8f) )
                        /* POP <rm> computes its EA post increment. */
                        ea.mem.off += ((mode_64bit() && (op_bytes == 4))
                                       ? 8 : op_bytes);
                }
                else if ( sib_base == 5 )
                {
                    ea.mem.seg  = x86_seg_ss;
                    ea.mem.off += state->regs->r(bp);
                }
                else
                    ea.mem.off += *(long *)decode_register(sib_base,
                                                           state->regs, 0);
            }
            else
            {
                modrm_rm |= (rex_prefix & 1) << 3;
                ea.mem.off = *(long *)decode_register(modrm_rm,
                                                      state->regs, 0);
                if ( (modrm_rm == 5) && (modrm_mod != 0) )
                    ea.mem.seg = x86_seg_ss;
            }
            switch ( modrm_mod )
            {
            case 0:
                if ( (modrm_rm & 7) != 5 )
                    break;
                ea.mem.off = insn_fetch_type(int32_t);
                pc_rel = mode_64bit();
                break;
            case 1:
                ea.mem.off += insn_fetch_type(int8_t);
                break;
            case 2:
                ea.mem.off += insn_fetch_type(int32_t);
                break;
            }
        }
    }

    if ( override_seg != x86_seg_none )
        ea.mem.seg = override_seg;

    /* Fetch the immediate operand, if present. */
    switch ( d & SrcMask )
    {
        unsigned int bytes;

    case SrcImm:
        if ( !(d & ByteOp) )
            bytes = op_bytes != 8 ? op_bytes : 4;
        else
        {
    case SrcImmByte:
            bytes = 1;
        }
        /* NB. Immediates are sign-extended as necessary. */
        switch ( bytes )
        {
        case 1: imm1 = insn_fetch_type(int8_t);  break;
        case 2: imm1 = insn_fetch_type(int16_t); break;
        case 4: imm1 = insn_fetch_type(int32_t); break;
        }
        break;
    case SrcImm16:
        imm1 = insn_fetch_type(uint16_t);
        break;
    }

    ctxt->opcode = opcode;
    state->desc = d;

    switch ( ext )
    {
    case ext_none:
        rc = x86_decode_onebyte(state, ctxt, ops);
        break;

    case ext_0f:
        rc = x86_decode_twobyte(state, ctxt, ops);
        break;

    case ext_0f38:
        rc = x86_decode_0f38(state, ctxt, ops);
        break;

    case ext_0f3a:
        if ( !vex.opcx )
            ctxt->opcode |= MASK_INSR(vex.pfx, X86EMUL_OPC_PFX_MASK);
        break;

    case ext_8f08:
    case ext_8f09:
    case ext_8f0a:
        break;

    default:
        ASSERT_UNREACHABLE();
        return X86EMUL_UNHANDLEABLE;
    }

    if ( ea.type == OP_MEM )
    {
        if ( pc_rel )
            ea.mem.off += state->ip;

        ea.mem.off = truncate_ea(ea.mem.off);
    }

    /*
     * When prefix 66 has a meaning different from operand-size override,
     * operand size defaults to 4 and can't be overridden to 2.
     */
    if ( op_bytes == 2 &&
         (ctxt->opcode & X86EMUL_OPC_PFX_MASK) == X86EMUL_OPC_66(0, 0) )
        op_bytes = 4;

 done:
    return rc;
}

/* No insn fetching past this point. */
#undef insn_fetch_bytes
#undef insn_fetch_type

/* Undo DEBUG wrapper. */
#undef x86_emulate

int
x86_emulate(
    struct x86_emulate_ctxt *ctxt,
    const struct x86_emulate_ops *ops)
{
    /* Shadow copy of register state. Committed on successful emulation. */
    struct cpu_user_regs _regs = *ctxt->regs;
    struct x86_emulate_state state;
    int rc;
    uint8_t b, d;
    bool singlestep = (_regs._eflags & EFLG_TF) && !is_branch_step(ctxt, ops);
    struct operand src = { .reg = PTR_POISON };
    struct operand dst = { .reg = PTR_POISON };
    enum x86_swint_type swint_type;
    struct fpu_insn_ctxt fic;
    struct x86_emulate_stub stub = {};
    DECLARE_ALIGNED(mmval_t, mmval);

    ASSERT(ops->read);

    rc = x86_decode(&state, ctxt, ops);
    if ( rc != X86EMUL_OKAY )
        return rc;

    /* Sync rIP to post decode value. */
    _regs.r(ip) = state.ip;

    if ( ops->validate )
    {
#ifndef NDEBUG
        state.caller = __builtin_return_address(0);
#endif
        rc = ops->validate(&state, ctxt);
#ifndef NDEBUG
        state.caller = NULL;
#endif
        if ( rc == X86EMUL_DONE )
            goto complete_insn;
        if ( rc != X86EMUL_OKAY )
            return rc;
    }

    b = ctxt->opcode;
    d = state.desc;
#define state (&state)

    generate_exception_if(state->not_64bit && mode_64bit(), EXC_UD);

    if ( ea.type == OP_REG )
        ea.reg = decode_register(modrm_rm, &_regs,
                                 (d & ByteOp) && !rex_prefix);

    /* Decode and fetch the source operand: register, memory or immediate. */
    switch ( d & SrcMask )
    {
    case SrcNone: /* case SrcImplicit: */
        src.type = OP_NONE;
        break;
    case SrcReg:
        src.type = OP_REG;
        if ( d & ByteOp )
        {
            src.reg = decode_register(modrm_reg, &_regs, (rex_prefix == 0));
            src.val = *(uint8_t *)src.reg;
            src.bytes = 1;
        }
        else
        {
            src.reg = decode_register(modrm_reg, &_regs, 0);
            switch ( (src.bytes = op_bytes) )
            {
            case 2: src.val = *(uint16_t *)src.reg; break;
            case 4: src.val = *(uint32_t *)src.reg; break;
            case 8: src.val = *(uint64_t *)src.reg; break;
            }
        }
        break;
    case SrcMem16:
        ea.bytes = 2;
        goto srcmem_common;
    case SrcMem:
        ea.bytes = (d & ByteOp) ? 1 : op_bytes;
    srcmem_common:
        src = ea;
        if ( src.type == OP_REG )
        {
            switch ( src.bytes )
            {
            case 1: src.val = *(uint8_t  *)src.reg; break;
            case 2: src.val = *(uint16_t *)src.reg; break;
            case 4: src.val = *(uint32_t *)src.reg; break;
            case 8: src.val = *(uint64_t *)src.reg; break;
            }
        }
        else if ( (rc = read_ulong(src.mem.seg, src.mem.off,
                                   &src.val, src.bytes, ctxt, ops)) )
            goto done;
        break;
    case SrcImm:
        if ( !(d & ByteOp) )
            src.bytes = op_bytes != 8 ? op_bytes : 4;
        else
        {
    case SrcImmByte:
            src.bytes = 1;
        }
        src.type  = OP_IMM;
        src.val   = imm1;
        break;
    case SrcImm16:
        src.type  = OP_IMM;
        src.bytes = 2;
        src.val   = imm1;
        break;
    }

    /* Decode and fetch the destination operand: register or memory. */
    switch ( d & DstMask )
    {
    case DstNone: /* case DstImplicit: */
        /*
         * The only implicit-operands instructions allowed a LOCK prefix are
         * CMPXCHG{8,16}B (MOV CRn is being handled elsewhere).
         */
        generate_exception_if(lock_prefix && (ext != ext_0f || b != 0xc7),
                              EXC_UD);
        dst.type = OP_NONE;
        break;

    case DstReg:
        generate_exception_if(lock_prefix, EXC_UD);
        dst.type = OP_REG;
        if ( d & ByteOp )
        {
            dst.reg = decode_register(modrm_reg, &_regs, (rex_prefix == 0));
            dst.val = *(uint8_t *)dst.reg;
            dst.bytes = 1;
        }
        else
        {
            dst.reg = decode_register(modrm_reg, &_regs, 0);
            switch ( (dst.bytes = op_bytes) )
            {
            case 2: dst.val = *(uint16_t *)dst.reg; break;
            case 4: dst.val = *(uint32_t *)dst.reg; break;
            case 8: dst.val = *(uint64_t *)dst.reg; break;
            }
        }
        break;
    case DstBitBase:
        if ( ea.type == OP_MEM )
        {
            /*
             * Instructions such as bt can reference an arbitrary offset from
             * their memory operand, but the instruction doing the actual
             * emulation needs the appropriate op_bytes read from memory.
             * Adjust both the source register and memory operand to make an
             * equivalent instruction.
             *
             * EA       += BitOffset DIV op_bytes*8
             * BitOffset = BitOffset MOD op_bytes*8
             * DIV truncates towards negative infinity.
             * MOD always produces a positive result.
             */
            if ( op_bytes == 2 )
                src.val = (int16_t)src.val;
            else if ( op_bytes == 4 )
                src.val = (int32_t)src.val;
            if ( (long)src.val < 0 )
                ea.mem.off -=
                    op_bytes + (((-src.val - 1) >> 3) & ~(op_bytes - 1L));
            else
                ea.mem.off += (src.val >> 3) & ~(op_bytes - 1L);
        }

        /* Bit index always truncated to within range. */
        src.val &= (op_bytes << 3) - 1;

        d = (d & ~DstMask) | DstMem;
        /* Becomes a normal DstMem operation from here on. */
    case DstMem:
        ea.bytes = (d & ByteOp) ? 1 : op_bytes;
        dst = ea;
        if ( dst.type == OP_REG )
        {
            generate_exception_if(lock_prefix, EXC_UD);
            switch ( dst.bytes )
            {
            case 1: dst.val = *(uint8_t  *)dst.reg; break;
            case 2: dst.val = *(uint16_t *)dst.reg; break;
            case 4: dst.val = *(uint32_t *)dst.reg; break;
            case 8: dst.val = *(uint64_t *)dst.reg; break;
            }
        }
        else if ( !(d & Mov) ) /* optimisation - avoid slow emulated read */
        {
            fail_if(lock_prefix ? !ops->cmpxchg : !ops->write);
            if ( (rc = read_ulong(dst.mem.seg, dst.mem.off,
                                  &dst.val, dst.bytes, ctxt, ops)) )
                goto done;
            dst.orig_val = dst.val;
        }
        else
        {
            /* Lock prefix is allowed only on RMW instructions. */
            generate_exception_if(lock_prefix, EXC_UD);
            fail_if(!ops->write);
        }
        break;
    }

    switch ( ctxt->opcode )
    {
        enum x86_segment seg;
        struct segment_register cs, sreg;
        unsigned long cr4;
        struct cpuid_leaf cpuid_leaf;

    case 0x00 ... 0x05: add: /* add */
        emulate_2op_SrcV("add", src, dst, _regs._eflags);
        break;

    case 0x08 ... 0x0d: or:  /* or */
        emulate_2op_SrcV("or", src, dst, _regs._eflags);
        break;

    case 0x10 ... 0x15: adc: /* adc */
        emulate_2op_SrcV("adc", src, dst, _regs._eflags);
        break;

    case 0x18 ... 0x1d: sbb: /* sbb */
        emulate_2op_SrcV("sbb", src, dst, _regs._eflags);
        break;

    case 0x20 ... 0x25: and: /* and */
        emulate_2op_SrcV("and", src, dst, _regs._eflags);
        break;

    case 0x28 ... 0x2d: sub: /* sub */
        emulate_2op_SrcV("sub", src, dst, _regs._eflags);
        break;

    case 0x30 ... 0x35: xor: /* xor */
        emulate_2op_SrcV("xor", src, dst, _regs._eflags);
        break;

    case 0x38 ... 0x3d: cmp: /* cmp */
        generate_exception_if(lock_prefix, EXC_UD);
        emulate_2op_SrcV("cmp", src, dst, _regs._eflags);
        dst.type = OP_NONE;
        break;

    case 0x06: /* push %%es */
    case 0x0e: /* push %%cs */
    case 0x16: /* push %%ss */
    case 0x1e: /* push %%ds */
    case X86EMUL_OPC(0x0f, 0xa0): /* push %%fs */
    case X86EMUL_OPC(0x0f, 0xa8): /* push %%gs */
        fail_if(ops->read_segment == NULL);
        if ( (rc = ops->read_segment((b >> 3) & 7, &sreg,
                                     ctxt)) != X86EMUL_OKAY )
            goto done;
        src.val = sreg.sel;
        goto push;

    case 0x07: /* pop %%es */
    case 0x17: /* pop %%ss */
    case 0x1f: /* pop %%ds */
    case X86EMUL_OPC(0x0f, 0xa1): /* pop %%fs */
    case X86EMUL_OPC(0x0f, 0xa9): /* pop %%gs */
        fail_if(ops->write_segment == NULL);
        /* 64-bit mode: POP defaults to a 64-bit operand. */
        if ( mode_64bit() && (op_bytes == 4) )
            op_bytes = 8;
        seg = (b >> 3) & 7;
        if ( (rc = read_ulong(x86_seg_ss, sp_post_inc(op_bytes), &dst.val,
                              op_bytes, ctxt, ops)) != X86EMUL_OKAY ||
             (rc = load_seg(seg, dst.val, 0, NULL, ctxt, ops)) != X86EMUL_OKAY )
            goto done;
        if ( seg == x86_seg_ss )
            ctxt->retire.mov_ss = true;
        break;

    case 0x27: /* daa */
    case 0x2f: /* das */ {
        uint8_t al = _regs.al;
        unsigned int eflags = _regs._eflags;

        _regs._eflags &= ~(EFLG_CF|EFLG_AF|EFLG_SF|EFLG_ZF|EFLG_PF);
        if ( ((al & 0x0f) > 9) || (eflags & EFLG_AF) )
        {
            _regs._eflags |= EFLG_AF;
            if ( b == 0x2f && (al < 6 || (eflags & EFLG_CF)) )
                _regs._eflags |= EFLG_CF;
            _regs.al += (b == 0x27) ? 6 : -6;
        }
        if ( (al > 0x99) || (eflags & EFLG_CF) )
        {
            _regs.al += (b == 0x27) ? 0x60 : -0x60;
            _regs._eflags |= EFLG_CF;
        }
        _regs._eflags |= !_regs.al ? EFLG_ZF : 0;
        _regs._eflags |= ((int8_t)_regs.al < 0) ? EFLG_SF : 0;
        _regs._eflags |= even_parity(_regs.al) ? EFLG_PF : 0;
        break;
    }

    case 0x37: /* aaa */
    case 0x3f: /* aas */
        _regs._eflags &= ~EFLG_CF;
        if ( (_regs.al > 9) || (_regs._eflags & EFLG_AF) )
        {
            _regs.al += (b == 0x37) ? 6 : -6;
            _regs.ah += (b == 0x37) ? 1 : -1;
            _regs._eflags |= EFLG_CF | EFLG_AF;
        }
        _regs.al &= 0x0f;
        break;

    case 0x40 ... 0x4f: /* inc/dec reg */
        dst.type  = OP_REG;
        dst.reg   = decode_register(b & 7, &_regs, 0);
        dst.bytes = op_bytes;
        dst.val   = *dst.reg;
        if ( b & 8 )
            emulate_1op("dec", dst, _regs._eflags);
        else
            emulate_1op("inc", dst, _regs._eflags);
        break;

    case 0x50 ... 0x57: /* push reg */
        src.val = *(unsigned long *)decode_register(
            (b & 7) | ((rex_prefix & 1) << 3), &_regs, 0);
        goto push;

    case 0x58 ... 0x5f: /* pop reg */
        dst.type  = OP_REG;
        dst.reg   = decode_register(
            (b & 7) | ((rex_prefix & 1) << 3), &_regs, 0);
        dst.bytes = op_bytes;
        if ( mode_64bit() && (dst.bytes == 4) )
            dst.bytes = 8;
        if ( (rc = read_ulong(x86_seg_ss, sp_post_inc(dst.bytes),
                              &dst.val, dst.bytes, ctxt, ops)) != 0 )
            goto done;
        break;

    case 0x60: /* pusha */ {
        int i;
        unsigned int regs[] = {
            _regs._eax, _regs._ecx, _regs._edx, _regs._ebx,
            _regs._esp, _regs._ebp, _regs._esi, _regs._edi };

        fail_if(!ops->write);
        for ( i = 0; i < 8; i++ )
            if ( (rc = ops->write(x86_seg_ss, sp_pre_dec(op_bytes),
                                  &regs[i], op_bytes, ctxt)) != 0 )
            goto done;
        break;
    }

    case 0x61: /* popa */ {
        int i;
        unsigned int dummy_esp, *regs[] = {
            &_regs._edi, &_regs._esi, &_regs._ebp, &dummy_esp,
            &_regs._ebx, &_regs._edx, &_regs._ecx, &_regs._eax };

        for ( i = 0; i < 8; i++ )
        {
            if ( (rc = read_ulong(x86_seg_ss, sp_post_inc(op_bytes),
                                  &dst.val, op_bytes, ctxt, ops)) != 0 )
                goto done;
            if ( op_bytes == 2 )
                *(uint16_t *)regs[i] = (uint16_t)dst.val;
            else
                *regs[i] = dst.val; /* 64b: zero-ext done by read_ulong() */
        }
        break;
    }

    case 0x62: /* bound */ {
        unsigned long src_val2;
        int lb, ub, idx;
        generate_exception_if(src.type != OP_MEM, EXC_UD);
        if ( (rc = read_ulong(src.mem.seg, src.mem.off + op_bytes,
                              &src_val2, op_bytes, ctxt, ops)) )
            goto done;
        ub  = (op_bytes == 2) ? (int16_t)src_val2 : (int32_t)src_val2;
        lb  = (op_bytes == 2) ? (int16_t)src.val  : (int32_t)src.val;
        idx = (op_bytes == 2) ? (int16_t)dst.val  : (int32_t)dst.val;
        generate_exception_if((idx < lb) || (idx > ub), EXC_BR);
        dst.type = OP_NONE;
        break;
    }

    case 0x63: /* movsxd (x86/64) / arpl (x86/32) */
        if ( mode_64bit() )
        {
            /* movsxd */
            if ( ea.type == OP_REG )
                src.val = *ea.reg;
            else if ( (rc = read_ulong(ea.mem.seg, ea.mem.off,
                                       &src.val, 4, ctxt, ops)) )
                goto done;
            dst.val = (int32_t)src.val;
        }
        else
        {
            /* arpl */
            unsigned int src_rpl = dst.val & 3;

            dst = ea;
            dst.bytes = 2;
            if ( dst.type == OP_REG )
                dst.val = *dst.reg;
            else if ( (rc = read_ulong(dst.mem.seg, dst.mem.off,
                                       &dst.val, 2, ctxt, ops)) )
                goto done;
            if ( src_rpl > (dst.val & 3) )
            {
                _regs._eflags |= EFLG_ZF;
                dst.val = (dst.val & ~3) | src_rpl;
            }
            else
            {
                _regs._eflags &= ~EFLG_ZF;
                dst.type = OP_NONE;
            }
            generate_exception_if(!in_protmode(ctxt, ops), EXC_UD);
        }
        break;

    case 0x68: /* push imm{16,32,64} */
    case 0x6a: /* push imm8 */
    push:
        ASSERT(d & Mov); /* writeback needed */
        dst.type  = OP_MEM;
        dst.bytes = mode_64bit() && (op_bytes == 4) ? 8 : op_bytes;
        dst.val = src.val;
        dst.mem.seg = x86_seg_ss;
        dst.mem.off = sp_pre_dec(dst.bytes);
        break;

    case 0x69: /* imul imm16/32 */
    case 0x6b: /* imul imm8 */
        if ( ea.type == OP_REG )
            dst.val = *ea.reg;
        else if ( (rc = read_ulong(ea.mem.seg, ea.mem.off,
                                   &dst.val, op_bytes, ctxt, ops)) )
            goto done;
        goto imul;

    case 0x6c ... 0x6d: /* ins %dx,%es:%edi */ {
        unsigned long nr_reps = get_rep_prefix(false, true);
        unsigned int port = _regs.dx;

        dst.bytes = !(b & 1) ? 1 : (op_bytes == 8) ? 4 : op_bytes;
        dst.mem.seg = x86_seg_es;
        dst.mem.off = truncate_ea_and_reps(_regs.r(di), nr_reps, dst.bytes);
        if ( (rc = ioport_access_check(port, dst.bytes, ctxt, ops)) != 0 )
            goto done;
        /* Try the presumably most efficient approach first. */
        if ( !ops->rep_ins )
            nr_reps = 1;
        rc = X86EMUL_UNHANDLEABLE;
        if ( nr_reps == 1 && ops->read_io && ops->write )
        {
            rc = ops->read_io(port, dst.bytes, &dst.val, ctxt);
            if ( rc == X86EMUL_OKAY )
                nr_reps = 0;
        }
        if ( (nr_reps > 1 || rc == X86EMUL_UNHANDLEABLE) && ops->rep_ins )
            rc = ops->rep_ins(port, dst.mem.seg, dst.mem.off, dst.bytes,
                              &nr_reps, ctxt);
        if ( nr_reps >= 1 && rc == X86EMUL_UNHANDLEABLE )
        {
            fail_if(!ops->read_io || !ops->write);
            if ( (rc = ops->read_io(port, dst.bytes, &dst.val, ctxt)) != 0 )
                goto done;
            nr_reps = 0;
        }
        if ( !nr_reps && rc == X86EMUL_OKAY )
        {
            dst.type = OP_MEM;
            nr_reps = 1;
        }
        register_address_adjust(_regs.r(di), nr_reps * dst.bytes);
        put_rep_prefix(nr_reps);
        if ( rc != X86EMUL_OKAY )
            goto done;
        break;
    }

    case 0x6e ... 0x6f: /* outs %esi,%dx */ {
        unsigned long nr_reps = get_rep_prefix(true, false);
        unsigned int port = _regs.dx;

        dst.bytes = !(b & 1) ? 1 : (op_bytes == 8) ? 4 : op_bytes;
        ea.mem.off = truncate_ea_and_reps(_regs.r(si), nr_reps, dst.bytes);
        if ( (rc = ioport_access_check(port, dst.bytes, ctxt, ops)) != 0 )
            goto done;
        /* Try the presumably most efficient approach first. */
        if ( !ops->rep_outs )
            nr_reps = 1;
        rc = X86EMUL_UNHANDLEABLE;
        if ( nr_reps == 1 && ops->write_io )
        {
            rc = read_ulong(ea.mem.seg, ea.mem.off, &dst.val, dst.bytes,
                            ctxt, ops);
            if ( rc == X86EMUL_OKAY )
                nr_reps = 0;
        }
        if ( (nr_reps > 1 || rc == X86EMUL_UNHANDLEABLE) && ops->rep_outs )
            rc = ops->rep_outs(ea.mem.seg, ea.mem.off, port, dst.bytes,
                               &nr_reps, ctxt);
        if ( nr_reps >= 1 && rc == X86EMUL_UNHANDLEABLE )
        {
            if ( (rc = read_ulong(ea.mem.seg, ea.mem.off, &dst.val,
                                  dst.bytes, ctxt, ops)) != X86EMUL_OKAY )
                goto done;
            fail_if(ops->write_io == NULL);
            nr_reps = 0;
        }
        if ( !nr_reps && rc == X86EMUL_OKAY )
        {
            if ( (rc = ops->write_io(port, dst.bytes, dst.val, ctxt)) != 0 )
                goto done;
            nr_reps = 1;
        }
        register_address_adjust(_regs.r(si), nr_reps * dst.bytes);
        put_rep_prefix(nr_reps);
        if ( rc != X86EMUL_OKAY )
            goto done;
        break;
    }

    case 0x70 ... 0x7f: /* jcc (short) */
        if ( test_cc(b, _regs._eflags) )
            jmp_rel((int32_t)src.val);
        adjust_bnd(ctxt, ops, vex.pfx);
        break;

    case 0x80: case 0x81: case 0x82: case 0x83: /* Grp1 */
        switch ( modrm_reg & 7 )
        {
        case 0: goto add;
        case 1: goto or;
        case 2: goto adc;
        case 3: goto sbb;
        case 4: goto and;
        case 5: goto sub;
        case 6: goto xor;
        case 7: goto cmp;
        }
        break;

    case 0xa8 ... 0xa9: /* test imm,%%eax */
    case 0x84 ... 0x85: test: /* test */
        emulate_2op_SrcV("test", src, dst, _regs._eflags);
        dst.type = OP_NONE;
        break;

    case 0x86 ... 0x87: xchg: /* xchg */
        /* Write back the register source. */
        switch ( dst.bytes )
        {
        case 1: *(uint8_t  *)src.reg = (uint8_t)dst.val; break;
        case 2: *(uint16_t *)src.reg = (uint16_t)dst.val; break;
        case 4: *src.reg = (uint32_t)dst.val; break; /* 64b reg: zero-extend */
        case 8: *src.reg = dst.val; break;
        }
        /* Write back the memory destination with implicit LOCK prefix. */
        dst.val = src.val;
        lock_prefix = 1;
        break;

    case 0xc6: /* Grp11: mov / xabort */
    case 0xc7: /* Grp11: mov / xbegin */
        if ( modrm == 0xf8 && vcpu_has_rtm() )
        {
            /*
             * xbegin unconditionally aborts, xabort is unconditionally
             * a nop.
             */
            if ( b & 1 )
            {
                jmp_rel((int32_t)src.val);
                _regs.r(ax) = 0;
            }
            dst.type = OP_NONE;
            break;
        }
        generate_exception_if((modrm_reg & 7) != 0, EXC_UD);
    case 0x88 ... 0x8b: /* mov */
    case 0xa0 ... 0xa1: /* mov mem.offs,{%al,%ax,%eax,%rax} */
    case 0xa2 ... 0xa3: /* mov {%al,%ax,%eax,%rax},mem.offs */
        dst.val = src.val;
        break;

    case 0x8c: /* mov Sreg,r/m */
        seg = modrm_reg & 7; /* REX.R is ignored. */
        generate_exception_if(!is_x86_user_segment(seg), EXC_UD);
    store_selector:
        fail_if(ops->read_segment == NULL);
        if ( (rc = ops->read_segment(seg, &sreg, ctxt)) != 0 )
            goto done;
        dst.val = sreg.sel;
        if ( dst.type == OP_MEM )
            dst.bytes = 2;
        break;

    case 0x8e: /* mov r/m,Sreg */
        seg = modrm_reg & 7; /* REX.R is ignored. */
        generate_exception_if(!is_x86_user_segment(seg) ||
                              seg == x86_seg_cs, EXC_UD);
        if ( (rc = load_seg(seg, src.val, 0, NULL, ctxt, ops)) != 0 )
            goto done;
        if ( seg == x86_seg_ss )
            ctxt->retire.mov_ss = true;
        dst.type = OP_NONE;
        break;

    case 0x8d: /* lea */
        generate_exception_if(ea.type != OP_MEM, EXC_UD);
        dst.val = ea.mem.off;
        break;

    case 0x8f: /* pop (sole member of Grp1a) */
        generate_exception_if((modrm_reg & 7) != 0, EXC_UD);
        /* 64-bit mode: POP defaults to a 64-bit operand. */
        if ( mode_64bit() && (dst.bytes == 4) )
            dst.bytes = 8;
        if ( (rc = read_ulong(x86_seg_ss, sp_post_inc(dst.bytes),
                              &dst.val, dst.bytes, ctxt, ops)) != 0 )
            goto done;
        break;

    case 0x90: /* nop / xchg %%r8,%%rax */
    case X86EMUL_OPC_F3(0, 0x90): /* pause / xchg %%r8,%%rax */
        if ( !(rex_prefix & REX_B) )
            break; /* nop / pause */
        /* fall through */

    case 0x91 ... 0x97: /* xchg reg,%%rax */
        dst.type = OP_REG;
        dst.bytes = op_bytes;
        dst.reg  = decode_register(
            (b & 7) | ((rex_prefix & 1) << 3), &_regs, 0);
        dst.val  = *dst.reg;
        goto xchg;

    case 0x98: /* cbw/cwde/cdqe */
        switch ( op_bytes )
        {
        case 2: _regs.ax = (int8_t)_regs.al; break; /* cbw */
        case 4: _regs.r(ax) = (uint32_t)(int16_t)_regs.ax; break; /* cwde */
        case 8: _regs.r(ax) = (int32_t)_regs._eax; break; /* cdqe */
        }
        break;

    case 0x99: /* cwd/cdq/cqo */
        switch ( op_bytes )
        {
        case 2: _regs.dx = -((int16_t)_regs.ax < 0); break;
        case 4: _regs.r(dx) = (uint32_t)-((int32_t)_regs._eax < 0); break;
#ifdef __x86_64__
        case 8: _regs.rdx = -((int64_t)_regs.rax < 0); break;
#endif
        }
        break;

    case 0x9a: /* call (far, absolute) */
        ASSERT(!mode_64bit());
    far_call:
        fail_if(!ops->read_segment || !ops->write);

        if ( (rc = ops->read_segment(x86_seg_cs, &sreg, ctxt)) ||
             (rc = load_seg(x86_seg_cs, imm2, 0, &cs, ctxt, ops)) ||
             (validate_far_branch(&cs, imm1),
              src.val = sreg.sel,
              rc = ops->write(x86_seg_ss, sp_pre_dec(op_bytes),
                              &src.val, op_bytes, ctxt)) ||
             (rc = ops->write(x86_seg_ss, sp_pre_dec(op_bytes),
                              &_regs.r(ip), op_bytes, ctxt)) ||
             (rc = ops->write_segment(x86_seg_cs, &cs, ctxt)) )
            goto done;

        _regs.r(ip) = imm1;
        singlestep = _regs._eflags & EFLG_TF;
        break;

    case 0x9b:  /* wait/fwait */
        fic.insn_bytes = 1;
        host_and_vcpu_must_have(fpu);
        get_fpu(X86EMUL_FPU_wait, &fic);
        asm volatile ( "fwait" ::: "memory" );
        put_fpu(&fic);
        break;

    case 0x9c: /* pushf */
        if ( (_regs._eflags & EFLG_VM) &&
             MASK_EXTR(_regs._eflags, EFLG_IOPL) != 3 )
        {
            cr4 = 0;
            if ( op_bytes == 2 && ops->read_cr )
            {
                rc = ops->read_cr(4, &cr4, ctxt);
                if ( rc != X86EMUL_OKAY )
                    goto done;
            }
            generate_exception_if(!(cr4 & CR4_VME), EXC_GP, 0);
            src.val = (_regs.flags & ~EFLG_IF) | EFLG_IOPL;
            if ( _regs._eflags & EFLG_VIF )
                src.val |= EFLG_IF;
        }
        else
            src.val = _regs.r(flags) & ~(EFLG_VM | EFLG_RF);
        goto push;

    case 0x9d: /* popf */ {
        uint32_t mask = EFLG_VIP | EFLG_VIF | EFLG_VM;

        cr4 = 0;
        if ( !mode_ring0() )
        {
            if ( _regs._eflags & EFLG_VM )
            {
                if ( op_bytes == 2 && ops->read_cr )
                {
                    rc = ops->read_cr(4, &cr4, ctxt);
                    if ( rc != X86EMUL_OKAY )
                        goto done;
                }
                generate_exception_if(!(cr4 & CR4_VME) &&
                                      MASK_EXTR(_regs._eflags, EFLG_IOPL) != 3,
                                      EXC_GP, 0);
            }
            mask |= EFLG_IOPL;
            if ( !mode_iopl() )
                mask |= EFLG_IF;
        }
        /* 64-bit mode: POP defaults to a 64-bit operand. */
        if ( mode_64bit() && (op_bytes == 4) )
            op_bytes = 8;
        if ( (rc = read_ulong(x86_seg_ss, sp_post_inc(op_bytes),
                              &dst.val, op_bytes, ctxt, ops)) != 0 )
            goto done;
        if ( op_bytes == 2 )
        {
            dst.val = (uint16_t)dst.val | (_regs._eflags & 0xffff0000u);
            if ( cr4 & CR4_VME )
            {
                if ( dst.val & EFLG_IF )
                {
                    generate_exception_if(_regs._eflags & EFLG_VIP, EXC_GP, 0);
                    dst.val |= EFLG_VIF;
                }
                else
                    dst.val &= ~EFLG_VIF;
                mask &= ~EFLG_VIF;
            }
        }
        dst.val &= EFLAGS_MODIFIABLE;
        _regs._eflags &= mask;
        _regs._eflags |= (dst.val & ~mask) | EFLG_MBS;
        break;
    }

    case 0x9e: /* sahf */
        if ( mode_64bit() )
            vcpu_must_have(lahf_lm);
        *(uint8_t *)&_regs._eflags = (_regs.ah & EFLAGS_MASK) | EFLG_MBS;
        break;

    case 0x9f: /* lahf */
        if ( mode_64bit() )
            vcpu_must_have(lahf_lm);
        _regs.ah = (_regs._eflags & EFLAGS_MASK) | EFLG_MBS;
        break;

    case 0xa4 ... 0xa5: /* movs */ {
        unsigned long nr_reps = get_rep_prefix(true, true);

        dst.bytes = (d & ByteOp) ? 1 : op_bytes;
        dst.mem.seg = x86_seg_es;
        dst.mem.off = truncate_ea_and_reps(_regs.r(di), nr_reps, dst.bytes);
        src.mem.off = truncate_ea_and_reps(_regs.r(si), nr_reps, dst.bytes);
        if ( (nr_reps == 1) || !ops->rep_movs ||
             ((rc = ops->rep_movs(ea.mem.seg, src.mem.off,
                                  dst.mem.seg, dst.mem.off, dst.bytes,
                                  &nr_reps, ctxt)) == X86EMUL_UNHANDLEABLE) )
        {
            if ( (rc = read_ulong(ea.mem.seg, src.mem.off,
                                  &dst.val, dst.bytes, ctxt, ops)) != 0 )
                goto done;
            dst.type = OP_MEM;
            nr_reps = 1;
        }
        register_address_adjust(_regs.r(si), nr_reps * dst.bytes);
        register_address_adjust(_regs.r(di), nr_reps * dst.bytes);
        put_rep_prefix(nr_reps);
        if ( rc != X86EMUL_OKAY )
            goto done;
        break;
    }

    case 0xa6 ... 0xa7: /* cmps */ {
        unsigned long next_eip = _regs.r(ip);

        get_rep_prefix(true, true);
        src.bytes = dst.bytes = (d & ByteOp) ? 1 : op_bytes;
        if ( (rc = read_ulong(ea.mem.seg, truncate_ea(_regs.r(si)),
                              &dst.val, dst.bytes, ctxt, ops)) ||
             (rc = read_ulong(x86_seg_es, truncate_ea(_regs.r(di)),
                              &src.val, src.bytes, ctxt, ops)) )
            goto done;
        register_address_adjust(_regs.r(si), dst.bytes);
        register_address_adjust(_regs.r(di), src.bytes);
        put_rep_prefix(1);
        /* cmp: dst - src ==> src=*%%edi,dst=*%%esi ==> *%%esi - *%%edi */
        emulate_2op_SrcV("cmp", src, dst, _regs._eflags);
        if ( (repe_prefix() && !(_regs._eflags & EFLG_ZF)) ||
             (repne_prefix() && (_regs._eflags & EFLG_ZF)) )
            _regs.r(ip) = next_eip;
        break;
    }

    case 0xaa ... 0xab: /* stos */ {
        unsigned long nr_reps = get_rep_prefix(false, true);

        dst.bytes = src.bytes;
        dst.mem.seg = x86_seg_es;
        dst.mem.off = truncate_ea(_regs.r(di));
        if ( (nr_reps == 1) || !ops->rep_stos ||
             ((rc = ops->rep_stos(&src.val,
                                  dst.mem.seg, dst.mem.off, dst.bytes,
                                  &nr_reps, ctxt)) == X86EMUL_UNHANDLEABLE) )
        {
            dst.val = src.val;
            dst.type = OP_MEM;
            nr_reps = 1;
            rc = X86EMUL_OKAY;
        }
        register_address_adjust(_regs.r(di), nr_reps * dst.bytes);
        put_rep_prefix(nr_reps);
        if ( rc != X86EMUL_OKAY )
            goto done;
        break;
    }

    case 0xac ... 0xad: /* lods */
        get_rep_prefix(true, false);
        if ( (rc = read_ulong(ea.mem.seg, truncate_ea(_regs.r(si)),
                              &dst.val, dst.bytes, ctxt, ops)) != 0 )
            goto done;
        register_address_adjust(_regs.r(si), dst.bytes);
        put_rep_prefix(1);
        break;

    case 0xae ... 0xaf: /* scas */ {
        unsigned long next_eip = _regs.r(ip);

        get_rep_prefix(false, true);
        if ( (rc = read_ulong(x86_seg_es, truncate_ea(_regs.r(di)),
                              &dst.val, src.bytes, ctxt, ops)) != 0 )
            goto done;
        register_address_adjust(_regs.r(di), src.bytes);
        put_rep_prefix(1);
        /* cmp: %%eax - *%%edi ==> src=%%eax,dst=*%%edi ==> src - dst */
        dst.bytes = src.bytes;
        emulate_2op_SrcV("cmp", dst, src, _regs._eflags);
        if ( (repe_prefix() && !(_regs._eflags & EFLG_ZF)) ||
             (repne_prefix() && (_regs._eflags & EFLG_ZF)) )
            _regs.r(ip) = next_eip;
        break;
    }

    case 0xb0 ... 0xb7: /* mov imm8,r8 */
        dst.reg = decode_register(
            (b & 7) | ((rex_prefix & 1) << 3), &_regs, (rex_prefix == 0));
        dst.val = src.val;
        break;

    case 0xb8 ... 0xbf: /* mov imm{16,32,64},r{16,32,64} */
        dst.reg = decode_register(
            (b & 7) | ((rex_prefix & 1) << 3), &_regs, 0);
        dst.val = src.val;
        break;

    case 0xc0 ... 0xc1: grp2: /* Grp2 */
        switch ( modrm_reg & 7 )
        {
        case 0: /* rol */
            emulate_2op_SrcB("rol", src, dst, _regs._eflags);
            break;
        case 1: /* ror */
            emulate_2op_SrcB("ror", src, dst, _regs._eflags);
            break;
        case 2: /* rcl */
            emulate_2op_SrcB("rcl", src, dst, _regs._eflags);
            break;
        case 3: /* rcr */
            emulate_2op_SrcB("rcr", src, dst, _regs._eflags);
            break;
        case 4: /* sal/shl */
        case 6: /* sal/shl */
            emulate_2op_SrcB("sal", src, dst, _regs._eflags);
            break;
        case 5: /* shr */
            emulate_2op_SrcB("shr", src, dst, _regs._eflags);
            break;
        case 7: /* sar */
            emulate_2op_SrcB("sar", src, dst, _regs._eflags);
            break;
        }
        break;

    case 0xc2: /* ret imm16 (near) */
    case 0xc3: /* ret (near) */
        op_bytes = ((op_bytes == 4) && mode_64bit()) ? 8 : op_bytes;
        if ( (rc = read_ulong(x86_seg_ss, sp_post_inc(op_bytes + src.val),
                              &dst.val, op_bytes, ctxt, ops)) != 0 ||
             (rc = ops->insn_fetch(x86_seg_cs, dst.val, NULL, 0, ctxt)) )
            goto done;
        _regs.r(ip) = dst.val;
        adjust_bnd(ctxt, ops, vex.pfx);
        break;

    case 0xc4: /* les */
    case 0xc5: /* lds */
        seg = (b & 1) * 3; /* es = 0, ds = 3 */
    les:
        generate_exception_if(src.type != OP_MEM, EXC_UD);
        if ( (rc = read_ulong(src.mem.seg, src.mem.off + src.bytes,
                              &dst.val, 2, ctxt, ops)) != X86EMUL_OKAY )
            goto done;
        ASSERT(is_x86_user_segment(seg));
        if ( (rc = load_seg(seg, dst.val, 0, NULL, ctxt, ops)) != X86EMUL_OKAY )
            goto done;
        dst.val = src.val;
        break;

    case 0xc8: /* enter imm16,imm8 */ {
        uint8_t depth = imm2 & 31;
        int i;

        dst.type = OP_REG;
        dst.bytes = (mode_64bit() && (op_bytes == 4)) ? 8 : op_bytes;
        dst.reg = (unsigned long *)&_regs.r(bp);
        fail_if(!ops->write);
        if ( (rc = ops->write(x86_seg_ss, sp_pre_dec(dst.bytes),
                              &_regs.r(bp), dst.bytes, ctxt)) )
            goto done;
        dst.val = _regs.r(sp);

        if ( depth > 0 )
        {
            for ( i = 1; i < depth; i++ )
            {
                unsigned long ebp, temp_data;
                ebp = truncate_word(_regs.r(bp) - i*dst.bytes, ctxt->sp_size/8);
                if ( (rc = read_ulong(x86_seg_ss, ebp,
                                      &temp_data, dst.bytes, ctxt, ops)) ||
                     (rc = ops->write(x86_seg_ss, sp_pre_dec(dst.bytes),
                                      &temp_data, dst.bytes, ctxt)) )
                    goto done;
            }
            if ( (rc = ops->write(x86_seg_ss, sp_pre_dec(dst.bytes),
                                  &dst.val, dst.bytes, ctxt)) )
                goto done;
        }

        sp_pre_dec(src.val);
        break;
    }

    case 0xc9: /* leave */
        /* First writeback, to %%esp. */
        dst.bytes = (mode_64bit() && (op_bytes == 4)) ? 8 : op_bytes;
        if ( dst.bytes == 2 )
            _regs.sp = _regs.bp;
        else
            _regs.r(sp) = dst.bytes == 4 ? _regs._ebp : _regs.r(bp);

        /* Second writeback, to %%ebp. */
        dst.type = OP_REG;
        dst.reg = (unsigned long *)&_regs.r(bp);
        if ( (rc = read_ulong(x86_seg_ss, sp_post_inc(dst.bytes),
                              &dst.val, dst.bytes, ctxt, ops)) )
            goto done;
        break;

    case 0xca: /* ret imm16 (far) */
    case 0xcb: /* ret (far) */
        if ( (rc = read_ulong(x86_seg_ss, sp_post_inc(op_bytes),
                              &dst.val, op_bytes, ctxt, ops)) ||
             (rc = read_ulong(x86_seg_ss, sp_post_inc(op_bytes + src.val),
                              &src.val, op_bytes, ctxt, ops)) ||
             (rc = load_seg(x86_seg_cs, src.val, 1, &cs, ctxt, ops)) ||
             (rc = commit_far_branch(&cs, dst.val)) )
            goto done;
        break;

    case 0xcc: /* int3 */
        src.val = EXC_BP;
        swint_type = x86_swint_int3;
        goto swint;

    case 0xcd: /* int imm8 */
        swint_type = x86_swint_int;
    swint:
        rc = inject_swint(swint_type, (uint8_t)src.val,
                          _regs.r(ip) - ctxt->regs->r(ip),
                          ctxt, ops) ? : X86EMUL_EXCEPTION;
        goto done;

    case 0xce: /* into */
        if ( !(_regs._eflags & EFLG_OF) )
            break;
        src.val = EXC_OF;
        swint_type = x86_swint_into;
        goto swint;

    case 0xcf: /* iret */ {
        unsigned long sel, eip, eflags;
        uint32_t mask = EFLG_VIP | EFLG_VIF | EFLG_VM;

        fail_if(!in_realmode(ctxt, ops));
        if ( (rc = read_ulong(x86_seg_ss, sp_post_inc(op_bytes),
                              &eip, op_bytes, ctxt, ops)) ||
             (rc = read_ulong(x86_seg_ss, sp_post_inc(op_bytes),
                              &sel, op_bytes, ctxt, ops)) ||
             (rc = read_ulong(x86_seg_ss, sp_post_inc(op_bytes),
                              &eflags, op_bytes, ctxt, ops)) )
            goto done;
        if ( op_bytes == 2 )
            eflags = (uint16_t)eflags | (_regs._eflags & 0xffff0000u);
        eflags &= EFLAGS_MODIFIABLE;
        _regs._eflags &= mask;
        _regs._eflags |= (eflags & ~mask) | EFLG_MBS;
        if ( (rc = load_seg(x86_seg_cs, sel, 1, &cs, ctxt, ops)) ||
             (rc = commit_far_branch(&cs, (uint32_t)eip)) )
            goto done;
        break;
    }

    case 0xd0 ... 0xd1: /* Grp2 */
        src.val = 1;
        goto grp2;

    case 0xd2 ... 0xd3: /* Grp2 */
        src.val = _regs.cl;
        goto grp2;

    case 0xd4: /* aam */
    case 0xd5: /* aad */ {
        unsigned int base = (uint8_t)src.val;

        if ( b & 0x01 )
        {
            uint16_t ax = _regs.ax;

            _regs.ax = (uint8_t)(ax + ((ax >> 8) * base));
        }
        else
        {
            uint8_t al = _regs.al;

            generate_exception_if(!base, EXC_DE);
            _regs.ax = ((al / base) << 8) | (al % base);
        }
        _regs._eflags &= ~(EFLG_SF|EFLG_ZF|EFLG_PF);
        _regs._eflags |= !_regs.al ? EFLG_ZF : 0;
        _regs._eflags |= ((int8_t)_regs.al < 0) ? EFLG_SF : 0;
        _regs._eflags |= even_parity(_regs.al) ? EFLG_PF : 0;
        break;
    }

    case 0xd6: /* salc */
        _regs.al = (_regs._eflags & EFLG_CF) ? 0xff : 0x00;
        break;

    case 0xd7: /* xlat */ {
        unsigned long al;

        if ( (rc = read_ulong(ea.mem.seg, truncate_ea(_regs.r(bx) + _regs.al),
                              &al, 1, ctxt, ops)) != 0 )
            goto done;
        _regs.al = al;
        break;
    }

    case 0xd8: /* FPU 0xd8 */
        host_and_vcpu_must_have(fpu);
        get_fpu(X86EMUL_FPU_fpu, &fic);
        switch ( modrm )
        {
        case 0xc0 ... 0xc7: /* fadd %stN,%st */
        case 0xc8 ... 0xcf: /* fmul %stN,%st */
        case 0xd0 ... 0xd7: /* fcom %stN,%st */
        case 0xd8 ... 0xdf: /* fcomp %stN,%st */
        case 0xe0 ... 0xe7: /* fsub %stN,%st */
        case 0xe8 ... 0xef: /* fsubr %stN,%st */
        case 0xf0 ... 0xf7: /* fdiv %stN,%st */
        case 0xf8 ... 0xff: /* fdivr %stN,%st */
            emulate_fpu_insn_stub(0xd8, modrm);
            break;
        default:
            ASSERT(ea.type == OP_MEM);
            if ( (rc = ops->read(ea.mem.seg, ea.mem.off, &src.val,
                                 4, ctxt)) != X86EMUL_OKAY )
                goto done;
            switch ( modrm_reg & 7 )
            {
            case 0: /* fadd */
                emulate_fpu_insn_memsrc("fadds", src.val);
                break;
            case 1: /* fmul */
                emulate_fpu_insn_memsrc("fmuls", src.val);
                break;
            case 2: /* fcom */
                emulate_fpu_insn_memsrc("fcoms", src.val);
                break;
            case 3: /* fcomp */
                emulate_fpu_insn_memsrc("fcomps", src.val);
                break;
            case 4: /* fsub */
                emulate_fpu_insn_memsrc("fsubs", src.val);
                break;
            case 5: /* fsubr */
                emulate_fpu_insn_memsrc("fsubrs", src.val);
                break;
            case 6: /* fdiv */
                emulate_fpu_insn_memsrc("fdivs", src.val);
                break;
            case 7: /* fdivr */
                emulate_fpu_insn_memsrc("fdivrs", src.val);
                break;
            }
        }
        put_fpu(&fic);
        break;

    case 0xd9: /* FPU 0xd9 */
        host_and_vcpu_must_have(fpu);
        get_fpu(X86EMUL_FPU_fpu, &fic);
        switch ( modrm )
        {
        case 0xfb: /* fsincos */
            fail_if(cpu_has_amd_erratum(573));
            /* fall through */
        case 0xc0 ... 0xc7: /* fld %stN */
        case 0xc8 ... 0xcf: /* fxch %stN */
        case 0xd0: /* fnop */
        case 0xd8 ... 0xdf: /* fstp %stN (alternative encoding) */
        case 0xe0: /* fchs */
        case 0xe1: /* fabs */
        case 0xe4: /* ftst */
        case 0xe5: /* fxam */
        case 0xe8: /* fld1 */
        case 0xe9: /* fldl2t */
        case 0xea: /* fldl2e */
        case 0xeb: /* fldpi */
        case 0xec: /* fldlg2 */
        case 0xed: /* fldln2 */
        case 0xee: /* fldz */
        case 0xf0: /* f2xm1 */
        case 0xf1: /* fyl2x */
        case 0xf2: /* fptan */
        case 0xf3: /* fpatan */
        case 0xf4: /* fxtract */
        case 0xf5: /* fprem1 */
        case 0xf6: /* fdecstp */
        case 0xf7: /* fincstp */
        case 0xf8: /* fprem */
        case 0xf9: /* fyl2xp1 */
        case 0xfa: /* fsqrt */
        case 0xfc: /* frndint */
        case 0xfd: /* fscale */
        case 0xfe: /* fsin */
        case 0xff: /* fcos */
            emulate_fpu_insn_stub(0xd9, modrm);
            break;
        default:
            generate_exception_if(ea.type != OP_MEM, EXC_UD);
            dst = ea;
            switch ( modrm_reg & 7 )
            {
            case 0: /* fld m32fp */
                if ( (rc = ops->read(ea.mem.seg, ea.mem.off, &src.val,
                                     4, ctxt)) != X86EMUL_OKAY )
                    goto done;
                emulate_fpu_insn_memsrc("flds", src.val);
                dst.type = OP_NONE;
                break;
            case 2: /* fst m32fp */
                emulate_fpu_insn_memdst("fsts", dst.val);
                dst.bytes = 4;
                break;
            case 3: /* fstp m32fp */
                emulate_fpu_insn_memdst("fstps", dst.val);
                dst.bytes = 4;
                break;
            case 4: /* fldenv - TODO */
                goto cannot_emulate;
            case 5: /* fldcw m2byte */
                if ( (rc = ops->read(ea.mem.seg, ea.mem.off, &src.val,
                                     2, ctxt)) != X86EMUL_OKAY )
                    goto done;
                emulate_fpu_insn_memsrc("fldcw", src.val);
                dst.type = OP_NONE;
                break;
            case 6: /* fnstenv - TODO */
                goto cannot_emulate;
            case 7: /* fnstcw m2byte */
                emulate_fpu_insn_memdst("fnstcw", dst.val);
                dst.bytes = 2;
                break;
            default:
                generate_exception(EXC_UD);
            }
            /*
             * Control instructions can't raise FPU exceptions, so we need
             * to consider suppressing writes only for non-control ones. All
             * of them in this group have data width 4.
             */
            if ( dst.type == OP_MEM && dst.bytes == 4 && !fpu_check_write() )
                dst.type = OP_NONE;
        }
        put_fpu(&fic);
        break;

    case 0xda: /* FPU 0xda */
        host_and_vcpu_must_have(fpu);
        get_fpu(X86EMUL_FPU_fpu, &fic);
        switch ( modrm )
        {
        case 0xc0 ... 0xc7: /* fcmovb %stN */
        case 0xc8 ... 0xcf: /* fcmove %stN */
        case 0xd0 ... 0xd7: /* fcmovbe %stN */
        case 0xd8 ... 0xdf: /* fcmovu %stN */
            vcpu_must_have(cmov);
            emulate_fpu_insn_stub_eflags(0xda, modrm);
            break;
        case 0xe9:          /* fucompp */
            emulate_fpu_insn_stub(0xda, modrm);
            break;
        default:
            generate_exception_if(ea.type != OP_MEM, EXC_UD);
            if ( (rc = ops->read(ea.mem.seg, ea.mem.off, &src.val,
                                 4, ctxt)) != X86EMUL_OKAY )
                goto done;
            switch ( modrm_reg & 7 )
            {
            case 0: /* fiadd m32i */
                emulate_fpu_insn_memsrc("fiaddl", src.val);
                break;
            case 1: /* fimul m32i */
                emulate_fpu_insn_memsrc("fimull", src.val);
                break;
            case 2: /* ficom m32i */
                emulate_fpu_insn_memsrc("ficoml", src.val);
                break;
            case 3: /* ficomp m32i */
                emulate_fpu_insn_memsrc("ficompl", src.val);
                break;
            case 4: /* fisub m32i */
                emulate_fpu_insn_memsrc("fisubl", src.val);
                break;
            case 5: /* fisubr m32i */
                emulate_fpu_insn_memsrc("fisubrl", src.val);
                break;
            case 6: /* fidiv m32i */
                emulate_fpu_insn_memsrc("fidivl", src.val);
                break;
            case 7: /* fidivr m32i */
                emulate_fpu_insn_memsrc("fidivrl", src.val);
                break;
            }
        }
        put_fpu(&fic);
        break;

    case 0xdb: /* FPU 0xdb */
        host_and_vcpu_must_have(fpu);
        get_fpu(X86EMUL_FPU_fpu, &fic);
        switch ( modrm )
        {
        case 0xc0 ... 0xc7: /* fcmovnb %stN */
        case 0xc8 ... 0xcf: /* fcmovne %stN */
        case 0xd0 ... 0xd7: /* fcmovnbe %stN */
        case 0xd8 ... 0xdf: /* fcmovnu %stN */
        case 0xe8 ... 0xef: /* fucomi %stN */
        case 0xf0 ... 0xf7: /* fcomi %stN */
            vcpu_must_have(cmov);
            emulate_fpu_insn_stub_eflags(0xdb, modrm);
            break;
        case 0xe0: /* fneni - 8087 only, ignored by 287 */
        case 0xe1: /* fndisi - 8087 only, ignored by 287 */
        case 0xe2: /* fnclex */
        case 0xe3: /* fninit */
        case 0xe4: /* fnsetpm - 287 only, ignored by 387 */
        /* case 0xe5: frstpm - 287 only, #UD on 387 */
            emulate_fpu_insn_stub(0xdb, modrm);
            break;
        default:
            generate_exception_if(ea.type != OP_MEM, EXC_UD);
            dst = ea;
            switch ( modrm_reg & 7 )
            {
            case 0: /* fild m32i */
                if ( (rc = ops->read(ea.mem.seg, ea.mem.off, &src.val,
                                     4, ctxt)) != X86EMUL_OKAY )
                    goto done;
                emulate_fpu_insn_memsrc("fildl", src.val);
                dst.type = OP_NONE;
                break;
            case 1: /* fisttp m32i */
                host_and_vcpu_must_have(sse3);
                emulate_fpu_insn_memdst("fisttpl", dst.val);
                dst.bytes = 4;
                break;
            case 2: /* fist m32i */
                emulate_fpu_insn_memdst("fistl", dst.val);
                dst.bytes = 4;
                break;
            case 3: /* fistp m32i */
                emulate_fpu_insn_memdst("fistpl", dst.val);
                dst.bytes = 4;
                break;
            case 5: /* fld m80fp */
                if ( (rc = ops->read(ea.mem.seg, ea.mem.off, mmvalp,
                                     10, ctxt)) != X86EMUL_OKAY )
                    goto done;
                emulate_fpu_insn_memsrc("fldt", *mmvalp);
                dst.type = OP_NONE;
                break;
            case 7: /* fstp m80fp */
                fail_if(!ops->write);
                emulate_fpu_insn_memdst("fstpt", *mmvalp);
                if ( fpu_check_write() &&
                     (rc = ops->write(ea.mem.seg, ea.mem.off, mmvalp,
                                      10, ctxt)) != X86EMUL_OKAY )
                    goto done;
                dst.type = OP_NONE;
                break;
            default:
                generate_exception(EXC_UD);
            }
            if ( dst.type == OP_MEM && !fpu_check_write() )
                dst.type = OP_NONE;
        }
        put_fpu(&fic);
        break;

    case 0xdc: /* FPU 0xdc */
        host_and_vcpu_must_have(fpu);
        get_fpu(X86EMUL_FPU_fpu, &fic);
        switch ( modrm )
        {
        case 0xc0 ... 0xc7: /* fadd %st,%stN */
        case 0xc8 ... 0xcf: /* fmul %st,%stN */
        case 0xd0 ... 0xd7: /* fcom %stN,%st (alternative encoding) */
        case 0xd8 ... 0xdf: /* fcomp %stN,%st (alternative encoding) */
        case 0xe0 ... 0xe7: /* fsubr %st,%stN */
        case 0xe8 ... 0xef: /* fsub %st,%stN */
        case 0xf0 ... 0xf7: /* fdivr %st,%stN */
        case 0xf8 ... 0xff: /* fdiv %st,%stN */
            emulate_fpu_insn_stub(0xdc, modrm);
            break;
        default:
            ASSERT(ea.type == OP_MEM);
            if ( (rc = ops->read(ea.mem.seg, ea.mem.off, &src.val,
                                 8, ctxt)) != X86EMUL_OKAY )
                goto done;
            switch ( modrm_reg & 7 )
            {
            case 0: /* fadd m64fp */
                emulate_fpu_insn_memsrc("faddl", src.val);
                break;
            case 1: /* fmul m64fp */
                emulate_fpu_insn_memsrc("fmull", src.val);
                break;
            case 2: /* fcom m64fp */
                emulate_fpu_insn_memsrc("fcoml", src.val);
                break;
            case 3: /* fcomp m64fp */
                emulate_fpu_insn_memsrc("fcompl", src.val);
                break;
            case 4: /* fsub m64fp */
                emulate_fpu_insn_memsrc("fsubl", src.val);
                break;
            case 5: /* fsubr m64fp */
                emulate_fpu_insn_memsrc("fsubrl", src.val);
                break;
            case 6: /* fdiv m64fp */
                emulate_fpu_insn_memsrc("fdivl", src.val);
                break;
            case 7: /* fdivr m64fp */
                emulate_fpu_insn_memsrc("fdivrl", src.val);
                break;
            }
        }
        put_fpu(&fic);
        break;

    case 0xdd: /* FPU 0xdd */
        host_and_vcpu_must_have(fpu);
        get_fpu(X86EMUL_FPU_fpu, &fic);
        switch ( modrm )
        {
        case 0xc0 ... 0xc7: /* ffree %stN */
        case 0xc8 ... 0xcf: /* fxch %stN (alternative encoding) */
        case 0xd0 ... 0xd7: /* fst %stN */
        case 0xd8 ... 0xdf: /* fstp %stN */
        case 0xe0 ... 0xe7: /* fucom %stN */
        case 0xe8 ... 0xef: /* fucomp %stN */
            emulate_fpu_insn_stub(0xdd, modrm);
            break;
        default:
            generate_exception_if(ea.type != OP_MEM, EXC_UD);
            dst = ea;
            switch ( modrm_reg & 7 )
            {
            case 0: /* fld m64fp */;
                if ( (rc = ops->read(ea.mem.seg, ea.mem.off, &src.val,
                                     8, ctxt)) != X86EMUL_OKAY )
                    goto done;
                emulate_fpu_insn_memsrc("fldl", src.val);
                dst.type = OP_NONE;
                break;
            case 1: /* fisttp m64i */
                host_and_vcpu_must_have(sse3);
                emulate_fpu_insn_memdst("fisttpll", dst.val);
                dst.bytes = 8;
                break;
            case 2: /* fst m64fp */
                emulate_fpu_insn_memdst("fstl", dst.val);
                dst.bytes = 8;
                break;
            case 3: /* fstp m64fp */
                emulate_fpu_insn_memdst("fstpl", dst.val);
                dst.bytes = 8;
                break;
            case 4: /* frstor - TODO */
            case 6: /* fnsave - TODO */
                goto cannot_emulate;
            case 7: /* fnstsw m2byte */
                emulate_fpu_insn_memdst("fnstsw", dst.val);
                dst.bytes = 2;
                break;
            default:
                generate_exception(EXC_UD);
            }
            /*
             * Control instructions can't raise FPU exceptions, so we need
             * to consider suppressing writes only for non-control ones. All
             * of them in this group have data width 8.
             */
            if ( dst.type == OP_MEM && dst.bytes == 8 && !fpu_check_write() )
                dst.type = OP_NONE;
        }
        put_fpu(&fic);
        break;

    case 0xde: /* FPU 0xde */
        host_and_vcpu_must_have(fpu);
        get_fpu(X86EMUL_FPU_fpu, &fic);
        switch ( modrm )
        {
        case 0xc0 ... 0xc7: /* faddp %stN */
        case 0xc8 ... 0xcf: /* fmulp %stN */
        case 0xd0 ... 0xd7: /* fcomp %stN (alternative encoding) */
        case 0xd9: /* fcompp */
        case 0xe0 ... 0xe7: /* fsubrp %stN */
        case 0xe8 ... 0xef: /* fsubp %stN */
        case 0xf0 ... 0xf7: /* fdivrp %stN */
        case 0xf8 ... 0xff: /* fdivp %stN */
            emulate_fpu_insn_stub(0xde, modrm);
            break;
        default:
            generate_exception_if(ea.type != OP_MEM, EXC_UD);
            switch ( modrm_reg & 7 )
            {
            case 0: /* fiadd m16i */
                emulate_fpu_insn_memsrc("fiadds", src.val);
                break;
            case 1: /* fimul m16i */
                emulate_fpu_insn_memsrc("fimuls", src.val);
                break;
            case 2: /* ficom m16i */
                emulate_fpu_insn_memsrc("ficoms", src.val);
                break;
            case 3: /* ficomp m16i */
                emulate_fpu_insn_memsrc("ficomps", src.val);
                break;
            case 4: /* fisub m16i */
                emulate_fpu_insn_memsrc("fisubs", src.val);
                break;
            case 5: /* fisubr m16i */
                emulate_fpu_insn_memsrc("fisubrs", src.val);
                break;
            case 6: /* fidiv m16i */
                emulate_fpu_insn_memsrc("fidivs", src.val);
                break;
            case 7: /* fidivr m16i */
                emulate_fpu_insn_memsrc("fidivrs", src.val);
                break;
            }
        }
        put_fpu(&fic);
        break;

    case 0xdf: /* FPU 0xdf */
        host_and_vcpu_must_have(fpu);
        get_fpu(X86EMUL_FPU_fpu, &fic);
        switch ( modrm )
        {
        case 0xe0:
            /* fnstsw %ax */
            dst.bytes = 2;
            dst.type = OP_REG;
            dst.reg = (void *)&_regs.ax;
            emulate_fpu_insn_memdst("fnstsw", dst.val);
            break;
        case 0xe8 ... 0xef: /* fucomip %stN */
        case 0xf0 ... 0xf7: /* fcomip %stN */
            vcpu_must_have(cmov);
            emulate_fpu_insn_stub_eflags(0xdf, modrm);
            break;
        case 0xc0 ... 0xc7: /* ffreep %stN */
        case 0xc8 ... 0xcf: /* fxch %stN (alternative encoding) */
        case 0xd0 ... 0xd7: /* fstp %stN (alternative encoding) */
        case 0xd8 ... 0xdf: /* fstp %stN (alternative encoding) */
            emulate_fpu_insn_stub(0xdf, modrm);
            break;
        default:
            generate_exception_if(ea.type != OP_MEM, EXC_UD);
            dst = ea;
            switch ( modrm_reg & 7 )
            {
            case 0: /* fild m16i */
                if ( (rc = ops->read(ea.mem.seg, ea.mem.off, &src.val,
                                     2, ctxt)) != X86EMUL_OKAY )
                    goto done;
                emulate_fpu_insn_memsrc("filds", src.val);
                dst.type = OP_NONE;
                break;
            case 1: /* fisttp m16i */
                host_and_vcpu_must_have(sse3);
                emulate_fpu_insn_memdst("fisttps", dst.val);
                dst.bytes = 2;
                break;
            case 2: /* fist m16i */
                emulate_fpu_insn_memdst("fists", dst.val);
                dst.bytes = 2;
                break;
            case 3: /* fistp m16i */
                emulate_fpu_insn_memdst("fistps", dst.val);
                dst.bytes = 2;
                break;
            case 4: /* fbld m80dec */
                if ( (rc = ops->read(ea.mem.seg, ea.mem.off, mmvalp,
                                     10, ctxt)) != X86EMUL_OKAY )
                    goto done;
                emulate_fpu_insn_memsrc("fbld", *mmvalp);
                dst.type = OP_NONE;
                break;
            case 5: /* fild m64i */
                if ( (rc = ops->read(ea.mem.seg, ea.mem.off, &src.val,
                                     8, ctxt)) != X86EMUL_OKAY )
                    goto done;
                emulate_fpu_insn_memsrc("fildll", src.val);
                dst.type = OP_NONE;
                break;
            case 6: /* fbstp packed bcd */
                fail_if(!ops->write);
                emulate_fpu_insn_memdst("fbstp", *mmvalp);
                if ( fpu_check_write() &&
                     (rc = ops->write(ea.mem.seg, ea.mem.off, mmvalp,
                                      10, ctxt)) != X86EMUL_OKAY )
                    goto done;
                dst.type = OP_NONE;
                break;
            case 7: /* fistp m64i */
                emulate_fpu_insn_memdst("fistpll", dst.val);
                dst.bytes = 8;
                break;
            }
            if ( dst.type == OP_MEM && !fpu_check_write() )
                dst.type = OP_NONE;
        }
        put_fpu(&fic);
        break;

    case 0xe0 ... 0xe2: /* loop{,z,nz} */ {
        unsigned long count = get_loop_count(&_regs, ad_bytes);
        int do_jmp = !(_regs._eflags & EFLG_ZF); /* loopnz */

        if ( b == 0xe1 )
            do_jmp = !do_jmp; /* loopz */
        else if ( b == 0xe2 )
            do_jmp = 1; /* loop */
        if ( count != 1 && do_jmp )
            jmp_rel((int32_t)src.val);
        put_loop_count(&_regs, ad_bytes, count - 1);
        break;
    }

    case 0xe3: /* jcxz/jecxz (short) */
        if ( !get_loop_count(&_regs, ad_bytes) )
            jmp_rel((int32_t)src.val);
        break;

    case 0xe4: /* in imm8,%al */
    case 0xe5: /* in imm8,%eax */
    case 0xe6: /* out %al,imm8 */
    case 0xe7: /* out %eax,imm8 */
    case 0xec: /* in %dx,%al */
    case 0xed: /* in %dx,%eax */
    case 0xee: /* out %al,%dx */
    case 0xef: /* out %eax,%dx */ {
        unsigned int port = ((b < 0xe8) ? (uint8_t)src.val : _regs.dx);

        op_bytes = !(b & 1) ? 1 : (op_bytes == 8) ? 4 : op_bytes;
        if ( (rc = ioport_access_check(port, op_bytes, ctxt, ops)) != 0 )
            goto done;
        if ( b & 2 )
        {
            /* out */
            fail_if(ops->write_io == NULL);
            rc = ops->write_io(port, op_bytes, _regs._eax, ctxt);
        }
        else
        {
            /* in */
            dst.bytes = op_bytes;
            fail_if(ops->read_io == NULL);
            rc = ops->read_io(port, dst.bytes, &dst.val, ctxt);
        }
        if ( rc != 0 )
        {
            if ( rc == X86EMUL_DONE )
                goto complete_insn;
            goto done;
        }
        break;
    }

    case 0xe8: /* call (near) */ {
        int32_t rel = src.val;

        op_bytes = ((op_bytes == 4) && mode_64bit()) ? 8 : op_bytes;
        src.val = _regs.r(ip);
        jmp_rel(rel);
        adjust_bnd(ctxt, ops, vex.pfx);
        goto push;
    }

    case 0xe9: /* jmp (near) */
    case 0xeb: /* jmp (short) */
        jmp_rel((int32_t)src.val);
        if ( !(b & 2) )
            adjust_bnd(ctxt, ops, vex.pfx);
        break;

    case 0xea: /* jmp (far, absolute) */
        ASSERT(!mode_64bit());
    far_jmp:
        if ( (rc = load_seg(x86_seg_cs, imm2, 0, &cs, ctxt, ops)) ||
             (rc = commit_far_branch(&cs, imm1)) )
            goto done;
        break;

    case 0xf1: /* int1 (icebp) */
        src.val = EXC_DB;
        swint_type = x86_swint_icebp;
        goto swint;

    case 0xf4: /* hlt */
        generate_exception_if(!mode_ring0(), EXC_GP, 0);
        ctxt->retire.hlt = true;
        break;

    case 0xf5: /* cmc */
        _regs._eflags ^= EFLG_CF;
        break;

    case 0xf6 ... 0xf7: /* Grp3 */
        if ( (d & DstMask) == DstEax )
            dst.reg = (unsigned long *)&_regs.r(ax);
        switch ( modrm_reg & 7 )
        {
            unsigned long u[2], v;

        case 0 ... 1: /* test */
            generate_exception_if(lock_prefix, EXC_UD);
            goto test;
        case 2: /* not */
            dst.val = ~dst.val;
            break;
        case 3: /* neg */
            emulate_1op("neg", dst, _regs._eflags);
            break;
        case 4: /* mul */
            _regs._eflags &= ~(EFLG_OF|EFLG_CF);
            switch ( dst.bytes )
            {
            case 1:
                dst.val = _regs.al;
                dst.val *= src.val;
                if ( (uint8_t)dst.val != (uint16_t)dst.val )
                    _regs._eflags |= EFLG_OF|EFLG_CF;
                dst.bytes = 2;
                break;
            case 2:
                dst.val = _regs.ax;
                dst.val *= src.val;
                if ( (uint16_t)dst.val != (uint32_t)dst.val )
                    _regs._eflags |= EFLG_OF|EFLG_CF;
                _regs.dx = dst.val >> 16;
                break;
#ifdef __x86_64__
            case 4:
                dst.val = _regs._eax;
                dst.val *= src.val;
                if ( (uint32_t)dst.val != dst.val )
                    _regs._eflags |= EFLG_OF|EFLG_CF;
                _regs.rdx = dst.val >> 32;
                break;
#endif
            default:
                u[0] = src.val;
                u[1] = _regs.r(ax);
                if ( mul_dbl(u) )
                    _regs._eflags |= EFLG_OF|EFLG_CF;
                _regs.r(dx) = u[1];
                dst.val = u[0];
                break;
            }
            break;
        case 5: /* imul */
        imul:
            _regs._eflags &= ~(EFLG_OF|EFLG_CF);
            switch ( dst.bytes )
            {
            case 1:
                dst.val = (int8_t)src.val * (int8_t)_regs.al;
                if ( (int8_t)dst.val != (int16_t)dst.val )
                    _regs._eflags |= EFLG_OF|EFLG_CF;
                ASSERT(b > 0x6b);
                dst.bytes = 2;
                break;
            case 2:
                dst.val = ((uint32_t)(int16_t)src.val *
                           (uint32_t)(int16_t)_regs.ax);
                if ( (int16_t)dst.val != (int32_t)dst.val )
                    _regs._eflags |= EFLG_OF|EFLG_CF;
                if ( b > 0x6b )
                    _regs.dx = dst.val >> 16;
                break;
#ifdef __x86_64__
            case 4:
                dst.val = ((uint64_t)(int32_t)src.val *
                           (uint64_t)(int32_t)_regs._eax);
                if ( (int32_t)dst.val != dst.val )
                    _regs._eflags |= EFLG_OF|EFLG_CF;
                if ( b > 0x6b )
                    _regs.rdx = dst.val >> 32;
                break;
#endif
            default:
                u[0] = src.val;
                u[1] = _regs.r(ax);
                if ( imul_dbl(u) )
                    _regs._eflags |= EFLG_OF|EFLG_CF;
                if ( b > 0x6b )
                    _regs.r(dx) = u[1];
                dst.val = u[0];
                break;
            }
            break;
        case 6: /* div */
            switch ( src.bytes )
            {
            case 1:
                u[0] = _regs.ax;
                u[1] = 0;
                v    = (uint8_t)src.val;
                generate_exception_if(
                    div_dbl(u, v) || ((uint8_t)u[0] != (uint16_t)u[0]),
                    EXC_DE);
                dst.val = (uint8_t)u[0];
                _regs.ah = u[1];
                break;
            case 2:
                u[0] = (_regs._edx << 16) | _regs.ax;
                u[1] = 0;
                v    = (uint16_t)src.val;
                generate_exception_if(
                    div_dbl(u, v) || ((uint16_t)u[0] != (uint32_t)u[0]),
                    EXC_DE);
                dst.val = (uint16_t)u[0];
                _regs.dx = u[1];
                break;
#ifdef __x86_64__
            case 4:
                u[0] = (_regs.rdx << 32) | _regs._eax;
                u[1] = 0;
                v    = (uint32_t)src.val;
                generate_exception_if(
                    div_dbl(u, v) || ((uint32_t)u[0] != u[0]),
                    EXC_DE);
                dst.val   = (uint32_t)u[0];
                _regs.rdx = (uint32_t)u[1];
                break;
#endif
            default:
                u[0] = _regs.r(ax);
                u[1] = _regs.r(dx);
                v    = src.val;
                generate_exception_if(div_dbl(u, v), EXC_DE);
                dst.val     = u[0];
                _regs.r(dx) = u[1];
                break;
            }
            break;
        case 7: /* idiv */
            switch ( src.bytes )
            {
            case 1:
                u[0] = (int16_t)_regs.ax;
                u[1] = ((long)u[0] < 0) ? ~0UL : 0UL;
                v    = (int8_t)src.val;
                generate_exception_if(
                    idiv_dbl(u, v) || ((int8_t)u[0] != (int16_t)u[0]),
                    EXC_DE);
                dst.val = (int8_t)u[0];
                _regs.ah = u[1];
                break;
            case 2:
                u[0] = (int32_t)((_regs._edx << 16) | _regs.ax);
                u[1] = ((long)u[0] < 0) ? ~0UL : 0UL;
                v    = (int16_t)src.val;
                generate_exception_if(
                    idiv_dbl(u, v) || ((int16_t)u[0] != (int32_t)u[0]),
                    EXC_DE);
                dst.val = (int16_t)u[0];
                _regs.dx = u[1];
                break;
#ifdef __x86_64__
            case 4:
                u[0] = (_regs.rdx << 32) | _regs._eax;
                u[1] = ((long)u[0] < 0) ? ~0UL : 0UL;
                v    = (int32_t)src.val;
                generate_exception_if(
                    idiv_dbl(u, v) || ((int32_t)u[0] != u[0]),
                    EXC_DE);
                dst.val   = (int32_t)u[0];
                _regs.rdx = (uint32_t)u[1];
                break;
#endif
            default:
                u[0] = _regs.r(ax);
                u[1] = _regs.r(dx);
                v    = src.val;
                generate_exception_if(idiv_dbl(u, v), EXC_DE);
                dst.val     = u[0];
                _regs.r(dx) = u[1];
                break;
            }
            break;
        }
        break;

    case 0xf8: /* clc */
        _regs._eflags &= ~EFLG_CF;
        break;

    case 0xf9: /* stc */
        _regs._eflags |= EFLG_CF;
        break;

    case 0xfa: /* cli */
        if ( mode_iopl() )
            _regs._eflags &= ~EFLG_IF;
        else
        {
            generate_exception_if(!mode_vif(), EXC_GP, 0);
            _regs._eflags &= ~EFLG_VIF;
        }
        break;

    case 0xfb: /* sti */
        if ( mode_iopl() )
        {
            if ( !(_regs._eflags & EFLG_IF) )
                ctxt->retire.sti = true;
            _regs._eflags |= EFLG_IF;
        }
        else
        {
            generate_exception_if((_regs._eflags & EFLG_VIP) || !mode_vif(),
                                  EXC_GP, 0);
            if ( !(_regs._eflags & EFLG_VIF) )
                ctxt->retire.sti = true;
            _regs._eflags |= EFLG_VIF;
        }
        break;

    case 0xfc: /* cld */
        _regs._eflags &= ~EFLG_DF;
        break;

    case 0xfd: /* std */
        _regs._eflags |= EFLG_DF;
        break;

    case 0xfe: /* Grp4 */
        generate_exception_if((modrm_reg & 7) >= 2, EXC_UD);
        /* Fallthrough. */
    case 0xff: /* Grp5 */
        switch ( modrm_reg & 7 )
        {
        case 0: /* inc */
            emulate_1op("inc", dst, _regs._eflags);
            break;
        case 1: /* dec */
            emulate_1op("dec", dst, _regs._eflags);
            break;
        case 2: /* call (near) */
            dst.val = _regs.r(ip);
            if ( (rc = ops->insn_fetch(x86_seg_cs, src.val, NULL, 0, ctxt)) )
                goto done;
            _regs.r(ip) = src.val;
            src.val = dst.val;
            adjust_bnd(ctxt, ops, vex.pfx);
            goto push;
        case 4: /* jmp (near) */
            if ( (rc = ops->insn_fetch(x86_seg_cs, src.val, NULL, 0, ctxt)) )
                goto done;
            _regs.r(ip) = src.val;
            dst.type = OP_NONE;
            adjust_bnd(ctxt, ops, vex.pfx);
            break;
        case 3: /* call (far, absolute indirect) */
        case 5: /* jmp (far, absolute indirect) */
            generate_exception_if(src.type != OP_MEM, EXC_UD);

            if ( (rc = read_ulong(src.mem.seg, src.mem.off + op_bytes,
                                  &imm2, 2, ctxt, ops)) )
                goto done;
            imm1 = src.val;
            if ( !(modrm_reg & 4) )
                goto far_call;
            goto far_jmp;
        case 6: /* push */
            goto push;
        case 7:
            generate_exception(EXC_UD);
        }
        break;

    case X86EMUL_OPC(0x0f, 0x00): /* Grp6 */
        seg = (modrm_reg & 1) ? x86_seg_tr : x86_seg_ldtr;
        generate_exception_if(!in_protmode(ctxt, ops), EXC_UD);
        switch ( modrm_reg & 6 )
        {
        case 0: /* sldt / str */
            generate_exception_if(umip_active(ctxt, ops), EXC_GP, 0);
            goto store_selector;
        case 2: /* lldt / ltr */
            generate_exception_if(!mode_ring0(), EXC_GP, 0);
            if ( (rc = load_seg(seg, src.val, 0, NULL, ctxt, ops)) != 0 )
                goto done;
            break;
        case 4: /* verr / verw */
            _regs._eflags &= ~EFLG_ZF;
            switch ( rc = protmode_load_seg(x86_seg_none, src.val, false,
                                            &sreg, ctxt, ops) )
            {
            case X86EMUL_OKAY:
                if ( sreg.attr.fields.s &&
                     ((modrm_reg & 1) ? ((sreg.attr.fields.type & 0xa) == 0x2)
                                      : ((sreg.attr.fields.type & 0xa) != 0x8)) )
                    _regs._eflags |= EFLG_ZF;
                break;
            case X86EMUL_EXCEPTION:
                if ( ctxt->event_pending )
                {
                    ASSERT(ctxt->event.vector == EXC_PF);
            default:
                    goto done;
                }
                /* Instead of the exception, ZF remains cleared. */
                rc = X86EMUL_OKAY;
                break;
            }
            break;
        default:
            generate_exception_if(true, EXC_UD);
            break;
        }
        break;

    case X86EMUL_OPC(0x0f, 0x01): /* Grp7 */ {
        unsigned long base, limit, cr0, cr0w;

        switch( modrm )
        {
        case 0xca: /* clac */
        case 0xcb: /* stac */
            vcpu_must_have(smap);
            generate_exception_if(vex.pfx || !mode_ring0(), EXC_UD);

            _regs._eflags &= ~EFLG_AC;
            if ( modrm == 0xcb )
                _regs._eflags |= EFLG_AC;
            goto complete_insn;

#ifdef __XEN__
        case 0xd1: /* xsetbv */
            generate_exception_if(vex.pfx, EXC_UD);
            if ( !ops->read_cr || ops->read_cr(4, &cr4, ctxt) != X86EMUL_OKAY )
                cr4 = 0;
            generate_exception_if(!(cr4 & X86_CR4_OSXSAVE), EXC_UD);
            generate_exception_if(!mode_ring0() ||
                                  handle_xsetbv(_regs._ecx,
                                                _regs._eax | (_regs.rdx << 32)),
                                  EXC_GP, 0);
            goto complete_insn;
#endif

        case 0xd4: /* vmfunc */
            generate_exception_if(vex.pfx, EXC_UD);
            fail_if(!ops->vmfunc);
            if ( (rc = ops->vmfunc(ctxt)) != X86EMUL_OKAY )
                goto done;
            goto complete_insn;

        case 0xd5: /* xend */
            generate_exception_if(vex.pfx, EXC_UD);
            generate_exception_if(!vcpu_has_rtm(), EXC_UD);
            generate_exception_if(vcpu_has_rtm(), EXC_GP, 0);
            break;

        case 0xd6: /* xtest */
            generate_exception_if(vex.pfx, EXC_UD);
            generate_exception_if(!vcpu_has_rtm() && !vcpu_has_hle(),
                                  EXC_UD);
            /* Neither HLE nor RTM can be active when we get here. */
            _regs._eflags |= EFLG_ZF;
            goto complete_insn;

        case 0xdf: /* invlpga */
            generate_exception_if(!in_protmode(ctxt, ops), EXC_UD);
            generate_exception_if(!mode_ring0(), EXC_GP, 0);
            fail_if(ops->invlpg == NULL);
            if ( (rc = ops->invlpg(x86_seg_none, truncate_ea(_regs.r(ax)),
                                   ctxt)) )
                goto done;
            goto complete_insn;

        case 0xf9: /* rdtscp */
        {
            uint64_t tsc_aux;
            fail_if(ops->read_msr == NULL);
            if ( (rc = ops->read_msr(MSR_TSC_AUX, &tsc_aux, ctxt)) != 0 )
                goto done;
            _regs.r(cx) = (uint32_t)tsc_aux;
            goto rdtsc;
        }

        case 0xfc: /* clzero */
        {
            unsigned long zero = 0;

            base = ad_bytes == 8 ? _regs.r(ax) :
                   ad_bytes == 4 ? _regs._eax : _regs.ax;
            limit = 0;
            if ( vcpu_has_clflush() &&
                 ops->cpuid(1, 0, &cpuid_leaf, ctxt) == X86EMUL_OKAY )
                limit = ((cpuid_leaf.b >> 8) & 0xff) * 8;
            generate_exception_if(limit < sizeof(long) ||
                                  (limit & (limit - 1)), EXC_UD);
            base &= ~(limit - 1);
            if ( ops->rep_stos )
            {
                unsigned long nr_reps = limit / sizeof(zero);

                rc = ops->rep_stos(&zero, ea.mem.seg, base, sizeof(zero),
                                   &nr_reps, ctxt);
                if ( rc == X86EMUL_OKAY )
                {
                    base += nr_reps * sizeof(zero);
                    limit -= nr_reps * sizeof(zero);
                }
                else if ( rc != X86EMUL_UNHANDLEABLE )
                    goto done;
            }
            fail_if(limit && !ops->write);
            while ( limit )
            {
                rc = ops->write(ea.mem.seg, base, &zero, sizeof(zero), ctxt);
                if ( rc != X86EMUL_OKAY )
                    goto done;
                base += sizeof(zero);
                limit -= sizeof(zero);
            }
            goto complete_insn;
        }
        }

        seg = (modrm_reg & 1) ? x86_seg_idtr : x86_seg_gdtr;

        switch ( modrm_reg & 7 )
        {
        case 0: /* sgdt */
        case 1: /* sidt */
            generate_exception_if(ea.type != OP_MEM, EXC_UD);
            generate_exception_if(umip_active(ctxt, ops), EXC_GP, 0);
            fail_if(!ops->read_segment || !ops->write);
            if ( (rc = ops->read_segment(seg, &sreg, ctxt)) )
                goto done;
            if ( mode_64bit() )
                op_bytes = 8;
            else if ( op_bytes == 2 )
            {
                sreg.base &= 0xffffff;
                op_bytes = 4;
            }
            if ( (rc = ops->write(ea.mem.seg, ea.mem.off, &sreg.limit,
                                  2, ctxt)) != X86EMUL_OKAY ||
                 (rc = ops->write(ea.mem.seg, ea.mem.off + 2, &sreg.base,
                                  op_bytes, ctxt)) != X86EMUL_OKAY )
                goto done;
            break;
        case 2: /* lgdt */
        case 3: /* lidt */
            generate_exception_if(!mode_ring0(), EXC_GP, 0);
            generate_exception_if(ea.type != OP_MEM, EXC_UD);
            fail_if(ops->write_segment == NULL);
            memset(&sreg, 0, sizeof(sreg));
            if ( (rc = read_ulong(ea.mem.seg, ea.mem.off+0,
                                  &limit, 2, ctxt, ops)) ||
                 (rc = read_ulong(ea.mem.seg, ea.mem.off+2,
                                  &base, mode_64bit() ? 8 : 4, ctxt, ops)) )
                goto done;
            generate_exception_if(!is_canonical_address(base), EXC_GP, 0);
            sreg.base = base;
            sreg.limit = limit;
            if ( !mode_64bit() && op_bytes == 2 )
                sreg.base &= 0xffffff;
            if ( (rc = ops->write_segment(seg, &sreg, ctxt)) )
                goto done;
            break;
        case 4: /* smsw */
            generate_exception_if(umip_active(ctxt, ops), EXC_GP, 0);
            if ( ea.type == OP_MEM )
            {
                fail_if(!ops->write);
                d |= Mov; /* force writeback */
                ea.bytes = 2;
            }
            else
                ea.bytes = op_bytes;
            dst = ea;
            fail_if(ops->read_cr == NULL);
            if ( (rc = ops->read_cr(0, &dst.val, ctxt)) )
                goto done;
            break;
        case 6: /* lmsw */
            fail_if(ops->read_cr == NULL);
            fail_if(ops->write_cr == NULL);
            generate_exception_if(!mode_ring0(), EXC_GP, 0);
            if ( (rc = ops->read_cr(0, &cr0, ctxt)) )
                goto done;
            if ( ea.type == OP_REG )
                cr0w = *ea.reg;
            else if ( (rc = read_ulong(ea.mem.seg, ea.mem.off,
                                       &cr0w, 2, ctxt, ops)) )
                goto done;
            /* LMSW can: (1) set bits 0-3; (2) clear bits 1-3. */
            cr0 = (cr0 & ~0xe) | (cr0w & 0xf);
            if ( (rc = ops->write_cr(0, cr0, ctxt)) )
                goto done;
            break;
        case 7: /* invlpg */
            generate_exception_if(!mode_ring0(), EXC_GP, 0);
            generate_exception_if(ea.type != OP_MEM, EXC_UD);
            fail_if(ops->invlpg == NULL);
            if ( (rc = ops->invlpg(ea.mem.seg, ea.mem.off, ctxt)) )
                goto done;
            break;
        default:
            goto cannot_emulate;
        }
        break;
    }

    case X86EMUL_OPC(0x0f, 0x02): /* lar */
        generate_exception_if(!in_protmode(ctxt, ops), EXC_UD);
        _regs._eflags &= ~EFLG_ZF;
        switch ( rc = protmode_load_seg(x86_seg_none, src.val, false, &sreg,
                                        ctxt, ops) )
        {
        case X86EMUL_OKAY:
            if ( !sreg.attr.fields.s )
            {
                switch ( sreg.attr.fields.type )
                {
                case 0x01: /* available 16-bit TSS */
                case 0x03: /* busy 16-bit TSS */
                case 0x04: /* 16-bit call gate */
                case 0x05: /* 16/32-bit task gate */
                    if ( in_longmode(ctxt, ops) )
                        break;
                    /* fall through */
                case 0x02: /* LDT */
                case 0x09: /* available 32/64-bit TSS */
                case 0x0b: /* busy 32/64-bit TSS */
                case 0x0c: /* 32/64-bit call gate */
                    _regs._eflags |= EFLG_ZF;
                    break;
                }
            }
            else
                _regs._eflags |= EFLG_ZF;
            break;
        case X86EMUL_EXCEPTION:
            if ( ctxt->event_pending )
            {
                ASSERT(ctxt->event.vector == EXC_PF);
        default:
                goto done;
            }
            /* Instead of the exception, ZF remains cleared. */
            rc = X86EMUL_OKAY;
            break;
        }
        if ( _regs._eflags & EFLG_ZF )
            dst.val = ((sreg.attr.bytes & 0xff) << 8) |
                      ((sreg.limit >> (sreg.attr.fields.g ? 12 : 0)) &
                       0xf0000) |
                      ((sreg.attr.bytes & 0xf00) << 12);
        else
            dst.type = OP_NONE;
        break;

    case X86EMUL_OPC(0x0f, 0x03): /* lsl */
        generate_exception_if(!in_protmode(ctxt, ops), EXC_UD);
        _regs._eflags &= ~EFLG_ZF;
        switch ( rc = protmode_load_seg(x86_seg_none, src.val, false, &sreg,
                                        ctxt, ops) )
        {
        case X86EMUL_OKAY:
            if ( !sreg.attr.fields.s )
            {
                switch ( sreg.attr.fields.type )
                {
                case 0x01: /* available 16-bit TSS */
                case 0x03: /* busy 16-bit TSS */
                    if ( in_longmode(ctxt, ops) )
                        break;
                    /* fall through */
                case 0x02: /* LDT */
                case 0x09: /* available 32/64-bit TSS */
                case 0x0b: /* busy 32/64-bit TSS */
                    _regs._eflags |= EFLG_ZF;
                    break;
                }
            }
            else
                _regs._eflags |= EFLG_ZF;
            break;
        case X86EMUL_EXCEPTION:
            if ( ctxt->event_pending )
            {
                ASSERT(ctxt->event.vector == EXC_PF);
        default:
                goto done;
            }
            /* Instead of the exception, ZF remains cleared. */
            rc = X86EMUL_OKAY;
            break;
        }
        if ( _regs._eflags & EFLG_ZF )
            dst.val = sreg.limit;
        else
            dst.type = OP_NONE;
        break;

    case X86EMUL_OPC(0x0f, 0x05): /* syscall */ {
        uint64_t msr_content;

        generate_exception_if(!in_protmode(ctxt, ops), EXC_UD);

        /* Inject #UD if syscall/sysret are disabled. */
        fail_if(ops->read_msr == NULL);
        if ( (rc = ops->read_msr(MSR_EFER, &msr_content, ctxt)) != 0 )
            goto done;
        generate_exception_if((msr_content & EFER_SCE) == 0, EXC_UD);

        if ( (rc = ops->read_msr(MSR_STAR, &msr_content, ctxt)) != 0 )
            goto done;

        cs.sel = (msr_content >> 32) & ~3; /* SELECTOR_RPL_MASK */
        sreg.sel = cs.sel + 8;

        cs.base = sreg.base = 0; /* flat segment */
        cs.limit = sreg.limit = ~0u;  /* 4GB limit */
        sreg.attr.bytes = 0xc93; /* G+DB+P+S+Data */

#ifdef __x86_64__
        rc = in_longmode(ctxt, ops);
        if ( rc < 0 )
            goto cannot_emulate;
        if ( rc )
        {
            cs.attr.bytes = 0xa9b; /* L+DB+P+S+Code */

            _regs.rcx = _regs.rip;
            _regs.r11 = _regs._eflags & ~EFLG_RF;

            if ( (rc = ops->read_msr(mode_64bit() ? MSR_LSTAR : MSR_CSTAR,
                                     &msr_content, ctxt)) != 0 )
                goto done;
            _regs.rip = msr_content;

            if ( (rc = ops->read_msr(MSR_FMASK, &msr_content, ctxt)) != 0 )
                goto done;
            _regs._eflags &= ~(msr_content | EFLG_RF);
        }
        else
#endif
        {
            cs.attr.bytes = 0xc9b; /* G+DB+P+S+Code */

            _regs.r(cx) = _regs._eip;
            _regs._eip = msr_content;
            _regs._eflags &= ~(EFLG_VM | EFLG_IF | EFLG_RF);
        }

        fail_if(ops->write_segment == NULL);
        if ( (rc = ops->write_segment(x86_seg_cs, &cs, ctxt)) ||
             (rc = ops->write_segment(x86_seg_ss, &sreg, ctxt)) )
            goto done;

        /*
         * SYSCALL (unlike most instructions) evaluates its singlestep action
         * based on the resulting EFLG_TF, not the starting EFLG_TF.
         *
         * As the #DB is raised after the CPL change and before the OS can
         * switch stack, it is a large risk for privilege escalation.
         *
         * 64bit kernels should mask EFLG_TF in MSR_FMASK to avoid any
         * vulnerability.  Running the #DB handler on an IST stack is also a
         * mitigation.
         *
         * 32bit kernels have no ability to mask EFLG_TF at all.  Their only
         * mitigation is to use a task gate for handling #DB (or to not use
         * enable EFER.SCE to start with).
         */
        singlestep = _regs._eflags & EFLG_TF;

        break;
    }

    case X86EMUL_OPC(0x0f, 0x06): /* clts */
        generate_exception_if(!mode_ring0(), EXC_GP, 0);
        fail_if((ops->read_cr == NULL) || (ops->write_cr == NULL));
        if ( (rc = ops->read_cr(0, &dst.val, ctxt)) != X86EMUL_OKAY ||
             (rc = ops->write_cr(0, dst.val & ~CR0_TS, ctxt)) != X86EMUL_OKAY )
            goto done;
        break;

    case X86EMUL_OPC(0x0f, 0x08): /* invd */
    case X86EMUL_OPC(0x0f, 0x09): /* wbinvd */
        generate_exception_if(!mode_ring0(), EXC_GP, 0);
        fail_if(ops->wbinvd == NULL);
        if ( (rc = ops->wbinvd(ctxt)) != 0 )
            goto done;
        break;

    case X86EMUL_OPC(0x0f, 0x0b): /* ud2 */
    case X86EMUL_OPC(0x0f, 0xb9): /* ud1 */
    case X86EMUL_OPC(0x0f, 0xff): /* ud0 */
        generate_exception(EXC_UD);

    case X86EMUL_OPC(0x0f, 0x0d): /* GrpP (prefetch) */
    case X86EMUL_OPC(0x0f, 0x18): /* Grp16 (prefetch/nop) */
    case X86EMUL_OPC(0x0f, 0x19) ... X86EMUL_OPC(0x0f, 0x1f): /* nop */
        break;

    case X86EMUL_OPC(0x0f, 0x2b):        /* movntps xmm,m128 */
    case X86EMUL_OPC_VEX(0x0f, 0x2b):    /* vmovntps xmm,m128 */
                                         /* vmovntps ymm,m256 */
    case X86EMUL_OPC_66(0x0f, 0x2b):     /* movntpd xmm,m128 */
    case X86EMUL_OPC_VEX_66(0x0f, 0x2b): /* vmovntpd xmm,m128 */
                                         /* vmovntpd ymm,m256 */
        fail_if(ea.type != OP_MEM);
        /* fall through */
    case X86EMUL_OPC(0x0f, 0x28):        /* movaps xmm/m128,xmm */
    case X86EMUL_OPC_VEX(0x0f, 0x28):    /* vmovaps xmm/m128,xmm */
                                         /* vmovaps ymm/m256,ymm */
    case X86EMUL_OPC_66(0x0f, 0x28):     /* movapd xmm/m128,xmm */
    case X86EMUL_OPC_VEX_66(0x0f, 0x28): /* vmovapd xmm/m128,xmm */
                                         /* vmovapd ymm/m256,ymm */
    case X86EMUL_OPC(0x0f, 0x29):        /* movaps xmm,xmm/m128 */
    case X86EMUL_OPC_VEX(0x0f, 0x29):    /* vmovaps xmm,xmm/m128 */
                                         /* vmovaps ymm,ymm/m256 */
    case X86EMUL_OPC_66(0x0f, 0x29):     /* movapd xmm,xmm/m128 */
    case X86EMUL_OPC_VEX_66(0x0f, 0x29): /* vmovapd xmm,xmm/m128 */
                                         /* vmovapd ymm,ymm/m256 */
    case X86EMUL_OPC(0x0f, 0x10):        /* movups xmm/m128,xmm */
    case X86EMUL_OPC_VEX(0x0f, 0x10):    /* vmovups xmm/m128,xmm */
                                         /* vmovups ymm/m256,ymm */
    case X86EMUL_OPC_66(0x0f, 0x10):     /* movupd xmm/m128,xmm */
    case X86EMUL_OPC_VEX_66(0x0f, 0x10): /* vmovupd xmm/m128,xmm */
                                         /* vmovupd ymm/m256,ymm */
    case X86EMUL_OPC_F3(0x0f, 0x10):     /* movss xmm/m32,xmm */
    case X86EMUL_OPC_VEX_F3(0x0f, 0x10): /* vmovss xmm/m32,xmm */
    case X86EMUL_OPC_F2(0x0f, 0x10):     /* movsd xmm/m64,xmm */
    case X86EMUL_OPC_VEX_F2(0x0f, 0x10): /* vmovsd xmm/m64,xmm */
    case X86EMUL_OPC(0x0f, 0x11):        /* movups xmm,xmm/m128 */
    case X86EMUL_OPC_VEX(0x0f, 0x11):    /* vmovups xmm,xmm/m128 */
                                         /* vmovups ymm,ymm/m256 */
    case X86EMUL_OPC_66(0x0f, 0x11):     /* movupd xmm,xmm/m128 */
    case X86EMUL_OPC_VEX_66(0x0f, 0x11): /* vmovupd xmm,xmm/m128 */
                                         /* vmovupd ymm,ymm/m256 */
    case X86EMUL_OPC_F3(0x0f, 0x11):     /* movss xmm,xmm/m32 */
    case X86EMUL_OPC_VEX_F3(0x0f, 0x11): /* vmovss xmm,xmm/m32 */
    case X86EMUL_OPC_F2(0x0f, 0x11):     /* movsd xmm,xmm/m64 */
    case X86EMUL_OPC_VEX_F2(0x0f, 0x11): /* vmovsd xmm,xmm/m64 */
    {
        uint8_t *buf = get_stub(stub);

        fic.insn_bytes = 5;
        buf[0] = 0x3e;
        buf[1] = 0x3e;
        buf[2] = 0x0f;
        buf[3] = b;
        buf[4] = modrm;
        buf[5] = 0xc3;
        if ( vex.opcx == vex_none )
        {
            if ( vex.pfx & VEX_PREFIX_DOUBLE_MASK )
                vcpu_must_have(sse2);
            else
                vcpu_must_have(sse);
            ea.bytes = 16;
            SET_SSE_PREFIX(buf[0], vex.pfx);
            get_fpu(X86EMUL_FPU_xmm, &fic);
        }
        else
        {
            fail_if((vex.reg != 0xf) &&
                    ((ea.type == OP_MEM) ||
                     !(vex.pfx & VEX_PREFIX_SCALAR_MASK)));
            host_and_vcpu_must_have(avx);
            get_fpu(X86EMUL_FPU_ymm, &fic);
            ea.bytes = 16 << vex.l;
        }
        if ( vex.pfx & VEX_PREFIX_SCALAR_MASK )
            ea.bytes = vex.pfx & VEX_PREFIX_DOUBLE_MASK ? 8 : 4;
        if ( ea.type == OP_MEM )
        {
            uint32_t mxcsr = 0;

            if ( b < 0x28 )
                mxcsr = MXCSR_MM;
            else if ( vcpu_has_misalignsse() )
                asm ( "stmxcsr %0" : "=m" (mxcsr) );
            generate_exception_if(!(mxcsr & MXCSR_MM) &&
                                  !is_aligned(ea.mem.seg, ea.mem.off, ea.bytes,
                                              ctxt, ops),
                                  EXC_GP, 0);
            if ( !(b & 1) )
                rc = ops->read(ea.mem.seg, ea.mem.off+0, mmvalp,
                               ea.bytes, ctxt);
            else
                fail_if(!ops->write); /* Check before running the stub. */
            /* convert memory operand to (%rAX) */
            rex_prefix &= ~REX_B;
            vex.b = 1;
            buf[4] &= 0x38;
        }
        if ( !rc )
        {
           copy_REX_VEX(buf, rex_prefix, vex);
           asm volatile ( "call *%0" : : "r" (stub.func), "a" (mmvalp)
                                     : "memory" );
        }
        put_fpu(&fic);
        put_stub(stub);
        if ( !rc && (b & 1) && (ea.type == OP_MEM) )
        {
            ASSERT(ops->write); /* See the fail_if() above. */
            rc = ops->write(ea.mem.seg, ea.mem.off, mmvalp,
                            ea.bytes, ctxt);
        }
        if ( rc )
            goto done;
        dst.type = OP_NONE;
        break;
    }

    case X86EMUL_OPC(0x0f, 0x20): /* mov cr,reg */
    case X86EMUL_OPC(0x0f, 0x21): /* mov dr,reg */
    case X86EMUL_OPC(0x0f, 0x22): /* mov reg,cr */
    case X86EMUL_OPC(0x0f, 0x23): /* mov reg,dr */
        generate_exception_if(!mode_ring0(), EXC_GP, 0);
        if ( b & 2 )
        {
            /* Write to CR/DR. */
            typeof(ops->write_cr) write = (b & 1) ? ops->write_dr
                                                  : ops->write_cr;

            fail_if(!write);
            rc = write(modrm_reg, src.val, ctxt);
        }
        else
        {
            /* Read from CR/DR. */
            typeof(ops->read_cr) read = (b & 1) ? ops->read_dr : ops->read_cr;

            fail_if(!read);
            rc = read(modrm_reg, &dst.val, ctxt);
        }
        if ( rc != X86EMUL_OKAY )
            goto done;
        break;

    case X86EMUL_OPC(0x0f, 0x30): /* wrmsr */
        generate_exception_if(!mode_ring0(), EXC_GP, 0);
        fail_if(ops->write_msr == NULL);
        if ( (rc = ops->write_msr(_regs._ecx,
                                  ((uint64_t)_regs.r(dx) << 32) | _regs._eax,
                                  ctxt)) != 0 )
            goto done;
        break;

    case X86EMUL_OPC(0x0f, 0x31): rdtsc: /* rdtsc */ {
        uint64_t val;

        if ( !mode_ring0() )
        {
            fail_if(ops->read_cr == NULL);
            if ( (rc = ops->read_cr(4, &cr4, ctxt)) )
                goto done;
            generate_exception_if(cr4 & CR4_TSD, EXC_GP, 0);
        }
        fail_if(ops->read_msr == NULL);
        if ( (rc = ops->read_msr(MSR_TSC, &val, ctxt)) != 0 )
            goto done;
        _regs.r(dx) = val >> 32;
        _regs.r(ax) = (uint32_t)val;
        break;
    }

    case X86EMUL_OPC(0x0f, 0x32): /* rdmsr */ {
        uint64_t val;
        generate_exception_if(!mode_ring0(), EXC_GP, 0);
        fail_if(ops->read_msr == NULL);
        if ( (rc = ops->read_msr(_regs._ecx, &val, ctxt)) != 0 )
            goto done;
        _regs.r(dx) = val >> 32;
        _regs.r(ax) = (uint32_t)val;
        break;
    }

    case X86EMUL_OPC(0x0f, 0x40) ... X86EMUL_OPC(0x0f, 0x4f): /* cmovcc */
        vcpu_must_have(cmov);
        if ( test_cc(b, _regs._eflags) )
            dst.val = src.val;
        break;

    case X86EMUL_OPC(0x0f, 0x34): /* sysenter */ {
        uint64_t msr_content;
        int lm;

        vcpu_must_have(sep);
        generate_exception_if(mode_ring0(), EXC_GP, 0);
        generate_exception_if(!in_protmode(ctxt, ops), EXC_GP, 0);

        fail_if(ops->read_msr == NULL);
        if ( (rc = ops->read_msr(MSR_SYSENTER_CS, &msr_content, ctxt)) != 0 )
            goto done;

        generate_exception_if(!(msr_content & 0xfffc), EXC_GP, 0);
        lm = in_longmode(ctxt, ops);
        if ( lm < 0 )
            goto cannot_emulate;

        _regs._eflags &= ~(EFLG_VM | EFLG_IF | EFLG_RF);

        cs.sel = msr_content & ~3; /* SELECTOR_RPL_MASK */
        cs.base = 0;   /* flat segment */
        cs.limit = ~0u;  /* 4GB limit */
        cs.attr.bytes = lm ? 0xa9b  /* G+L+P+S+Code */
                           : 0xc9b; /* G+DB+P+S+Code */

        sreg.sel = cs.sel + 8;
        sreg.base = 0;   /* flat segment */
        sreg.limit = ~0u;  /* 4GB limit */
        sreg.attr.bytes = 0xc93; /* G+DB+P+S+Data */

        fail_if(ops->write_segment == NULL);
        if ( (rc = ops->write_segment(x86_seg_cs, &cs, ctxt)) != 0 ||
             (rc = ops->write_segment(x86_seg_ss, &sreg, ctxt)) != 0 )
            goto done;

        if ( (rc = ops->read_msr(MSR_SYSENTER_EIP, &msr_content, ctxt)) != 0 )
            goto done;
        _regs.r(ip) = lm ? msr_content : (uint32_t)msr_content;

        if ( (rc = ops->read_msr(MSR_SYSENTER_ESP, &msr_content, ctxt)) != 0 )
            goto done;
        _regs.r(sp) = lm ? msr_content : (uint32_t)msr_content;

        singlestep = _regs._eflags & EFLG_TF;
        break;
    }

    case X86EMUL_OPC(0x0f, 0x35): /* sysexit */
    {
        uint64_t msr_content;

        vcpu_must_have(sep);
        generate_exception_if(!mode_ring0(), EXC_GP, 0);
        generate_exception_if(!in_protmode(ctxt, ops), EXC_GP, 0);

        fail_if(ops->read_msr == NULL);
        if ( (rc = ops->read_msr(MSR_SYSENTER_CS, &msr_content, ctxt)) != 0 )
            goto done;

        generate_exception_if(!(msr_content & 0xfffc), EXC_GP, 0);
        generate_exception_if(op_bytes == 8 &&
                              (!is_canonical_address(_regs.r(dx)) ||
                               !is_canonical_address(_regs.r(cx))),
                              EXC_GP, 0);

        cs.sel = (msr_content | 3) + /* SELECTOR_RPL_MASK */
                 (op_bytes == 8 ? 32 : 16);
        cs.base = 0;   /* flat segment */
        cs.limit = ~0u;  /* 4GB limit */
        cs.attr.bytes = op_bytes == 8 ? 0xafb  /* L+DB+P+DPL3+S+Code */
                                      : 0xcfb; /* G+DB+P+DPL3+S+Code */

        sreg.sel = cs.sel + 8;
        sreg.base = 0;   /* flat segment */
        sreg.limit = ~0u;  /* 4GB limit */
        sreg.attr.bytes = 0xcf3; /* G+DB+P+DPL3+S+Data */

        fail_if(ops->write_segment == NULL);
        if ( (rc = ops->write_segment(x86_seg_cs, &cs, ctxt)) != 0 ||
             (rc = ops->write_segment(x86_seg_ss, &sreg, ctxt)) != 0 )
            goto done;

        _regs.r(ip) = op_bytes == 8 ? _regs.r(dx) : _regs._edx;
        _regs.r(sp) = op_bytes == 8 ? _regs.r(cx) : _regs._ecx;

        singlestep = _regs._eflags & EFLG_TF;
        break;
    }

    case X86EMUL_OPC(0x0f, 0xe7):        /* movntq mm,m64 */
    case X86EMUL_OPC_66(0x0f, 0xe7):     /* movntdq xmm,m128 */
    case X86EMUL_OPC_VEX_66(0x0f, 0xe7): /* vmovntdq xmm,m128 */
                                         /* vmovntdq ymm,m256 */
        fail_if(ea.type != OP_MEM);
        /* fall through */
    case X86EMUL_OPC(0x0f, 0x6f):        /* movq mm/m64,mm */
    case X86EMUL_OPC_66(0x0f, 0x6f):     /* movdqa xmm/m128,xmm */
    case X86EMUL_OPC_F3(0x0f, 0x6f):     /* movdqu xmm/m128,xmm */
    case X86EMUL_OPC_VEX_66(0x0f, 0x6f): /* vmovdqa xmm/m128,xmm */
                                         /* vmovdqa ymm/m256,ymm */
    case X86EMUL_OPC_VEX_F3(0x0f, 0x6f): /* vmovdqu xmm/m128,xmm */
                                         /* vmovdqu ymm/m256,ymm */
    case X86EMUL_OPC(0x0f, 0x7e):        /* movd mm,r/m32 */
                                         /* movq mm,r/m64 */
    case X86EMUL_OPC_66(0x0f, 0x7e):     /* movd xmm,r/m32 */
                                         /* movq xmm,r/m64 */
    case X86EMUL_OPC_VEX_66(0x0f, 0x7e): /* vmovd xmm,r/m32 */
                                         /* vmovq xmm,r/m64 */
    case X86EMUL_OPC(0x0f, 0x7f):        /* movq mm,mm/m64 */
    case X86EMUL_OPC_66(0x0f, 0x7f):     /* movdqa xmm,xmm/m128 */
    case X86EMUL_OPC_VEX_66(0x0f, 0x7f): /* vmovdqa xmm,xmm/m128 */
                                         /* vmovdqa ymm,ymm/m256 */
    case X86EMUL_OPC_F3(0x0f, 0x7f):     /* movdqu xmm,xmm/m128 */
    case X86EMUL_OPC_VEX_F3(0x0f, 0x7f): /* vmovdqu xmm,xmm/m128 */
                                         /* vmovdqu ymm,ymm/m256 */
    case X86EMUL_OPC_66(0x0f, 0xd6):     /* movq xmm,xmm/m64 */
    case X86EMUL_OPC_VEX_66(0x0f, 0xd6): /* vmovq xmm,xmm/m64 */
    {
        uint8_t *buf = get_stub(stub);

        fic.insn_bytes = 5;
        buf[0] = 0x3e;
        buf[1] = 0x3e;
        buf[2] = 0x0f;
        buf[3] = b;
        buf[4] = modrm;
        buf[5] = 0xc3;
        if ( vex.opcx == vex_none )
        {
            switch ( vex.pfx )
            {
            case vex_66:
            case vex_f3:
                vcpu_must_have(sse2);
                /* Converting movdqu to movdqa here: Our buffer is aligned. */
                buf[0] = 0x66;
                get_fpu(X86EMUL_FPU_xmm, &fic);
                ea.bytes = 16;
                break;
            case vex_none:
                if ( b != 0xe7 )
                    host_and_vcpu_must_have(mmx);
                else
                    vcpu_must_have(sse);
                get_fpu(X86EMUL_FPU_mmx, &fic);
                ea.bytes = 8;
                break;
            default:
                goto cannot_emulate;
            }
        }
        else
        {
            fail_if(vex.reg != 0xf);
            host_and_vcpu_must_have(avx);
            get_fpu(X86EMUL_FPU_ymm, &fic);
            ea.bytes = 16 << vex.l;
        }
        switch ( b )
        {
        case 0x7e:
            generate_exception_if(vex.l, EXC_UD);
            ea.bytes = op_bytes;
            break;
        case 0xd6:
            generate_exception_if(vex.l, EXC_UD);
            ea.bytes = 8;
            break;
        }
        if ( ea.type == OP_MEM )
        {
            uint32_t mxcsr = 0;

            if ( ea.bytes < 16 || vex.pfx == vex_f3 )
                mxcsr = MXCSR_MM;
            else if ( vcpu_has_misalignsse() )
                asm ( "stmxcsr %0" : "=m" (mxcsr) );
            generate_exception_if(!(mxcsr & MXCSR_MM) &&
                                  !is_aligned(ea.mem.seg, ea.mem.off, ea.bytes,
                                              ctxt, ops),
                                  EXC_GP, 0);
            if ( b == 0x6f )
                rc = ops->read(ea.mem.seg, ea.mem.off+0, mmvalp,
                               ea.bytes, ctxt);
            else
                fail_if(!ops->write); /* Check before running the stub. */
        }
        if ( ea.type == OP_MEM || b == 0x7e )
        {
            /* Convert memory operand or GPR destination to (%rAX) */
            rex_prefix &= ~REX_B;
            vex.b = 1;
            buf[4] &= 0x38;
            if ( ea.type == OP_MEM )
                ea.reg = (void *)mmvalp;
            else /* Ensure zero-extension of a 32-bit result. */
                *ea.reg = 0;
        }
        if ( !rc )
        {
           copy_REX_VEX(buf, rex_prefix, vex);
           asm volatile ( "call *%0" : : "r" (stub.func), "a" (ea.reg)
                                     : "memory" );
        }
        put_fpu(&fic);
        put_stub(stub);
        if ( !rc && (b != 0x6f) && (ea.type == OP_MEM) )
        {
            ASSERT(ops->write); /* See the fail_if() above. */
            rc = ops->write(ea.mem.seg, ea.mem.off, mmvalp,
                            ea.bytes, ctxt);
        }
        if ( rc )
            goto done;
        dst.type = OP_NONE;
        break;
    }

    case X86EMUL_OPC(0x0f, 0x80) ... X86EMUL_OPC(0x0f, 0x8f): /* jcc (near) */
        if ( test_cc(b, _regs._eflags) )
            jmp_rel((int32_t)src.val);
        adjust_bnd(ctxt, ops, vex.pfx);
        break;

    case X86EMUL_OPC(0x0f, 0x90) ... X86EMUL_OPC(0x0f, 0x9f): /* setcc */
        dst.val = test_cc(b, _regs._eflags);
        break;

    case X86EMUL_OPC(0x0f, 0xa2): /* cpuid */
        fail_if(ops->cpuid == NULL);
        rc = ops->cpuid(_regs._eax, _regs._ecx, &cpuid_leaf, ctxt);
        generate_exception_if(rc == X86EMUL_EXCEPTION,
                              EXC_GP, 0); /* CPUID Faulting? */
        if ( rc != X86EMUL_OKAY )
            goto done;
        _regs.r(ax) = cpuid_leaf.a;
        _regs.r(bx) = cpuid_leaf.b;
        _regs.r(cx) = cpuid_leaf.c;
        _regs.r(dx) = cpuid_leaf.d;
        break;

    case X86EMUL_OPC(0x0f, 0xa3): bt: /* bt */
        generate_exception_if(lock_prefix, EXC_UD);
        emulate_2op_SrcV_nobyte("bt", src, dst, _regs._eflags);
        dst.type = OP_NONE;
        break;

    case X86EMUL_OPC(0x0f, 0xa4): /* shld imm8,r,r/m */
    case X86EMUL_OPC(0x0f, 0xa5): /* shld %%cl,r,r/m */
    case X86EMUL_OPC(0x0f, 0xac): /* shrd imm8,r,r/m */
    case X86EMUL_OPC(0x0f, 0xad): /* shrd %%cl,r,r/m */ {
        uint8_t shift, width = dst.bytes << 3;

        generate_exception_if(lock_prefix, EXC_UD);
        if ( b & 1 )
            shift = _regs.cl;
        else
        {
            shift = src.val;
            src.reg = decode_register(modrm_reg, &_regs, 0);
            src.val = truncate_word(*src.reg, dst.bytes);
        }
        if ( (shift &= width - 1) == 0 )
            break;
        dst.orig_val = truncate_word(dst.val, dst.bytes);
        dst.val = ((shift == width) ? src.val :
                   (b & 8) ?
                   /* shrd */
                   ((dst.orig_val >> shift) |
                    truncate_word(src.val << (width - shift), dst.bytes)) :
                   /* shld */
                   ((dst.orig_val << shift) |
                    ((src.val >> (width - shift)) & ((1ull << shift) - 1))));
        dst.val = truncate_word(dst.val, dst.bytes);
        _regs._eflags &= ~(EFLG_OF|EFLG_SF|EFLG_ZF|EFLG_PF|EFLG_CF);
        if ( (dst.val >> ((b & 8) ? (shift - 1) : (width - shift))) & 1 )
            _regs._eflags |= EFLG_CF;
        if ( ((dst.val ^ dst.orig_val) >> (width - 1)) & 1 )
            _regs._eflags |= EFLG_OF;
        _regs._eflags |= ((dst.val >> (width - 1)) & 1) ? EFLG_SF : 0;
        _regs._eflags |= (dst.val == 0) ? EFLG_ZF : 0;
        _regs._eflags |= even_parity(dst.val) ? EFLG_PF : 0;
        break;
    }

    case X86EMUL_OPC(0x0f, 0xab): bts: /* bts */
        emulate_2op_SrcV_nobyte("bts", src, dst, _regs._eflags);
        break;

    case X86EMUL_OPC(0x0f, 0xae): case X86EMUL_OPC_66(0x0f, 0xae): /* Grp15 */
        switch ( modrm_reg & 7 )
        {
        case 5: /* lfence */
            fail_if(modrm_mod != 3);
            generate_exception_if(vex.pfx, EXC_UD);
            vcpu_must_have(sse2);
            asm volatile ( "lfence" ::: "memory" );
            break;
        case 6:
            if ( modrm_mod == 3 ) /* mfence */
            {
                generate_exception_if(vex.pfx, EXC_UD);
                vcpu_must_have(sse2);
                asm volatile ( "mfence" ::: "memory" );
                break;
            }
            /* else clwb */
            fail_if(!vex.pfx);
            vcpu_must_have(clwb);
            fail_if(!ops->wbinvd);
            if ( (rc = ops->wbinvd(ctxt)) != X86EMUL_OKAY )
                goto done;
            break;
        case 7:
            if ( modrm_mod == 3 ) /* sfence */
            {
                generate_exception_if(vex.pfx, EXC_UD);
                vcpu_must_have(sse);
                asm volatile ( "sfence" ::: "memory" );
                break;
            }
            /* else clflush{,opt} */
            if ( !vex.pfx )
                vcpu_must_have(clflush);
            else
                vcpu_must_have(clflushopt);
            fail_if(ops->wbinvd == NULL);
            if ( (rc = ops->wbinvd(ctxt)) != 0 )
                goto done;
            break;
        default:
            goto cannot_emulate;
        }
        break;

    case X86EMUL_OPC_F3(0x0f, 0xae): /* Grp15 */
        fail_if(modrm_mod != 3);
        generate_exception_if((modrm_reg & 4) || !mode_64bit(), EXC_UD);
        fail_if(!ops->read_cr);
        if ( (rc = ops->read_cr(4, &cr4, ctxt)) != X86EMUL_OKAY )
            goto done;
        generate_exception_if(!(cr4 & CR4_FSGSBASE), EXC_UD);
        seg = modrm_reg & 1 ? x86_seg_gs : x86_seg_fs;
        fail_if(!ops->read_segment);
        if ( (rc = ops->read_segment(seg, &sreg, ctxt)) != X86EMUL_OKAY )
            goto done;
        dst.reg = decode_register(modrm_rm, &_regs, 0);
        if ( !(modrm_reg & 2) )
        {
            /* rd{f,g}sbase */
            dst.type = OP_REG;
            dst.bytes = (op_bytes == 8) ? 8 : 4;
            dst.val = sreg.base;
        }
        else
        {
            /* wr{f,g}sbase */
            if ( op_bytes == 8 )
            {
                sreg.base = *dst.reg;
                generate_exception_if(!is_canonical_address(sreg.base),
                                      EXC_GP, 0);
            }
            else
                sreg.base = (uint32_t)*dst.reg;
            fail_if(!ops->write_segment);
            if ( (rc = ops->write_segment(seg, &sreg, ctxt)) != X86EMUL_OKAY )
                goto done;
        }
        break;

    case X86EMUL_OPC(0x0f, 0xaf): /* imul */
        emulate_2op_SrcV_srcmem("imul", src, dst, _regs._eflags);
        break;

    case X86EMUL_OPC(0x0f, 0xb0): case X86EMUL_OPC(0x0f, 0xb1): /* cmpxchg */
        /* Save real source value, then compare EAX against destination. */
        src.orig_val = src.val;
        src.val = _regs.r(ax);
        /* cmp: %%eax - dst ==> dst and src swapped for macro invocation */
        emulate_2op_SrcV("cmp", dst, src, _regs._eflags);
        if ( _regs._eflags & EFLG_ZF )
        {
            /* Success: write back to memory. */
            dst.val = src.orig_val;
        }
        else
        {
            /* Failure: write the value we saw to EAX. */
            dst.type = OP_REG;
            dst.reg  = (unsigned long *)&_regs.r(ax);
        }
        break;

    case X86EMUL_OPC(0x0f, 0xb2): /* lss */
    case X86EMUL_OPC(0x0f, 0xb4): /* lfs */
    case X86EMUL_OPC(0x0f, 0xb5): /* lgs */
        seg = b & 7;
        goto les;

    case X86EMUL_OPC(0x0f, 0xb3): btr: /* btr */
        emulate_2op_SrcV_nobyte("btr", src, dst, _regs._eflags);
        break;

    case X86EMUL_OPC(0x0f, 0xb6): /* movzx rm8,r{16,32,64} */
        /* Recompute DstReg as we may have decoded AH/BH/CH/DH. */
        dst.reg   = decode_register(modrm_reg, &_regs, 0);
        dst.bytes = op_bytes;
        dst.val   = (uint8_t)src.val;
        break;

    case X86EMUL_OPC(0x0f, 0xb7): /* movzx rm16,r{16,32,64} */
        dst.val = (uint16_t)src.val;
        break;

    case X86EMUL_OPC_F3(0x0f, 0xb8): /* popcnt r/m,r */
        host_and_vcpu_must_have(popcnt);
        asm ( "popcnt %1,%0" : "=r" (dst.val) : "rm" (src.val) );
        _regs._eflags &= ~EFLAGS_MASK;
        if ( !dst.val )
            _regs._eflags |= EFLG_ZF;
        break;

    case X86EMUL_OPC(0x0f, 0xba): /* Grp8 */
        switch ( modrm_reg & 7 )
        {
        case 4: goto bt;
        case 5: goto bts;
        case 6: goto btr;
        case 7: goto btc;
        default: generate_exception(EXC_UD);
        }
        break;

    case X86EMUL_OPC(0x0f, 0xbb): btc: /* btc */
        emulate_2op_SrcV_nobyte("btc", src, dst, _regs._eflags);
        break;

    case X86EMUL_OPC(0x0f, 0xbc): /* bsf or tzcnt */
    {
        bool zf;

        asm ( "bsf %2,%0" ASM_FLAG_OUT(, "; setz %1")
              : "=r" (dst.val), ASM_FLAG_OUT("=@ccz", "=qm") (zf)
              : "rm" (src.val) );
        _regs._eflags &= ~EFLG_ZF;
        if ( (vex.pfx == vex_f3) && vcpu_has_bmi1() )
        {
            _regs._eflags &= ~EFLG_CF;
            if ( zf )
            {
                _regs._eflags |= EFLG_CF;
                dst.val = op_bytes * 8;
            }
            else if ( !dst.val )
                _regs._eflags |= EFLG_ZF;
        }
        else if ( zf )
        {
            _regs._eflags |= EFLG_ZF;
            dst.type = OP_NONE;
        }
        break;
    }

    case X86EMUL_OPC(0x0f, 0xbd): /* bsr or lzcnt */
    {
        bool zf;

        asm ( "bsr %2,%0" ASM_FLAG_OUT(, "; setz %1")
              : "=r" (dst.val), ASM_FLAG_OUT("=@ccz", "=qm") (zf)
              : "rm" (src.val) );
        _regs._eflags &= ~EFLG_ZF;
        if ( (vex.pfx == vex_f3) && vcpu_has_lzcnt() )
        {
            _regs._eflags &= ~EFLG_CF;
            if ( zf )
            {
                _regs._eflags |= EFLG_CF;
                dst.val = op_bytes * 8;
            }
            else
            {
                dst.val = op_bytes * 8 - 1 - dst.val;
                if ( !dst.val )
                    _regs._eflags |= EFLG_ZF;
            }
        }
        else if ( zf )
        {
            _regs._eflags |= EFLG_ZF;
            dst.type = OP_NONE;
        }
        break;
    }

    case X86EMUL_OPC(0x0f, 0xbe): /* movsx rm8,r{16,32,64} */
        /* Recompute DstReg as we may have decoded AH/BH/CH/DH. */
        dst.reg   = decode_register(modrm_reg, &_regs, 0);
        dst.bytes = op_bytes;
        dst.val   = (int8_t)src.val;
        break;

    case X86EMUL_OPC(0x0f, 0xbf): /* movsx rm16,r{16,32,64} */
        dst.val = (int16_t)src.val;
        break;

    case X86EMUL_OPC(0x0f, 0xc0): case X86EMUL_OPC(0x0f, 0xc1): /* xadd */
        /* Write back the register source. */
        switch ( dst.bytes )
        {
        case 1: *(uint8_t  *)src.reg = (uint8_t)dst.val; break;
        case 2: *(uint16_t *)src.reg = (uint16_t)dst.val; break;
        case 4: *src.reg = (uint32_t)dst.val; break; /* 64b reg: zero-extend */
        case 8: *src.reg = dst.val; break;
        }
        goto add;

    case X86EMUL_OPC(0x0f, 0xc3): /* movnti */
        /* Ignore the non-temporal hint for now. */
        vcpu_must_have(sse2);
        dst.val = src.val;
        break;

    case X86EMUL_OPC(0x0f, 0xc7): /* Grp9 */
    {
        union {
            uint32_t u32[2];
            uint64_t u64[2];
        } *old, *aux;

        if ( ea.type == OP_REG )
        {
            bool __maybe_unused carry;

            switch ( modrm_reg & 7 )
            {
            default:
                goto cannot_emulate;

#ifdef HAVE_GAS_RDRAND
            case 6: /* rdrand */
                generate_exception_if(rep_prefix(), EXC_UD);
                host_and_vcpu_must_have(rdrand);
                dst = ea;
                switch ( op_bytes )
                {
                case 2:
                    asm ( "rdrand %w0" ASM_FLAG_OUT(, "; setc %1")
                          : "=r" (dst.val), ASM_FLAG_OUT("=@ccc", "=qm") (carry) );
                    break;
                default:
# ifdef __x86_64__
                    asm ( "rdrand %k0" ASM_FLAG_OUT(, "; setc %1")
                          : "=r" (dst.val), ASM_FLAG_OUT("=@ccc", "=qm") (carry) );
                    break;
                case 8:
# endif
                    asm ( "rdrand %0" ASM_FLAG_OUT(, "; setc %1")
                          : "=r" (dst.val), ASM_FLAG_OUT("=@ccc", "=qm") (carry) );
                    break;
                }
                _regs._eflags &= ~EFLAGS_MASK;
                if ( carry )
                    _regs._eflags |= EFLG_CF;
                break;
#endif

            case 7: /* rdseed / rdpid */
                if ( repe_prefix() ) /* rdpid */
                {
                    uint64_t tsc_aux;

                    generate_exception_if(ea.type != OP_REG, EXC_UD);
                    vcpu_must_have(rdpid);
                    fail_if(!ops->read_msr);
                    if ( (rc = ops->read_msr(MSR_TSC_AUX, &tsc_aux,
                                             ctxt)) != X86EMUL_OKAY )
                        goto done;
                    dst = ea;
                    dst.val = tsc_aux;
                    dst.bytes = 4;
                    break;
                }
#ifdef HAVE_GAS_RDSEED
                generate_exception_if(rep_prefix(), EXC_UD);
                host_and_vcpu_must_have(rdseed);
                dst = ea;
                switch ( op_bytes )
                {
                case 2:
                    asm ( "rdseed %w0" ASM_FLAG_OUT(, "; setc %1")
                          : "=r" (dst.val), ASM_FLAG_OUT("=@ccc", "=qm") (carry) );
                    break;
                default:
# ifdef __x86_64__
                    asm ( "rdseed %k0" ASM_FLAG_OUT(, "; setc %1")
                          : "=r" (dst.val), ASM_FLAG_OUT("=@ccc", "=qm") (carry) );
                    break;
                case 8:
# endif
                    asm ( "rdseed %0" ASM_FLAG_OUT(, "; setc %1")
                          : "=r" (dst.val), ASM_FLAG_OUT("=@ccc", "=qm") (carry) );
                    break;
                }
                _regs._eflags &= ~EFLAGS_MASK;
                if ( carry )
                    _regs._eflags |= EFLG_CF;
                break;
#endif
            }
            break;
        }

        /* cmpxchg8b/cmpxchg16b */
        generate_exception_if((modrm_reg & 7) != 1, EXC_UD);
        fail_if(!ops->cmpxchg);
        if ( rex_prefix & REX_W )
        {
            host_and_vcpu_must_have(cx16);
            generate_exception_if(!is_aligned(ea.mem.seg, ea.mem.off, 16,
                                              ctxt, ops),
                                  EXC_GP, 0);
            op_bytes = 16;
        }
        else
        {
            vcpu_must_have(cx8);
            op_bytes = 8;
        }

        old = container_of(&mmvalp->ymm[0], typeof(*old), u64[0]);
        aux = container_of(&mmvalp->ymm[2], typeof(*aux), u64[0]);

        /* Get actual old value. */
        if ( (rc = ops->read(ea.mem.seg, ea.mem.off, old, op_bytes,
                             ctxt)) != X86EMUL_OKAY )
            goto done;

        /* Get expected value. */
        if ( !(rex_prefix & REX_W) )
        {
            aux->u32[0] = _regs._eax;
            aux->u32[1] = _regs._edx;
        }
        else
        {
            aux->u64[0] = _regs.r(ax);
            aux->u64[1] = _regs.r(dx);
        }

        if ( memcmp(old, aux, op_bytes) )
        {
            /* Expected != actual: store actual to rDX:rAX and clear ZF. */
            _regs.r(ax) = !(rex_prefix & REX_W) ? old->u32[0] : old->u64[0];
            _regs.r(dx) = !(rex_prefix & REX_W) ? old->u32[1] : old->u64[1];
            _regs._eflags &= ~EFLG_ZF;
        }
        else
        {
            /*
             * Expected == actual: Get proposed value, attempt atomic cmpxchg
             * and set ZF.
             */
            if ( !(rex_prefix & REX_W) )
            {
                aux->u32[0] = _regs._ebx;
                aux->u32[1] = _regs._ecx;
            }
            else
            {
                aux->u64[0] = _regs.r(bx);
                aux->u64[1] = _regs.r(cx);
            }

            if ( (rc = ops->cmpxchg(ea.mem.seg, ea.mem.off, old, aux,
                                    op_bytes, ctxt)) != X86EMUL_OKAY )
                goto done;
            _regs._eflags |= EFLG_ZF;
        }
        break;
    }

    case X86EMUL_OPC(0x0f, 0xc8) ... X86EMUL_OPC(0x0f, 0xcf): /* bswap */
        dst.type = OP_REG;
        dst.reg  = decode_register(
            (b & 7) | ((rex_prefix & 1) << 3), &_regs, 0);
        switch ( dst.bytes = op_bytes )
        {
        default: /* case 2: */
            /* Undefined behaviour. Writes zero on all tested CPUs. */
            dst.val = 0;
            break;
        case 4:
#ifdef __x86_64__
            asm ( "bswap %k0" : "=r" (dst.val) : "0" (*(uint32_t *)dst.reg) );
            break;
        case 8:
#endif
            asm ( "bswap %0" : "=r" (dst.val) : "0" (*dst.reg) );
            break;
        }
        break;

    case X86EMUL_OPC(0x0f38, 0xf0): /* movbe m,r */
    case X86EMUL_OPC(0x0f38, 0xf1): /* movbe r,m */
        vcpu_must_have(movbe);
        switch ( op_bytes )
        {
        case 2:
            asm ( "xchg %h0,%b0" : "=Q" (dst.val)
                                 : "0" (*(uint32_t *)&src.val) );
            break;
        case 4:
#ifdef __x86_64__
            asm ( "bswap %k0" : "=r" (dst.val)
                              : "0" (*(uint32_t *)&src.val) );
            break;
        case 8:
#endif
            asm ( "bswap %0" : "=r" (dst.val) : "0" (src.val) );
            break;
        default:
            ASSERT_UNREACHABLE();
        }
        break;
#ifdef HAVE_GAS_SSE4_2
    case X86EMUL_OPC_F2(0x0f38, 0xf0): /* crc32 r/m8, r{32,64} */
    case X86EMUL_OPC_F2(0x0f38, 0xf1): /* crc32 r/m{16,32,64}, r{32,64} */
        host_and_vcpu_must_have(sse4_2);
        dst.bytes = rex_prefix & REX_W ? 8 : 4;
        switch ( op_bytes )
        {
        case 1:
            asm ( "crc32b %1,%k0" : "+r" (dst.val)
                                  : "qm" (*(uint8_t *)&src.val) );
            break;
        case 2:
            asm ( "crc32w %1,%k0" : "+r" (dst.val)
                                  : "rm" (*(uint16_t *)&src.val) );
            break;
        case 4:
            asm ( "crc32l %1,%k0" : "+r" (dst.val)
                                  : "rm" (*(uint32_t *)&src.val) );
            break;
# ifdef __x86_64__
        case 8:
            asm ( "crc32q %1,%0" : "+r" (dst.val) : "rm" (src.val) );
            break;
# endif
        default:
            ASSERT_UNREACHABLE();
        }
        break;
#endif

    case X86EMUL_OPC_VEX(0x0f38, 0xf2):    /* andn r/m,r,r */
    case X86EMUL_OPC_VEX(0x0f38, 0xf5):    /* bzhi r,r/m,r */
    case X86EMUL_OPC_VEX_F3(0x0f38, 0xf5): /* pext r/m,r,r */
    case X86EMUL_OPC_VEX_F2(0x0f38, 0xf5): /* pdep r/m,r,r */
    case X86EMUL_OPC_VEX(0x0f38, 0xf7):    /* bextr r,r/m,r */
    case X86EMUL_OPC_VEX_66(0x0f38, 0xf7): /* shlx r,r/m,r */
    case X86EMUL_OPC_VEX_F3(0x0f38, 0xf7): /* sarx r,r/m,r */
    case X86EMUL_OPC_VEX_F2(0x0f38, 0xf7): /* shrx r,r/m,r */
    {
        uint8_t *buf = get_stub(stub);
        typeof(vex) *pvex = container_of(buf + 1, typeof(vex), raw[0]);

        if ( b == 0xf5 || vex.pfx )
            host_and_vcpu_must_have(bmi2);
        else
            host_and_vcpu_must_have(bmi1);
        generate_exception_if(vex.l, EXC_UD);

        buf[0] = 0xc4;
        *pvex = vex;
        pvex->b = 1;
        pvex->r = 1;
        pvex->reg = 0xf; /* rAX */
        buf[3] = b;
        buf[4] = 0x09; /* reg=rCX r/m=(%rCX) */
        buf[5] = 0xc3;

        src.reg = decode_vex_gpr(vex.reg, &_regs, ctxt);
        emulate_stub([dst] "=&c" (dst.val), "[dst]" (&src.val), "a" (*src.reg));

        put_stub(stub);
        break;
    }

    case X86EMUL_OPC_VEX(0x0f38, 0xf3): /* Grp 17 */
    {
        uint8_t *buf = get_stub(stub);
        typeof(vex) *pvex = container_of(buf + 1, typeof(vex), raw[0]);

        switch ( modrm_reg & 7 )
        {
        case 1: /* blsr r,r/m */
        case 2: /* blsmsk r,r/m */
        case 3: /* blsi r,r/m */
            host_and_vcpu_must_have(bmi1);
            break;
        default:
            goto cannot_emulate;
        }

        generate_exception_if(vex.l, EXC_UD);

        buf[0] = 0xc4;
        *pvex = vex;
        pvex->b = 1;
        pvex->r = 1;
        pvex->reg = 0xf; /* rAX */
        buf[3] = b;
        buf[4] = (modrm & 0x38) | 0x01; /* r/m=(%rCX) */
        buf[5] = 0xc3;

        dst.reg = decode_vex_gpr(vex.reg, &_regs, ctxt);
        emulate_stub("=&a" (dst.val), "c" (&src.val));

        put_stub(stub);
        break;
    }

    case X86EMUL_OPC_66(0x0f38, 0xf6): /* adcx r/m,r */
    case X86EMUL_OPC_F3(0x0f38, 0xf6): /* adox r/m,r */
    {
        unsigned int mask = rep_prefix() ? EFLG_OF : EFLG_CF;
        unsigned int aux = _regs._eflags & mask ? ~0 : 0;
        bool carry;

        vcpu_must_have(adx);
#ifdef __x86_64__
        if ( op_bytes == 8 )
            asm ( "add %[aux],%[aux]\n\t"
                  "adc %[src],%[dst]\n\t"
                  ASM_FLAG_OUT(, "setc %[carry]")
                  : [dst] "+r" (dst.val),
                    [carry] ASM_FLAG_OUT("=@ccc", "=qm") (carry),
                    [aux] "+r" (aux)
                  : [src] "rm" (src.val) );
        else
#endif
            asm ( "add %[aux],%[aux]\n\t"
                  "adc %k[src],%k[dst]\n\t"
                  ASM_FLAG_OUT(, "setc %[carry]")
                  : [dst] "+r" (dst.val),
                    [carry] ASM_FLAG_OUT("=@ccc", "=qm") (carry),
                    [aux] "+r" (aux)
                  : [src] "rm" (src.val) );
        if ( carry )
            _regs._eflags |= mask;
        else
            _regs._eflags &= ~mask;
        break;
    }

    case X86EMUL_OPC_VEX_F2(0x0f38, 0xf6): /* mulx r/m,r,r */
        vcpu_must_have(bmi2);
        generate_exception_if(vex.l, EXC_UD);
        ea.reg = decode_vex_gpr(vex.reg, &_regs, ctxt);
        if ( mode_64bit() && vex.w )
            asm ( "mulq %3" : "=a" (*ea.reg), "=d" (dst.val)
                            : "0" (src.val), "rm" (_regs.r(dx)) );
        else
            asm ( "mull %3" : "=a" (*ea.reg), "=d" (dst.val)
                            : "0" ((uint32_t)src.val), "rm" (_regs._edx) );
        break;

    case X86EMUL_OPC_VEX_F2(0x0f3a, 0xf0): /* rorx imm,r/m,r */
        vcpu_must_have(bmi2);
        generate_exception_if(vex.l || vex.reg != 0xf, EXC_UD);
        if ( ea.type == OP_REG )
            src.val = *ea.reg;
        else if ( (rc = read_ulong(ea.mem.seg, ea.mem.off, &src.val, op_bytes,
                                   ctxt, ops)) != X86EMUL_OKAY )
            goto done;
        if ( mode_64bit() && vex.w )
            asm ( "rorq %b1,%0" : "=g" (dst.val) : "c" (imm1), "0" (src.val) );
        else
            asm ( "rorl %b1,%k0" : "=g" (dst.val) : "c" (imm1), "0" (src.val) );
        break;

    case X86EMUL_OPC_XOP(09, 0x01): /* XOP Grp1 */
        switch ( modrm_reg & 7 )
        {
        case 1: /* blcfill r/m,r */
        case 2: /* blsfill r/m,r */
        case 3: /* blcs r/m,r */
        case 4: /* tzmsk r/m,r */
        case 5: /* blcic r/m,r */
        case 6: /* blsic r/m,r */
        case 7: /* t1mskc r/m,r */
            host_and_vcpu_must_have(tbm);
            break;
        default:
            goto cannot_emulate;
        }

    xop_09_rm_rv:
    {
        uint8_t *buf = get_stub(stub);
        typeof(vex) *pxop = container_of(buf + 1, typeof(vex), raw[0]);

        generate_exception_if(vex.l, EXC_UD);

        buf[0] = 0x8f;
        *pxop = vex;
        pxop->b = 1;
        pxop->r = 1;
        pxop->reg = 0xf; /* rAX */
        buf[3] = b;
        buf[4] = (modrm & 0x38) | 0x01; /* r/m=(%rCX) */
        buf[5] = 0xc3;

        dst.reg = decode_vex_gpr(vex.reg, &_regs, ctxt);
        emulate_stub([dst] "=&a" (dst.val), "c" (&src.val));

        put_stub(stub);
        break;
    }

    case X86EMUL_OPC_XOP(09, 0x02): /* XOP Grp2 */
        switch ( modrm_reg & 7 )
        {
        case 1: /* blcmsk r/m,r */
        case 6: /* blci r/m,r */
            host_and_vcpu_must_have(tbm);
            goto xop_09_rm_rv;
        }
        goto cannot_emulate;

    case X86EMUL_OPC_XOP(0a, 0x10): /* bextr imm,r/m,r */
    {
        uint8_t *buf = get_stub(stub);
        typeof(vex) *pxop = container_of(buf + 1, typeof(vex), raw[0]);

        host_and_vcpu_must_have(tbm);
        generate_exception_if(vex.l || vex.reg != 0xf, EXC_UD);

        if ( ea.type == OP_REG )
            src.val = *ea.reg;
        else if ( (rc = read_ulong(ea.mem.seg, ea.mem.off, &src.val, op_bytes,
                                   ctxt, ops)) != X86EMUL_OKAY )
            goto done;

        buf[0] = 0x8f;
        *pxop = vex;
        pxop->b = 1;
        pxop->r = 1;
        buf[3] = b;
        buf[4] = 0x09; /* reg=rCX r/m=(%rCX) */
        *(uint32_t *)(buf + 5) = imm1;
        buf[9] = 0xc3;

        emulate_stub([dst] "=&c" (dst.val), "[dst]" (&src.val));

        put_stub(stub);
        break;
    }

    default:
        goto cannot_emulate;
    }

    switch ( dst.type )
    {
    case OP_REG:
        /* The 4-byte case *is* correct: in 64-bit mode we zero-extend. */
        switch ( dst.bytes )
        {
        case 1: *(uint8_t  *)dst.reg = (uint8_t)dst.val; break;
        case 2: *(uint16_t *)dst.reg = (uint16_t)dst.val; break;
        case 4: *dst.reg = (uint32_t)dst.val; break; /* 64b: zero-ext */
        case 8: *dst.reg = dst.val; break;
        }
        break;
    case OP_MEM:
        if ( !(d & Mov) && (dst.orig_val == dst.val) &&
             !ctxt->force_writeback )
            /* nothing to do */;
        else if ( lock_prefix )
        {
            fail_if(!ops->cmpxchg);
            rc = ops->cmpxchg(
                dst.mem.seg, dst.mem.off, &dst.orig_val,
                &dst.val, dst.bytes, ctxt);
        }
        else
        {
            fail_if(!ops->write);
            rc = ops->write(
                dst.mem.seg, dst.mem.off, &dst.val, dst.bytes, ctxt);
        }
        if ( rc != 0 )
            goto done;
    default:
        break;
    }

 complete_insn: /* Commit shadow register state. */
    /* Zero the upper 32 bits of %rip if not in 64-bit mode. */
    if ( !mode_64bit() )
        _regs.r(ip) = _regs._eip;

    /* Should a singlestep #DB be raised? */
    if ( rc == X86EMUL_OKAY && singlestep && !ctxt->retire.mov_ss )
    {
        ctxt->retire.singlestep = true;
        ctxt->retire.sti = false;
    }

    if ( rc != X86EMUL_DONE )
        *ctxt->regs = _regs;
    else
    {
        ctxt->regs->r(ip) = _regs.r(ip);
        rc = X86EMUL_OKAY;
    }

    ctxt->regs->_eflags &= ~EFLG_RF;

 done:
    _put_fpu();
    put_stub(stub);
    return rc;

 cannot_emulate:
    _put_fpu();
    put_stub(stub);
    return X86EMUL_UNHANDLEABLE;
#undef state
}

#undef op_bytes
#undef ad_bytes
#undef ext
#undef modrm
#undef modrm_mod
#undef modrm_reg
#undef modrm_rm
#undef rex_prefix
#undef lock_prefix
#undef vex
#undef ea

static void __init __maybe_unused build_assertions(void)
{
    /* Check the values against SReg3 encoding in opcode/ModRM bytes. */
    BUILD_BUG_ON(x86_seg_es != 0);
    BUILD_BUG_ON(x86_seg_cs != 1);
    BUILD_BUG_ON(x86_seg_ss != 2);
    BUILD_BUG_ON(x86_seg_ds != 3);
    BUILD_BUG_ON(x86_seg_fs != 4);
    BUILD_BUG_ON(x86_seg_gs != 5);

    /*
     * Check X86_EVENTTYPE_* against VMCB EVENTINJ and VMCS INTR_INFO type
     * fields.
     */
    BUILD_BUG_ON(X86_EVENTTYPE_EXT_INTR != 0);
    BUILD_BUG_ON(X86_EVENTTYPE_NMI != 2);
    BUILD_BUG_ON(X86_EVENTTYPE_HW_EXCEPTION != 3);
    BUILD_BUG_ON(X86_EVENTTYPE_SW_INTERRUPT != 4);
    BUILD_BUG_ON(X86_EVENTTYPE_PRI_SW_EXCEPTION != 5);
    BUILD_BUG_ON(X86_EVENTTYPE_SW_EXCEPTION != 6);
}

#ifndef NDEBUG
/*
 * In debug builds, wrap x86_emulate() with some assertions about its expected
 * behaviour.
 */
int x86_emulate_wrapper(
    struct x86_emulate_ctxt *ctxt,
    const struct x86_emulate_ops *ops)
{
    unsigned long orig_ip = ctxt->regs->r(ip);
    int rc = x86_emulate(ctxt, ops);

    /* Retire flags should only be set for successful instruction emulation. */
    if ( rc != X86EMUL_OKAY )
        ASSERT(ctxt->retire.raw == 0);

    /* All cases returning X86EMUL_EXCEPTION should have fault semantics. */
    if ( rc == X86EMUL_EXCEPTION )
        ASSERT(ctxt->regs->r(ip) == orig_ip);

    /*
     * TODO: Make this true:
     *
    ASSERT(ctxt->event_pending == (rc == X86EMUL_EXCEPTION));
     *
     * Some codepaths still raise exceptions behind the back of the
     * emulator. (i.e. return X86EMUL_EXCEPTION but without
     * event_pending being set).  In the meantime, use a slightly
     * relaxed check...
     */
    if ( ctxt->event_pending )
        ASSERT(rc == X86EMUL_EXCEPTION);

    return rc;
}
#endif

#ifdef __XEN__

#include <xen/err.h>

struct x86_emulate_state *
x86_decode_insn(
    struct x86_emulate_ctxt *ctxt,
    int (*insn_fetch)(
        enum x86_segment seg, unsigned long offset,
        void *p_data, unsigned int bytes,
        struct x86_emulate_ctxt *ctxt))
{
    static DEFINE_PER_CPU(struct x86_emulate_state, state);
    struct x86_emulate_state *state = &this_cpu(state);
    const struct x86_emulate_ops ops = {
        .insn_fetch = insn_fetch,
        .read       = x86emul_unhandleable_rw,
    };
    int rc = x86_decode(state, ctxt, &ops);

    if ( unlikely(rc != X86EMUL_OKAY) )
        return ERR_PTR(-rc);

#ifndef NDEBUG
    /*
     * While we avoid memory allocation (by use of per-CPU data) above,
     * nevertheless make sure callers properly release the state structure
     * for forward compatibility.
     */
    if ( state->caller )
    {
        printk(XENLOG_ERR "Unreleased emulation state acquired by %ps\n",
               state->caller);
        dump_execution_state();
    }
    state->caller = __builtin_return_address(0);
#endif

    return state;
}

static inline void check_state(const struct x86_emulate_state *state)
{
#ifndef NDEBUG
    ASSERT(state->caller);
#endif
}

#ifndef NDEBUG
void x86_emulate_free_state(struct x86_emulate_state *state)
{
    check_state(state);
    state->caller = NULL;
}
#endif

unsigned int
x86_insn_opsize(const struct x86_emulate_state *state)
{
    check_state(state);

    return state->op_bytes << 3;
}

int
x86_insn_modrm(const struct x86_emulate_state *state,
               unsigned int *rm, unsigned int *reg)
{
    check_state(state);

    if ( !(state->desc & ModRM) )
        return -EINVAL;

    if ( rm )
        *rm = state->modrm_rm;
    if ( reg )
        *reg = state->modrm_reg;

    return state->modrm_mod;
}

unsigned long
x86_insn_operand_ea(const struct x86_emulate_state *state,
                    enum x86_segment *seg)
{
    *seg = state->ea.type == OP_MEM ? state->ea.mem.seg : x86_seg_none;

    check_state(state);

    return state->ea.mem.off;
}

bool
x86_insn_is_mem_access(const struct x86_emulate_state *state,
                       const struct x86_emulate_ctxt *ctxt)
{
    if ( state->ea.type == OP_MEM )
        return ctxt->opcode != 0x8d /* LEA */ &&
               (ctxt->opcode != X86EMUL_OPC(0x0f, 0x01) ||
                (state->modrm_reg & 7) != 7) /* INVLPG */;

    switch ( ctxt->opcode )
    {
    case 0x6c ... 0x6f: /* INS / OUTS */
    case 0xa4 ... 0xa7: /* MOVS / CMPS */
    case 0xaa ... 0xaf: /* STOS / LODS / SCAS */
    case 0xd7:          /* XLAT */
        return true;

    case X86EMUL_OPC(0x0f, 0x01):
        /* Cover CLZERO. */
        return (state->modrm_rm & 7) == 4 && (state->modrm_reg & 7) == 7;
    }

    return false;
}

bool
x86_insn_is_mem_write(const struct x86_emulate_state *state,
                      const struct x86_emulate_ctxt *ctxt)
{
    switch ( state->desc & DstMask )
    {
    case DstMem:
        return state->modrm_mod != 3;

    case DstBitBase:
    case DstImplicit:
        break;

    default:
        return false;
    }

    if ( state->modrm_mod == 3 )
        /* CLZERO is the odd one. */
        return ctxt->opcode == X86EMUL_OPC(0x0f, 0x01) &&
               (state->modrm_rm & 7) == 4 && (state->modrm_reg & 7) == 7;

    switch ( ctxt->opcode )
    {
    case 0x6c: case 0x6d:                /* INS */
    case 0xa4: case 0xa5:                /* MOVS */
    case 0xaa: case 0xab:                /* STOS */
    case X86EMUL_OPC(0x0f, 0x11):        /* MOVUPS */
    case X86EMUL_OPC_VEX(0x0f, 0x11):    /* VMOVUPS */
    case X86EMUL_OPC_66(0x0f, 0x11):     /* MOVUPD */
    case X86EMUL_OPC_VEX_66(0x0f, 0x11): /* VMOVUPD */
    case X86EMUL_OPC_F3(0x0f, 0x11):     /* MOVSS */
    case X86EMUL_OPC_VEX_F3(0x0f, 0x11): /* VMOVSS */
    case X86EMUL_OPC_F2(0x0f, 0x11):     /* MOVSD */
    case X86EMUL_OPC_VEX_F2(0x0f, 0x11): /* VMOVSD */
    case X86EMUL_OPC(0x0f, 0x29):        /* MOVAPS */
    case X86EMUL_OPC_VEX(0x0f, 0x29):    /* VMOVAPS */
    case X86EMUL_OPC_66(0x0f, 0x29):     /* MOVAPD */
    case X86EMUL_OPC_VEX_66(0x0f, 0x29): /* VMOVAPD */
    case X86EMUL_OPC(0x0f, 0x2b):        /* MOVNTPS */
    case X86EMUL_OPC_VEX(0x0f, 0x2b):    /* VMOVNTPS */
    case X86EMUL_OPC_66(0x0f, 0x2b):     /* MOVNTPD */
    case X86EMUL_OPC_VEX_66(0x0f, 0x2b): /* VMOVNTPD */
    case X86EMUL_OPC(0x0f, 0x7e):        /* MOVD/MOVQ */
    case X86EMUL_OPC_66(0x0f, 0x7e):     /* MOVD/MOVQ */
    case X86EMUL_OPC_VEX_66(0x0f, 0x7e): /* VMOVD/VMOVQ */
    case X86EMUL_OPC(0x0f, 0x7f):        /* VMOVQ */
    case X86EMUL_OPC_66(0x0f, 0x7f):     /* MOVDQA */
    case X86EMUL_OPC_VEX_66(0x0f, 0x7f): /* VMOVDQA */
    case X86EMUL_OPC_F3(0x0f, 0x7f):     /* MOVDQU */
    case X86EMUL_OPC_VEX_F3(0x0f, 0x7f): /* VMOVDQU */
    case X86EMUL_OPC(0x0f, 0xab):        /* BTS */
    case X86EMUL_OPC(0x0f, 0xb3):        /* BTR */
    case X86EMUL_OPC(0x0f, 0xbb):        /* BTC */
    case X86EMUL_OPC_66(0x0f, 0xd6):     /* MOVQ */
    case X86EMUL_OPC_VEX_66(0x0f, 0xd6): /* VMOVQ */
    case X86EMUL_OPC(0x0f, 0xe7):        /* MOVNTQ */
    case X86EMUL_OPC_66(0x0f, 0xe7):     /* MOVNTDQ */
    case X86EMUL_OPC_VEX_66(0x0f, 0xe7): /* VMOVNTDQ */
        return true;

    case 0xd9:
        switch ( state->modrm_reg & 7 )
        {
        case 2: /* FST m32fp */
        case 3: /* FSTP m32fp */
        case 6: /* FNSTENV */
        case 7: /* FNSTCW */
            return true;
        }
        break;

    case 0xdb:
        switch ( state->modrm_reg & 7 )
        {
        case 1: /* FISTTP m32i */
        case 2: /* FIST m32i */
        case 3: /* FISTP m32i */
        case 7: /* FSTP m80fp */
            return true;
        }
        break;

    case 0xdd:
        switch ( state->modrm_reg & 7 )
        {
        case 1: /* FISTTP m64i */
        case 2: /* FST m64fp */
        case 3: /* FSTP m64fp */
        case 6: /* FNSAVE */
        case 7: /* FNSTSW */
            return true;
        }
        break;

    case 0xdf:
        switch ( state->modrm_reg & 7 )
        {
        case 1: /* FISTTP m16i */
        case 2: /* FIST m16i */
        case 3: /* FISTP m16i */
        case 6: /* FBSTP */
        case 7: /* FISTP m64i */
            return true;
        }
        break;

    case X86EMUL_OPC(0x0f, 0x01):
        return !(state->modrm_reg & 6); /* SGDT / SIDT */

    case X86EMUL_OPC(0x0f, 0xba):
        return (state->modrm_reg & 7) > 4; /* BTS / BTR / BTC */

    case X86EMUL_OPC(0x0f, 0xc7):
        return (state->modrm_reg & 7) == 1; /* CMPXCHG{8,16}B */
    }

    return false;
}

bool
x86_insn_is_portio(const struct x86_emulate_state *state,
                   const struct x86_emulate_ctxt *ctxt)
{
    switch ( ctxt->opcode )
    {
    case 0x6c ... 0x6f: /* INS / OUTS */
    case 0xe4 ... 0xe7: /* IN / OUT imm8 */
    case 0xec ... 0xef: /* IN / OUT %dx */
        return true;
    }

    return false;
}

bool
x86_insn_is_cr_access(const struct x86_emulate_state *state,
                      const struct x86_emulate_ctxt *ctxt)
{
    switch ( ctxt->opcode )
    {
        unsigned int ext;

    case X86EMUL_OPC(0x0f, 0x01):
        if ( x86_insn_modrm(state, NULL, &ext) >= 0
             && (ext & 5) == 4 ) /* SMSW / LMSW */
            return true;
        break;

    case X86EMUL_OPC(0x0f, 0x06): /* CLTS */
    case X86EMUL_OPC(0x0f, 0x20): /* MOV from CRn */
    case X86EMUL_OPC(0x0f, 0x22): /* MOV to CRn */
        return true;
    }

    return false;
}

unsigned long
x86_insn_immediate(const struct x86_emulate_state *state, unsigned int nr)
{
    check_state(state);

    switch ( nr )
    {
    case 0:
        return state->imm1;
    case 1:
        return state->imm2;
    }

    return 0;
}

unsigned int
x86_insn_length(const struct x86_emulate_state *state,
                const struct x86_emulate_ctxt *ctxt)
{
    check_state(state);

    return state->ip - ctxt->regs->r(ip);
}

#endif
