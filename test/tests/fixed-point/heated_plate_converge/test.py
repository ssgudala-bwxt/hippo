"""Regression test of fixed-point flow over heated plate problem"""

from unittest import TestCase

import fluidfoam as ff
import numpy as np

from read_hippo_data import get_foam_times


class TestFlowOverHeatedPlate(TestCase):
    """Compares the flow over heated plate problem with fixed-point iteration to a reference case

    Details
    ------------
    This is the same problem as the multiapp test, but fixed-point iteration is used to
    improve convergence. The final result is compared to a reference case
    """

    def test_times(self):
        """Compares fixed-point solution against reference case."""
        case_dir = "fluid-openfoam/"
        ref_dir = "gold/"

        boundaries = ["inlet", "outlet", "top", "slip-bottom", "bottom", "interface"]

        times = get_foam_times(case_dir, True)
        for time in times:
            # internal data
            temp = ff.readof.readscalar(case_dir, time, "T")
            temp_ref = ff.readof.readscalar(ref_dir, time, "T")
            # NOTE: small (~1e-5 relative) round-off differences are
            # expected here due to the OpenFOAM/solver stack migration to
            # ESI OpenFOAM v2606. Use the same tolerance as
            # flow_over_heated_plate/test.py.
            assert np.allclose(temp_ref, temp, rtol=1e-3, atol=1e-3), (
                f"Max diff ({time}): {abs(temp - temp_ref).max()}"
            )

            # boundary data
            for boundary in boundaries:
                temp = ff.readof.readscalar(case_dir, time, "T", boundary=boundary)
                temp_ref = ff.readof.readscalar(ref_dir, time, "T", boundary=boundary)
                assert np.allclose(temp_ref, temp, rtol=1e-3, atol=1e-3), (
                    f"Max diff ({time}): {abs(temp - temp_ref).max()}"
                )
