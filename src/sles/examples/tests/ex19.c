/* "$Id: ex19.c,v 1.11 2001/08/07 03:03:57 balay Exp bsmith $" */

static char help[] ="Solvers Laplacian with multigrid, bad way.\n\
  -mx <xg>, where <xg> = number of grid points in the x-direction\n\
  -my <yg>, where <yg> = number of grid points in the y-direction\n\
  -Nx <npx>, where <npx> = number of processors in the x-direction\n\
  -Ny <npy>, where <npy> = number of processors in the y-direction\n\n";

/*  
    This problem is modeled by
    the partial differential equation
  
            -Laplacian u  = g,  0 < x,y < 1,
  
    with boundary conditions
   
             u = 0  for  x = 0, x = 1, y = 0, y = 1.
  
    A finite difference approximation with the usual 5-point stencil
    is used to discretize the boundary value problem to obtain a nonlinear 
    system of equations.
*/

#include "petscsles.h"
#include "petscda.h"
#include "petscmg.h"

/* User-defined application contexts */

typedef struct {
   int        mx,my;            /* number grid points in x and y direction */
   Vec        localX,localF;    /* local vectors with ghost region */
   DA         da;
   Vec        x,b,r;            /* global vectors */
   Mat        J;                /* Jacobian on grid */
} GridCtx;

typedef struct {
   GridCtx     fine;
   GridCtx     coarse;
   SLES        sles_coarse;
   int         ratio;
   Mat         I;               /* interpolation from coarse to fine */
} AppCtx;

#define COARSE_LEVEL 0
#define FINE_LEVEL   1

extern int FormJacobian_Grid(AppCtx *,GridCtx *,Mat *);

/*
      Mm_ratio - ration of grid lines between fine and coarse grids.
*/
#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **argv)
{
  AppCtx        user;                      
  int           ierr,its,N,n,Nx = PETSC_DECIDE,Ny = PETSC_DECIDE;
  int           size,nlocal,Nlocal;
  SLES          sles,sles_fine;
  PC            pc;
  PetscScalar   one = 1.0;

  PetscInitialize(&argc,&argv,PETSC_NULL,help);

  user.ratio = 2;
  user.coarse.mx = 5; user.coarse.my = 5; 
  ierr = PetscOptionsGetInt(PETSC_NULL,"-Mx",&user.coarse.mx,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(PETSC_NULL,"-My",&user.coarse.my,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(PETSC_NULL,"-ratio",&user.ratio,PETSC_NULL);CHKERRQ(ierr);
  user.fine.mx = user.ratio*(user.coarse.mx-1)+1; user.fine.my = user.ratio*(user.coarse.my-1)+1;

  PetscPrintf(PETSC_COMM_WORLD,"Coarse grid size %d by %d\n",user.coarse.mx,user.coarse.my);
  PetscPrintf(PETSC_COMM_WORLD,"Fine grid size %d by %d\n",user.fine.mx,user.fine.my);

  n = user.fine.mx*user.fine.my; N = user.coarse.mx*user.coarse.my;

  MPI_Comm_size(PETSC_COMM_WORLD,&size);
  ierr = PetscOptionsGetInt(PETSC_NULL,"-Nx",&Nx,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(PETSC_NULL,"-Ny",&Ny,PETSC_NULL);CHKERRQ(ierr);

  /* Set up distributed array for fine grid */
  ierr = DACreate2d(PETSC_COMM_WORLD,DA_NONPERIODIC,DA_STENCIL_STAR,user.fine.mx,
                    user.fine.my,Nx,Ny,1,1,PETSC_NULL,PETSC_NULL,&user.fine.da);CHKERRQ(ierr);
  ierr = DACreateGlobalVector(user.fine.da,&user.fine.x);CHKERRQ(ierr);
  ierr = VecDuplicate(user.fine.x,&user.fine.r);CHKERRQ(ierr);
  ierr = VecDuplicate(user.fine.x,&user.fine.b);CHKERRQ(ierr);
  ierr = VecGetLocalSize(user.fine.x,&nlocal);CHKERRQ(ierr);
  ierr = DACreateLocalVector(user.fine.da,&user.fine.localX);CHKERRQ(ierr);
  ierr = VecDuplicate(user.fine.localX,&user.fine.localF);CHKERRQ(ierr);
  ierr = MatCreateMPIAIJ(PETSC_COMM_WORLD,nlocal,nlocal,n,n,5,PETSC_NULL,3,PETSC_NULL,&user.fine.J);CHKERRQ(ierr);

  /* Set up distributed array for coarse grid */
  ierr = DACreate2d(PETSC_COMM_WORLD,DA_NONPERIODIC,DA_STENCIL_STAR,user.coarse.mx,
                    user.coarse.my,Nx,Ny,1,1,PETSC_NULL,PETSC_NULL,&user.coarse.da);CHKERRQ(ierr);
  ierr = DACreateGlobalVector(user.coarse.da,&user.coarse.x);CHKERRQ(ierr);
  ierr = VecDuplicate(user.coarse.x,&user.coarse.b);CHKERRQ(ierr);
  ierr = VecGetLocalSize(user.coarse.x,&Nlocal);CHKERRQ(ierr);
  ierr = DACreateLocalVector(user.coarse.da,&user.coarse.localX);CHKERRQ(ierr);
  ierr = VecDuplicate(user.coarse.localX,&user.coarse.localF);CHKERRQ(ierr);
  ierr = MatCreateMPIAIJ(PETSC_COMM_WORLD,Nlocal,Nlocal,N,N,5,PETSC_NULL,3,PETSC_NULL,&user.coarse.J);CHKERRQ(ierr);

  /* Create linear solver */
  ierr = SLESCreate(PETSC_COMM_WORLD,&sles);CHKERRQ(ierr);

  /* set two level additive Schwarz preconditioner */
  ierr = SLESGetPC(sles,&pc);CHKERRQ(ierr);
  ierr = PCSetType(pc,PCMG);CHKERRQ(ierr);
  ierr = MGSetLevels(pc,2,PETSC_NULL);CHKERRQ(ierr);
  ierr = MGSetType(pc,MGADDITIVE);CHKERRQ(ierr);

  ierr = FormJacobian_Grid(&user,&user.coarse,&user.coarse.J);CHKERRQ(ierr);
  ierr = FormJacobian_Grid(&user,&user.fine,&user.fine.J);CHKERRQ(ierr);

  /* Create coarse level */
  ierr = MGGetCoarseSolve(pc,&user.sles_coarse);CHKERRQ(ierr);
  ierr = SLESSetOptionsPrefix(user.sles_coarse,"coarse_");CHKERRQ(ierr);
  ierr = SLESSetFromOptions(user.sles_coarse);CHKERRQ(ierr);
  ierr = SLESSetOperators(user.sles_coarse,user.coarse.J,user.coarse.J,DIFFERENT_NONZERO_PATTERN);CHKERRQ(ierr);
  ierr = MGSetX(pc,COARSE_LEVEL,user.coarse.x);CHKERRQ(ierr); 
  ierr = MGSetRhs(pc,COARSE_LEVEL,user.coarse.b);CHKERRQ(ierr); 

  /* Create fine level */
  ierr = MGGetSmoother(pc,FINE_LEVEL,&sles_fine);CHKERRQ(ierr);
  ierr = SLESSetOptionsPrefix(sles_fine,"fine_");CHKERRQ(ierr);
  ierr = SLESSetFromOptions(sles_fine);CHKERRQ(ierr);
  ierr = SLESSetOperators(sles_fine,user.fine.J,user.fine.J,DIFFERENT_NONZERO_PATTERN);CHKERRQ(ierr);
  ierr = MGSetR(pc,FINE_LEVEL,user.fine.r);CHKERRQ(ierr); 
  ierr = MGSetResidual(pc,FINE_LEVEL,MGDefaultResidual,user.fine.J);CHKERRQ(ierr);

  /* Create interpolation between the levels */
  ierr = DAGetInterpolation(user.coarse.da,user.fine.da,&user.I,PETSC_NULL);CHKERRQ(ierr);
  ierr = MGSetInterpolate(pc,FINE_LEVEL,user.I);CHKERRQ(ierr);
  ierr = MGSetRestriction(pc,FINE_LEVEL,user.I);CHKERRQ(ierr);

  ierr = SLESSetOperators(sles,user.fine.J,user.fine.J,DIFFERENT_NONZERO_PATTERN);CHKERRQ(ierr);

  ierr = VecSet(&one,user.fine.b);CHKERRQ(ierr);
  {
    PetscRandom rand;
    ierr = PetscRandomCreate(PETSC_COMM_WORLD,RANDOM_DEFAULT,&rand);CHKERRQ(ierr);
    ierr = VecSetRandom(rand,user.fine.b);CHKERRQ(ierr);
    ierr = PetscRandomDestroy(rand);CHKERRQ(ierr);
  }

  /* Set options, then solve nonlinear system */
  ierr = SLESSetFromOptions(sles);CHKERRQ(ierr);

  ierr = SLESSolve(sles,user.fine.b,user.fine.x,&its);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Number of iterations = %d\n",its);CHKERRQ(ierr);

  /* Free data structures */
  ierr = MatDestroy(user.fine.J);CHKERRQ(ierr);
  ierr = VecDestroy(user.fine.x);CHKERRQ(ierr);
  ierr = VecDestroy(user.fine.r);CHKERRQ(ierr);
  ierr = VecDestroy(user.fine.b);CHKERRQ(ierr);
  ierr = DADestroy(user.fine.da);CHKERRQ(ierr);
  ierr = VecDestroy(user.fine.localX);CHKERRQ(ierr);
  ierr = VecDestroy(user.fine.localF);CHKERRQ(ierr);

  ierr = MatDestroy(user.coarse.J);CHKERRQ(ierr);
  ierr = VecDestroy(user.coarse.x);CHKERRQ(ierr);
  ierr = VecDestroy(user.coarse.b);CHKERRQ(ierr);
  ierr = DADestroy(user.coarse.da);CHKERRQ(ierr);
  ierr = VecDestroy(user.coarse.localX);CHKERRQ(ierr);
  ierr = VecDestroy(user.coarse.localF);CHKERRQ(ierr);

  ierr = SLESDestroy(sles);CHKERRQ(ierr);
  ierr = MatDestroy(user.I);CHKERRQ(ierr); 
  ierr = PetscFinalize();CHKERRQ(ierr);

  return 0;
}

#undef __FUNCT__
#define __FUNCT__ "FormJacobian_Grid"
int FormJacobian_Grid(AppCtx *user,GridCtx *grid,Mat *J)
{
  Mat     jac = *J;
  int     ierr,i,j,row,mx,my,xs,ys,xm,ym,Xs,Ys,Xm,Ym,col[5];
  int     nloc,*ltog,grow;
  PetscScalar  two = 2.0,one = 1.0,v[5],hx,hy,hxdhy,hydhx,value;

  mx = grid->mx;            my = grid->my;            
  hx = one/(PetscReal)(mx-1);  hy = one/(PetscReal)(my-1);
  hxdhy = hx/hy;            hydhx = hy/hx;

  /* Get ghost points */
  ierr = DAGetCorners(grid->da,&xs,&ys,0,&xm,&ym,0);CHKERRQ(ierr);
  ierr = DAGetGhostCorners(grid->da,&Xs,&Ys,0,&Xm,&Ym,0);CHKERRQ(ierr);
  ierr = DAGetGlobalIndices(grid->da,&nloc,&ltog);CHKERRQ(ierr);

  /* Evaluate Jacobian of function */
  for (j=ys; j<ys+ym; j++) {
    row = (j - Ys)*Xm + xs - Xs - 1; 
    for (i=xs; i<xs+xm; i++) {
      row++;
      grow = ltog[row];
      if (i > 0 && i < mx-1 && j > 0 && j < my-1) {
        v[0] = -hxdhy; col[0] = ltog[row - Xm];
        v[1] = -hydhx; col[1] = ltog[row - 1];
        v[2] = two*(hydhx + hxdhy); col[2] = grow;
        v[3] = -hydhx; col[3] = ltog[row + 1];
        v[4] = -hxdhy; col[4] = ltog[row + Xm];
        ierr = MatSetValues(jac,1,&grow,5,col,v,INSERT_VALUES);CHKERRQ(ierr);
      } else if ((i > 0 && i < mx-1) || (j > 0 && j < my-1)){
        value = .5*two*(hydhx + hxdhy);
        ierr = MatSetValues(jac,1,&grow,1,&grow,&value,INSERT_VALUES);CHKERRQ(ierr);
      } else {
        value = .25*two*(hydhx + hxdhy);
        ierr = MatSetValues(jac,1,&grow,1,&grow,&value,INSERT_VALUES);CHKERRQ(ierr);
      }
    }
  }
  ierr = MatAssemblyBegin(jac,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(jac,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);

  return 0;
}
