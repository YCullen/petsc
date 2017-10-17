#include <petscdmadaptor.h>            /*I "petscdmadaptor.h" I*/
#include <petscdmplex.h>
#include <petscdmforest.h>
#include <petscds.h>
#include <petscblaslapack.h>

#include <petsc/private/dmadaptorimpl.h>
#include <petsc/private/dmpleximpl.h>

static PetscErrorCode DMAdaptorTransferSolution_Exact_Private(DMAdaptor adaptor, DM dm, Vec u, DM adm, Vec au, void *ctx)
{
  PetscErrorCode ierr;

  PetscFunctionBeginUser;
  ierr = DMProjectFunction(adm, 0.0, adaptor->exactSol, (void **) ctx, INSERT_ALL_VALUES, au);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*@
  DMAdaptorCreate - Create a DMAdaptor object. Its purpose is to construct a adaptation DMLabel or metric Vec that can be used to modify the DM.

  Collective on MPI_Comm

  Input Parameter:
. comm - The communicator for the DMAdaptor object

  Output Parameter:
. adaptor   - The DMAdaptor object

  Level: beginner

.keywords: DMAdaptor, convergence, create
.seealso: DMAdaptorDestroy(), DMAdaptorAdapt()
@*/
PetscErrorCode DMAdaptorCreate(MPI_Comm comm, DMAdaptor *adaptor)
{
  VecTaggerBox   refineBox, coarsenBox;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidPointer(adaptor, 2);
  ierr = PetscSysInitializePackage();CHKERRQ(ierr);
  ierr = PetscHeaderCreate(*adaptor, PETSC_OBJECT_CLASSID, "DMAdaptor", "DM Adaptor", "SNES", comm, DMAdaptorDestroy, DMAdaptorView);CHKERRQ(ierr);

  (*adaptor)->monitor = PETSC_FALSE;
  (*adaptor)->adaptCriterion = DM_ADAPTATION_NONE;
  (*adaptor)->numSeq  = 1;
  (*adaptor)->Nadapt  = -1;
  (*adaptor)->refinementFactor = 2.0;
  (*adaptor)->h_min = 1.;
  (*adaptor)->h_max = 10000.;
  refineBox.min = refineBox.max = PETSC_MAX_REAL;
  ierr = VecTaggerCreate(PetscObjectComm((PetscObject) *adaptor), &(*adaptor)->refineTag);CHKERRQ(ierr);
  ierr = PetscObjectSetOptionsPrefix((PetscObject) (*adaptor)->refineTag, "refine_");CHKERRQ(ierr);
  ierr = VecTaggerSetType((*adaptor)->refineTag, VECTAGGERABSOLUTE);CHKERRQ(ierr);
  ierr = VecTaggerAbsoluteSetBox((*adaptor)->refineTag, &refineBox);CHKERRQ(ierr);
  coarsenBox.min = coarsenBox.max = PETSC_MAX_REAL;
  ierr = VecTaggerCreate(PetscObjectComm((PetscObject) *adaptor), &(*adaptor)->coarsenTag);CHKERRQ(ierr);
  ierr = PetscObjectSetOptionsPrefix((PetscObject) (*adaptor)->coarsenTag, "coarsen_");CHKERRQ(ierr);
  ierr = VecTaggerSetType((*adaptor)->coarsenTag, VECTAGGERABSOLUTE);CHKERRQ(ierr);
  ierr = VecTaggerAbsoluteSetBox((*adaptor)->coarsenTag, &coarsenBox);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*@
  DMAdaptorDestroy - Destroys a DMAdaptor object

  Collective on DMAdaptor

  Input Parameter:
. adaptor - The DMAdaptor object

  Level: beginner

.keywords: DMAdaptor, convergence, destroy
.seealso: DMAdaptorCreate(), DMAdaptorAdapt()
@*/
PetscErrorCode DMAdaptorDestroy(DMAdaptor *adaptor)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!*adaptor) PetscFunctionReturn(0);
  PetscValidHeaderSpecific((*adaptor), PETSC_OBJECT_CLASSID, 1);
  if (--((PetscObject)(*adaptor))->refct > 0) {
    *adaptor = NULL;
    PetscFunctionReturn(0);
  }
  ierr = VecTaggerDestroy(&(*adaptor)->refineTag);CHKERRQ(ierr);
  ierr = VecTaggerDestroy(&(*adaptor)->coarsenTag);CHKERRQ(ierr);
  ierr = PetscFree((*adaptor)->exactSol);CHKERRQ(ierr);
  ierr = PetscHeaderDestroy(adaptor);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*@
  DMAdaptorSetFromOptions - Sets a DMAdaptor object from options

  Collective on DMAdaptor

  Input Parameters:
. adaptor - The DMAdaptor object

  Options Database Keys:
+ -adaptor_monitor <bool>        : Monitor the adaptation process
. -adaptor_sequence_num <num>    : Number of adaptations to generate an optimal grid
. -adaptor_target_num <num>      : Set the target number of vertices N_adapt, -1 for automatic determination
. -adaptor_refinement_factor <r> : Set r such that N_adapt = r^dim N_orig
. -adaptor_metric_h_min <min>    : Set the minimum eigenvalue of Hessian (sqr max edge length)
- -adaptor_metric_h_max <max>    : Set the maximum eigenvalue of Hessian (sqr min edge length)

  Level: beginner

.keywords: DMAdaptor, convergence, options
.seealso: DMAdaptorCreate(), DMAdaptorAdapt()
@*/
PetscErrorCode DMAdaptorSetFromOptions(DMAdaptor adaptor)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscOptionsBegin(PetscObjectComm((PetscObject) adaptor), "", "DM Adaptor Options", "DMAdaptor");CHKERRQ(ierr);
  ierr = PetscOptionsBool("-adaptor_monitor", "Monitor the adaptation process", "DMAdaptorMonitor", adaptor->monitor, &adaptor->monitor, NULL);CHKERRQ(ierr);
  ierr = PetscOptionsInt("-adaptor_sequence_num", "Number of adaptations to generate an optimal grid", "DMAdaptorSetSequenceLength", adaptor->numSeq, &adaptor->numSeq, NULL);CHKERRQ(ierr);
  ierr = PetscOptionsInt("-adaptor_target_num", "Set the target number of vertices N_adapt, -1 for automatic determination", "DMAdaptor", adaptor->Nadapt, &adaptor->Nadapt, NULL);CHKERRQ(ierr);
  ierr = PetscOptionsReal("-adaptor_refinement_factor", "Set r such that N_adapt = r^dim N_orig", "DMAdaptor", adaptor->refinementFactor, &adaptor->refinementFactor, NULL);CHKERRQ(ierr);
  ierr = PetscOptionsReal("-adaptor_metric_h_min", "Set the minimum eigenvalue of Hessian (sqr max edge length)", "DMAdaptor", adaptor->h_min, &adaptor->h_min, NULL);CHKERRQ(ierr);
  ierr = PetscOptionsReal("-adaptor_metric_h_max", "Set the maximum eigenvalue of Hessian (sqr min edge length)", "DMAdaptor", adaptor->h_max, &adaptor->h_max, NULL);CHKERRQ(ierr);
  ierr = PetscOptionsEnd();
  ierr = VecTaggerSetFromOptions(adaptor->refineTag);CHKERRQ(ierr);
  ierr = VecTaggerSetFromOptions(adaptor->coarsenTag);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*@
  DMAdaptorView - Views a DMAdaptor object

  Collective on DMAdaptor

  Input Parameters:
+ adaptor     - The DMAdaptor object
- viewer - The PetscViewer object

  Level: beginner

.keywords: DMAdaptor, adaptivity, view
.seealso: DMAdaptorCreate(), DMAdaptorAdapt()
@*/
PetscErrorCode DMAdaptorView(DMAdaptor adaptor, PetscViewer viewer)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscObjectPrintClassNamePrefixType((PetscObject) adaptor, viewer);CHKERRQ(ierr);
  ierr = PetscViewerASCIIPrintf(viewer, "DM Adaptor\n");CHKERRQ(ierr);
  ierr = PetscViewerASCIIPrintf(viewer, "  sequence length: %D\n", adaptor->numSeq);CHKERRQ(ierr);
  ierr = VecTaggerView(adaptor->refineTag,  viewer);CHKERRQ(ierr);
  ierr = VecTaggerView(adaptor->coarsenTag, viewer);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*@
  DMAdaptorGetSolver - Gets the solver used to produce discrete solutions

  Not collective

  Input Parameter:
. adaptor   - The DMAdaptor object

  Output Parameter:
. snes - The solver

  Level: intermediate

.keywords: DMAdaptor, convergence
.seealso: DMAdaptorSetSolver(), DMAdaptorCreate(), DMAdaptorAdapt()
@*/
PetscErrorCode DMAdaptorGetSolver(DMAdaptor adaptor, SNES *snes)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(adaptor, PETSC_OBJECT_CLASSID, 1);
  PetscValidPointer(snes, 2);
  *snes = adaptor->snes;
  PetscFunctionReturn(0);
}

/*@
  DMAdaptorSetSolver - Sets the solver used to produce discrete solutions

  Not collective

  Input Parameters:
+ adaptor   - The DMAdaptor object
- snes - The solver

  Level: intermediate

  Note: The solver MUST have an attached DM/DS, so that we know the exact solution

.keywords: DMAdaptor, convergence
.seealso: DMAdaptorGetSolver(), DMAdaptorCreate(), DMAdaptorAdapt()
@*/
PetscErrorCode DMAdaptorSetSolver(DMAdaptor adaptor, SNES snes)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(adaptor, PETSC_OBJECT_CLASSID, 1);
  PetscValidHeaderSpecific(snes, SNES_CLASSID, 2);
  adaptor->snes = snes;
  ierr = SNESGetDM(adaptor->snes, &adaptor->idm);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*@
  DMAdaptorGetSequenceLength - Gets the number of sequential adaptations

  Not collective

  Input Parameter:
. adaptor - The DMAdaptor object

  Output Parameter:
. num - The number of adaptations

  Level: intermediate

.keywords: DMAdaptor, convergence
.seealso: DMAdaptorSetSequenceLength(), DMAdaptorCreate(), DMAdaptorAdapt()
@*/
PetscErrorCode DMAdaptorGetSequenceLength(DMAdaptor adaptor, PetscInt *num)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(adaptor, PETSC_OBJECT_CLASSID, 1);
  PetscValidPointer(num, 2);
  *num = adaptor->numSeq;
  PetscFunctionReturn(0);
}

/*@
  DMAdaptorSetSequenceLength - Sets the number of sequential adaptations

  Not collective

  Input Parameters:
+ adaptor - The DMAdaptor object
- num - The number of adaptations

  Level: intermediate

.keywords: DMAdaptor, convergence
.seealso: DMAdaptorGetSequenceLength(), DMAdaptorCreate(), DMAdaptorAdapt()
@*/
PetscErrorCode DMAdaptorSetSequenceLength(DMAdaptor adaptor, PetscInt num)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(adaptor, PETSC_OBJECT_CLASSID, 1);
  adaptor->numSeq = num;
  PetscFunctionReturn(0);
}

/*@
  DMAdaptorSetUp - After the solver is specified, we create structures for controlling adaptivity

  Collective on DMAdaptor

  Input Parameters:
. adaptor - The DMAdaptor object

  Level: beginner

.keywords: DMAdaptor, convergence, setup
.seealso: DMAdaptorCreate(), DMAdaptorAdapt()
@*/
PetscErrorCode DMAdaptorSetUp(DMAdaptor adaptor)
{
  PetscDS        prob;
  PetscInt       Nf, f;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = DMGetDS(adaptor->idm, &prob);CHKERRQ(ierr);
  ierr = VecTaggerSetUp(adaptor->refineTag);CHKERRQ(ierr);
  ierr = VecTaggerSetUp(adaptor->coarsenTag);CHKERRQ(ierr);
  ierr = PetscDSGetNumFields(prob, &Nf);CHKERRQ(ierr);
  ierr = PetscMalloc1(Nf, &adaptor->exactSol);CHKERRQ(ierr);
  for (f = 0; f < Nf; ++f) {
    ierr = PetscDSGetExactSolution(prob, f, &adaptor->exactSol[f]);CHKERRQ(ierr);
    /* TODO Have a flag that forces projection rather than using the exact solution */
    if (adaptor->exactSol[0]) {ierr = DMAdaptorSetTransferFunction(adaptor, DMAdaptorTransferSolution_Exact_Private);CHKERRQ(ierr);}
  }
  PetscFunctionReturn(0);
}

PetscErrorCode DMAdaptorGetTransferFunction(DMAdaptor adaptor, PetscErrorCode (**tfunc)(DMAdaptor, DM, Vec, DM, Vec, void *))
{
  PetscFunctionBegin;
  *tfunc = adaptor->ops->transfersolution;
  PetscFunctionReturn(0);
}

PetscErrorCode DMAdaptorSetTransferFunction(DMAdaptor adaptor, PetscErrorCode (*tfunc)(DMAdaptor, DM, Vec, DM, Vec, void *))
{
  PetscFunctionBegin;
  adaptor->ops->transfersolution = tfunc;
  PetscFunctionReturn(0);
}

PetscErrorCode DMAdaptorPreAdapt(DMAdaptor adaptor, Vec locX)
{
  DM             plex;
  PetscDS        prob;
  PetscObject    obj;
  PetscClassId   id;
  PetscBool      isForest;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = DMConvert(adaptor->idm, DMPLEX, &plex);CHKERRQ(ierr);
  ierr = DMGetDS(adaptor->idm, &prob);CHKERRQ(ierr);
  ierr = PetscDSGetDiscretization(prob, 0, &obj);CHKERRQ(ierr);
  ierr = PetscObjectGetClassId(obj, &id);CHKERRQ(ierr);
  ierr = DMIsForest(adaptor->idm, &isForest);CHKERRQ(ierr);
  if (adaptor->adaptCriterion == DM_ADAPTATION_NONE) {
    if (isForest) {adaptor->adaptCriterion = DM_ADAPTATION_LABEL;}
#ifdef PETSC_HAVE_PRAGMATIC
    else          {adaptor->adaptCriterion = DM_ADAPTATION_METRIC;}
#else
    else          {adaptor->adaptCriterion = DM_ADAPTATION_REFINE;}
#endif
  }
  if (id == PETSCFV_CLASSID) {adaptor->femType = PETSC_FALSE;}
  else                       {adaptor->femType = PETSC_TRUE;}
  if (adaptor->femType) {
    /* Compute local solution bc */
    ierr = DMPlexInsertBoundaryValues(plex, PETSC_TRUE, locX, 0.0, adaptor->faceGeom, adaptor->cellGeom, NULL);CHKERRQ(ierr);
  } else {
    PetscFV      fvm = (PetscFV) obj;
    PetscLimiter noneLimiter;
    Vec          grad;

    ierr = PetscFVGetComputeGradients(fvm, &adaptor->computeGradient);CHKERRQ(ierr);
    ierr = PetscFVSetComputeGradients(fvm, PETSC_TRUE);CHKERRQ(ierr);
    /* Use no limiting when reconstructing gradients for adaptivity */
    ierr = PetscFVGetLimiter(fvm, &adaptor->limiter);CHKERRQ(ierr);
    ierr = PetscObjectReference((PetscObject) adaptor->limiter);CHKERRQ(ierr);
    ierr = PetscLimiterCreate(PetscObjectComm((PetscObject) fvm), &noneLimiter);CHKERRQ(ierr);
    ierr = PetscLimiterSetType(noneLimiter, PETSCLIMITERNONE);CHKERRQ(ierr);
    ierr = PetscFVSetLimiter(fvm, noneLimiter);CHKERRQ(ierr);
    /* Get FVM data */
    ierr = DMPlexGetDataFVM(plex, fvm, &adaptor->cellGeom, &adaptor->faceGeom, &adaptor->gradDM);CHKERRQ(ierr);
    ierr = VecGetDM(adaptor->cellGeom, &adaptor->cellDM);CHKERRQ(ierr);
    ierr = VecGetArrayRead(adaptor->cellGeom, &adaptor->cellGeomArray);CHKERRQ(ierr);
    /* Compute local solution bc */
    ierr = DMPlexInsertBoundaryValues(plex, PETSC_TRUE, locX, 0.0, adaptor->faceGeom, adaptor->cellGeom, NULL);CHKERRQ(ierr);
    /* Compute gradients */
    ierr = DMCreateGlobalVector(adaptor->gradDM, &grad);CHKERRQ(ierr);
    ierr = DMPlexReconstructGradientsFVM(plex, locX, grad);CHKERRQ(ierr);
    ierr = DMGetLocalVector(adaptor->gradDM, &adaptor->cellGrad);CHKERRQ(ierr);
    ierr = DMGlobalToLocalBegin(adaptor->gradDM, grad, INSERT_VALUES, adaptor->cellGrad);CHKERRQ(ierr);
    ierr = DMGlobalToLocalEnd(adaptor->gradDM, grad, INSERT_VALUES, adaptor->cellGrad);CHKERRQ(ierr);
    ierr = VecDestroy(&grad);CHKERRQ(ierr);
    ierr = VecGetArrayRead(adaptor->cellGrad, &adaptor->cellGradArray);CHKERRQ(ierr);
  }
  ierr = DMDestroy(&plex);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

PetscErrorCode DMAdaptorTransferSolution(DMAdaptor adaptor, DM dm, Vec x, DM adm, Vec ax)
{
  PetscReal      time = 0.0;
  Mat            interp;
  void          *ctx;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = DMGetApplicationContext(dm, &ctx);CHKERRQ(ierr);
  if (adaptor->ops->transfersolution) {
    ierr = (*adaptor->ops->transfersolution)(adaptor, dm, x, adm, ax, ctx);CHKERRQ(ierr);
  } else {
    switch (adaptor->adaptCriterion) {
    case DM_ADAPTATION_LABEL:
      ierr = DMForestTransferVec(dm, x, adm, ax, PETSC_TRUE, time);CHKERRQ(ierr);
      break;
    case DM_ADAPTATION_REFINE:
    case DM_ADAPTATION_METRIC:
      ierr = DMCreateInterpolation(dm, adm, &interp, NULL);CHKERRQ(ierr);
      ierr = MatInterpolate(interp, x, ax);CHKERRQ(ierr);
      ierr = DMInterpolate(dm, interp, adm);CHKERRQ(ierr);
      ierr = MatDestroy(&interp);CHKERRQ(ierr);
      break;
    default: SETERRQ1(PetscObjectComm((PetscObject) adaptor), PETSC_ERR_SUP, "No built-in projection for this adaptation criterion: %D", adaptor->adaptCriterion);
    }
  }
  PetscFunctionReturn(0);
}

PetscErrorCode DMAdaptorPostAdapt(DMAdaptor adaptor)
{
  PetscDS        prob;
  PetscObject    obj;
  PetscClassId   id;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = DMGetDS(adaptor->idm, &prob);CHKERRQ(ierr);
  ierr = PetscDSGetDiscretization(prob, 0, &obj);CHKERRQ(ierr);
  ierr = PetscObjectGetClassId(obj, &id);CHKERRQ(ierr);
  if (id == PETSCFV_CLASSID) {
    PetscFV fvm = (PetscFV) obj;

    ierr = PetscFVSetComputeGradients(fvm, adaptor->computeGradient);CHKERRQ(ierr);
    /* Restore original limiter */
    ierr = PetscFVSetLimiter(fvm, adaptor->limiter);CHKERRQ(ierr);

    ierr = VecRestoreArrayRead(adaptor->cellGeom, &adaptor->cellGeomArray);CHKERRQ(ierr);
    ierr = VecRestoreArrayRead(adaptor->cellGrad, &adaptor->cellGradArray);CHKERRQ(ierr);
    ierr = DMRestoreLocalVector(adaptor->gradDM, &adaptor->cellGrad);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

/*@
  PetscGlobalMinMax - Get the global min/max from local min/max input

  Collective on comm

  Input Parameter:
. minMaxVal - An array with the local min and max

  Output Parameter:
. minMaxValGlobal - An array with the global min and max

  Level: beginner

.keywords: minimum, maximum
.seealso: PetscSplitOwnership()
@*/
PetscErrorCode PetscGlobalMinMax(MPI_Comm comm, PetscReal minMaxVal[2], PetscReal minMaxValGlobal[2])
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  minMaxVal[1] = -minMaxVal[1];
  ierr = MPI_Allreduce(minMaxVal, minMaxValGlobal, 2, MPIU_REAL, MPI_MIN, comm);CHKERRQ(ierr);
  minMaxValGlobal[1] = -minMaxValGlobal[1];
  PetscFunctionReturn(0);
}

static PetscErrorCode DMAdaptorModifyHessian_Private(PetscInt dim, PetscReal h_min, PetscReal h_max, PetscScalar Hp[])
{
  PetscScalar   *Hpos;
  PetscReal     *eigs;
  PetscInt       i, j, k;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscMalloc2(dim*dim, &Hpos, dim, &eigs);CHKERRQ(ierr);
#if 0
  ierr = PetscPrintf(PETSC_COMM_SELF, "H = [");CHKERRQ(ierr);
  for (i = 0; i < dim; ++i) {
    if (i > 0) {ierr = PetscPrintf(PETSC_COMM_SELF, "     ");CHKERRQ(ierr);}
    for (j = 0; j < dim; ++j) {
      if (j > 0) {ierr = PetscPrintf(PETSC_COMM_SELF, ", ");CHKERRQ(ierr);}
      ierr = PetscPrintf(PETSC_COMM_SELF, "%g", Hp[i*dim+j]);CHKERRQ(ierr);
    }
    ierr = PetscPrintf(PETSC_COMM_SELF, "\n");CHKERRQ(ierr);
  }
  ierr = PetscPrintf(PETSC_COMM_SELF, "]\n");CHKERRQ(ierr);
#endif
  /* Symmetrize */
  for (i = 0; i < dim; ++i) {
    Hpos[i*dim+i] = Hp[i*dim+i];
    for (j = i+1; j < dim; ++j) {
      Hpos[i*dim+j] = 0.5*(Hp[i*dim+j] + Hp[j*dim+i]);
      Hpos[j*dim+i] = Hpos[i*dim+j];
    }
  }
#if 0
  ierr = PetscPrintf(PETSC_COMM_SELF, "Hs = [");CHKERRQ(ierr);
  for (i = 0; i < dim; ++i) {
    if (i > 0) {ierr = PetscPrintf(PETSC_COMM_SELF, "      ");CHKERRQ(ierr);}
    for (j = 0; j < dim; ++j) {
      if (j > 0) {ierr = PetscPrintf(PETSC_COMM_SELF, ", ");CHKERRQ(ierr);}
      ierr = PetscPrintf(PETSC_COMM_SELF, "%g", Hpos[i*dim+j]);CHKERRQ(ierr);
    }
    ierr = PetscPrintf(PETSC_COMM_SELF, "\n");CHKERRQ(ierr);
  }
  ierr = PetscPrintf(PETSC_COMM_SELF, "]\n");CHKERRQ(ierr);
#endif
  /* Compute eigendecomposition */
  {
    PetscScalar  *work;
    PetscBLASInt lwork;

    lwork = 5*dim;
    ierr = PetscMalloc1(5*dim, &work);CHKERRQ(ierr);
#if defined(PETSC_MISSING_LAPACK_GEEV)
    SETERRQ(PetscObjectComm((PetscObject) dm), PETSC_ERR_SUP, "GEEV - Lapack routine is unavailable\nNot able to provide eigen values.");
#else
    {
      PetscBLASInt lierr;
      PetscBLASInt nb;

      ierr = PetscBLASIntCast(dim, &nb);CHKERRQ(ierr);
      ierr = PetscFPTrapPush(PETSC_FP_TRAP_OFF);CHKERRQ(ierr);
      PetscStackCallBLAS("LAPACKsyev",LAPACKsyev_("V","U",&nb,Hpos,&nb,eigs,work,&lwork,&lierr));
      if (lierr) SETERRQ1(PETSC_COMM_SELF, PETSC_ERR_LIB, "Error in LAPACK routine %d", (int) lierr);
      ierr = PetscFPTrapPop();CHKERRQ(ierr);
    }
#endif
    ierr = PetscFree(work);CHKERRQ(ierr);
  }
#if 0
  ierr = PetscPrintf(PETSC_COMM_SELF, "L = [");CHKERRQ(ierr);
  for (i = 0; i < dim; ++i) {
    if (i > 0) {ierr = PetscPrintf(PETSC_COMM_SELF, ", ");CHKERRQ(ierr);}
    ierr = PetscPrintf(PETSC_COMM_SELF, "%g", eigs[i]);CHKERRQ(ierr);
  }
  ierr = PetscPrintf(PETSC_COMM_SELF, "]\n");CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_SELF, "Q = [");CHKERRQ(ierr);
  for (i = 0; i < dim; ++i) {
    if (i > 0) {ierr = PetscPrintf(PETSC_COMM_SELF, "     ");CHKERRQ(ierr);}
    for (j = 0; j < dim; ++j) {
      if (j > 0) {ierr = PetscPrintf(PETSC_COMM_SELF, ", ");CHKERRQ(ierr);}
      ierr = PetscPrintf(PETSC_COMM_SELF, "%g", Hpos[i*dim+j]);CHKERRQ(ierr);
    }
    ierr = PetscPrintf(PETSC_COMM_SELF, "\n");CHKERRQ(ierr);
  }
  ierr = PetscPrintf(PETSC_COMM_SELF, "]\n");CHKERRQ(ierr);
#endif
  /* Reflect to positive orthant, enforce maximum and minimum size, \lambda \propto 1/h^2
       TODO get domain bounding box */
  for (i = 0; i < dim; ++i) eigs[i] = PetscMin(h_max, PetscMax(h_min, PetscAbsReal(eigs[i])));
  /* Reconstruct Hessian */
  for (i = 0; i < dim; ++i) {
    for (j = 0; j < dim; ++j) {
      Hp[i*dim+j] = 0.0;
      for (k = 0; k < dim; ++k) {
        Hp[i*dim+j] += Hpos[k*dim+i] * eigs[k] * Hpos[k*dim+j];
      }
    }
  }
  ierr = PetscFree2(Hpos, eigs);CHKERRQ(ierr);
#if 0
  ierr = PetscPrintf(PETSC_COMM_SELF, "H+ = [");CHKERRQ(ierr);
  for (i = 0; i < dim; ++i) {
    if (i > 0) {ierr = PetscPrintf(PETSC_COMM_SELF, "      ");CHKERRQ(ierr);}
    for (j = 0; j < dim; ++j) {
      if (j > 0) {ierr = PetscPrintf(PETSC_COMM_SELF, ", ");CHKERRQ(ierr);}
      ierr = PetscPrintf(PETSC_COMM_SELF, "%g", Hp[i*dim+j]);CHKERRQ(ierr);
    }
    ierr = PetscPrintf(PETSC_COMM_SELF, "\n");CHKERRQ(ierr);
  }
  ierr = PetscPrintf(PETSC_COMM_SELF, "]\n");CHKERRQ(ierr);
#endif
  PetscFunctionReturn(0);
}

static void detHFunc(PetscInt dim, PetscInt Nf, PetscInt NfAux,
                     const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[],
                     const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[],
                     PetscReal t, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f0[])
{
  const PetscInt p = 1;
  PetscReal      detH = 0.0;

  if      (dim == 2) DMPlex_Det2D_Scalar_Internal(&detH, u);
  else if (dim == 3) DMPlex_Det3D_Scalar_Internal(&detH, u);
  f0[0] = PetscPowReal(detH, p/(2.*p + dim));
}

static PetscErrorCode DMAdaptorAdapt_Sequence_Private(DMAdaptor adaptor, Vec x, PetscBool doSolve, DM *adm, Vec *ax)
{
  PetscDS        prob;
  void          *ctx;
  MPI_Comm       comm;
  PetscInt       numAdapt = adaptor->numSeq, adaptIter;
  PetscInt       dim, coordDim, numFields, cStart, cEnd, cEndInterior, c;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = DMViewFromOptions(adaptor->idm, NULL, "-dm_adapt_pre_view");CHKERRQ(ierr);
  ierr = VecViewFromOptions(x, NULL, "-sol_adapt_pre_view");CHKERRQ(ierr);
  ierr = PetscObjectGetComm((PetscObject) adaptor, &comm);CHKERRQ(ierr);
  ierr = DMGetDimension(adaptor->idm, &dim);CHKERRQ(ierr);
  ierr = DMGetCoordinateDim(adaptor->idm, &coordDim);CHKERRQ(ierr);
  ierr = DMGetApplicationContext(adaptor->idm, &ctx);CHKERRQ(ierr);
  ierr = DMGetDS(adaptor->idm, &prob);CHKERRQ(ierr);
  ierr = PetscDSGetNumFields(prob, &numFields);CHKERRQ(ierr);

  /* Adapt until nothing changes */
  /* Adapt for a specified number of iterates */
  for (adaptIter = 0; adaptIter < numAdapt-1; ++adaptIter) {ierr = PetscViewerASCIIPushTab(PETSC_VIEWER_STDOUT_(comm));CHKERRQ(ierr);}
  for (adaptIter = 0; adaptIter < numAdapt;   ++adaptIter) {
    PetscBool adapted = PETSC_FALSE;
    DM        dm      = adaptIter ? *adm : adaptor->idm, plex, odm;
    Vec       locX, ox;

    ierr = DMConvert(dm, DMPLEX, &plex);CHKERRQ(ierr);
    ierr = DMGetLocalVector(dm, &locX);CHKERRQ(ierr);
    ierr = DMGlobalToLocalBegin(dm, adaptIter ? *ax : x, INSERT_VALUES, locX);CHKERRQ(ierr);
    ierr = DMGlobalToLocalEnd(dm, adaptIter ? *ax : x, INSERT_VALUES, locX);CHKERRQ(ierr);
    ierr = DMAdaptorPreAdapt(adaptor, locX);CHKERRQ(ierr);
    if (doSolve) {
      SNES snes;

      ierr = DMAdaptorGetSolver(adaptor, &snes);CHKERRQ(ierr);
      ierr = SNESSolve(snes, NULL, adaptIter ? *ax : x);CHKERRQ(ierr);
    }
    /* ierr = DMAdaptorMonitor(adaptor);CHKERRQ(ierr);
       Print iterate, memory used, DM, solution */
    switch (adaptor->adaptCriterion) {
    case DM_ADAPTATION_REFINE:
      ierr = DMRefine(dm, comm, &odm);CHKERRQ(ierr);
      if (!odm) SETERRQ(comm, PETSC_ERR_ARG_INCOMP, "DMRefine() did not perform any refinement, cannot continue grid sequencing");
      adapted = PETSC_TRUE;
      break;
    case DM_ADAPTATION_LABEL:
    {
      /* Adapt DM
           Create local solution
           Reconstruct gradients (FVM) or solve adjoint equation (FEM)
           Produce cellwise error indicator */
      DMLabel            adaptLabel;
      IS                 refineIS, coarsenIS;
      Vec                errVec;
      PetscScalar       *errArray;
      const PetscScalar *pointSols;
      PetscReal          minMaxInd[2] = {PETSC_MAX_REAL, PETSC_MIN_REAL}, minMaxIndGlobal[2];
      PetscInt           nRefine, nCoarsen;

      ierr = DMLabelCreate("adapt", &adaptLabel);CHKERRQ(ierr);
      ierr = DMPlexGetHeightStratum(plex, 0, &cStart, &cEnd);CHKERRQ(ierr);
      ierr = DMPlexGetHybridBounds(plex, &cEndInterior, NULL, NULL, NULL);CHKERRQ(ierr);
      cEnd = (cEndInterior < 0) ? cEnd : cEndInterior;

      ierr = VecCreateMPI(PetscObjectComm((PetscObject) adaptor), cEnd-cStart, PETSC_DETERMINE, &errVec);CHKERRQ(ierr);
      ierr = VecSetUp(errVec);CHKERRQ(ierr);
      ierr = VecGetArray(errVec, &errArray);CHKERRQ(ierr);
      ierr = VecGetArrayRead(locX, &pointSols);CHKERRQ(ierr);
      for (c = cStart; c < cEnd; ++c) {
        PetscReal    errInd = 0.;
        PetscScalar *pointSol;
        PetscScalar *pointGrad;
        PetscFVCellGeom *cg;

        ierr = DMPlexPointLocalRead(plex, c, pointSols, &pointSol);CHKERRQ(ierr);
        ierr = DMPlexPointLocalRead(adaptor->gradDM, c, adaptor->cellGradArray, &pointGrad);CHKERRQ(ierr);
        ierr = DMPlexPointLocalRead(adaptor->cellDM, c, adaptor->cellGeomArray, &cg);CHKERRQ(ierr);
        ierr = (*adaptor->ops->computeerrorindicator)(adaptor, dim, c, pointSol, pointGrad, cg, &errInd, ctx);CHKERRQ(ierr);
        errArray[c-cStart] = errInd;
        minMaxInd[0] = PetscMin(minMaxInd[0], errInd);
        minMaxInd[1] = PetscMax(minMaxInd[1], errInd);
      }
      ierr = VecRestoreArrayRead(locX, &pointSols);CHKERRQ(ierr);
      ierr = VecRestoreArray(errVec, &errArray);CHKERRQ(ierr);
      ierr = PetscGlobalMinMax(PetscObjectComm((PetscObject) adaptor), minMaxInd, minMaxIndGlobal);CHKERRQ(ierr);
      ierr = PetscInfo2(adaptor, "error indicator range (%E, %E)\n", minMaxIndGlobal[0], minMaxIndGlobal[1]);CHKERRQ(ierr);
      /*     Compute IS from VecTagger */
      ierr = VecTaggerComputeIS(adaptor->refineTag, errVec, &refineIS);CHKERRQ(ierr);
      ierr = VecTaggerComputeIS(adaptor->coarsenTag, errVec, &coarsenIS);CHKERRQ(ierr);
      ierr = ISGetSize(refineIS, &nRefine);CHKERRQ(ierr);
      ierr = ISGetSize(coarsenIS, &nCoarsen);CHKERRQ(ierr);
      if (nRefine)  {ierr = DMLabelSetStratumIS(adaptLabel, DM_ADAPT_REFINE,  refineIS);CHKERRQ(ierr);}
      if (nCoarsen) {ierr = DMLabelSetStratumIS(adaptLabel, DM_ADAPT_COARSEN, coarsenIS);CHKERRQ(ierr);}
      ierr = ISDestroy(&coarsenIS);CHKERRQ(ierr);
      ierr = ISDestroy(&refineIS);CHKERRQ(ierr);
      ierr = VecDestroy(&errVec);CHKERRQ(ierr);
      /*     Adapt DM from label */
      if (nRefine || nCoarsen) {
        ierr = DMAdaptLabel(dm, adaptLabel, &odm);CHKERRQ(ierr);
        adapted = PETSC_TRUE;
      }
      ierr = DMLabelDestroy(&adaptLabel);CHKERRQ(ierr);
    }
    break;
    case DM_ADAPTATION_METRIC:
    {
      DM           dmGrad,   dmHess,   dmMetric;
      PetscDS      probGrad, probHess;
      Vec          xGrad,    xHess,    metric;
      PetscSection sec, msec;
      PetscScalar *H, *M;
      PetscReal    N, integral;
      DMLabel      bdLabel;
      PetscInt     Nd = coordDim*coordDim, f, vStart, vEnd, v;

      /*     Compute vertexwise gradients from cellwise gradients */
      ierr = DMClone(dm, &dmGrad);CHKERRQ(ierr);
      ierr = DMClone(dm, &dmHess);CHKERRQ(ierr);
      ierr = DMGetDS(dmGrad, &probGrad);CHKERRQ(ierr);
      ierr = DMGetDS(dmHess, &probHess);CHKERRQ(ierr);
      for (f = 0; f < numFields; ++f) {
        PetscFE         fe, feGrad, feHess;
        PetscDualSpace  Q;
        DM              K;
        PetscQuadrature q;
        PetscInt        Nc, qorder;
        const char     *prefix;

        ierr = PetscDSGetDiscretization(prob, f, (PetscObject *) &fe);CHKERRQ(ierr);
        ierr = PetscFEGetNumComponents(fe, &Nc);CHKERRQ(ierr);
        ierr = PetscFEGetDualSpace(fe, &Q);CHKERRQ(ierr);
        ierr = PetscDualSpaceGetDM(Q, &K);CHKERRQ(ierr);
        ierr = DMPlexGetDepthStratum(K, 0, &vStart, &vEnd);CHKERRQ(ierr);
        ierr = PetscFEGetQuadrature(fe, &q);CHKERRQ(ierr);
        ierr = PetscQuadratureGetOrder(q, &qorder);CHKERRQ(ierr);
        ierr = PetscObjectGetOptionsPrefix((PetscObject) fe, &prefix);CHKERRQ(ierr);
        ierr = PetscFECreateDefault(dmGrad, dim, Nc*coordDim, (vEnd-vStart) == dim+1 ? PETSC_TRUE : PETSC_FALSE, prefix, qorder, &feGrad);CHKERRQ(ierr);
        ierr = PetscDSSetDiscretization(probGrad, f, (PetscObject) feGrad);CHKERRQ(ierr);
        ierr = PetscFECreateDefault(dmHess, dim, Nc*Nd, (vEnd-vStart) == dim+1 ? PETSC_TRUE : PETSC_FALSE, prefix, qorder, &feHess);CHKERRQ(ierr);
        ierr = PetscDSSetDiscretization(probHess, f, (PetscObject) feHess);CHKERRQ(ierr);
        ierr = PetscFEDestroy(&feGrad);CHKERRQ(ierr);
        ierr = PetscFEDestroy(&feHess);CHKERRQ(ierr);
      }
      ierr = DMGetGlobalVector(dmGrad, &xGrad);CHKERRQ(ierr);
      ierr = VecViewFromOptions(x, NULL, "-sol_adapt_loc_pre_view");CHKERRQ(ierr);
      ierr = DMPlexComputeGradientClementInterpolant(dm, locX, xGrad);CHKERRQ(ierr);
      ierr = VecViewFromOptions(xGrad, NULL, "-adapt_gradient_view");CHKERRQ(ierr);
      /*     Compute vertexwise Hessians from cellwise Hessians */
      ierr = DMGetGlobalVector(dmHess, &xHess);CHKERRQ(ierr);
      ierr = DMPlexComputeGradientClementInterpolant(dmGrad, xGrad, xHess);CHKERRQ(ierr);
      ierr = VecViewFromOptions(xHess, NULL, "-adapt_hessian_view");CHKERRQ(ierr);
      /*     Compute metric */
      ierr = DMClone(dm, &dmMetric);CHKERRQ(ierr);
      ierr = DMGetDefaultSection(dm, &sec);CHKERRQ(ierr);
      ierr = DMPlexGetDepthStratum(dm, 0, &vStart, &vEnd);CHKERRQ(ierr);
      ierr = PetscSectionCreate(PetscObjectComm((PetscObject) dm), &msec);CHKERRQ(ierr);
      ierr = PetscSectionSetNumFields(msec, 1);CHKERRQ(ierr);
      ierr = PetscSectionSetFieldComponents(msec, 0, Nd);CHKERRQ(ierr);
      ierr = PetscSectionSetChart(msec, vStart, vEnd);CHKERRQ(ierr);
      for (v = vStart; v < vEnd; ++v) {
        ierr = PetscSectionSetDof(msec, v, Nd);CHKERRQ(ierr);
        ierr = PetscSectionSetFieldDof(msec, v, 0, Nd);CHKERRQ(ierr);
      }
      ierr = PetscSectionSetUp(msec);CHKERRQ(ierr);
      ierr = DMSetDefaultSection(dmMetric, msec);CHKERRQ(ierr);
      ierr = PetscSectionDestroy(&msec);CHKERRQ(ierr);
      ierr = DMGetLocalVector(dmMetric, &metric);CHKERRQ(ierr);
      /*       N is the target size */
      N    = adaptor->Nadapt >= 0 ? adaptor->Nadapt : PetscPowRealInt(adaptor->refinementFactor, dim)*((PetscReal) (vEnd - vStart));
      if (adaptor->monitor) {ierr = PetscPrintf(PETSC_COMM_SELF, "N_orig: %D N_adapt: %g\n", vEnd - vStart, N);CHKERRQ(ierr);}
      /*       |H| means take the absolute value of eigenvalues */
      ierr = VecGetArray(xHess, &H);CHKERRQ(ierr);
      ierr = VecGetArray(metric, &M);CHKERRQ(ierr);
      for (v = vStart; v < vEnd; ++v) {
        PetscScalar *Hp;

        ierr = DMPlexPointLocalRef(dmHess, v, H, &Hp);CHKERRQ(ierr);
        ierr = DMAdaptorModifyHessian_Private(coordDim, adaptor->h_min, adaptor->h_max, Hp);CHKERRQ(ierr);
      }
      /*       Pointwise on vertices M(x) = N^{2/d} (\int_\Omega det(|H|)^{p/(2p+d)})^{-2/d} det(|H|)^{-1/(2p+d)} |H| for L_p */
      ierr = PetscDSSetObjective(probHess, 0, detHFunc);CHKERRQ(ierr);
      ierr = DMPlexComputeIntegralFEM(dmHess, xHess, &integral, NULL);CHKERRQ(ierr);
      for (v = vStart; v < vEnd; ++v) {
        const PetscInt     p = 1;
        const PetscScalar *Hp;
        PetscScalar       *Mp;
        PetscReal          detH, fact;
        PetscInt           i;

        ierr = DMPlexPointLocalRead(dmHess, v, H, &Hp);CHKERRQ(ierr);
        ierr = DMPlexPointLocalRef(dmMetric, v, M, &Mp);CHKERRQ(ierr);
        if      (dim == 2) DMPlex_Det2D_Scalar_Internal(&detH, Hp);
        else if (dim == 3) DMPlex_Det3D_Scalar_Internal(&detH, Hp);
        else SETERRQ1(PetscObjectComm((PetscObject) adaptor), PETSC_ERR_SUP, "Dimension %d not supported", dim);
        fact = PetscPowReal(N, 2.0/dim) * PetscPowReal(integral, -2.0/dim) * PetscPowReal(PetscAbsReal(detH), -1.0/(2*p+dim));
#if 0
        ierr = PetscPrintf(PETSC_COMM_SELF, "fact: %g integral: %g |detH|: %g termA: %g termB: %g\n", fact, integral, PetscAbsReal(detH), PetscPowReal(integral, -2.0/dim), PetscPowReal(PetscAbsReal(detH), -1.0/(2*p+dim)));CHKERRQ(ierr);
        ierr = DMPrintCellMatrix(v, "H", coordDim, coordDim, Hp);CHKERRQ(ierr);
#endif
        for (i = 0; i < Nd; ++i) {
          Mp[i] = fact * Hp[i];
        }
      }
      ierr = VecRestoreArray(xHess, &H);CHKERRQ(ierr);
      ierr = VecRestoreArray(metric, &M);CHKERRQ(ierr);
      /*     Adapt DM from metric */
      ierr = DMGetLabel(dm, "marker", &bdLabel);CHKERRQ(ierr);
      ierr = DMAdaptMetric(dm, metric, bdLabel, &odm);CHKERRQ(ierr);
      adapted = PETSC_TRUE;
      /* Cleanup */
      ierr = DMRestoreLocalVector(dmMetric, &metric);CHKERRQ(ierr);
      ierr = DMRestoreGlobalVector(dmGrad, &xGrad);CHKERRQ(ierr);
      ierr = DMRestoreGlobalVector(dmHess, &xHess);CHKERRQ(ierr);
      ierr = DMDestroy(&dmMetric);CHKERRQ(ierr);
      ierr = DMDestroy(&dmGrad);CHKERRQ(ierr);
      ierr = DMDestroy(&dmHess);CHKERRQ(ierr);
    }
    break;
    default: SETERRQ1(comm, PETSC_ERR_ARG_WRONG, "Invalid adaptation type: %D", adaptor->adaptCriterion);
    }
    /* If DM was adapted, replace objects and recreate solution */
    if (adapted) {
      const char *name;

      ierr = PetscObjectGetName((PetscObject) dm, &name);CHKERRQ(ierr);
      ierr = PetscObjectSetName((PetscObject) odm, name);CHKERRQ(ierr);
      /* Reconfigure solver */
      ierr = SNESReset(adaptor->snes);CHKERRQ(ierr);
      ierr = SNESSetDM(adaptor->snes, odm);CHKERRQ(ierr);
      ierr = DMAdaptorSetSolver(adaptor, adaptor->snes);CHKERRQ(ierr);
      ierr = DMPlexSetSNESLocalFEM(odm, ctx, ctx, ctx);CHKERRQ(ierr);
      ierr = SNESSetFromOptions(adaptor->snes);CHKERRQ(ierr);
      /* Transfer system */
      ierr = DMSetDS(odm, prob);CHKERRQ(ierr);
      /* Transfer solution */
      ierr = DMCreateGlobalVector(odm, &ox);CHKERRQ(ierr);
      ierr = DMAdaptorTransferSolution(adaptor, dm, x, odm, ox);CHKERRQ(ierr);
      /* Cleanup adaptivity info */
      if (adaptIter > 0) {ierr = PetscViewerASCIIPopTab(PETSC_VIEWER_STDOUT_(comm));CHKERRQ(ierr);}
      ierr = DMForestSetAdaptivityForest(dm, NULL);CHKERRQ(ierr); /* clear internal references to the previous dm */
      *adm = odm;
      *ax  = ox;
    } else {
      *adm = dm;
      *ax  = x;
      break;
    }
    if (adaptIter < numAdapt-1) {
      ierr = DMViewFromOptions(odm, NULL, "-dm_adapt_iter_view");CHKERRQ(ierr);
      ierr = VecViewFromOptions(ox, NULL, "-sol_adapt_iter_view");CHKERRQ(ierr);
    }
    ierr = DMAdaptorPostAdapt(adaptor);CHKERRQ(ierr);
    ierr = DMRestoreLocalVector(dm, &locX);CHKERRQ(ierr);
    ierr = DMDestroy(&plex);CHKERRQ(ierr);
  }
  ierr = DMViewFromOptions(*adm, NULL, "-dm_adapt_view");CHKERRQ(ierr);
  ierr = VecViewFromOptions(*ax, NULL, "-sol_adapt_view");CHKERRQ(ierr);
  /* Destroy old structures */
  if (*adm != adaptor->idm) {
    ierr = DMDestroy(&adaptor->idm);CHKERRQ(ierr);
    ierr = VecDestroy(&x);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

/*@
  DMAdaptorAdapt - Creates a new DM that is adapted to the problem

  Not collective

  Input Parameter:
+ adaptor  - The DMAdaptor object
. x        - The global approximate solution
- strategy - The adaptation strategy

  Output Parameters:
+ adm - The adapted DM
- ax  - The adapted solution

  Options database keys:
. -snes_adapt <strategy> : initial, sequential, multigrid
. -adapt_gradient_view : View the Clement interpolant of the solution gradient
. -adapt_hessian_view : View the Clement interpolant of the solution Hessian
. -adapt_metric_view : View the metric tensor for adaptive mesh refinement

  Note: The available adaptation strategies are:
$ 1) Adapt the intial mesh until a quality metric, e,g, a priori error bound, is satisfied
$ 2) Solve the problem on a series of adapted meshes until a quality metric, e.g. a posteriori error bound, is satisfied
$ 3) Solve the problem on a hierarchy of adapted meshes generated to satisfy a quality metric using multigrid

  Level: intermediate

.keywords: DMAdaptor, convergence
.seealso: DMAdaptorSetSolver(), DMAdaptorCreate(), DMAdaptorAdapt()
@*/
PetscErrorCode DMAdaptorAdapt(DMAdaptor adaptor, Vec x, DMAdaptationStrategy strategy, DM *adm, Vec *ax)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  switch (strategy)
  {
  case DM_ADAPTATION_INITIAL:
    ierr = DMAdaptorAdapt_Sequence_Private(adaptor, x, PETSC_FALSE, adm, ax);CHKERRQ(ierr);
    break;
  case DM_ADAPTATION_SEQUENTIAL:
    ierr = DMAdaptorAdapt_Sequence_Private(adaptor, x, PETSC_TRUE, adm, ax);CHKERRQ(ierr);
    break;
  default: SETERRQ1(PetscObjectComm((PetscObject) adaptor), PETSC_ERR_ARG_WRONG, "Unrecognized adaptation strategy %d", strategy);
  }
  PetscFunctionReturn(0);
}
