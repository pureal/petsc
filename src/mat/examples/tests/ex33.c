/*$Id: ex33.c,v 1.17 2001/01/17 22:23:09 bsmith Exp balay $*/

static char help[] = 
"Writes a matrix using the PETSc sparse format. Input arguments are:\n\
   -fout <file> : output file name\n\n";

#include "petscmat.h"

#undef __FUNC__
#define __FUNC__ "main"
int main(int argc,char **args)
{
  Mat        A;
  Vec        b;
  char       fileout[128];
  int        i,j,m = 6,n = 6,N = 36,ierr,I,J;
  PetscTruth flg;
  Scalar     val,v;
  PetscViewer     view;

  PetscInitialize(&argc,&args,(char *)0,help);

  ierr = PetscOptionsHasName(PETSC_NULL,"-use_mataij",&flg);CHKERRQ(ierr);
  if (flg) {
    ierr = MatCreateMPIAIJ(PETSC_COMM_WORLD,PETSC_DECIDE,PETSC_DECIDE,N,N,
                           PETSC_DEFAULT,PETSC_NULL,PETSC_DEFAULT,PETSC_NULL,&A);CHKERRQ(ierr);
#if defined(PETSC_HAVE_BLOCKSOLVE) && !defined(PETSC_USE_COMPLEX)
  } else {
    ierr = MatCreateMPIRowbs(PETSC_COMM_WORLD,PETSC_DECIDE,N,6,PETSC_NULL,&A);CHKERRQ(ierr);
#endif
  }

  for (i=0; i<m; i++) {
    for (j=0; j<n; j++) {
      v = -1.0;  I = j + n*i;
      if (i>0)   {J = I - n; ierr = MatSetValues(A,1,&I,1,&J,&v,INSERT_VALUES);CHKERRQ(ierr);}
      if (i<m-1) {J = I + n; ierr = MatSetValues(A,1,&I,1,&J,&v,INSERT_VALUES);CHKERRQ(ierr);}
      if (j>0)   {J = I - 1; ierr = MatSetValues(A,1,&I,1,&J,&v,INSERT_VALUES);CHKERRQ(ierr);}
      if (j<n-1) {J = I + 1; ierr = MatSetValues(A,1,&I,1,&J,&v,INSERT_VALUES);CHKERRQ(ierr);}
      v = 4.0; ierr = MatSetValues(A,1,&I,1,&I,&v,INSERT_VALUES);CHKERRQ(ierr);
    }
  }
  ierr = MatAssemblyBegin(A,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(A,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);

  ierr = VecCreateMPI(PETSC_COMM_WORLD,PETSC_DECIDE,N,&b);CHKERRQ(ierr);
  for (i=0; i<N; i++) {
    val = i + 1;
    ierr = VecSetValues(b,1,&i,&val,INSERT_VALUES);CHKERRQ(ierr);
  }
  ierr = VecAssemblyBegin(b);CHKERRQ(ierr);
  ierr = VecAssemblyEnd(b);CHKERRQ(ierr);

  ierr = PetscOptionsGetString(PETSC_NULL,"-fout",fileout,127,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,fileout,PETSC_BINARY_CREATE,&view);CHKERRQ(ierr);
  ierr = MatView(A,view);CHKERRQ(ierr);
  ierr = VecView(b,view);CHKERRQ(ierr);
  ierr = PetscViewerDestroy(view);CHKERRQ(ierr);

  ierr = VecDestroy(b);CHKERRQ(ierr);
  ierr = MatDestroy(A);CHKERRQ(ierr);

  ierr = PetscFinalize();CHKERRQ(ierr);
  return 0;
}

