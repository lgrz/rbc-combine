/*
 * This file is a part of RBC Combine.
 *
 * Copyright (c) 2017 Luke Gallagher <luke.gallagher@rmit.edu.au>
 *
 * For the full copyright and license information, please view the LICENSE file
 * that was distributed with this source code.
 */

#include "rbc.h"

struct topic_list {
    int *ary;
    size_t size;
};

static double *weights = NULL;
static size_t weight_sz = 0;

static struct rbc_topic *topic_tab = NULL;
static struct topic_list qids = {NULL, 0};

/*
 * Allocate RBC weight to the deepest topic seen in all run files.
 */
void
rbc_weight_alloc(const double phi, const size_t len)
{
    static bool first = true;
    static double w;
    static double _phi;
    size_t prev = weight_sz;

    if (len <= weight_sz) {
        return;
    }

    weight_sz = len;
    if (first) {
        weights = (double *)bmalloc(sizeof(double) * weight_sz);
        w = 1.0 - phi;
        _phi = phi;
        first = false;
    } else {
        weights = (double *)brealloc(weights, sizeof(double) * weight_sz);
    }

    for (size_t i = prev; i < weight_sz; i++) {
        weights[i] = w;
        w *= _phi;
    }
}

void
rbc_init(const struct trec_topic *topics)
{
    qids.ary = bmalloc(sizeof(int) * topics->len);
    qids.size = topics->len;
    memcpy(qids.ary, topics->ary, sizeof(int) * topics->len);

    // create an accumulator for each topic
    topic_tab = rbc_topic_create(topics->len);
    for (size_t i = 0; i < topics->len; i++) {
        rbc_topic_insert(&topic_tab, topics->ary[i]);
    }
}

void
rbc_destory()
{
    free(weights);
    free(qids.ary);
    rbc_topic_free(topic_tab);
}

void
rbc_accumulate(struct trec_run *r)
{
    for (size_t i = 0; i < r->len; i++) {
        size_t rank = r->ary[i].rank - 1;
        if (rank < weight_sz) {
            double w = weights[rank];
            struct rbc_accum **curr;
            curr = rbc_topic_lookup(topic_tab, r->ary[i].qid);
            if (*curr) {
                rbc_accum_update(curr, r->ary[i].docno, w);
            }
        }
    }
}

void
rbc_present(FILE *stream, const char *id, size_t depth)
{
    if (depth < 1) {
        err_exit("`depth` is 0");
    }

    if (depth > weight_sz) {
        depth = weight_sz;
    }

    for (size_t i = 0; i < qids.size; i++) {
        struct rbc_accum *curr;
        curr = *rbc_topic_lookup(topic_tab, qids.ary[i]);
        struct pq *pq = pq_create(weight_sz);
        // this is why we use linear probing
        for (size_t j = 0; j < curr->capacity; j++) {
            if (!curr->data[j].is_set) {
                continue;
            }
            pq_insert(pq, curr->data[j].docno, curr->data[j].val);
        }
        struct accum_node *res =
            bmalloc(sizeof(struct accum_node) * weight_sz);
        size_t sz = 0;
        while (sz < weight_sz && pq->size > 0) {
            pq_remove(pq, res + sz++);
        }
        long long c = depth;
        for (size_t j = sz, k = 1; c >= 0; j--, c--) {
            if (res[j].is_set) {
                fprintf(stream, "%d Q0 %s %lu %.4f %s\n", qids.ary[i],
                    res[j].docno, k++, j + res[j].val, id);
            }
            if (0 == j) {
                break;
            }
        }
        free(res);
        pq_destroy(pq);
    }
}
