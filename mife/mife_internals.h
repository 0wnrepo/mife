#ifndef _MIFE_INTERNALS_H_
#define _MIFE_INTERNALS_H_

#include "mmap/mmap.h"
#include "mife_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int g_parallel;

/* MIFE internal functions */
void reset_T();
float get_T();
void set_NUM_ENC(int val);
int get_NUM_ENC();

void mife_apply_randomizer(mife_pp_t pp, aes_randstate_t randstate, fmpz_mat_t m);

void mife_apply_randomizers(mife_mat_clr_t met, mife_pp_t pp, mife_sk_t sk,
                            aes_randstate_t randstate);

int ***mife_partitions(mife_pp_t pp, fmpz_t index);

void mife_partitions_clear(mife_pp_t pp, int ***partitions);

void mife_apply_kilian(mife_pp_t pp, mife_sk_t sk, fmpz_mat_t m, int global_index);

void mife_set_encodings       (const_mmap_vtable mmap, mife_ciphertext_t ct,
                               mife_mat_clr_t met, fmpz_t index, mife_pp_t pp,
                               mife_sk_t sk, aes_randstate_t randstate);
void mife_clear_pp_read       (const_mmap_vtable mmap, mife_pp_t pp);
void mife_clear_pp            (mife_pp_t pp);
void mife_clear_sk            (const_mmap_vtable mmap, mife_sk_t sk);
void mife_mat_clr_clear       (mife_pp_t pp, mife_mat_clr_t met);
void mife_gen_partitioning    (int *partitioning, fmpz_t index, int L, int nu);
void mife_mat_encode          (const_mmap_vtable mmap, mife_pp_t pp,
                               mife_sk_t sk, mmap_enc_mat_t enc, fmpz_mat_t m,
                               int *group, aes_randstate_t randstate);
void mmap_enc_mat_zeros_print (const_mmap_vtable mmap, mife_pp_t pp,
                               mmap_enc_mat_t m);
void mife_ciphertext_clear    (const_mmap_vtable mmap, mife_pp_t pp, mife_ciphertext_t ct);
void message_to_dary          (ulong *dary, int bitstring_len, fmpz_t message, int d);

#ifdef __cplusplus
}
#endif

#endif /* _MIFE_INTERNALS_H_ */
