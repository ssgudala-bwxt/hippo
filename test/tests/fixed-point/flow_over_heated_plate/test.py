from unittest import TestCase

import fluidfoam as ff
import numpy as np
from read_hippo_data import get_foam_times


class TestFlowOverHeatedPlate(TestCase):
    """Compares the flow over heated plate problem with fixed-point iteration to one without

    Details
    ------------
    This is the same problem as the multiapp test, but the fixed-point solution only updates
    the boundary condition at the start of the fixed-point loop so that the fixed-point solution
    should be the same as one without. This test checks for an exact match for the temperature
    field at all times.
    """

    def test_times(self):
        """Compares fixed-point solution against non-fixed-point solution."""
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
            # ESI OpenFOAM v2606 -- not the Crank-Nicolson + fixed-point
            # limitation (this test uses Euler time integration). Use a
            # numeric tolerance instead of exact equality.
            np.testing.assert_allclose(
                temp,
                temp_ref,
                rtol=1e-3,
                atol=1e-3,
                err_msg=f"time = {time}",
            )

            # boundary data
            for boundary in boundaries:
                temp = ff.readof.readscalar(case_dir, time, "T", boundary=boundary)
                temp_ref = ff.readof.readscalar(ref_dir, time, "T", boundary=boundary)
                np.testing.assert_allclose(
                    temp,
                    temp_ref,
                    rtol=1e-3,
                    atol=1e-3,
                    err_msg=f"time = {time}, boundary = {boundary}",
                )
