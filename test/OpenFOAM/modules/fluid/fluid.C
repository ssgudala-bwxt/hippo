// buoyantPimpleFoam physics as a Hippo::HippoSolver.
// Converted from $WM_PROJECT_DIR/applications/solvers/heatTransfer/buoyantPimpleFoam/
// for use with ESI OpenFOAM-v2606 (static mesh, no LTS, no MRF active).

#include "fluid.H"
#include "constrainHbyA.H"
#include "constrainPressure.H"
#include "fvcSmooth.H"

// ---------------------------------------------------------------------------
// Factory symbol for HippoSolverRegistry dlopen pull model.
// ---------------------------------------------------------------------------
extern "C" Hippo::HippoSolver *
hippo_solver_factory_fluid(Foam::fvMesh & mesh)
{
  return new Foam::solvers::fluid(mesh);
}

// ---------------------------------------------------------------------------
// Constructor — initialises all fields in member-declaration order.
// ---------------------------------------------------------------------------
Foam::solvers::fluid::fluid(fvMesh & mesh)
  : Hippo::HippoSolver(mesh),
    // pimpleControl must come before pressureControl (needs pimple_.dict())
    pimple_(mesh),
    pThermo_(rhoThermo::New(mesh)),
    thermo_(pThermo_()),
    p_(thermo_.p()),
    rho_(IOobject("rho", mesh.time().name(), mesh, IOobject::NO_READ, IOobject::NO_WRITE),
         thermo_.rho()),
    U_(IOobject("U", mesh.time().name(), mesh, IOobject::MUST_READ, IOobject::AUTO_WRITE), mesh),
    phi_(IOobject("phi", mesh.time().name(), mesh, IOobject::READ_IF_PRESENT, IOobject::AUTO_WRITE),
         linearInterpolate(rho_ * U_) & mesh.Sf()),
    turbulence_(compressible::turbulenceModel::New(rho_, U_, phi_, thermo_)),
    g_(IOobject("g", mesh.time().constant(), mesh, IOobject::MUST_READ, IOobject::NO_WRITE)),
    hRef_("hRef", dimLength, 0),
    ghRef_(mag(g_) * hRef_),
    gh_("gh", (g_ & mesh.C()) - ghRef_),
    ghf_("ghf", (g_ & mesh.Cf()) - ghRef_),
    p_rgh_(IOobject("p_rgh", mesh.time().name(), mesh, IOobject::MUST_READ, IOobject::AUTO_WRITE),
           mesh),
    pRefCell_(0),
    pRefValue_(0.0),
    initialMass_("initialMass", fvc::domainIntegrate(rho_)),
    pressureControl_(p_, rho_, pimple_.dict(), false),
    rhoMax_("rhoMax", dimDensity, Foam::GREAT, pimple_.dict()),
    rhoMin_("rhoMin", dimDensity, Zero, pimple_.dict()),
    dpdt_(IOobject("dpdt", mesh.time().name(), mesh, IOobject::NO_READ, IOobject::NO_WRITE),
          fvc::ddt(p_)),
    K_("K", 0.5 * magSqr(U_)),
    MRF_(mesh),
    radiation_(radiation::radiationModel::New(thermo_.T())),
    fvOptions_(fv::options::New(mesh))
{
  thermo_.validate("fluid", "h", "e");
  turbulence_->validate();

  // Force p_rgh consistent with p
  p_rgh_ = p_ - rho_ * gh_;
  mesh.setFluxRequired(p_rgh_.name());

  if (p_rgh_.needReference())
  {
    setRefCell(p_, p_rgh_, pimple_.dict(), pRefCell_, pRefValue_);
    p_ += dimensionedScalar("p", p_.dimensions(), pRefValue_ - getRefCellValue(p_, pRefCell_));
  }
}

// ---------------------------------------------------------------------------
// solve() — one time step of PIMPLE loop (buoyantPimpleFoam logic).
// ---------------------------------------------------------------------------
void
Foam::solvers::fluid::solve()
{
  auto & mesh = this->mesh();
  auto & thermo = thermo_;
  auto & rho = rho_;
  auto & U = U_;
  auto & phi = phi_;
  auto & p = p_;
  auto & p_rgh = p_rgh_;
  auto & gh = gh_;
  auto & ghf = ghf_;
  auto & dpdt = dpdt_;
  auto & K = K_;
  auto & MRF = MRF_;
  auto & fvOptions = fvOptions_;
  auto & radiation = *radiation_;
  auto & turbulence = *turbulence_;
  auto & pimple = pimple_;
  const auto & g = g_;
  const auto & psi = thermo_.psi();

  // --- rhoEqn (first iter) ---
  if (pimple.firstIter())
  {
    (fvm::ddt(rho) + fvc::div(phi)).solve();
  }

  while (pimple.loop())
  {
    // ---- UEqn ----
    MRF.correctBoundaryVelocity(U);

    fvVectorMatrix UEqn(fvm::ddt(rho, U) + fvm::div(phi, U) + MRF.DDt(rho, U) +
                        turbulence.divDevRhoReff(U) ==
                        fvOptions(rho, U));

    UEqn.relax();
    fvOptions.constrain(UEqn);

    if (pimple.momentumPredictor())
    {
      (UEqn ==
                  fvc::reconstruct((-ghf * fvc::snGrad(rho) - fvc::snGrad(p_rgh)) * mesh.magSf())).solve();
      fvOptions.correct(U);
      K = 0.5 * magSqr(U);
    }

    // ---- EEqn ----
    {
      volScalarField & he = thermo.he();

      fvScalarMatrix EEqn(
          fvm::ddt(rho, he) + fvm::div(phi, he) + fvc::ddt(rho, K) + fvc::div(phi, K) +
              (he.name() == "e"
                   ? fvc::div(fvc::absolute(phi / fvc::interpolate(rho), U), p, "div(phiv,p)")
                   : -dpdt) -
              fvm::laplacian(turbulence.alphaEff(), he) ==
          rho * (U & g) + radiation.Sh(thermo, he) + fvOptions(rho, he));

      EEqn.relax();
      fvOptions.constrain(EEqn);
      EEqn.solve();
      fvOptions.correct(he);

      thermo.correct();
      radiation.correct();
    }

    // ---- pEqn ----
    {
      dimensionedScalar compressibility = fvc::domainIntegrate(psi);
      bool isCompressible = (compressibility.value() > Foam::SMALL);

      rho = thermo.rho();
      const volScalarField psip0(psi * p);

      volScalarField rAU(1.0 / UEqn.A());
      surfaceScalarField rhorAUf("rhorAUf", fvc::interpolate(rho * rAU));
      volVectorField HbyA(constrainHbyA(rAU * UEqn.H(), U, p_rgh));

      surfaceScalarField phig(-rhorAUf * ghf * fvc::snGrad(rho) * mesh.magSf());
      surfaceScalarField phiHbyA(
          "phiHbyA",
          fvc::flux(rho * HbyA) + MRF.zeroFilter(rhorAUf * fvc::ddtCorr(rho, U, phi)) + phig);

      MRF.makeRelative(fvc::interpolate(rho), phiHbyA);
      constrainPressure(p_rgh, rho, U, phiHbyA, rhorAUf, MRF);
      fvc::makeRelative(phiHbyA, rho, U);

      fvScalarMatrix p_rghDDtEqn(fvc::ddt(rho) + psi * correction(fvm::ddt(p_rgh)) +
                                  fvc::div(phiHbyA) == fvOptions(psi, p_rgh, rho.name()));

      while (pimple.correctNonOrthogonal())
      {
        fvScalarMatrix p_rghEqn(p_rghDDtEqn - fvm::laplacian(rhorAUf, p_rgh));
        p_rghEqn.setReference(pRefCell_,
                               isCompressible ? getRefCellValue(p_rgh, pRefCell_) : pRefValue_);
        p_rghEqn.solve(p_rgh.select(pimple.finalInnerIter()));

        if (pimple.finalNonOrthogonalIter())
        {
          phi = phiHbyA + p_rghEqn.flux();
          p_rgh.relax();
          U = HbyA + rAU * fvc::reconstruct((phig + p_rghEqn.flux()) / rhorAUf);
          U.correctBoundaryConditions();
          fvOptions.correct(U);
          K = 0.5 * magSqr(U);
        }
      }

      p = p_rgh + rho * gh;
      pressureControl_.limit(p);

      // Continuity correction
      if (!isCompressible)
      {
        if (p_rgh.needReference())
        {
          p += dimensionedScalar("p", p.dimensions(), pRefValue_ - getRefCellValue(p, pRefCell_));
        }
      }
      else
      {
        thermo.correctRho(psi * p - psip0, rhoMin_, rhoMax_);
        rho = thermo.rho();
        p_rgh = p - rho * gh;
        p_rgh.correctBoundaryConditions();
      }

      // rhoEqn for flux correction
      {
        (fvm::ddt(rho) + fvc::div(phi)).solve();
        rho = thermo.rho();
      }

      if (thermo.dpdt())
        dpdt = fvc::ddt(p);
    }

    if (pimple.turbCorr())
      turbulence.correct();
  }

  rho = thermo.rho();
}
