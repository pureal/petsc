/*$Id: ex5.c,v 1.87 2001/01/17 22:25:35 bsmith Exp balay $*/

static char help[] = "Solves two linear systems in parallel with SLES.  The code\n\
illustrates repeated solution of linear systems with the same preconditioner\n\
method but different matrices (having the same nonzero structure).  The code\n\
also uses multiple profiling stages.  Input arguments are\n\
  -m <size> : problem size\n\
  -mat_nonsym : use nonsymmetric matrix (default is symmetric)\n\n";

/*T
   Concepts: SLES^repeatedly solving linear systems;
   Concepts: PetscLog^profiling multiple stages of code;
   Processors: n
T*/

/* 
  Include "petscsles.h" so that we can use SLES solvers.  Note that this file
  automatically includes:
     petsc.h       - base PETSc routines   petscvec.h - vectors
     petscsys.h    - system routines       petscmat.h - matrices
     petscis.h     - index sets            petscksp.h - Krylov subspace methods
     petscviewer.h - viewers               petscpc.h  - preconditioners
*/
#include "petscsles.h"

#undef __FUNC__
#define __FUNC__ "main"
int main(int argc,char **args)
{
  SLES       sles;             /* linear solver context */
  Mat        C;                /* matrix */
  Vec        x,u,b;          /* approx solution, RHS, exact solution */
  double     norm;             /* norm of solution error */
  Scalar     v,none = -1.0;
  int        I,J,ldim,ierr,low,high,iglobal,Istart,Iend;
  int        i,j,m = 3,n = 2,rank,size,its;
  PetscTruth mat_nonsymmetric;

  PetscInitialize(&argc,&args,(char *)0,help);
  ierr = PetscOptionsGetInt(PETSC_NULL,"-m",&m,PETSC_NULL);CHKERRQ(ierr);
  ierr = MPI_Comm_rank(PETSC_COMM_WORLD,&rank);CHKERRQ(ierr);
  ierr = MPI_Comm_size(PETSC_COMM_WORLD,&size);CHKERRQ(ierr);
  n = 2*size;

  /*
     Set flag if we are doing a nonsymmetric problem; the default is symmetric.
  */
  ierr = PetscOptionsHasName(PETSC_NULL,"-mat_nonsym",&mat_nonsymmetric);CHKERRQ(ierr);

  /*
     Register two stages for separate profiling of the two linear solves.
     Use the runtime option -log_summary for a printout of performance
     statistics at the program's conlusion.
  */
  ierr = PetscLogStageRegister(0,"Original Solve");CHKERRQ(ierr);
  ierr = PetscLogStageRegister(1,"Second Solve");CHKERRQ(ierr);

  /* -------------- Stage 0: Solve Original System ---------------------- */
  /* 
     Indicate to PETSc profiling that we're beginning the first stage
  */
  ierr = PetscLogStagePush(0);CHKERRQ(ierr);

  /* 
     Create parallel matrix, specifying only its global dimensions.
     When using MatCreate(), the matrix format can be specified at
     runtime. Also, the parallel partitioning of the matrix is
     determined by PETSc at runtime.
  */
  ierr = MatCreate(PETSC_COMM_WORLD,PETSC_DECIDE,PETSC_DECIDE,m*n,m*n,&C);CHKERRQ(ierr);
  ierr = MatSetFromOptions(C);CHKERRQ(ierr);

  /* 
     Currently, all PETSc parallel matrix formats are partitioned by
     contiguous chunks of rows across the processors.  Determine which
     rows of the matrix are locally owned. 
  */
  ierr = MatGetOwnershipRange(C,&Istart,&Iend);CHKERRQ(ierr);

  /* 
     Set matrix entries matrix in parallel.
      - Each processor needs to insert only elements that it owns
        locally (but any non-local elements will be sent to the
        appropriate processor during matrix assembly). 
      - Always specify global row and columns of matrix entries.
  */
  for (I=Istart; I<Iend; I++) { 
    v = -1.0; i = I/n; j = I - i*n;  
    if (i>0)   {J = I - n; ierr = MatSetValues(C,1,&I,1,&J,&v,ADD_VALUES);CHKERRQ(ierr);}
    if (i<m-1) {J = I + n; ierr = MatSetValues(C,1,&I,1,&J,&v,ADD_VALUES);CHKERRQ(ierr);}
    if (j>0)   {J = I - 1; ierr = MatSetValues(C,1,&I,1,&J,&v,ADD_VALUES);CHKERRQ(ierr);}
    if (j<n-1) {J = I + 1; ierr = MatSetValues(C,1,&I,1,&J,&v,ADD_VALUES);CHKERRQ(ierr);}
    v = 4.0; ierr = MatSetValues(C,1,&I,1,&I,&v,ADD_VALUES);
  }

  /*
     Make the matrix nonsymmetric if desired
  */
  if (mat_nonsymmetric) {
    for (I=Istart; I<Iend; I++) { 
      v = -1.5; i = I/n;
      if (i>1)   {J = I-n-1; ierr = MatSetValues(C,1,&I,1,&J,&v,ADD_VALUES);CHKERRQ(ierr);}
    }
  } else {
    ierr = MatSetOption(C,MAT_SYMMETRIC);CHKERRQ(ierr);
  }

  /* 
     Assemble matrix, using the 2-step process:
       MatAssemblyBegin(), MatAssemblyEnd()
     Computations can be done while messages are in transition
     by placing code between these two statements.
  */
  ierr = MatAssemblyBegin(C,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(C,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);

  /* 
     Create parallel vectors.
      - When using VecCreate(), we specify only the vector's global
        dimension; the parallel partitioning is determined at runtime. 
      - Note: We form 1 vector from scratch and then duplicate as needed.
  */
  ierr = VecCreateMPI(PETSC_COMM_WORLD,PETSC_DECIDE,m*n,&u);CHKERRQ(ierr);
  ierr = VecSetFromOptions(u);CHKERRQ(ierr);
  ierr = VecDuplicate(u,&b);CHKERRQ(ierr);
  ierr = VecDuplicate(b,&x);CHKERRQ(ierr);

  /* 
     Currently, all parallel PETSc vectors are partitioned by
     contiguous chunks across the processors.  Determine which
     range of entries are locally owned.
  */
  ierr = VecGetOwnershipRange(x,&low,&high);CHKERRQ(ierr);

  /*
    Set elements within the exact solution vector in parallel.
     - Each processor needs to insert only elements that it owns
       locally (but any non-local entries will be sent to the
       appropriate processor during vector assembly).
     - Always specify global locations of vector entries.
  */
  ierr = VecGetLocalSize(x,&ldim);CHKERRQ(ierr);
  for (i=0; i<ldim; i++) {
    iglobal = i + low;
    v = (Scalar)(i + 100*rank);
    ierr = VecSetValues(u,1,&iglobal,&v,INSERT_VALUES);CHKERRQ(ierr);
  }

  /* 
     Assemble vector, using the 2-step process:
       VecAssemblyBegin(), VecAssemblyEnd()
     Computations can be done while messages are in transition,
     by placing code between these two statements.
  */
  ierr = VecAssemblyBegin(u);CHKERRQ(ierr);
  ierr = VecAssemblyEnd(u);CHKERRQ(ierr);

  /* 
     Compute right-hand-side vector
  */
  ierr = MatMult(C,u,b);CHKERRQ(ierr);
 
  /* 
    Create linear solver context
  */
  ierr = SLESCreate(PETSC_COMM_WORLD,&sles);CHKERRQ(ierr);

  /* 
     Set operators. Here the matrix that defines the linear system
     also serves as the preconditioning matrix.
  */
  ierr = SLESSetOperators(sles,C,C,DIFFERENT_NONZERO_PATTERN);CHKERRQ(ierr);

  /* 
     Set runtime options (e.g., -ksp_type <type> -pc_type <type>)
  */

  ierr = SLESSetFromOptions(sles);CHKERRQ(ierr);

  /* 
     Solve linear system.  Here we explicitly call SLESSetUp() for more
     detailed performance monitoring of certain preconditioners, such
     as ICC and ILU.  This call is optional, as SLESSetUp() will
     automatically be called within SLESSolve() if it hasn't been
     called already.
  */
  ierr = SLESSetUp(sles,b,x);CHKERRQ(ierr);
  ierr = SLESSolve(sles,b,x,&its);CHKERRQ(ierr);
 
  /* 
     Check the error
  */
  ierr = VecAXPY(&none,u,x);CHKERRQ(ierr);
  ierr = VecNorm(x,NORM_2,&norm);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Norm of error %A, Iterations %d\n",norm,its);CHKERRQ(ierr);

  /* -------------- Stage 1: Solve Second System ---------------------- */
  /* 
     Solve another linear system with the same method.  We reuse the SLES
     context, matrix and vector data structures, and hence save the
     overhead of creating new ones.

     Indicate to PETSc profiling that we're concluding the first
     stage with PetscLogStagePop(), and beginning the second stage with
     PetscLogStagePush().
  */
  ierr = PetscLogStagePop();CHKERRQ(ierr);
  ierr = PetscLogStagePush(1);CHKERRQ(ierr);

  /* 
     Initialize all matrix entries to zero.  MatZeroEntries() retains the
     nonzero structure of the matrix for sparse formats.
  */
  ierr = MatZeroEntries(C);CHKERRQ(ierr);

  /* 
     Assemble matrix again.  Note that we retain the same matrix data
     structure and the same nonzero pattern; we just change the values
     of the matrix entries.
  */
  for (i=0; i<m; i++) { 
    for (j=2*rank; j<2*rank+2; j++) {
      v = -1.0;  I = j + n*i;
      if (i>0)   {J = I - n; ierr = MatSetValues(C,1,&I,1,&J,&v,ADD_VALUES);CHKERRQ(ierr);}
      if (i<m-1) {J = I + n; ierr = MatSetValues(C,1,&I,1,&J,&v,ADD_VALUES);CHKERRQ(ierr);}
      if (j>0)   {J = I - 1; ierr = MatSetValues(C,1,&I,1,&J,&v,ADD_VALUES);CHKERRQ(ierr);}
      if (j<n-1) {J = I + 1; ierr = MatSetValues(C,1,&I,1,&J,&v,ADD_VALUES);CHKERRQ(ierr);}
      v = 6.0; ierr = MatSetValues(C,1,&I,1,&I,&v,ADD_VALUES);CHKERRQ(ierr);
    }
  } 
  if (mat_nonsymmetric) {
    for (I=Istart; I<Iend; I++) { 
      v = -1.5; i = I/n;
      if (i>1)   {J = I-n-1; ierr = MatSetValues(C,1,&I,1,&J,&v,ADD_VALUES);CHKERRQ(ierr);}
    }
  }
  ierr = MatAssemblyBegin(C,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(C,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr); 

  /* 
     Compute another right-hand-side vector
  */
  ierr = MatMult(C,u,b);CHKERRQ(ierr);

  /* 
     Set operators. Here the matrix that defines the linear system
     also serves as the preconditioning matrix.
      - The flag SAME_NONZERO_PATTERN indicates that the
        preconditioning matrix has identical nonzero structure
        as during the last linear solve (although the values of
        the entries have changed). Thus, we can save some
        work in setting up the preconditioner (e.g., no need to
        redo symbolic factorization for ILU/ICC preconditioners).
      - If the nonzero structure of the matrix is different during
        the second linear solve, then the flag DIFFERENT_NONZERO_PATTERN
        must be used instead.  If you are unsure whether the
        matrix structure has changed or not, use the flag
        DIFFERENT_NONZERO_PATTERN.
      - Caution:  If you specify SAME_NONZERO_PATTERN, PETSc
        believes your assertion and does not check the structure
        of the matrix.  If you erroneously claim that the structure
        is the same when it actually is not, the new preconditioner
        will not function correctly.  Thus, use this optimization
        feature with caution!
  */
  ierr = SLESSetOperators(sles,C,C,SAME_NONZERO_PATTERN);CHKERRQ(ierr);

  /* 
     Solve linear system
  */
  ierr = SLESSetUp(sles,b,x);CHKERRQ(ierr);
  ierr = SLESSolve(sles,b,x,&its);CHKERRQ(ierr);

  /* 
     Check the error
  */
  ierr = VecAXPY(&none,u,x);CHKERRQ(ierr);
  ierr = VecNorm(x,NORM_2,&norm);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Norm of error %A, Iterations %d\n",norm,its);CHKERRQ(ierr);

  /* 
     Free work space.  All PETSc objects should be destroyed when they
     are no longer needed.
  */
  ierr = SLESDestroy(sles);CHKERRQ(ierr);
  ierr = VecDestroy(u);CHKERRQ(ierr);
  ierr = VecDestroy(x);CHKERRQ(ierr);
  ierr = VecDestroy(b);CHKERRQ(ierr);
  ierr = MatDestroy(C);CHKERRQ(ierr);

  /*
     Indicate to PETSc profiling that we're concluding the second stage 
  */
  ierr = PetscLogStagePop();CHKERRQ(ierr);

  ierr = PetscFinalize();CHKERRQ(ierr);
  return 0;
}


