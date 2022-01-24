/* SPDX-License-Identifier: MIT */
/* Op function declarations for smart crunch, cleanup TODO

Copyright 2021 Matthew Ling

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files(the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and / or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions :
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE. */
#ifndef QOIP_FUNC
#define QOIP_FUNC

/* === Hash cache index functions */
/* This function encodes all index1_* ops */
int qoip_sim_index(qoip_working_t *q);
int qoip_enc_index(qoip_working_t *q, u8 opcode);
void qoip_dec_index(qoip_working_t *q);

int qoip_sim_index8(qoip_working_t *q);
int qoip_enc_index8(qoip_working_t *q, u8 opcode);
void qoip_dec_index8(qoip_working_t *q);

/* === Length 1 RGB delta functions */
int qoip_sim_delta(qoip_working_t *q);
int qoip_enc_delta(qoip_working_t *q, u8 opcode);
void qoip_dec_delta(qoip_working_t *q);

int qoip_sim_diff1_222(qoip_working_t *q);
int qoip_enc_diff1_222(qoip_working_t *q, u8 opcode);
void qoip_dec_diff1_222(qoip_working_t *q);


int qoip_sim_luma1_232_bias(qoip_working_t *q);
int qoip_enc_luma1_232_bias(qoip_working_t *q, u8 opcode);
void qoip_dec_luma1_232_bias(qoip_working_t *q);

int qoip_sim_luma1_232(qoip_working_t *q);
int qoip_enc_luma1_232(qoip_working_t *q, u8 opcode);
void qoip_dec_luma1_232(qoip_working_t *q);

/* === Length 2 RGB delta functions */
int qoip_sim_luma2_454(qoip_working_t *q);
int qoip_enc_luma2_454(qoip_working_t *q, u8 opcode);
void qoip_dec_luma2_454(qoip_working_t *q);

int qoip_sim_luma2_464(qoip_working_t *q);
int qoip_enc_luma2_464(qoip_working_t *q, u8 opcode);
void qoip_dec_luma2_464(qoip_working_t *q);

/* === Length 3 RGB delta functions */
int qoip_sim_luma3_676(qoip_working_t *q);
int qoip_enc_luma3_676(qoip_working_t *q, u8 opcode);
void qoip_dec_luma3_676(qoip_working_t *q);

int qoip_sim_luma3_686(qoip_working_t *q);
int qoip_enc_luma3_686(qoip_working_t *q, u8 opcode);
void qoip_dec_luma3_686(qoip_working_t *q);

int qoip_sim_luma3_787(qoip_working_t *q);
int qoip_enc_luma3_787(qoip_working_t *q, u8 opcode);
void qoip_dec_luma3_787(qoip_working_t *q);

/* === length 1 RGBA delta functions */
int qoip_sim_deltaa(qoip_working_t *q);
int qoip_enc_deltaa(qoip_working_t *q, u8 opcode);
void qoip_dec_deltaa(qoip_working_t *q);

/* === Length 2 RGBA delta functions */
int qoip_sim_a(qoip_working_t *q);
int qoip_enc_a(qoip_working_t *q, u8 opcode);
void qoip_dec_a(qoip_working_t *q);

int qoip_sim_luma2_3433(qoip_working_t *q);
int qoip_enc_luma2_3433(qoip_working_t *q, u8 opcode);
void qoip_dec_luma2_3433(qoip_working_t *q);

/* === Length 3 RGBA delta functions */
int qoip_sim_luma3_4645(qoip_working_t *q);
int qoip_enc_luma3_4645(qoip_working_t *q, u8 opcode);
void qoip_dec_luma3_4645(qoip_working_t *q);

int qoip_sim_luma3_5654(qoip_working_t *q);
int qoip_enc_luma3_5654(qoip_working_t *q, u8 opcode);
void qoip_dec_luma3_5654(qoip_working_t *q);

/* === Length 4 RGBA delta functions */
int qoip_sim_luma4_7777(qoip_working_t *q);
int qoip_enc_luma4_7777(qoip_working_t *q, u8 opcode);
void qoip_dec_luma4_7777(qoip_working_t *q);

#endif
