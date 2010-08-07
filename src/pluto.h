/*
 * PLuTo: An automatic parallelier and locality optimizer
 * 
 * Copyright (C) 2007 Uday Kumar Bondhugula
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * A copy of the GNU General Public Licence can be found in the 
 * top-level directory of this program (`COPYING') 
 *
 */
#ifndef _PLUTO_H_
#define _PLUTO_H_

#include <stdbool.h>

#include "math_support.h"
#include "constraints.h"
#include "ddg.h"

#define IF_DEBUG(foo) {if (options->debug || options->moredebug) { foo; }}
#define IF_DEBUG2(foo) {if (options->moredebug) {foo; }}

/* Do not fuse across SCCs */
#define NO_FUSE 0
/* Geared towards maximal fusion, but not really maximal fusion */
#define MAXIMAL_FUSE 1
/* Something in between the above two */
#define SMART_FUSE 2

/* max for various things */
#define MAX_VARS 100
#define MAX_PARS 100
#define MAX_STMTS 100
#define H_MAX_ROWS 100
#define MAX_DIM 20

#define MAX_CONSTRAINTS 10000
#define MAX_FARKAS_CST  2000

/* Maximum number of rows in the transformation matrix of a stmt */
#define MAX_TRANS_ROWS 256

#define MAX_TILING_LEVELS 2

#define DEFAULT_L1_TILE_SIZE 32

#define CST_WIDTH (npar+1+nstmts*(nvar+1)+1)

#define DEP_ZERO 0
#define DEP_PLUS 1
#define DEP_MINUS -1
#define DEP_STAR 2

#define H_LOOP 1
#define H_TILE_SPACE_LOOP 2
#define H_SCALAR 3

/* Candl dependences are not marked uniform/non-uniform */
#define IS_UNIFORM(type) (0)
#define IS_RAR(type) (type == CANDL_RAR)


struct plutoOptions{

    /* To tile or not? */
    int tile;

    /* parallel code gen */
    int parallel;

    /* Automatic unroll/unroll-jamming of loops */
    int unroll;

    /* unroll/jam factor */
    int ufactor;

    /* Enable or disable post-transformations to make code amenable to
     * vectorization (default - enabled) */
    int prevector;

    /* consider RAR dependences */
    int rar;

    /* Decides the fusion algorithm (MAXIMAL_FUSE, NO_FUSE, or SMART_FUSE) */
    int fuse;

    /* for debugging - print default cloog-style total */
    int scancount;

    /* parameters will be more than at least this much */
    /* setting parameter context for cloog */
    int context;

    /* multiple (currently two) degrees of pipelined parallelism */
    int multipipe;

    /* Tile for L2 too */
    /* By default, only L1 tiling is done; under parallel execution, every
     * processor executes a sequence of L1 tiles (OpenMP adds another blocking
     * on the parallel loop). With L2 tiling, each processor executes a
     * sequence of L2 tiles and barrier is done after a group of L2 tiles is
     * exectuted -- causes load imbalance due to pipe startup when problem
     * sizes are not huge */
    int l2tile;


    /* NOTE: --ft and --lt are to manually force tiling depths */
    /* First depth to tile (starting from 0) */
    int ft;
    /* Last depth to tile (indexed from 0)  */
    int lt;

    /* Output for debugging */
    int debug;

    /* More debugging output */
    int moredebug;

    /* Not implemented yet: Don't output anything unless something fails */
    int quiet;

    /* Pure polyhedral unrolling (instead of postpass) */
    int polyunroll;

    /* Generate scheduling pragmas for Bee+Cl@k */
    int bee;

    /* Force this for cloog's -f */
    int cloogf;

    /* Force this for cloog's -l */
    int cloogl;

    /* Use candl lastwriter */
    int lastwriter;

    /* DEV: Don't use cost function */
    int nobound;

    /* Ask candl to privatize */
    int scalpriv;

    /* No output from Pluto if everything goes right */
    int silent;
};
typedef struct plutoOptions PlutoOptions;

enum looptype {UNKNOWN=0, PARALLEL, PIPE_PARALLEL, SEQ, PIPE_PARALLEL_INNER_PARALLEL} PlutoLoopType;

struct statement{
    int id;

    PlutoInequalities *domain;

    /* Original iterator names */
    char **iterators;

    /* Statement text */
    char *text;

    /* Does this dimension appear in the original statement's domain? 
     * We have this due to LooPo's uniform structure of nvar 
     * dimensions for every statement's domain - but only some of them 
     * appear - the cleanup is done later  */
    bool is_outer_loop[MAX_VARS];

    /* Actual dimensionality of the statement's domain */
    int dim;

    /* Should you tile even if it's tilable? */
    int tile;

    /* Affine transformation matrix that will be completed step by step */
    /* this captures the A/B/G notation of the INRIA group - except that
     * we don't have parametric shifts
     */
    PlutoMatrix *trans;

    /* Is this loop a tile-space loop (supernode) or not? */
    bool is_supernode[MAX_VARS];

    /* Num of scattering dimensions tiled */
    int num_tiled_loops;

    /* Num of independent soln's needed */
    int num_ind_sols;

    /* ID of the SCC this statement belongs to */
    int scc_id;
};
typedef struct statement Stmt;


struct dependence{

    /* Unique number of the dependence: starts with 0  */
    int id;

    /* Source statement ID -- can be used to directly index into Stmt *stmts */
    int src;

    /* Dest statement ID */
    int dest;

    /* 
     * Dependence polyhedra (both src & dest) 
     * (product space)
     *
     * [src|dest|par|const] >= 0
     * [nvar|nvar|npar|1]
     */
    PlutoInequalities *dpolytope;

    /* Dependence type from Candl (raw, war, or rar) */
    int type;

    /* Has this dependence been satisfied? */
    bool satisfied;

    /* Level at which the dependence is satisfied */
    int satisfaction_level;

    /* Dependence direction in the transformed space */
    int direction[MAX_TRANS_ROWS];

};
typedef struct dependence Dep;


typedef enum unrollType {NO_UNROLL, UNROLL, UNROLLJAM} UnrollType;


/* Properties of the new hyperplanes found. These are common across all
 * statements or apply at a level across all statements 
 */
struct hyperplane_properties{

    /* Hyperplane property: see looptype enum definition */
    enum looptype dep_prop;

    /* Hyperplane type: scalar or loop */
    int type;

    /* The band number this hyperplane belongs to. Note that everything is a
     * hierarchy of permutable loop nests (it's not a tree, but a straight
     * hierarchy) */
    int band_num;

    /* Unroll or Unroll-jam this dimension? */
    UnrollType unroll;
};
typedef struct hyperplane_properties HyperplaneProperties;

struct plutoProg{
    /* Array of statements */
    Stmt *stmts;
    int nstmts;

    /* Array of dependences */
    Dep *deps;
    int ndeps;

    /* Parameters */
    char **params;

    /* Number of hyperplanes that represent the transformed space
     * same as stmts[i].trans->nrows, for all i */
    int num_hyperplanes;

    /* Data dependence graph of the program */
    Graph *ddg;

    /* Options for Pluto */
    PlutoOptions *options;

    /* Hyperplane properties */
    HyperplaneProperties *hProps;
};
typedef struct plutoProg PlutoProg;


/* Globally visible, easily accessible data */
extern int npar;
extern int nvar;
extern PlutoOptions *options;

void dep_alloc_members(Dep *);
void dep_free(Dep *);

bool dep_satisfaction_test (Dep *dep, Stmt *stmts, int nstmts, int level);
int dep_satisfaction_update (PlutoProg *prog, int level);
bool dep_is_satisfied(Dep *dep);

PlutoInequalities *get_permutability_constraints(Dep *, int, Stmt *, int);
PlutoInequalities **get_stmt_ortho_constraints(Stmt *stmt, int, HyperplaneProperties *, int *);
PlutoInequalities *get_non_trivial_sol_constraints(Stmt *stmts, int nstmts);

void pluto_auto_transform (PlutoProg *prog);
int  pluto_codegen (FILE *fp, FILE *outfp, PlutoProg *prog);

int  find_permutable_hyperplanes (PlutoProg *prog, int max_sols);
void detect_hyperplane_type(Stmt *stmts, int nstmts, Dep *deps, int ndeps, int, int, int);
int  get_dep_direction (Dep *dep, Stmt *stmts, int nstmts, int level);

void getInnermostTilableBand(PlutoProg *prog, int *bandStart, int *bandEnd);
void getOutermostTilableBand(PlutoProg *prog, int *bandStart, int *bandEnd);

void print_cloog_file(FILE *fp, PlutoProg *prog);
void cut_lightest_edge (Stmt *stmts, int nstmts, Dep *deps, int ndeps, int);
void pluto_tile(PlutoProg *);
void tile_scattering_dims(PlutoProg *, int, int, int *);
bool create_tile_schedule(PlutoProg *prog, int firstD, int lastD);

int generate_openmp_pragmas(PlutoProg *prog);

void   ddg_update (Graph *g, PlutoProg *prog);
void   ddg_compute_scc (PlutoProg *prog);
Graph *ddg_create (PlutoProg *prog);
int    ddg_sccs_direct_conn (Graph *g, PlutoProg *prog, int scc1, int scc2);

void unroll_phis (PlutoProg *prog, int unroll_dim, int ufactor);
void pluto_print_transformations(PlutoProg *prog);

void pretty_print_affine_function(FILE *fp, Stmt *stmt, int level);
void print_hyperplane_properties(HyperplaneProperties *hProps, int num_hyperplanes);

#endif