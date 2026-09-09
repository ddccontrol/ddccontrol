#!/usr/bin/env python3
"""Check the extraction boundary used with downloaded release assets."""

import importlib.util
import io
from pathlib import Path
import tarfile
import tempfile
import unittest

spec = importlib.util.spec_from_file_location(
    "bundles", Path(__file__).with_name("extract-bundles.py")
)
bundles = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bundles)


class BundleTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.archive = self.root / "bundle.tar.gz"
        self.output = self.root / "output"

    def archive_member(self, name, kind=tarfile.REGTYPE):
        with tarfile.open(self.archive, "w:gz") as archive:
            member = tarfile.TarInfo(name)
            member.type = kind
            member.linkname = "../../outside"
            member.size = 3 if kind == tarfile.REGTYPE else 0
            archive.addfile(member, io.BytesIO(b"deb"))

    def test_extracts_package(self):
        name = "debian/trixie/amd64/ddccontrol_3.3.0_amd64.deb"
        self.archive_member(name)
        bundles.extract(self.archive, self.output)
        self.assertEqual((self.output / name).read_bytes(), b"deb")

    def test_rejects_traversal_and_unexpected_roots(self):
        for name in ("/tmp/outside", "../outside", "debian/../../outside", "index.html"):
            with self.subTest(name=name):
                self.archive_member(name)
                with self.assertRaises(ValueError):
                    bundles.extract(self.archive, self.output)

    def test_rejects_links(self):
        for kind in (tarfile.SYMTYPE, tarfile.LNKTYPE):
            with self.subTest(kind=kind):
                self.archive_member("debian/link", kind)
                with self.assertRaises(ValueError):
                    bundles.extract(self.archive, self.output)

    def test_rejects_colliding_release_assets(self):
        self.archive_member("debian/package.deb")
        bundles.extract(self.archive, self.output)
        with self.assertRaises(ValueError):
            bundles.extract(self.archive, self.output)


if __name__ == "__main__":
    unittest.main()
