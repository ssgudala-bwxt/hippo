"""Test module for the unsteady 1D heat conduction problem"""

import unittest
from pathlib import Path

import fluidfoam as ff
import numpy as np
import pyvista as pv

# import Hippo test python functions
from read_hippo_data import get_foam_times, read_moose_exodus_data

RUN_DIR = Path(__file__).parent


class TestUnsteadyHeatConductionInInfiniteSystem(unittest.TestCase):
    """Test class for 1D unsteady heat conduction problem"""

    def test_solid_fixed_point(self):
        reader = pv.get_reader("main_out.e")
        times = reader.time_values

        for time in times:
            _, solid_temp = read_moose_exodus_data(RUN_DIR / "main_out.e", time, "T")
            _, solid_temp_ref = read_moose_exodus_data(
                RUN_DIR / "gold" / "main_out.e", time, "T"
            )

            # KNOWN LIMITATION: foam/system/fvSchemes uses
            # ddtSchemes { default CrankNicolson 1; }. Crank-Nicolson +
            # fixed-point iteration has a documented accuracy loss (see the
            # comment on removeOldTime() in include/mesh/FoamDataStore.h:
            # "Schemes known not to work: Crank-Nicolson. Current behaviour
            # does not clear the old time base field for CN even though this
            # would result in a small error compared to not using
            # fixed-point."). This causes a real (not roundoff) drift of the
            # coupled solid T field vs. the single-iteration gold/ reference,
            # so this assertion is expected to fail until that limitation is
            # addressed in Hippo's core fixed-point restore logic.
            np.testing.assert_allclose(
                solid_temp,
                solid_temp_ref,
                rtol=1e-8,
                atol=1e-8,
                err_msg="EXPECTED: known Crank-Nicolson + fixed-point limitation, "
                "see FoamDataStore.h removeOldTime() comment",
            )

    def test_fluid_fixed_point(self):
        times = get_foam_times("foam")
        for time in times:
            temp = ff.readof.readscalar("foam", f"{time:g}", "T", verbose=False)

            temp_ref = ff.readof.readscalar("gold", f"{time:g}", "T", verbose=False)

            np.testing.assert_allclose(temp, temp_ref, rtol=1e-8, atol=1e-8)
