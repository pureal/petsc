/*$Id: ex14.c,v 1.8 2001/01/17 22:20:33 bsmith Exp balay $*/

/* 
   Tests PetscOptionsGetScalar() for complex numbers
 */

#include "petsc.h"


#undef __FUNC__
#define __FUNC__ "main"
int main(int argc,char **argv)
{
  int    ierr;
  Scalar a;

  PetscInitialize(&argc,&argv,(char *)0,0);
  ierr = PetscOptionsGetScalar(PETSC_NULL,"-a",&a,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_SELF,"Scalar a = %g + %gi\n",PetscRealPart(a),PetscImaginaryPart(a));CHKERRQ(ierr);
  ierr = PetscFinalize();CHKERRQ(ierr);
  return 0;
}
 
