/**
 * Copyright (c) 2017, Armando Faz <armfazh@ic.unicamp.br>. All rights reserved.
 * Institute of Computing.
 * University of Campinas, Brazil.
 *
 * Copyright (C) 2018 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *  * Neither the name of University of Campinas nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <string.h>
#include "fp25519_x64.h"
#include "x25519_precomputed.h"
#include "table_ladder_x25519.h"

static inline void cswap_x64(uint64_t bit, uint64_t *const px,
                             uint64_t *const py) {
  int i = 0;
  uint64_t mask = (uint64_t)0 - bit;
  for (i = 0; i < NUM_WORDS_ELTFP25519_X64; i++) {
    uint64_t t = mask & (px[i] ^ py[i]);
    px[i] = px[i] ^ t;
    py[i] = py[i] ^ t;
  }
}


/** Original rfc7748_precomputed name: 'x25519_shared_secret_x64' */
void x25519_precomputed_scalarmult(uint8_t *shared, uint8_t *private_key,
                                   uint8_t *session_key) {
  ALIGN uint64_t buffer[4 * NUM_WORDS_ELTFP25519_X64];
  ALIGN uint64_t coordinates[4 * NUM_WORDS_ELTFP25519_X64];
  ALIGN uint64_t workspace[6 * NUM_WORDS_ELTFP25519_X64];
  ALIGN uint8_t session[X25519_KEYSIZE_BYTES];
  ALIGN uint8_t private[X25519_KEYSIZE_BYTES];

  int i = 0, j = 0;
  uint64_t prev = 0;
  uint64_t *const X1 = (uint64_t *)session;
  uint64_t *const key = (uint64_t *)private;
  uint64_t *const Px = coordinates + 0;
  uint64_t *const Pz = coordinates + 4;
  uint64_t *const Qx = coordinates + 8;
  uint64_t *const Qz = coordinates + 12;
  uint64_t *const X2 = Qx;
  uint64_t *const Z2 = Qz;
  uint64_t *const X3 = Px;
  uint64_t *const Z3 = Pz;
  uint64_t *const X2Z2 = Qx;
  uint64_t *const X3Z3 = Px;

  uint64_t *const A = workspace + 0;
  uint64_t *const B = workspace + 4;
  uint64_t *const D = workspace + 8;
  uint64_t *const C = workspace + 12;
  uint64_t *const DA = workspace + 16;
  uint64_t *const CB = workspace + 20;
  uint64_t *const AB = A;
  uint64_t *const DC = D;
  uint64_t *const DACB = DA;
  uint64_t *const buffer_1w = buffer;
  uint64_t *const buffer_2w = buffer;

  memcpy(private, private_key, sizeof(private));
  memcpy(session, session_key, sizeof(session));

  /* clampC function */
 private
  [0] = private[0] & (~(uint8_t)0x7);
 private
  [X25519_KEYSIZE_BYTES - 1] =
      (uint8_t)64 | (private[X25519_KEYSIZE_BYTES - 1] & (uint8_t)0x7F);

  /**
  * As in the draft:
  * When receiving such an array, implementations of curve25519
  * MUST mask the most-significant bit in the final byte. This
  * is done to preserve compatibility with point formats which
  * reserve the sign bit for use in other protocols and to
  * increase resistance to implementation fingerprinting
  **/
  session[X25519_KEYSIZE_BYTES - 1] &= (1 << (255 % 8)) - 1;

  copy_EltFp25519_1w_x64(Px, X1);
  setzero_EltFp25519_1w_x64(Pz);
  setzero_EltFp25519_1w_x64(Qx);
  setzero_EltFp25519_1w_x64(Qz);

  Pz[0] = 1;
  Qx[0] = 1;

  /* main-loop */
  prev = 0;
  j = 62;
  for (i = 3; i >= 0; i--) {
    while (j >= 0) {
      uint64_t bit = (key[i] >> j) & 0x1;
      uint64_t swap = bit ^ prev;
      prev = bit;

      add_EltFp25519_1w_x64(A, X2, Z2);    /* A = (X2+Z2)                   */
      sub_EltFp25519_1w_x64(B, X2, Z2);    /* B = (X2-Z2)                   */
      add_EltFp25519_1w_x64(C, X3, Z3);    /* C = (X3+Z3)                   */
      sub_EltFp25519_1w_x64(D, X3, Z3);    /* D = (X3-Z3)                   */
      mul_EltFp25519_2w_x64(DACB, AB, DC); /* [DA|CB] = [A|B]*[D|C]         */

      cswap_x64(swap, A, C);
      cswap_x64(swap, B, D);

      sqr_EltFp25519_2w_x64(AB);         /* [AA|BB] = [A^2|B^2]           */
      add_EltFp25519_1w_x64(X3, DA, CB); /* X3 = (DA+CB)                  */
      sub_EltFp25519_1w_x64(Z3, DA, CB); /* Z3 = (DA-CB)                  */
      sqr_EltFp25519_2w_x64(X3Z3);       /* [X3|Z3] = [(DA+CB)|(DA+CB)]^2 */

      copy_EltFp25519_1w_x64(X2, B);   /* X2 = B^2                      */
      sub_EltFp25519_1w_x64(Z2, A, B); /* Z2 = E = AA-BB                */

      mul_a24_EltFp25519_1w_x64(B, Z2);      /* B = a24*E                     */
      add_EltFp25519_1w_x64(B, B, X2);       /* B = a24*E+B                   */
      mul_EltFp25519_2w_x64(X2Z2, X2Z2, AB); /* [X2|Z2] = [B|E]*[A|a24*E+B]   */
      mul_EltFp25519_1w_x64(Z3, Z3, X1);     /* Z3 = Z3*X1                    */
      j--;
    }
    j = 63;
  }

  inv_EltFp25519_1w_x64(A, Qz);
  mul_EltFp25519_1w_x64((uint64_t *)shared, Qx, A);
  fred_EltFp25519_1w_x64((uint64_t *)shared);
}

/* Original rfc7748_precomputed name: 'x25519_keygen_precmp_x64' */
void x25519_precomputed_scalarmult_base(uint8_t *session_key, uint8_t *private_key) {
  ALIGN uint64_t buffer[4 * NUM_WORDS_ELTFP25519_X64];
  ALIGN uint64_t coordinates[4 * NUM_WORDS_ELTFP25519_X64];
  ALIGN uint64_t workspace[4 * NUM_WORDS_ELTFP25519_X64];
  ALIGN uint8_t private[X25519_KEYSIZE_BYTES];

  int i = 0, j = 0, k = 0;
  uint64_t *const key = (uint64_t *)private;
  uint64_t *const Ur1 = coordinates + 0;
  uint64_t *const Zr1 = coordinates + 4;
  uint64_t *const Ur2 = coordinates + 8;
  uint64_t *const Zr2 = coordinates + 12;

  uint64_t *const UZr1 = coordinates + 0;
  uint64_t *const ZUr2 = coordinates + 8;

  uint64_t *const A = workspace + 0;
  uint64_t *const B = workspace + 4;
  uint64_t *const C = workspace + 8;
  uint64_t *const D = workspace + 12;

  uint64_t *const AB = workspace + 0;
  uint64_t *const CD = workspace + 8;

  uint64_t *const buffer_1w = buffer;
  uint64_t *const buffer_2w = buffer;
  uint64_t *P = (uint64_t *)Table_Ladder_8k;

  memcpy(private, private_key, sizeof(private));

  /* clampC function */
 private
  [0] = private[0] & (~(uint8_t)0x7);
 private
  [X25519_KEYSIZE_BYTES - 1] =
      (uint8_t)64 | (private[X25519_KEYSIZE_BYTES - 1] & (uint8_t)0x7F);

  setzero_EltFp25519_1w_x64(Ur1);
  setzero_EltFp25519_1w_x64(Zr1);
  setzero_EltFp25519_1w_x64(Zr2);
  Ur1[0] = 1;
  Zr1[0] = 1;
  Zr2[0] = 1;

  /* G-S */
  Ur2[3] = 0x1eaecdeee27cab34;
  Ur2[2] = 0xadc7a0b9235d48e2;
  Ur2[1] = 0xbbf095ae14b2edf8;
  Ur2[0] = 0x7e94e1fec82faabd;

  /* main-loop */
  const int ite[4] = {64, 64, 64, 63};
  const int q = 3;
  uint64_t swap = 1;

  j = q;
  for (i = 0; i < NUM_WORDS_ELTFP25519_X64; i++) {
    while (j < ite[i]) {
      k = (64 * i + j - q);
      uint64_t bit = (key[i] >> j) & 0x1;
      swap = swap ^ bit;
      cswap_x64(swap, Ur1, Ur2);
      cswap_x64(swap, Zr1, Zr2);
      swap = bit;
      /** Addition */
      sub_EltFp25519_1w_x64(B, Ur1, Zr1);     /* B = Ur1-Zr1                 */
      add_EltFp25519_1w_x64(A, Ur1, Zr1);     /* A = Ur1+Zr1                 */
      mul_EltFp25519_1w_x64(C, &P[4 * k], B); /* C = M0-B                    */
      sub_EltFp25519_1w_x64(B, A, C);         /* B = (Ur1+Zr1) - M*(Ur1-Zr1) */
      add_EltFp25519_1w_x64(A, A, C);         /* A = (Ur1+Zr1) + M*(Ur1-Zr1) */
      sqr_EltFp25519_2w_x64(AB);              /* A = A^2      |  B = B^2     */
      mul_EltFp25519_2w_x64(UZr1, ZUr2, AB);  /* Ur1 = Zr2*A  |  Zr1 = Ur2*B */
      j++;
    }
    j = 0;
  }

  /** Doubling */
  for (i = 0; i < q; i++) {
    add_EltFp25519_1w_x64(A, Ur1, Zr1);  /*  A = Ur1+Zr1   */
    sub_EltFp25519_1w_x64(B, Ur1, Zr1);  /*  B = Ur1-Zr1   */
    sqr_EltFp25519_2w_x64(AB);           /*  A = A**2     B = B**2   */
    copy_EltFp25519_1w_x64(C, B);        /*  C = B         */
    sub_EltFp25519_1w_x64(B, A, B);      /*  B = A-B       */
    mul_a24_EltFp25519_1w_x64(D, B);     /*  D = my_a24*B  */
    add_EltFp25519_1w_x64(D, D, C);      /*  D = D+C       */
    mul_EltFp25519_2w_x64(UZr1, AB, CD); /*  Ur1 = A*B   Zr1 = Zr1*A */
  }

  /* Convert to affine coordinates */
  inv_EltFp25519_1w_x64(A, Zr1);
  mul_EltFp25519_1w_x64((uint64_t *)session_key, Ur1, A);
  fred_EltFp25519_1w_x64((uint64_t *)session_key);
}
