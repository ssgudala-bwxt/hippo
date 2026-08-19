# Build custom OpenFOAM source files: this should be improved in future
all: build_foam_tests

WMAKE ?= wmake
MAKEFLAGS += --no-print-directory

# postprocessorTestSolver requires libfluidThermophysicalTransportModels and
# libcompressibleMomentumTransportModels which are only present in full ESI
# installations that include the fluid-solver stack.  Skip it when absent.
FLUID_THERMO_TRANSPORT_LIB := $(wildcard $(WM_PROJECT_DIR)/platforms/$(WM_OPTIONS)/lib/libfluidThermophysicalTransportModels.so)
FLUID_THERMO_LIB := $(wildcard $(FOAM_LIBBIN)/libfluidThermophysicalModels.so)

build_foam_tests:
	$(info Building Hippo's OpenFOAM test modules)
	+@$(WMAKE) -s -j $(MOOSE_JOBS) test/OpenFOAM/modules/transferTestSolver/
	+@$(WMAKE) -s -j $(MOOSE_JOBS) test/OpenFOAM/modules/bcTestSolver/
	+@$(WMAKE) -s -j $(MOOSE_JOBS) test/OpenFOAM/modules/functionTestSolver/
	+@$(WMAKE) -s -j $(MOOSE_JOBS) test/OpenFOAM/modules/laplacianTestSolver/
	+@$(WMAKE) -s -j $(MOOSE_JOBS) test/OpenFOAM/modules/odeTestSolver/
ifneq ($(FLUID_THERMO_TRANSPORT_LIB),)
	+@$(WMAKE) -s -j $(MOOSE_JOBS) test/OpenFOAM/modules/postprocessorTestSolver/
else
	$(info Skipping postprocessorTestSolver: libfluidThermophysicalTransportModels not found)
endif
ifneq ($(FLUID_THERMO_LIB),)
	+@$(WMAKE) -s -j $(MOOSE_JOBS) test/OpenFOAM/modules/fluid/
else
	$(info Skipping fluid: libfluidThermophysicalModels.so not found)
endif
