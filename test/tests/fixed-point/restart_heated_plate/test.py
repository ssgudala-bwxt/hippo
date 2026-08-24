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
        ref_dir = "../heated_plate_converge/fluid-openfoam"

        boundaries = ["inlet", "outlet", "top", "slip-bottom", "bottom", "interface"]

        times = get_foam_times(case_dir, True)
        for time in times:
            # internal data
            temp = ff.readof.readscalar(case_dir, time, "T")
            temp_ref = ff.readof.readscalar(ref_dir, time, "T")
            # NOTE: gold/ was generated with the original OpenFOAM stack; the
            # ESI-migrated solidConductionTestSolver stack matches to ~10
            # significant digits but not bit-for-bit, so use a numeric
            # tolerance instead of exact equality.
            np.testing.assert_allclose(
                temp, temp_ref, rtol=1e-8, atol=1e-8, err_msg=f"time = {time}"
            )

            # boundary data
            for boundary in boundaries:
                temp = ff.readof.readscalar(case_dir, time, "T", boundary=boundary)
                temp_ref = ff.readof.readscalar(ref_dir, time, "T", boundary=boundary)
                np.testing.assert_allclose(
                    temp,
                    temp_ref,
                    rtol=1e-8,
                    atol=1e-8,
                    err_msg=f"time = {time}, boundary = {boundary}",
                )
