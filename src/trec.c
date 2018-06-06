/*
 * This file is a part of RBC Combine.
 *
 * Copyright (c) 2017 Luke Gallagher <luke.gallagher@rmit.edu.au>
 *
 * For the full copyright and license information, please view the LICENSE file
 * that was distributed with this source code.
 */

#include "trec.h"

#define INIT_SZ 16

static int prev_top = 0;
static int max_rank = 1;

/*
 * Allocate more memory if required.
 */
static void
trec_entry_alloc(struct trec_run *r)
{
    if (r && r->len == r->alloc) {
        r->alloc *= 2;
        r->ary = brealloc(r->ary, sizeof(struct trec_entry) * r->alloc);
    }
}

/*
 * Allocate more memory if required.
 */
static void
trec_topic_alloc(struct trec_topic *t)
{
    if (t && t->len == t->alloc) {
        t->alloc *= 2;
        t->ary = brealloc(t->ary, sizeof(int) * t->alloc);
    }
}

/**
 * Alloc a new trec run array.
 */
struct trec_run *
trec_create()
{
    struct trec_run *run = NULL;

    run = bmalloc(sizeof(struct trec_run));
    run->ary = bmalloc(sizeof(struct trec_entry) * INIT_SZ);
    run->len = 0;
    run->alloc = INIT_SZ;
    run->max_rank = 0;

    run->topics.ary = bmalloc(sizeof(int) * INIT_SZ);
    run->topics.len = 0;
    run->topics.alloc = INIT_SZ;

    return run;
}

void
trec_destroy(struct trec_run *run)
{
    if (run) {
        for (size_t i = 0; i < run->len; i++) {
            free(run->ary[i].docno);
            free(run->ary[i].name);
        }
        free(run->ary);
        free(run->topics.ary);
        free(run);
    }
}

static struct trec_entry
parse_line(char *line, int *topic)
{
    static int rank = 1;
    const char *delim = "\t ";
    const int num_sep = 5; // 6 columns
    struct trec_entry tentry;
    char *dup = strndup(line, strlen(line));
    char *tok, *p;
    int c = 0;
    int ch;

    p = line;
    while ((ch = *p++)) {
        if (isspace(ch)) {
            c++;
        }
    }
    if (c != num_sep) {
        err_exit("found %d fields but should be %d", c, num_sep + 1);
    }

    tok = strtok(dup, delim);
    tentry.qid = strtol(tok, NULL, 10);

    if (prev_top != tentry.qid) {
        if (rank > max_rank) {
            max_rank = rank;
        }
        rank = 1;
        prev_top = tentry.qid;
        *topic = tentry.qid;
    }

    tok = strtok(NULL, delim); // skip over column 2
    tok = strtok(NULL, delim);
    tentry.docno = strndup(tok, strlen(tok));
    tok = strtok(NULL, delim); // skip rank column
    tentry.rank = rank++;
    strtol(tok, NULL, 10);
    tok = strtok(NULL, delim);
    tentry.score = strtod(tok, NULL);
    tok = strtok(NULL, delim);
    tentry.name = strndup(tok, strlen(tok));

    free(dup);

    return tentry;
}

void
trec_read(struct trec_run *r, FILE *fp)
{
    char buf[BUFSIZ] = {0};
    int curr_topic;

    prev_top = 0;

    while (fgets(buf, BUFSIZ, fp)) {
        if (buf[strlen(buf) - 1] != '\n') {
            err_exit("input line exceeds %d", BUFSIZ);
        }
        buf[strlen(buf) - 1] = '\0';
        curr_topic = 0;

        trec_entry_alloc(r);
        r->ary[r->len++] = parse_line(buf, &curr_topic);

        trec_topic_alloc(&r->topics);
        if (curr_topic > 0) {
            r->topics.ary[r->topics.len++] = curr_topic;
        }
    }

    r->max_rank = max_rank;
}
