/*$Id: ex62.c,v 1.22 2001/08/07 03:03:07 balay Exp bsmith $*/

static char help[] = "Tests the use of MatSolveTranspose().\n\n";

#include "petscmat.h"

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **args)
{
  Mat         C,A;
  int         i,j,m,ierr,size;
  IS          row,col;
  Vec         x,u,b;
  PetscReal   norm;
  PetscViewer fd;
  char        type[256];
  char        file[128];
  PetscScalar one = 1.0,mone = -1.0;
  PetscTruth  flg;

  PetscInitialize(&argc,&args,(char *)0,help);
  ierr = MPI_Comm_size(PETSC_COMM_WORLD,&size);CHKERRQ(ierr);
  if (size > 1) SETERRQ(1,"Can only run on one processor");

  ierr = PetscOptionsGetString(PETSC_NULL,"-f",file,127,&flg);CHKERRQ(ierr);
  if (!flg) SETERRQ(1,"Must indicate binary file with the -f option");
  /* 
     Open binary file.  Note that we use PETSC_BINARY_RDONLY to indicate
     reading from this file.
  */
  ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,file,PETSC_BINARY_RDONLY,&fd);CHKERRQ(ierr);

  /* 
     Determine matrix format to be used (specified at runtime).
     See the manpage for MatLoad() for available formats.
  */
  ierr = PetscStrcpy(type,MATSEQAIJ);CHKERRQ(ierr);
  ierr = PetscOptionsGetString(PETSC_NULL,"-mat_type",type,256,PETSC_NULL);CHKERRQ(ierr);

  /*
     Load the matrix and vector; then destroy the viewer.
  */
  ierr = MatLoad(fd,type,&C);CHKERRQ(ierr);
  ierr = VecLoad(fd,&u);CHKERRQ(ierr);
  ierr = PetscViewerDestroy(fd);CHKERRQ(ierr);

  ierr = VecDuplicate(u,&x);CHKERRQ(ierr);
  ierr = VecDuplicate(u,&b);CHKERRQ(ierr);

  ierr = MatMultTranspose(C,u,b);CHKERRQ(ierr);

  /* Set default ordering to be Quotient Minimum Degree; also read
     orderings from the options database */
  ierr = MatGetOrdering(C,MATORDERING_QMD,&row,&col);CHKERRQ(ierr);

  ierr = MatLUFactorSymbolic(C,row,col,PETSC_NULL,&A);CHKERRQ(ierr);
  ierr = MatLUFactorNumeric(C,&A);CHKERRQ(ierr);
  ierr = MatSolveTranspose(A,b,x);CHKERRQ(ierr);

  ierr = VecAXPY(&mone,u,x);CHKERRQ(ierr);
  ierr = VecNorm(x,NORM_2,&norm);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_SELF,"Norm of error %g\n",norm);CHKERRQ(ierr);

  ierr = ISDestroy(row);CHKERRQ(ierr);
  ierr = ISDestroy(col);CHKERRQ(ierr);
  ierr = VecDestroy(u);CHKERRQ(ierr);
  ierr = VecDestroy(x);CHKERRQ(ierr);
  ierr = VecDestroy(b);CHKERRQ(ierr);
  ierr = MatDestroy(C);CHKERRQ(ierr);
  ierr = MatDestroy(A);CHKERRQ(ierr);
  ierr = PetscFinalize();CHKERRQ(ierr);
  return 0;
}
