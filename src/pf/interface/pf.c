/*
    The PF mathematical functions interface routines, callable by users.
*/
#include "src/pf/pfimpl.h"            /*I "petscpf.h" I*/

/* Logging support */
int PF_COOKIE = 0;

PetscFList PPetscFList         = PETSC_NULL; /* list of all registered PD functions */
PetscTruth PFRegisterAllCalled = PETSC_FALSE;

#undef __FUNCT__  
#define __FUNCT__ "PFSet"
/*@C
   PFSet - Sets the C/C++/Fortran functions to be used by the PF function

   Collective on PF

   Input Parameter:
+  pf - the function context
.  apply - function to apply to an array
.  applyvec - function to apply to a Vec
.  view - function that prints information about the PF
.  destroy - function to free the private function context
-  ctx - private function context

   Level: beginner

.keywords: PF, setting

.seealso: PFCreate(), PFDestroy(), PFSetType(), PFApply(), PFApplyVec()
@*/
int PFSet(PF pf,int(*apply)(void*,int,PetscScalar*,PetscScalar*),int(*applyvec)(void*,Vec,Vec),int(*view)(void*,PetscViewer),int(*destroy)(void*),void*ctx)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pf,PF_COOKIE,1);
  pf->data             = ctx;

  pf->ops->destroy     = destroy;
  pf->ops->apply       = apply;
  pf->ops->applyvec    = applyvec;
  pf->ops->view        = view;

  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PFDestroy"
/*@C
   PFDestroy - Destroys PF context that was created with PFCreate().

   Collective on PF

   Input Parameter:
.  pf - the function context

   Level: beginner

.keywords: PF, destroy

.seealso: PFCreate(), PFSet(), PFSetType()
@*/
int PFDestroy(PF pf)
{
  int        ierr;
  PetscTruth flg;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pf,PF_COOKIE,1);
  if (--pf->refct > 0) PetscFunctionReturn(0);

  ierr = PetscOptionsHasName(pf->prefix,"-pf_view",&flg);CHKERRQ(ierr);
  if (flg) {
    ierr = PFView(pf,PETSC_VIEWER_STDOUT_(pf->comm));CHKERRQ(ierr);
  }

  /* if memory was published with AMS then destroy it */
  ierr = PetscObjectDepublish(pf);CHKERRQ(ierr);

  if (pf->ops->destroy) {ierr =  (*pf->ops->destroy)(pf->data);CHKERRQ(ierr);}
  PetscLogObjectDestroy(pf);
  PetscHeaderDestroy(pf);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PFPublish_Petsc"
static int PFPublish_Petsc(PetscObject obj)
{
#if defined(PETSC_HAVE_AMS)
  PF          v = (PF) obj;
  int         ierr;
#endif

  PetscFunctionBegin;

#if defined(PETSC_HAVE_AMS)
  /* if it is already published then return */
  if (v->amem >=0) PetscFunctionReturn(0);

  ierr = PetscObjectPublishBaseBegin(obj);CHKERRQ(ierr);
  ierr = PetscObjectPublishBaseEnd(obj);CHKERRQ(ierr);
#endif

  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PFCreate"
/*@C
   PFCreate - Creates a mathematical function context.

   Collective on MPI_Comm

   Input Parameter:
+  comm - MPI communicator 
.  dimin - dimension of the space you are mapping from
-  dimout - dimension of the space you are mapping to

   Output Parameter:
.  pf - the function context

   Level: developer

.keywords: PF, create, context

.seealso: PFSetUp(), PFApply(), PFDestroy(), PFApplyVec()
@*/
int PFCreate(MPI_Comm comm,int dimin,int dimout,PF *pf)
{
  PF  newpf;
  int ierr;

  PetscFunctionBegin;
  PetscValidPointer(pf,1);
  *pf = PETSC_NULL;
#ifndef PETSC_USE_DYNAMIC_LIBRARIES
  ierr = VecInitializePackage(PETSC_NULL);CHKERRQ(ierr);   
#endif

  PetscHeaderCreate(newpf,_p_PF,struct _PFOps,PF_COOKIE,-1,"PF",comm,PFDestroy,PFView);
  PetscLogObjectCreate(newpf);
  newpf->bops->publish    = PFPublish_Petsc;
  newpf->data             = 0;

  newpf->ops->destroy     = 0;
  newpf->ops->apply       = 0;
  newpf->ops->applyvec    = 0;
  newpf->ops->view        = 0;
  newpf->dimin            = dimin;
  newpf->dimout           = dimout;

  *pf                     = newpf;
  ierr = PetscPublishAll(pf);CHKERRQ(ierr);
  PetscFunctionReturn(0);

}

/* -------------------------------------------------------------------------------*/

#undef __FUNCT__  
#define __FUNCT__ "PFApplyVec"
/*@
   PFApplyVec - Applies the mathematical function to a vector

   Collective on PF

   Input Parameters:
+  pf - the function context
-  x - input vector (or PETSC_NULL for the vector (0,1, .... N-1)

   Output Parameter:
.  y - output vector

   Level: beginner

.keywords: PF, apply

.seealso: PFApply(), PFCreate(), PFDestroy(), PFSetType(), PFSet()
@*/
int PFApplyVec(PF pf,Vec x,Vec y)
{
  int        ierr,i,rstart,rend,n,p;
  PetscTruth nox = PETSC_FALSE;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pf,PF_COOKIE,1);
  PetscValidHeaderSpecific(y,VEC_COOKIE,3);
  if (x) {
    PetscValidHeaderSpecific(x,VEC_COOKIE,2);
    if (x == y) SETERRQ(PETSC_ERR_ARG_IDN,"x and y must be different vectors");
  } else {
    PetscScalar *xx;

    ierr = VecDuplicate(y,&x);CHKERRQ(ierr);
    nox  = PETSC_TRUE;
    ierr = VecGetOwnershipRange(x,&rstart,&rend);CHKERRQ(ierr);
    ierr = VecGetArray(x,&xx);CHKERRQ(ierr);
    for (i=rstart; i<rend; i++) {
      xx[i-rstart] = (PetscScalar)i;
    }
    ierr = VecRestoreArray(x,&xx);CHKERRQ(ierr);
  }

  ierr = VecGetLocalSize(x,&n);CHKERRQ(ierr);
  ierr = VecGetLocalSize(y,&p);CHKERRQ(ierr);
  if (pf->dimin*(n/pf->dimin) != n) SETERRQ2(PETSC_ERR_ARG_IDN,"Local input vector length %d not divisible by dimin %d of function",n,pf->dimin);
  if (pf->dimout*(p/pf->dimout) != p) SETERRQ2(PETSC_ERR_ARG_IDN,"Local output vector length %d not divisible by dimout %d of function",p,pf->dimout);
  if (n/pf->dimin != p/pf->dimout) SETERRQ4(PETSC_ERR_ARG_IDN,"Local vector lengths %d %d are wrong for dimin and dimout %d %d of function",n,p,pf->dimin,pf->dimout);

  if (pf->ops->applyvec) {
    ierr = (*pf->ops->applyvec)(pf->data,x,y);CHKERRQ(ierr);
  } else {
    PetscScalar *xx,*yy;

    ierr = VecGetLocalSize(x,&n);CHKERRQ(ierr);
    n    = n/pf->dimin;
    ierr = VecGetArray(x,&xx);CHKERRQ(ierr);
    ierr = VecGetArray(y,&yy);CHKERRQ(ierr);
    if (!pf->ops->apply) SETERRQ(1,"No function has been provided for this PF");
    ierr = (*pf->ops->apply)(pf->data,n,xx,yy);CHKERRQ(ierr);
    ierr = VecRestoreArray(x,&xx);CHKERRQ(ierr);
    ierr = VecRestoreArray(y,&yy);CHKERRQ(ierr);
  }
  if (nox) {
    ierr = VecDestroy(x);CHKERRQ(ierr);
  } 
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PFApply"
/*@
   PFApply - Applies the mathematical function to an array of values.

   Collective on PF

   Input Parameters:
+  pf - the function context
.  n - number of pointwise function evaluations to perform, each pointwise function evaluation
       is a function of dimin variables and computes dimout variables where dimin and dimout are defined
       in the call to PFCreate()
-  x - input array

   Output Parameter:
.  y - output array

   Level: beginner

   Notes: 

.keywords: PF, apply

.seealso: PFApplyVec(), PFCreate(), PFDestroy(), PFSetType(), PFSet()
@*/
int PFApply(PF pf,int n,PetscScalar* x,PetscScalar* y)
{
  int        ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pf,PF_COOKIE,1);
  PetscValidScalarPointer(x,2);
  PetscValidScalarPointer(y,3);
  if (x == y) SETERRQ(PETSC_ERR_ARG_IDN,"x and y must be different arrays");
  if (!pf->ops->apply) SETERRQ(1,"No function has been provided for this PF");

  ierr = (*pf->ops->apply)(pf->data,n,x,y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PFView"
/*@ 
   PFView - Prints information about a mathematical function

   Collective on PF unless PetscViewer is PETSC_VIEWER_STDOUT_SELF  

   Input Parameters:
+  PF - the PF context
-  viewer - optional visualization context

   Note:
   The available visualization contexts include
+     PETSC_VIEWER_STDOUT_SELF - standard output (default)
-     PETSC_VIEWER_STDOUT_WORLD - synchronized standard
         output where only the first processor opens
         the file.  All other processors send their 
         data to the first processor to print. 

   The user can open an alternative visualization contexts with
   PetscViewerASCIIOpen() (output to a specified file).

   Level: developer

.keywords: PF, view

.seealso: PetscViewerCreate(), PetscViewerASCIIOpen()
@*/
int PFView(PF pf,PetscViewer viewer)
{
  PFType            cstr;
  int               ierr;
  PetscTruth        isascii;
  PetscViewerFormat format;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pf,PF_COOKIE,1);
  if (!viewer) viewer = PETSC_VIEWER_STDOUT_(pf->comm);
  PetscValidHeaderSpecific(viewer,PETSC_VIEWER_COOKIE,2); 
  PetscCheckSameComm(pf,1,viewer,2);

  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_ASCII,&isascii);CHKERRQ(ierr);
  if (isascii) {
    ierr = PetscViewerGetFormat(viewer,&format);CHKERRQ(ierr);
    ierr = PetscViewerASCIIPrintf(viewer,"PF Object:\n");CHKERRQ(ierr);
    ierr = PFGetType(pf,&cstr);CHKERRQ(ierr);
    if (cstr) {
      ierr = PetscViewerASCIIPrintf(viewer,"  type: %s\n",cstr);CHKERRQ(ierr);
    } else {
      ierr = PetscViewerASCIIPrintf(viewer,"  type: not yet set\n");CHKERRQ(ierr);
    }
    if (pf->ops->view) {
      ierr = PetscViewerASCIIPushTab(viewer);CHKERRQ(ierr);
      ierr = (*pf->ops->view)(pf->data,viewer);CHKERRQ(ierr);
      ierr = PetscViewerASCIIPopTab(viewer);CHKERRQ(ierr);
    }
  } else {
    SETERRQ1(1,"Viewer type %s not supported by PF",((PetscObject)viewer)->type_name);
  }
  PetscFunctionReturn(0);
}

/*MC
   PFRegisterDynamic - Adds a method to the mathematical function package.

   Synopsis:
   int PFRegisterDynamic(char *name_solver,char *path,char *name_create,int (*routine_create)(PF))

   Not collective

   Input Parameters:
+  name_solver - name of a new user-defined solver
.  path - path (either absolute or relative) the library containing this solver
.  name_create - name of routine to create method context
-  routine_create - routine to create method context

   Notes:
   PFRegisterDynamic() may be called multiple times to add several user-defined functions

   If dynamic libraries are used, then the fourth input argument (routine_create)
   is ignored.

   Sample usage:
.vb
   PFRegisterDynamic("my_function","/home/username/my_lib/lib/libO/solaris/mylib",
              "MyFunctionCreate",MyFunctionSetCreate);
.ve

   Then, your solver can be chosen with the procedural interface via
$     PFSetType(pf,"my_function")
   or at runtime via the option
$     -pf_type my_function

   Level: advanced

   ${PETSC_ARCH}, ${PETSC_DIR}, ${PETSC_LIB_DIR}, ${BOPT}, or ${any environmental variable}
 occuring in pathname will be replaced with appropriate values.

.keywords: PF, register

.seealso: PFRegisterAll(), PFRegisterDestroy(), PFRegister()
M*/

#undef __FUNCT__  
#define __FUNCT__ "PFRegister"
int PFRegister(const char sname[],const char path[],const char name[],int (*function)(PF,void*))
{
  int  ierr;
  char fullname[256];

  PetscFunctionBegin;
  ierr = PetscFListConcat(path,name,fullname);CHKERRQ(ierr);
  ierr = PetscFListAdd(&PPetscFList,sname,fullname,(void (*)(void))function);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}



#undef __FUNCT__  
#define __FUNCT__ "PFGetType"
/*@C
   PFGetType - Gets the PF method type and name (as a string) from the PF
   context.

   Not Collective

   Input Parameter:
.  pf - the function context

   Output Parameter:
.  name - name of function 

   Level: intermediate

.keywords: PF, get, method, name, type

.seealso: PFSetType()

@*/
int PFGetType(PF pf,PFType *meth)
{
  PetscFunctionBegin;
  *meth = (PFType) pf->type_name;
  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "PFSetType"
/*@C
   PFSetType - Builds PF for a particular function

   Collective on PF

   Input Parameter:
+  pf - the function context.
.  type - a known method
-  ctx - optional type dependent context

   Options Database Key:
.  -pf_type <type> - Sets PF type


  Notes:
  See "petsc/include/petscpf.h" for available methods (for instance,
  PFCONSTANT)

  Level: intermediate

.keywords: PF, set, method, type

.seealso: PFSet(), PFRegisterDynamic(), PFCreate(), DACreatePF()

@*/
int PFSetType(PF pf,const PFType type,void *ctx)
{
  int        ierr,(*r)(PF,void*);
  PetscTruth match;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pf,PF_COOKIE,1);
  PetscValidCharPointer(type,2);

  ierr = PetscTypeCompare((PetscObject)pf,type,&match);CHKERRQ(ierr);
  if (match) PetscFunctionReturn(0);

  if (pf->ops->destroy) {ierr =  (*pf->ops->destroy)(pf);CHKERRQ(ierr);}
  pf->data        = 0;

  /* Get the function pointers for the method requested */
  if (!PFRegisterAllCalled) {ierr = PFRegisterAll(0);CHKERRQ(ierr);}

  /* Determine the PFCreateXXX routine for a particular function */
  ierr =  PetscFListFind(pf->comm,PPetscFList,type,(void (**)(void)) &r);CHKERRQ(ierr);
  if (!r) SETERRQ1(1,"Unable to find requested PF type %s",type);

  pf->ops->destroy             = 0;
  pf->ops->view                = 0;
  pf->ops->apply               = 0;
  pf->ops->applyvec            = 0;

  /* Call the PFCreateXXX routine for this particular function */
  ierr = (*r)(pf,ctx);CHKERRQ(ierr);

  ierr = PetscObjectChangeTypeName((PetscObject)pf,type);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PFSetFromOptions"
/*@
   PFSetFromOptions - Sets PF options from the options database.

   Collective on PF

   Input Parameters:
.  pf - the mathematical function context

   Options Database Keys:

   Notes:  
   To see all options, run your program with the -help option
   or consult the users manual.

   Level: intermediate

.keywords: PF, set, from, options, database

.seealso:
@*/
int PFSetFromOptions(PF pf)
{
  int        ierr;
  char       type[256];
  PetscTruth flg;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pf,PF_COOKIE,1);

  if (!PFRegisterAllCalled) {ierr = PFRegisterAll(0);CHKERRQ(ierr);}
  ierr = PetscOptionsBegin(pf->comm,pf->prefix,"Mathematical functions options","Vec");CHKERRQ(ierr);
    ierr = PetscOptionsList("-pf_type","Type of function","PFSetType",PPetscFList,0,type,256,&flg);CHKERRQ(ierr);
    if (flg) {
      ierr = PFSetType(pf,type,PETSC_NULL);CHKERRQ(ierr);
    }
    if (pf->ops->setfromoptions) {
      ierr = (*pf->ops->setfromoptions)(pf);CHKERRQ(ierr);
    }
  ierr = PetscOptionsEnd();CHKERRQ(ierr);

  PetscFunctionReturn(0);
}










