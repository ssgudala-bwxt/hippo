# Build custom OpenFOAM source files: this should be improved in future
all: build_foam_tests

WMAKE ?= wmake
MAKEFLAGS += --no-print-directory

build_foam_tests:
	$(info Building Hippo's OpenFOAM test modules)
	+@$(WMAKE) -s -j $(MOOSE_JOBS) test/OpenFOAM/modules/transferTestSolver/
	+@$(WMAKE) -s -j $(MOOSE_JOBS) test/OpenFOAM/modules/bcTestSolver/
	+@$(WMAKE) -s -j $(MOOSE_JOBS) test/OpenFOAM/modules/functionTestSolver/
	+@$(WMAKE) -s -j $(MOOSE_JOBS) test/OpenFOAM/modules/laplacianTestSolver/
	+@$(WMAKE) -s -j $(MOOSE_JOBS) test/OpenFOAM/modules/odeTestSolver/
	+@if [ -f "$$FOAM_LIBBIN/libfluid.so" ] && \
	     [ -f "$$FOAM_LIBBIN/libfluidSolver.so" ] && \
	     [ -f "$$FOAM_LIBBIN/libisothermalFluid.so" ] && \
	     [ -f "$$FOAM_LIBBIN/libcompressibleMomentumTransportModels.so" ]; then \
	       $(WMAKE) -s -j $(MOOSE_JOBS) test/OpenFOAM/modules/postprocessorTestSolver/; \
	   else \
	       echo "Skipping postprocessorTestSolver (missing ESI module libs in $$FOAM_LIBBIN)"; \
	   fi
