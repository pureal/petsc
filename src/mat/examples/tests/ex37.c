/*$Id: ex37.c,v 1.19 2001/01/17 22:23:09 bsmith Exp balay $*/

static char help[] = "Tests MatCopy() and MatStore/RetrieveValues().\n\n"; 

#include "petscmat.h"

#undef __FUNC__
#define __FUNC__ "main"
int main(int argc,char **args)
{
  Mat         C,A; 
  int         i, n = 10,midx[3],ierr;
  Scalar      v[3];
  PetscTruth  flg;

  PetscInitialize(&argc,&args,(char *)0,help);
  ierr = PetscOptionsGetInt(PETSC_NULL,"-n",&n,PETSC_NULL);CHKERRQ(ierr);

  ierr = MatCreate(PETSC_COMM_WORLD,PETSC_DECIDE,PETSC_DECIDE,n,n,&C);CHKERRQ(ierr);
  ierr = MatSetFromOptions(C);CHKERRQ(ierr);
  ierr = MatCreate(PETSC_COMM_WORLD,PETSC_DECIDE,PETSC_DECIDE,n,n,&A);CHKERRQ(ierr);
  ierr = MatSetFromOptions(A);CHKERRQ(ierr);

  v[0] = -1.; v[1] = 2.; v[2] = -1.;
  for (i=1; i<n-1; i++){
    midx[2] = i-1; midx[1] = i; midx[0] = i+1;
    ierr = MatSetValues(C,1,&i,3,midx,v,INSERT_VALUES);CHKERRQ(ierr);
  }
  i = 0; midx[0] = 0; midx[1] = 1;
  v[0] = 2.0; v[1] = -1.; 
  ierr = MatSetValues(C,1,&i,2,midx,v,INSERT_VALUES);CHKERRQ(ierr);
  i = n-1; midx[0] = n-2; midx[1] = n-1;
  v[0] = -1.0; v[1] = 2.; 
  ierr = MatSetValues(C,1,&i,2,midx,v,INSERT_VALUES);CHKERRQ(ierr);

  ierr = MatAssemblyBegin(C,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(C,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);

  /* test matrices with different nonzero patterns */
  ierr = MatCopy(C,A,DIFFERENT_NONZERO_PATTERN);CHKERRQ(ierr);

  /* Now C and A have the same nonzero pattern */
  ierr = MatSetOption(C,MAT_NO_NEW_NONZERO_LOCATIONS);CHKERRQ(ierr);
  ierr = MatSetOption(A,MAT_NO_NEW_NONZERO_LOCATIONS);CHKERRQ(ierr);
  ierr = MatCopy(C,A,SAME_NONZERO_PATTERN);CHKERRQ(ierr);

  ierr = MatView(C,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
  ierr = MatView(A,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);

  ierr = MatEqual(A,C,&flg);CHKERRQ(ierr);
  if (flg) {
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Matrices are equal\n");CHKERRQ(ierr);
  } else {
    SETERRQ(1,"Matrices are NOT equal");
  }

  ierr = MatStoreValues(A);CHKERRQ(ierr);
  ierr = MatZeroEntries(A);CHKERRQ(ierr);
  ierr = MatRetrieveValues(A);CHKERRQ(ierr);
  ierr = MatEqual(A,C,&flg);CHKERRQ(ierr);
  if (flg) {
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Matrices are equal\n");CHKERRQ(ierr);
  } else {
    SETERRQ(1,"Matrices are NOT equal");
  }

  ierr = MatDestroy(C);CHKERRQ(ierr);
  ierr = MatDestroy(A);CHKERRQ(ierr);

  ierr = PetscFinalize();CHKERRQ(ierr);
  return 0;
}

 
