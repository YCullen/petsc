/*$Id: ex53.c,v 1.23 2001/08/07 03:03:07 balay Exp bsmith $*/

static char help[] = "Tests the vatious routines in MatMPIBAIJ format.\n";


#include "petscmat.h"
#define IMAX 15
#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **args)
{
  Mat         A,B,C,At,Bt;
  PetscViewer fd;
  char        file[128];
  PetscRandom rand;
  Vec         xx,yy,s1,s2;
  PetscReal   s1norm,s2norm,rnorm,tol = 1.e-10;
  int         rstart,rend,rows[2],cols[2],m,n,i,j,ierr,M,N,rank,ct,row,ncols1;
  int         *cols1,ncols2,*cols2,bs;
  PetscScalar vals1[4],vals2[4],v,*v1,*v2;
  PetscTruth  flg;

  PetscInitialize(&argc,&args,(char *)0,help);
  ierr = MPI_Comm_rank(PETSC_COMM_WORLD,&rank);CHKERRQ(ierr);

#if defined(PETSC_USE_COMPLEX)
  SETERRQ(1,"This example does not work with complex numbers");
#else

 /* Check out if MatLoad() works */
  ierr = PetscOptionsGetString(PETSC_NULL,"-f",file,127,&flg);CHKERRQ(ierr);
  if (!flg) SETERRQ(1,"Input file not specified");
  ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,file,PETSC_BINARY_RDONLY,&fd);CHKERRQ(ierr);
  ierr = MatLoad(fd,MATMPIBAIJ,&A);CHKERRQ(ierr);
  ierr = PetscViewerDestroy(fd);CHKERRQ(ierr);

  ierr = MatConvert(A,MATMPIAIJ,&B);CHKERRQ(ierr);
 
  ierr = PetscRandomCreate(PETSC_COMM_WORLD,RANDOM_DEFAULT,&rand);CHKERRQ(ierr);
  ierr = MatGetLocalSize(A,&m,&n);CHKERRQ(ierr);
  ierr = VecCreateMPI(PETSC_COMM_WORLD,m,PETSC_DECIDE,&xx);CHKERRQ(ierr);
  ierr = VecDuplicate(xx,&s1);CHKERRQ(ierr);
  ierr = VecDuplicate(xx,&s2);CHKERRQ(ierr);
  ierr = VecDuplicate(xx,&yy);CHKERRQ(ierr);

  ierr = MatGetBlockSize(A,&bs);CHKERRQ(ierr);
  /* Test MatMult() */ 
  for (i=0; i<IMAX; i++) {
    ierr = VecSetRandom(rand,xx);CHKERRQ(ierr);
    ierr = MatMult(A,xx,s1);CHKERRQ(ierr);
    ierr = MatMult(B,xx,s2);CHKERRQ(ierr);
    ierr = VecNorm(s1,NORM_2,&s1norm);CHKERRQ(ierr);
    ierr = VecNorm(s2,NORM_2,&s2norm);CHKERRQ(ierr);
    rnorm = s2norm-s1norm;
    if (rnorm<-tol || rnorm>tol) { 
      ierr = PetscPrintf(PETSC_COMM_SELF,"Error:MatMult - Norm1=%16.14e Norm2=%16.14e bs = %d\n",
s1norm,s2norm,bs);CHKERRQ(ierr);  
    }
  } 
  /* test MatMultAdd() */
  for (i=0; i<IMAX; i++) {
    ierr = VecSetRandom(rand,xx);CHKERRQ(ierr);
    ierr = VecSetRandom(rand,yy);CHKERRQ(ierr);
    ierr = MatMultAdd(A,xx,yy,s1);CHKERRQ(ierr);
    ierr = MatMultAdd(B,xx,yy,s2);CHKERRQ(ierr);
    ierr = VecNorm(s1,NORM_2,&s1norm);CHKERRQ(ierr);
    ierr = VecNorm(s2,NORM_2,&s2norm);CHKERRQ(ierr);
    rnorm = s2norm-s1norm;
    if (rnorm<-tol || rnorm>tol) { 
      ierr = PetscPrintf(PETSC_COMM_SELF,"Error:MatMultAdd - Norm1=%16.14e Norm2=%16.14e bs = %d\n",s1norm,s2norm,bs);CHKERRQ(ierr);
    } 
  }
    /* Test MatMultTranspose() */
  for (i=0; i<IMAX; i++) {
    ierr = VecSetRandom(rand,xx);CHKERRQ(ierr);
    ierr = MatMultTranspose(A,xx,s1);CHKERRQ(ierr);
    ierr = MatMultTranspose(B,xx,s2);CHKERRQ(ierr);
    ierr = VecNorm(s1,NORM_2,&s1norm);CHKERRQ(ierr);
    ierr = VecNorm(s2,NORM_2,&s2norm);CHKERRQ(ierr);
    rnorm = s2norm-s1norm;
    if (rnorm<-tol || rnorm>tol) { 
      ierr = PetscPrintf(PETSC_COMM_SELF,"Error:MatMultTranspose - Norm1=%16.14e Norm2=%16.14e bs = %d\n",s1norm,s2norm,bs);CHKERRQ(ierr);  
    } 
  }
  /* Test MatMultTransposeAdd() */
  for (i=0; i<IMAX; i++) {
    ierr = VecSetRandom(rand,xx);CHKERRQ(ierr);
    ierr = VecSetRandom(rand,yy);CHKERRQ(ierr);
    ierr = MatMultTransposeAdd(A,xx,yy,s1);CHKERRQ(ierr);
    ierr = MatMultTransposeAdd(B,xx,yy,s2);CHKERRQ(ierr);
    ierr = VecNorm(s1,NORM_2,&s1norm);CHKERRQ(ierr);
    ierr = VecNorm(s2,NORM_2,&s2norm);CHKERRQ(ierr);
    rnorm = s2norm-s1norm;
    if (rnorm<-tol || rnorm>tol) { 
      ierr = PetscPrintf(PETSC_COMM_SELF,"Error:MatMultTransposeAdd - Norm1=%16.14e Norm2=%16.14e bs = %d\n",s1norm,s2norm,bs);CHKERRQ(ierr);
    } 
  }

  /* Check MatGetValues() */
  ierr = MatGetOwnershipRange(A,&rstart,&rend);CHKERRQ(ierr);
  ierr = MatGetSize(A,&M,&N);CHKERRQ(ierr);


  for (i=0; i<IMAX; i++) {
    /* Create random row numbers ad col numbers */
    ierr = PetscRandomGetValue(rand,&v);CHKERRQ(ierr);
    cols[0] = (int)(PetscRealPart(v)*N);
    ierr = PetscRandomGetValue(rand,&v);CHKERRQ(ierr);
    cols[1] = (int)(PetscRealPart(v)*N);
    ierr = PetscRandomGetValue(rand,&v);CHKERRQ(ierr);
    rows[0] = rstart + (int)(PetscRealPart(v)*m);
    ierr = PetscRandomGetValue(rand,&v);CHKERRQ(ierr);
    rows[1] = rstart + (int)(PetscRealPart(v)*m);
    
    ierr = MatGetValues(A,2,rows,2,cols,vals1);CHKERRQ(ierr);
    ierr = MatGetValues(B,2,rows,2,cols,vals2);CHKERRQ(ierr);


    for (j=0; j<4; j++) {
      if(vals1[j] != vals2[j])
        ierr = PetscPrintf(PETSC_COMM_SELF,"[%d]: Error:MatGetValues rstart = %2d  row = %2d col = %2d val1 = %e val2 = %e bs = %d\n",rank,rstart,rows[j/2],cols[j%2],PetscRealPart(vals1[j]),PetscRealPart(vals2[j]),bs);CHKERRQ(ierr);
    }
  }

  /* Test MatGetRow()/ MatRestoreRow() */
  for (ct=0; ct<100; ct++) {
    ierr = PetscRandomGetValue(rand,&v);
    row  = rstart + (int)(PetscRealPart(v)*m);
    ierr = MatGetRow(A,row,&ncols1,&cols1,&v1);CHKERRQ(ierr);
    ierr = MatGetRow(B,row,&ncols2,&cols2,&v2);CHKERRQ(ierr);
    
    for (i=0,j=0; i<ncols1 && j<ncols2; j++) {
      while (cols2[j] != cols1[i]) i++;
      if (v1[i] != v2[j]) SETERRQ(1,"MatGetRow() failed - vals incorrect.");
    }
    if (j<ncols2) SETERRQ(1,"MatGetRow() failed - cols incorrect");
    
    ierr = MatRestoreRow(A,row,&ncols1,&cols1,&v1);CHKERRQ(ierr);
    ierr = MatRestoreRow(B,row,&ncols2,&cols2,&v2);CHKERRQ(ierr);
  }
  
  /* Test MatConvert() */
  ierr = MatConvert(A,MATSAME,&C);CHKERRQ(ierr);
  
  /* See if MatMult Says both are same */ 
  for (i=0; i<IMAX; i++) {
    ierr = VecSetRandom(rand,xx);CHKERRQ(ierr);
    ierr = MatMult(A,xx,s1);CHKERRQ(ierr);
    ierr = MatMult(C,xx,s2);CHKERRQ(ierr);
    ierr = VecNorm(s1,NORM_2,&s1norm);CHKERRQ(ierr);
    ierr = VecNorm(s2,NORM_2,&s2norm);CHKERRQ(ierr);
    rnorm = s2norm-s1norm;
    if (rnorm<-tol || rnorm>tol) { 
      ierr = PetscPrintf(PETSC_COMM_SELF,"Error in MatConvert:MatMult - Norm1=%16.14e Norm2=%16.14e bs = %d\n",
s1norm,s2norm,bs);CHKERRQ(ierr);  
    }
  }
  ierr = MatDestroy(C);CHKERRQ(ierr);

  /* Test MatTranspose() */
  ierr = MatTranspose(A,&At);CHKERRQ(ierr);
  ierr = MatTranspose(B,&Bt);CHKERRQ(ierr);
  for (i=0; i<IMAX; i++) {
    ierr = VecSetRandom(rand,xx);CHKERRQ(ierr);
    ierr = MatMult(At,xx,s1);CHKERRQ(ierr);
    ierr = MatMult(Bt,xx,s2);CHKERRQ(ierr);
    ierr = VecNorm(s1,NORM_2,&s1norm);CHKERRQ(ierr);
    ierr = VecNorm(s2,NORM_2,&s2norm);CHKERRQ(ierr);
    rnorm = s2norm-s1norm;
    if (rnorm<-tol || rnorm>tol) { 
      ierr = PetscPrintf(PETSC_COMM_SELF,"Error in MatConvert:MatMult - Norm1=%16.14e Norm2=%16.14e bs = %d\n",
                  s1norm,s2norm,bs);CHKERRQ(ierr);
    }
  }
  ierr = MatDestroy(At);CHKERRQ(ierr);
  ierr = MatDestroy(Bt);CHKERRQ(ierr);

  ierr = MatDestroy(A);CHKERRQ(ierr); 
  ierr = MatDestroy(B);CHKERRQ(ierr);
  ierr = VecDestroy(xx);CHKERRQ(ierr);
  ierr = VecDestroy(yy);CHKERRQ(ierr);
  ierr = VecDestroy(s1);CHKERRQ(ierr);
  ierr = VecDestroy(s2);CHKERRQ(ierr);
  ierr = PetscRandomDestroy(rand);CHKERRQ(ierr);
  ierr = PetscFinalize();CHKERRQ(ierr);
#endif
  return 0;
}
