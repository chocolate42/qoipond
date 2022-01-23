#ifndef QOIP_FUNC
#define QOIP_FUNC
/* Op encode/decode functions used by qoip.h implementation. Included by QOIP_C only

* qoip_enc_* is the encoder for OP_*, qoip_dec_* is the decoder
* The encode functions detect if an op can be used and encodes it if it can. If
  the op is used 1 is returned so qoip_encode knows to proceed to the next pixel
* The decode functions are called when qoip_decode has determined the op was
  used, no detection necessary
*/

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
