from unittest import TestCase

import fluidfoam as ff
import numpy as np

from read_hippo_data import get_foam_times


class TestUnsteadyFixedPointConverge(TestCase):
    """Regression test of the 1D unsteady heat conduction problem using fixed-point to improve convergence."""

    def test_times(self):
        """Compares fixed-point solution against non-fixed-point solution."""
        case_dir = "foam/"
        ref_dir = "gold/"

        boundaries = ["left", "right", "top", "bottom", "front", "back"]

        times = get_foam_times(case_dir, True)
        for time in times:
            # internal data
            T = ff.readof.readscalar(case_dir, time, "T")
            T_ref = ff.readof.readscalar(ref_dir, time, "T")
            # NOTE: gold/ was generated with the original OpenFOAM "solid"
            # solver, which no longer exists in ESI OpenFOAM. Since the
            # migration to solidConductionTestSolver, the solution matches to
            # ~13 significant digits but not bit-for-bit (different linear
            # solver/algorithm round-off), so use a tight numeric tolerance
            # instead of exact equality.
            np.testing.assert_allclose(T, T_ref, rtol=1e-8, atol=1e-8)

            # boundary data
            for boundary in boundaries:
                T = ff.readof.readscalar(case_dir, time, "T", boundary=boundary)
                T_ref = ff.readof.readscalar(ref_dir, time, "T", boundary=boundary)
                np.testing.assert_allclose(T, T_ref, rtol=1e-8, atol=1e-8)
