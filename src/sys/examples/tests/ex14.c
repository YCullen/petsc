/*$Id: ex14.c,v 1.11 2001/08/07 03:02:12 balay Exp bsmith $*/

/* 
   Tests PetscOptionsGetScalar() for complex numbers
 */

#include "petsc.h"


#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **argv)
{
  int         ierr;
  PetscScalar a;

  PetscInitialize(&argc,&argv,(char *)0,0);
  ierr = PetscOptionsGetScalar(PETSC_NULL,"-a",&a,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_SELF,"Scalar a = %g + %gi\n",PetscRealPart(a),PetscImaginaryPart(a));CHKERRQ(ierr);
  ierr = PetscFinalize();CHKERRQ(ierr);
  return 0;
}
 
