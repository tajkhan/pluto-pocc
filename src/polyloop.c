/*
 * PLUTO: An automatic parallelizer and locality optimizer
 * 
 * Copyright (C) 2007-2012 Uday Bondhugula
 *
 * This file is part of Pluto.
 *
 * Pluto is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * A copy of the GNU General Public Licence can be found in the file
 * `LICENSE' in the top-level directory of this distribution. 
 *
 * Ploop is a loop dimension in the scattering tree. When the final
 * AST is generated, a single Ploop can get separated into multiple ones.
 *
 */
#include <assert.h>

#include "pluto.h"
#include "program.h"

Ploop *pluto_loop_alloc()
{
    Ploop *loop = malloc(sizeof(Ploop));
    loop->nstmts = 0;
    loop->depth = -1;
    loop->stmts = NULL;
    return loop;
}


void pluto_loop_print(const Ploop *loop)
{
    int i;
    printf("t%d {loop with stmts: ", loop->depth+1);
    for (i=0; i<loop->nstmts; i++) {
        printf("S%d, ", loop->stmts[i]->id+1);
    }
    printf("}\n");
}

void pluto_loops_print(Ploop **loops, int num)
{
    int i;
    for (i=0; i<num; i++) {
        pluto_loop_print(loops[i]);
    }
}



Ploop *pluto_loop_dup(Ploop *l)
{
    Ploop *loop;
    loop = pluto_loop_alloc();
    loop->depth = l->depth;
    loop->nstmts = l->nstmts;
    loop->stmts = malloc(loop->nstmts*sizeof(Stmt *));
    memcpy(loop->stmts, l->stmts, l->nstmts*sizeof(Stmt *));
    return loop;
}



void pluto_loop_free(Ploop *loop)
{
    if(!loop)
      return;

    if (loop->stmts != NULL) {
        free(loop->stmts);
    }
    free(loop);
}

void pluto_loops_free(Ploop **loops, int nloops)
{
    int i;
    for (i=0; i<nloops; i++) {
        pluto_loop_free(loops[i]);
    }
    free(loops);
}


/* Concatenates both and returns the results; pointers are copied - not the
 * loop structures */
Ploop **pluto_loops_concat(Ploop **loops1, int num1, Ploop **loops2, int num2)
{
    Ploop **cat_loops;
    if (num1 == 0 && num2 == 0) {
        return NULL;
    }
    cat_loops = malloc((num1+num2)*sizeof(Ploop *));
    memcpy(cat_loops, loops1, num1*sizeof(Ploop *));
    memcpy(cat_loops+num1, loops2, num2*sizeof(Ploop *));
    return cat_loops;
}

/* Get loops at this depth under the scattering tree that contain only and all
 * of the statements in 'stmts'
 * Caller has to ensure that 'stmts' are all fused up to depth-1 (or else 
 * no loops exist by definition)
 */
Ploop **pluto_get_loops_under(Stmt **stmts, int nstmts, int depth,
        const PlutoProg *prog, int *num)
{
    int i;
    Ploop **all_loops = NULL;
    bool loop;

    if (depth >= prog->num_hyperplanes) {
        *num = 0;
        return NULL;
    }

    for (i=0; i<nstmts; i++) {
        if (stmts[i]->type != ORIG) continue;
        if (pluto_is_hyperplane_loop(stmts[i], depth)) break;
    }

    loop = (i<nstmts);

    if (loop) {
        /* Even if one of the statements is a loop at this level, 
         * all of the statements are treated as being fused with it; 
         * check any limitations of this later */
        int num_inner=0, loop_nstmts, i;
        Ploop **inner_loops, *this_loop;
        this_loop = pluto_loop_alloc();
        this_loop->stmts = malloc(nstmts*sizeof(Stmt *));

        loop_nstmts = 0;
        for (i=0; i<nstmts; i++) {
            if (!pluto_is_hyperplane_scalar(stmts[i], depth)) {
                memcpy(this_loop->stmts+loop_nstmts, stmts+i, sizeof(Stmt *));
                loop_nstmts++;
            }
        }
        this_loop->nstmts = loop_nstmts;
        this_loop->depth = depth;
        /* Even with H_LOOP, some statements can have a scalar dimension - at
         * least one of the statements is a real loop (doesn't happen often
         * though */
        /* TODO: What if the hyperplane was 0 for S1, 1 for S2, i for S3 */
        /* The filter (first arg) for call below is not changed since if a
         * stmt has a scalar dimension here, it may still be fused with
         * other ones which do not have a scalar dimension here */ 
        inner_loops = pluto_get_loops_under(stmts, nstmts, depth+1, prog, &num_inner);
        if (loop_nstmts >= 1) {
            all_loops = pluto_loops_concat(&this_loop, 1, inner_loops, num_inner);
            free(inner_loops);
            *num = num_inner + 1;
        }else{
            all_loops = inner_loops;
            pluto_loop_free(this_loop);
            *num = num_inner;
        }
    }else{
        /* All statements have a scalar dimension at this depth */
        /* Regroup stmts */
        Stmt ***groups;
        int i, j, num_grps;
        int *num_stmts_per_grp;

        num_grps = 0;
        num_stmts_per_grp = NULL;
        groups = NULL;

        for (i=0; i<nstmts; i++)  {
            Stmt *stmt = stmts[i];
            for (j=0; j<num_grps; j++)  {
                if (stmt->trans->val[depth][stmt->trans->ncols-1] == 
                        groups[j][0]->trans->val[depth][groups[j][0]->trans->ncols-1]) {
                    /* Add to end of array */
                    groups[j] = (Stmt **) realloc(groups[j], 
                            (num_stmts_per_grp[j]+1)*sizeof(Stmt *));
                    groups[j][num_stmts_per_grp[j]] = stmt;
                    num_stmts_per_grp[j]++;
                    break;
                }
            }
            if (j==num_grps)    {
                /* New group */
                groups = (Stmt ***)realloc(groups, (num_grps+1)*sizeof(Stmt **));
                groups[num_grps] = (Stmt **) malloc(sizeof(Stmt *));
                groups[num_grps][0] = stmt;

                num_stmts_per_grp = (int *)realloc(num_stmts_per_grp, 
                        (num_grps+1)*sizeof(int));
                num_stmts_per_grp[num_grps] = 1;
                num_grps++;
            }
        }

        int nloops = 0;
        for (i=0; i<num_grps; i++) {
            int num_inner = 0;
            Ploop **inner_loops, **tmp_loops;
            inner_loops =  pluto_get_loops_under(groups[i], num_stmts_per_grp[i], 
                    depth+1, prog, &num_inner);
            tmp_loops = pluto_loops_concat(all_loops, nloops, inner_loops, num_inner);
            nloops += num_inner;
            free(inner_loops);
            free(all_loops);
            all_loops = tmp_loops;
        }
        *num = nloops;

        for (i=0; i<num_grps; i++) {
            free(groups[i]);
        }

        free(num_stmts_per_grp);
        free(groups);
    }

    return all_loops;
}


Ploop **pluto_get_all_loops(const PlutoProg *prog, int *num)
{
    Ploop **loops;
    loops = pluto_get_loops_under(prog->stmts, prog->nstmts, 0, prog, num);
    return loops;
}


int pluto_loop_is_parallel(const PlutoProg *prog, Ploop *loop)
{
    int parallel, i;

    /* All statements under a parallel loop should be of type orig */
    for (i=0; i<loop->nstmts; i++) {
        if (loop->stmts[i]->type != ORIG) break;
    }
    if (i<loop->nstmts) {
        return 0;
    }

    if (prog->hProps[loop->depth].dep_prop == PARALLEL) {
        return 1;
    }

    parallel = 1;

    for (i=0; i<prog->ndeps; i++) {
        Dep *dep = prog->deps[i];
        if (IS_RAR(dep->type)) continue;
        assert(dep->satvec != NULL);
        if (pluto_stmt_is_member_of(prog->stmts[dep->src], loop->stmts, loop->nstmts)
                && pluto_stmt_is_member_of(prog->stmts[dep->dest], loop->stmts, 
                    loop->nstmts)) {
            if (dep->satvec[loop->depth]) {
                parallel = 0;
                break;
            }
        }
    }

    return parallel;
}

/* Is loop1 dominated by loop2 */
static int is_loop_dominated(Ploop *loop1, Ploop *loop2, const PlutoProg *prog)
{
    int i;
    assert(loop1->nstmts >= 1);
    assert(loop2->nstmts >= 1);

    if (loop2->depth >= loop1->depth)   return 0;

    Stmt *stmt1 = loop1->stmts[0];
    Stmt *stmt2 = loop2->stmts[0];

    for (i=0; i<=loop2->depth; i++) {
        if (pluto_is_hyperplane_scalar(stmt1, i) &&
                pluto_is_hyperplane_scalar(stmt2, i)) {
            if (stmt1->trans->val[i][stmt1->trans->ncols-1] !=
                    stmt2->trans->val[i][stmt2->trans->ncols-1]) {
                return 0;
            }
        }
    }
    return 1;
}

/* For sorting loops in increasing order of their depths */
int pluto_loop_compar(const void *_l1, const void *_l2)
{
    Ploop *l1 = *(Ploop **) _l1;
    Ploop *l2 = *(Ploop **) _l2;

    if (l1->depth < l2->depth) {
        return -1;
    }
    
    if (l1->depth == l2->depth) {
        return 0;
    }

    return 1;
}


Ploop **pluto_get_parallel_loops(const PlutoProg *prog, int *nploops)
{
    Ploop **loops, **ploops;
    int num=0, i;

    ploops = NULL;
    loops = pluto_get_all_loops(prog, &num);

    // pluto_loops_print(loops, num);

    *nploops = 0;
    for (i=0; i<num; i++) {
        if (pluto_loop_is_parallel(prog, loops[i])) {
            ploops = realloc(ploops, (*nploops+1)*sizeof(Ploop *));
            ploops[*nploops] = pluto_loop_dup(loops[i]);
            (*nploops)++;
        }
    }

    for (i=0; i<num; i++) {
        pluto_loop_free(loops[i]);
    }
    free(loops);

    return ploops;
}


/* List of parallel loops such that no loop dominates another in the list */
Ploop **pluto_get_dom_parallel_loops(const PlutoProg *prog, int *ndploops)
{
    Ploop **loops, **tmp_loops, **dom_loops;
    int i, j, ndomloops, nploops;
    int *loop_copied = NULL;

    loops = pluto_get_parallel_loops(prog, &nploops);
    loop_copied = (int *)malloc(sizeof(int)*nploops);
    for(i=0; i< nploops; i++)
      loop_copied[i] = 0;

    dom_loops = NULL;
    ndomloops = 0;
    for (i=0; i<nploops; i++) {
        for (j=0; j<nploops; j++) {
            if (is_loop_dominated(loops[i], loops[j], prog)) break;
        }
        if (j==nploops) {
            tmp_loops = pluto_loops_concat(dom_loops, ndomloops, &loops[i], 1);
            loop_copied[i] = 1;
            free(dom_loops);
            dom_loops = tmp_loops;
            ndomloops++;
        }
    }
    *ndploops = ndomloops;

    for(i=0; i< nploops; i++)
      if(!loop_copied[i])
        pluto_loop_free(loops[i]);
    free(loops);
    free(loop_copied);

    return dom_loops;
}


/* Returns a list of outermost loops */
Ploop **pluto_get_outermost_loops(const PlutoProg *prog, int *ndploops)
{
    Ploop **loops, **tmp_loops, **dom_loops;
    int i, j, ndomloops, nploops;

    loops = pluto_get_all_loops(prog, &nploops);

    dom_loops = NULL;
    ndomloops = 0;
    for (i=0; i<nploops; i++) {
        for (j=0; j<nploops; j++) {
            if (is_loop_dominated(loops[i], loops[j], prog)) break;
        }
        if (j==nploops) {
            tmp_loops = pluto_loops_concat(dom_loops, ndomloops, &loops[i], 1);
            free(dom_loops);
            dom_loops = tmp_loops;
            ndomloops++;
        }
    }
    *ndploops = ndomloops;

    return dom_loops;
}


/* Band support */

Band *pluto_band_alloc(Ploop *loop, int width)
{
    Band *band = malloc(sizeof(Band));
    band->width = width;
    band->loop = pluto_loop_dup(loop);
    band->children = NULL;
    return band;
}

void pluto_band_print(const Band *band)
{
    int i;
    printf("(");
    for (i=band->loop->depth; i<band->loop->depth+band->width; i++) {
        printf("t%d, ", i+1);
    }
    printf(") with stmts {");
    for (i=0; i<band->loop->nstmts; i++) {
        printf("S%d, ", band->loop->stmts[i]->id+1);
    }
    printf("}\n");
}


void pluto_bands_print(Band **bands, int num)
{
    int i;
    for (i=0; i<num; i++) {
        pluto_band_print(bands[i]);
    }
}


Band *pluto_band_dup(Band *b)
{
    return pluto_band_alloc(b->loop, b->width);
}


/* Concatenates both and puts them in bands1; pointers are copied - not 
 * the structures */
void pluto_bands_concat(Band ***bands1, int num1, Band **bands2, int num2)
{
    if (num1 == 0 && num2 == 0) return;
    *bands1 = realloc(*bands1, (num1+num2)*sizeof(Band *));
    memcpy((*bands1)+num1, bands2, num2*sizeof(Band *));
}



/* Is band1 dominated by band2; a band is dominated by another if the latter either
 * completely includes the former or the former is in the subtree rooted at the parent */
static int is_band_dominated(Band *band1, Band *band2)
{
    int is_subset = pluto_stmt_is_subset_of(band1->loop->stmts, band1->loop->nstmts,
            band2->loop->stmts, band2->loop->nstmts);

    if (is_subset && band1->loop->depth > band2->loop->depth) return 1;

    if (is_subset && band1->loop->depth == band2->loop->depth 
            && band1->width < band2->width) return 1;

    return 0;
}


void pluto_band_free(Band *band)
{
    pluto_loop_free(band->loop);
    free(band);
}


void pluto_bands_free(Band **bands, int nbands)
{
    int i;

    for (i=0; i<nbands; i++) {
        pluto_band_free(bands[i]);
    }
    free(bands);
}

int pluto_is_depth_scalar(Ploop *loop, int depth)
{
    int i;

    assert(depth >= loop->depth);

    for (i=0; i<loop->nstmts; i++) {
        if (!pluto_is_hyperplane_scalar(loop->stmts[i], depth)) return 0;
    }

    return 1;
}


/* Returns a non-trivial permutable band starting from this loop; NULL
 * if the band is trivial (just the loop itself */
Band *pluto_get_permutable_band(Ploop *loop, PlutoProg *prog)
{
    int i, depth;

    depth = loop->depth;

    do{
        //printf("Level = %d\n", depth);
        for (i=0; i<prog->ndeps; i++) {
            Dep *dep = prog->deps[i];
            if (IS_RAR(dep->type)) continue;
            assert(dep->satvec != NULL);
            if (dep->satisfaction_level < loop->depth) continue;
            if (dep->satisfaction_level < depth && 
                    pluto_is_depth_scalar(loop, dep->satisfaction_level)) continue;
            if (pluto_stmt_is_member_of(prog->stmts[dep->src], loop->stmts, loop->nstmts)
                    && pluto_stmt_is_member_of(prog->stmts[dep->dest], loop->stmts, 
                        loop->nstmts)) {
                if (dep->dirvec[depth] == DEP_STAR || dep->dirvec[depth] == DEP_MINUS) 
                    break;
            }
            // printf("Dep %d\n", i+1);
        }
        if (i<prog->ndeps) {
            break;
        }
        depth++;
    }while (depth < prog->num_hyperplanes);

    /* Peel off scalar dimensions from the end */
    while (pluto_is_depth_scalar(loop, depth-1)) depth--;

    int width = depth - loop->depth;

    if (width == 1) {
        return NULL;
    }else{
        return pluto_band_alloc(loop, width);
    }
}

/* Returns subset of these bands that are not dominated by any other band */
Band **pluto_get_dominator_bands(Band **bands, int nbands, int *ndom_bands)
{
    Band **dom_bands;
    int i, j, ndbands;

    dom_bands = NULL;
    ndbands = 0;
    for (i=0; i<nbands; i++) {
        for (j=0; j<nbands; j++) {
            if (is_band_dominated(bands[i], bands[j])) break;
        }
        if (j==nbands) {
            Band *dup = pluto_band_dup(bands[i]);
            pluto_bands_concat(&dom_bands, ndbands, &dup, 1);
            ndbands++;
        }
    }
    *ndom_bands = ndbands;

    return dom_bands;
}


/* Set of outermost non-trivial permutable bands (of width >= 2) */
Band **pluto_get_outermost_permutable_bands(PlutoProg *prog, int *ndbands)
{
    Ploop **loops;
    int num, i, nbands;
    Band **bands, **dbands;

    num = 0;
    loops = pluto_get_all_loops(prog, &num);

    // pluto_print_dep_directions(prog);
    // pluto_loops_print(loops, num);

    bands = NULL;
    nbands = 0;
    for (i=0; i<num; i++) {
        Band *band = pluto_get_permutable_band(loops[i], prog);
        if (band != NULL) {
            bands = realloc(bands, (nbands+1)*sizeof(Band *));
            bands[nbands++] = band;
        }
    }

    // pluto_bands_print(bands, nbands);

    dbands = pluto_get_dominator_bands(bands, nbands, ndbands);

    pluto_loops_free(loops, num);

    pluto_bands_free(bands, nbands);

    return dbands;
}

int pluto_is_loop_innermost(const Ploop *loop, const PlutoProg *prog)
{
    int num=-1;

    pluto_get_loops_under(loop->stmts, loop->nstmts, loop->depth+1, prog, &num);

    return (num==0);
}

int pluto_is_band_innermost(const Band *band, int num_tiling_levels)
{
    int i, j;
    for (i=0; i<band->loop->nstmts; i++) {
        int firstd = band->loop->depth+(num_tiling_levels+1)*band->width;
        for (j=firstd; j<band->loop->stmts[i]->trans->nrows; j++) {
            if (pluto_is_hyperplane_loop(band->loop->stmts[i], j)) return 0;
        }
    }

    return 1;
}


/* Set of innermost non-trivial permutable bands (of width >= 2) */
Band **pluto_get_innermost_permutable_bands(PlutoProg *prog, int *ndbands)
{
    Ploop **loops;
    int num, i, nbands;
    Band **bands, **dbands;

    bands = NULL;
    num = 0;
    loops = pluto_get_all_loops(prog, &num);

    // pluto_print_dep_directions(prog);

    nbands = 0;
    for (i=0; i<num; i++) {
        Band *band = pluto_get_permutable_band(loops[i], prog);
        if (band != NULL && pluto_is_band_innermost(band, 0)) {
            bands = realloc(bands, (nbands+1)*sizeof(Band *));
            bands[nbands] = band;
            nbands++;
        }
    }

    // pluto_bands_print(bands, nbands);

    dbands = pluto_get_dominator_bands(bands, nbands, ndbands);

    for (i=0; i<num; i++) {
        pluto_loop_free(loops[i]);
    }
    free(loops);

    for (i=0; i<nbands; i++) {
        pluto_band_free(bands[i]);
    }
    free(bands);

    // pluto_bands_print(dbands, *ndbands);

    return dbands;
}
