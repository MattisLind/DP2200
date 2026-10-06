import unittest
from disk_diagnostics import run_diagnostic


class DiskDiagnosticsTests(unittest.TestCase):
    def test_md370_v11(self):
        run_diagnostic("MD370_V1.1")

    def test_md370_v12(self):
        run_diagnostic("MD370_V1.2")

    def test_md374(self):
        run_diagnostic("MD374_V1.1")

    def test_ma374(self):
        run_diagnostic("MA374_V1.2.B")

    def test_sv374_full(self):
        run_diagnostic("SV374_V1.1")

    def test_sv374_read_only(self):
        run_diagnostic("SV374_V1.1", full_surface=False)
