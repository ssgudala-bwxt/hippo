# Build custom OpenFOAM source files: this should be improved in future
all: build_foam_tests

WMAKE ?= wmake
MAKEFLAGS += --no-print-directory

# fluid module requires libfluidThermophysicalModels which is only present in
# full ESI installations that include the fluid-solver stack. Skip it when absent.
FLUID_THERMO_LIB := $(wildcard $(FOAM_LIBBIN)/libfluidThermophysicalModels.so)

build_foam_tests:
	$(info Building Hippo's OpenFOAM test modules)
	+@$(WMAKE) -s -j $(MOOSE_JOBS) test/OpenFOAM/modules/transferTestSolver/
	+@$(WMAKE) -s -j $(MOOSE_JOBS) test/OpenFOAM/modules/bcTestSolver/
	+@$(WMAKE) -s -j $(MOOSE_JOBS) test/OpenFOAM/modules/functionTestSolver/
	+@$(WMAKE) -s -j $(MOOSE_JOBS) test/OpenFOAM/modules/laplacianTestSolver/
	+@$(WMAKE) -s -j $(MOOSE_JOBS) test/OpenFOAM/modules/odeTestSolver/
	+@$(WMAKE) -s -j $(MOOSE_JOBS) test/OpenFOAM/modules/postprocessorTestSolver/
ifneq ($(FLUID_THERMO_LIB),)
	+@$(WMAKE) -s -j $(MOOSE_JOBS) test/OpenFOAM/modules/fluid/
else
	$(info Skipping fluid: libfluidThermophysicalModels.so not found)
endif
