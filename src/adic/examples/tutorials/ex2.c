#ifndef lint
static char vcid[] = "$Id: ex1.c,v 1.1 1997/03/20 23:12:06 bsmith Exp bsmith $";
#endif

static char help[] = "Demonstrates using ADIC to compute a derivative.\n\n";

/*T
   Concepts: Vectors^Using basic vector routines;
   Routines: VecCreate(); VecDuplicate(); VecSet();
   Routines: VecScale(); VecNorm(); 
   Routines: VecDestroy(); 
   Processors: n
T*/

/* 
  Include "vec.h" so that we can use vectors.  Note that this file
  automatically includes:
     petsc.h  - base PETSc routines
     sys.h    - system routines
*/

#include "vec.h"
#include <math.h>
#include "petscadic.h"

extern int Function(Vec,Vec);
extern int ad_Function(Vec,Vec);

int main(int argc,char **argv)
{
  Vec               x,y,z,Az;               /* vectors */
  int               n = 20, ierr, flg;
  Scalar            one = 1.0, onethird = 1.0/3.0;
  PetscADICFunction adicctx;
  Mat               grad,igrad;

  PetscInitialize(&argc,&argv,(char*)0,help);
  ad_PetscInitialize(&argc,&argv,(char*)0,help);
  OptionsGetInt(PETSC_NULL,"-n",&n,&flg);
  
  ad_AD_Init();

  /* 
     Create a vector, specifying only its global dimension.
     When using VecCreate(), the vector format (currently parallel
     or sequential) is determined at runtime.  Also, the parallel
     partitioning of the vector is determined by PETSc at runtime.

     Routines for creating particular vector types directly are:
        VecCreateSeq() - uniprocessor vector
        VecCreateMPI() - distributed vector, where the user can
                         determine the parallel partitioning
  */
  ierr = VecCreate(PETSC_COMM_WORLD,n,&x); CHKERRA(ierr);
  ierr = VecDuplicate(x,&y); CHKERRA(ierr);
  ierr = VecDuplicate(x,&z); CHKERRA(ierr);
  ierr = VecDuplicate(z,&Az); CHKERRA(ierr);

  ierr = VecSet(&one,x); CHKERRA(ierr);

  /*
     Create a dense matrix to store the Jacobian in 
  */
  ierr = MatCreateSeqDense(PETSC_COMM_WORLD,n,n,PETSC_NULL,&grad); CHKERRA(ierr);
  /*
        Register a function that will be differentiated.
  */
  ierr = PetscADICFunctionCreate(x,y,ad_Function,0,&adicctx); CHKERRA(ierr);


  /*
     First evaluate the function itself 
  */
  ierr = Function(x,y); CHKERRA(ierr);
  VecView(y,0);
  
  /*
     Now evalute the function and its gradient
  */
  ierr = PetscADICFunctionEvaluateGradient(adicctx,x,y,grad); CHKERRA(ierr);
  VecView(y,0); MatView(grad,0);
  ierr = VecSet(&onethird,z); CHKERRA(ierr);
  ierr = PetscADICFunctionEvaluateGradient(adicctx,z,y,grad); CHKERRA(ierr);
  VecView(y,0); MatView(grad,0);

  /*
     Now apply the gradient to a vector z
  */
  ierr = PetscADICFunctionApplyGradientInitialize(adicctx,x,&igrad); CHKERRA(ierr);
  ierr = MatMult(igrad,z,Az); CHKERRA(ierr);
  VecView(Az,0); 
  
  ierr = PetscADICFunctionApplyGradientReset(igrad,z); CHKERRA(ierr);
  ierr = MatMult(igrad,z,Az); CHKERRA(ierr);
  VecView(Az,0); 

  /* 
     Free work space.  All PETSc objects should be destroyed when they
     are no longer needed.
  */
  ierr = MatDestroy(grad); CHKERRA(ierr);
  ierr = MatDestroy(igrad); CHKERRA(ierr);
  ierr = VecDestroy(x); CHKERRA(ierr);
  ierr = VecDestroy(y); CHKERRA(ierr);
  ierr = VecDestroy(z); CHKERRA(ierr);
  ierr = VecDestroy(Az); CHKERRA(ierr);
  ad_AD_Final();
  PetscFinalize();
  ad_PetscFinalize();
  return 0;
}
 



