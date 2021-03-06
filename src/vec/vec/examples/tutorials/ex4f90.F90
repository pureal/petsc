!
!
!  Description:  Illustrates the use of VecSetValues() to set
!  multiple values at once; demonstrates VecGetArrayF90().
!
!/*T
!   Concepts: vectors^assembling vectors;
!   Concepts: vectors^arrays;
!   Concepts: Fortran90^assembling vectors;
!   Processors: 1
!T*/
! -----------------------------------------------------------------------

      program main
#include <petsc/finclude/petscvec.h>
      use petscvec
      implicit none

! - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
!                 Beginning of program
! - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

       PetscScalar  xwork(6)
       PetscScalar, pointer ::  xx_v(:),yy_v(:)
       PetscInt i,n,loc(6)
       PetscErrorCode ierr
       Vec     x,y


       call PetscInitialize(PETSC_NULL_CHARACTER,ierr)
       if (ierr .ne. 0) then
         print*,'PetscInitialize failed'
         stop
       endif
       n = 6

!  Create initial vector and duplicate it

       call VecCreateSeq(PETSC_COMM_SELF,n,x,ierr);CHKERRQ(ierr)
       call VecDuplicate(x,y,ierr);CHKERRQ(ierr)

!  Fill work arrays with vector entries and locations.  Note that
!  the vector indices are 0-based in PETSc (for both Fortran and
!  C vectors)

       do 10 i=1,n
          loc(i) = i-1
          xwork(i) = 10.0*real(i)
  10   continue

!  Set vector values.  Note that we set multiple entries at once.
!  Of course, usually one would create a work array that is the
!  natural size for a particular problem (not one that is as long
!  as the full vector).

       call VecSetValues(x,n,loc,xwork,INSERT_VALUES,ierr);CHKERRQ(ierr)

!  Assemble vector

       call VecAssemblyBegin(x,ierr);CHKERRQ(ierr)
       call VecAssemblyEnd(x,ierr);CHKERRQ(ierr)

!  View vector
       call PetscObjectSetName(x, 'initial vector:',ierr);CHKERRQ(ierr)
       call VecView(x,PETSC_VIEWER_STDOUT_SELF,ierr);CHKERRQ(ierr)
       call VecCopy(x,y,ierr);CHKERRQ(ierr)

!  Get a pointer to vector data.
!    - For default PETSc vectors, VecGetArrayF90() returns a pointer to
!      the data array.  Otherwise, the routine is implementation dependent.
!    - You MUST call VecRestoreArray() when you no longer need access to
!      the array.

       call VecGetArrayF90(x,xx_v,ierr);CHKERRQ(ierr)
       call VecGetArrayF90(y,yy_v,ierr);CHKERRQ(ierr)

!  Modify vector data

       do 30 i=1,n
          xx_v(i) = 100.0*real(i)
          yy_v(i) = 1000.0*real(i)
  30   continue

!  Restore vectors

       call VecRestoreArrayF90(x,xx_v,ierr);CHKERRQ(ierr)
       call VecRestoreArrayF90(y,yy_v,ierr);CHKERRQ(ierr)

!  View vectors
       call PetscObjectSetName(x, 'new vector 1:',ierr);CHKERRQ(ierr)
       call VecView(x,PETSC_VIEWER_STDOUT_SELF,ierr);CHKERRQ(ierr)

       call PetscObjectSetName(y, 'new vector 2:',ierr);CHKERRQ(ierr)
       call VecView(y,PETSC_VIEWER_STDOUT_SELF,ierr);CHKERRQ(ierr)

!  Free work space.  All PETSc objects should be destroyed when they
!  are no longer needed.

       call VecDestroy(x,ierr);CHKERRQ(ierr)
       call VecDestroy(y,ierr);CHKERRQ(ierr)
       call PetscFinalize(ierr)
       end

