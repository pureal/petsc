/*$Id: ex21.c,v 1.6 2001/01/22 23:07:10 bsmith Exp balay $*/

static char help[] = "Solves PDE optimization problem\n\n";

#include "petscda.h"
#include "petscpf.h"
#include "petscsnes.h"

/*

       w - design variables (what we change to get an optimal solution)
       u - state variables (i.e. the PDE solution)
       lambda - the Lagrange multipliers

            U = (w u lambda)

       fu, fw, flambda contain the gradient of L(w,u,lambda)

            FU = (fw fu flambda)

       In this example the PDE is 
                             Uxx = 2, 
                            u(0) = w(0), thus this is the free parameter
                            u(1) = 0
       the function we wish to minimize is 
                            \integral u^{2}

       The exact solution for u is given by u(x) = x*x - 1.25*x + .25

       Use the usual centered finite differences.

       Note we treat the problem as non-linear though it happens to be linear

       See ex22.c for the same code, but that interlaces the u and the lambda

*/

typedef struct {
  DA      da1,da2;
  int     nredundant;
  VecPack packer;
  PetscViewer  u_viewer,lambda_viewer;
  PetscViewer  fu_viewer,flambda_viewer;
} UserCtx;

extern int FormFunction(SNES,Vec,Vec,void*);
extern int Monitor(SNES,int,PetscReal,void*);


#undef __FUNC__
#define __FUNC__ "main"
int main(int argc,char **argv)
{
  int     ierr,N = 5,its;
  Vec     U,FU;
  SNES    snes;
  UserCtx user;

  ierr = PetscInitialize(&argc,&argv,(char*)0,help);CHKERRQ(ierr); 
  ierr = PetscOptionsGetInt(PETSC_NULL,"-N",&N,PETSC_NULL);CHKERRQ(ierr);

  /* Create a global vector that includes a single redundant array and two da arrays */
  ierr = VecPackCreate(PETSC_COMM_WORLD,&user.packer);CHKERRQ(ierr);
  user.nredundant = 1;
  ierr = VecPackAddArray(user.packer,user.nredundant);CHKERRQ(ierr);
  ierr = DACreate1d(PETSC_COMM_WORLD,DA_NONPERIODIC,N,1,1,PETSC_NULL,&user.da1);CHKERRQ(ierr);
  ierr = VecPackAddDA(user.packer,user.da1);CHKERRQ(ierr);
  ierr = DACreate1d(PETSC_COMM_WORLD,DA_NONPERIODIC,N,1,1,PETSC_NULL,&user.da2);CHKERRQ(ierr);
  ierr = VecPackAddDA(user.packer,user.da2);CHKERRQ(ierr);
  ierr = VecPackCreateGlobalVector(user.packer,&U);CHKERRQ(ierr);
  ierr = VecDuplicate(U,&FU);CHKERRQ(ierr);

  /* create graphics windows */
  ierr = PetscViewerDrawOpen(PETSC_COMM_WORLD,0,"u - state variables",-1,-1,-1,-1,&user.u_viewer);CHKERRQ(ierr);
  ierr = PetscViewerDrawOpen(PETSC_COMM_WORLD,0,"lambda - Lagrange multipliers",-1,-1,-1,-1,&user.lambda_viewer);CHKERRQ(ierr);
  ierr = PetscViewerDrawOpen(PETSC_COMM_WORLD,0,"fu - derivate w.r.t. state variables",-1,-1,-1,-1,&user.fu_viewer);CHKERRQ(ierr);
  ierr = PetscViewerDrawOpen(PETSC_COMM_WORLD,0,"flambda - derivate w.r.t. Lagrange multipliers",-1,-1,-1,-1,&user.flambda_viewer);CHKERRQ(ierr);


  /* create nonlinear solver */
  ierr = SNESCreate(PETSC_COMM_WORLD,SNES_NONLINEAR_EQUATIONS,&snes);CHKERRQ(ierr);
  ierr = SNESSetFunction(snes,FU,FormFunction,&user);CHKERRQ(ierr);
  ierr = SNESSetFromOptions(snes);CHKERRQ(ierr);
  ierr = SNESSetMonitor(snes,Monitor,&user,0);CHKERRQ(ierr);
  ierr = SNESSolve(snes,U,&its);CHKERRQ(ierr);
  ierr = SNESDestroy(snes);CHKERRQ(ierr);

  ierr = DADestroy(user.da1);CHKERRQ(ierr);
  ierr = DADestroy(user.da2);CHKERRQ(ierr);
  ierr = VecPackDestroy(user.packer);CHKERRQ(ierr);
  ierr = VecDestroy(U);CHKERRQ(ierr);
  ierr = VecDestroy(FU);CHKERRQ(ierr);
  ierr = PetscViewerDestroy(user.u_viewer);CHKERRQ(ierr);
  ierr = PetscViewerDestroy(user.lambda_viewer);CHKERRQ(ierr);
  ierr = PetscViewerDestroy(user.fu_viewer);CHKERRQ(ierr);
  ierr = PetscViewerDestroy(user.flambda_viewer);CHKERRQ(ierr);
  ierr = PetscFinalize();CHKERRQ(ierr);
  return 0;
}
 
/*
      Evaluates FU = Gradiant(L(w,u,lambda))

*/
int FormFunction(SNES snes,Vec U,Vec FU,void* dummy)
{
  UserCtx *user = (UserCtx*)dummy;
  int     ierr,xs,xm,i,N;
  Scalar  *u,*lambda,*w,*fu,*fw,*flambda,d,h;
  Vec     vu,vlambda,vfu,vflambda;

  PetscFunctionBegin;
  ierr = VecPackGetLocalVectors(user->packer,&w,&vu,&vlambda);CHKERRQ(ierr);
  ierr = VecPackGetLocalVectors(user->packer,&fw,&vfu,&vflambda);CHKERRQ(ierr);
  ierr = VecPackScatter(user->packer,U,w,vu,vlambda);CHKERRQ(ierr);

  ierr = DAGetCorners(user->da1,&xs,PETSC_NULL,PETSC_NULL,&xm,PETSC_NULL,PETSC_NULL);CHKERRQ(ierr);
  ierr = DAGetInfo(user->da1,0,&N,0,0,0,0,0,0,0,0,0);CHKERRQ(ierr);
  ierr = DAVecGetArray(user->da1,vu,(void**)&u);CHKERRQ(ierr);
  ierr = DAVecGetArray(user->da1,vfu,(void**)&fu);CHKERRQ(ierr);
  ierr = DAVecGetArray(user->da1,vlambda,(void**)&lambda);CHKERRQ(ierr);
  ierr = DAVecGetArray(user->da1,vflambda,(void**)&flambda);CHKERRQ(ierr);
  d    = (N-1.0);
  h    = 1.0/d;

  /* derivative of L() w.r.t. w */
  if (xs == 0) { /* only first processor computes this */
    fw[0] = -2.*d*lambda[0];
  }

  /* derivative of L() w.r.t. u */
  for (i=xs; i<xs+xm; i++) {
    if      (i == 0)   flambda[0]   =    h*u[0]   + 2.*d*lambda[0]   - d*lambda[1];
    else if (i == 1)   flambda[1]   = 2.*h*u[1]   + 2.*d*lambda[1]   - d*lambda[2];
    else if (i == N-1) flambda[N-1] =    h*u[N-1] + 2.*d*lambda[N-1] - d*lambda[N-2];
    else if (i == N-2) flambda[N-2] = 2.*h*u[N-2] + 2.*d*lambda[N-2] - d*lambda[N-3];
    else               flambda[i]   = 2.*h*u[i]   - d*(lambda[i+1] - 2.0*lambda[i] + lambda[i-1]);
  } 

  /* derivative of L() w.r.t. lambda */
  for (i=xs; i<xs+xm; i++) {
    if      (i == 0)   fu[0]   = 2.0*d*(u[0] - w[0]);
    else if (i == N-1) fu[N-1] = 2.0*d*u[N-1];
    else               fu[i]   = -(d*(u[i+1] - 2.0*u[i] + u[i-1]) - 2.0*h);
  } 

  ierr = DAVecRestoreArray(user->da1,vu,(void**)&u);CHKERRQ(ierr);
  ierr = DAVecRestoreArray(user->da1,vfu,(void**)&fu);CHKERRQ(ierr);
  ierr = DAVecRestoreArray(user->da1,vlambda,(void**)&lambda);CHKERRQ(ierr);
  ierr = DAVecRestoreArray(user->da1,vflambda,(void**)&flambda);CHKERRQ(ierr);

  ierr = VecPackGather(user->packer,FU,fw,vfu,vflambda);CHKERRQ(ierr);
  ierr = VecPackRestoreLocalVectors(user->packer,&w,&vu,&vlambda);CHKERRQ(ierr);
  ierr = VecPackRestoreLocalVectors(user->packer,&fw,&vfu,&vflambda);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

int Monitor(SNES snes,int its,PetscReal rnorm,void *dummy)
{
  UserCtx *user = (UserCtx*)dummy;
  int     ierr;
  Scalar  *w;
  Vec     u,lambda,U,F;

  PetscFunctionBegin;
  ierr = SNESGetSolution(snes,&U);CHKERRQ(ierr);
  ierr = VecPackGetAccess(user->packer,U,&w,&u,&lambda);CHKERRQ(ierr);
  ierr = VecView(u,user->u_viewer);
  ierr = VecView(lambda,user->lambda_viewer);
  ierr = VecPackRestoreAccess(user->packer,U,&w,&u,&lambda);CHKERRQ(ierr);

  ierr = SNESGetFunction(snes,&F,0,0);CHKERRQ(ierr);
  ierr = VecPackGetAccess(user->packer,F,&w,&u,&lambda);CHKERRQ(ierr);
  ierr = VecView(u,user->fu_viewer);
  ierr = VecView(lambda,user->flambda_viewer);
  ierr = VecPackRestoreAccess(user->packer,F,&w,&u,&lambda);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/* 
   This is currently not used. It computes the exact solution */

/*
int u_solution(void *dummy,int n,Scalar *x,Scalar *u)
{
  int i;
  PetscFunctionBegin;
  for (i=0; i<n; i++) {
    u[i] = x[i]*x[i] - 1.25*x[i] + .25;
  }
  PetscFunctionReturn(0);
}

  PF      pf;
  Vec     x;
  Vec     u_global;
  Scalar  *w;
  ierr = PFCreate(PETSC_COMM_WORLD,1,1,&pf);CHKERRQ(ierr);
  ierr = PFSetType(pf,PFQUICK,(void*)u_solution);CHKERRQ(ierr);
  ierr = DASetUniformCoordinates(user.da1,0.0,1.0,0.0,1.0,0.0,1.0);CHKERRQ(ierr);
  ierr = DAGetCoordinates(user.da1,&x);CHKERRQ(ierr);
  ierr = VecPackGetAccess(user.packer,U,&w,&u_global,0);CHKERRQ(ierr);
  w[0] = .25;
  ierr = PFApplyVec(pf,x,u_global);CHKERRQ(ierr);
  ierr = PFDestroy(pf);CHKERRQ(ierr);
*/


