#!/usr/bin/env python3
"""Check recipe invalidation and image selection without downloading packages."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class BuildImages(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for directory in (".github/ci", ".github/build-images", "scripts/release"):
            shutil.copytree(ROOT / directory, self.root / directory)
        self.script = self.root / "scripts/release/build-image.sh"
        self.bin = self.root / "bin"
        self.bin.mkdir()
        self.log = self.root / "docker.log"
        docker = self.bin / "docker"
        docker.write_text('''#!/usr/bin/env bash
printf '%s\\n' "$*" >> "$DOCKER_LOG"
if [[ "$1" == pull && "${PULL_FAIL:-0}" == 1 ]]; then exit 1; fi
''')
        docker.chmod(0o755)
        self.env = dict(os.environ, PATH=f"{self.bin}:{os.environ['PATH']}",
                        DOCKER_LOG=str(self.log))

    def call(self, *args, **kwargs):
        return subprocess.run([str(self.script), *args], env=self.env,
                              text=True, capture_output=True, check=True, **kwargs).stdout.strip()

    def test_source_key_covers_dockerfile_and_installer(self):
        original = self.call("ref", "source", "linux/arm64")
        self.assertEqual(original, self.call("ref", "source", "linux/arm64"))
        for name in ("Dockerfile", "install-rust.sh"):
            path = self.root / ".github/ci" / name
            before = path.read_text()
            path.write_text(before + "\n# changed dependency recipe\n")
            self.assertNotEqual(original, self.call("ref", "source", "linux/arm64"))
            path.write_text(before)

    def test_packaging_key_covers_spec(self):
        packaging = self.root / "packaging"
        packaging.mkdir()
        spec = packaging / "ddccontrol.spec"
        spec.write_text("BuildRequires: gcc\n")
        original = self.call("ref", "fedora", "linux/arm64", str(packaging))
        spec.write_text("BuildRequires: gcc rust\n")
        self.assertNotEqual(original, self.call("ref", "fedora", "linux/arm64", str(packaging)))

    def test_platform_tags_preserve_all_debian_architectures(self):
        for platform, arch in (("linux/amd64", "amd64"), ("linux/arm64", "arm64"),
                               ("linux/arm/v7", "armhf"), ("linux/ppc64le", "ppc64el"),
                               ("linux/riscv64", "riscv64"), ("linux/s390x", "s390x")):
            self.assertTrue(self.call("ref", "source", platform).endswith("-" + arch))

    def test_pull_hit_does_not_build(self):
        image = self.call("ensure", "source", "linux/arm64")
        self.assertEqual(self.log.read_text().splitlines(), [f"pull --platform linux/arm64 {image}"])

    def test_missing_image_builds_for_the_requested_architecture(self):
        self.env["PULL_FAIL"] = "1"
        image = self.call("ensure", "source", "linux/arm64")
        calls = self.log.read_text().splitlines()
        self.assertEqual(len(calls), 2)
        self.assertTrue(calls[1].startswith(f"build --platform linux/arm64 --tag {image} "))

    def test_publisher_refreshes_os_packages_and_pushes_matching_tag(self):
        image = self.call("publish", "source", "linux/arm64")
        call = self.log.read_text()
        self.assertIn("buildx build --platform linux/arm64 --pull", call)
        self.assertIn(f"--tag {image} --push", call)

    def test_wrapper_forwards_stdin_and_artifact_mounts(self):
        self.env.update(GITHUB_WORKSPACE=str(self.root), RUNNER_TEMP=str(self.root / "tmp"))
        subprocess.run([str(self.root / "scripts/release/run-build-image.sh"),
                        "source", "linux/arm64", "bash", "-se"],
                       env=self.env, cwd=self.root, input="echo test\n", text=True,
                       capture_output=True, check=True)
        call = self.log.read_text().splitlines()[-1]
        self.assertIn("run --rm --interactive --platform linux/arm64", call)
        self.assertIn(f"-v {self.root}:{self.root}", call)
        self.assertIn("-e GITHUB_OUTPUT", call)
        self.assertTrue(call.endswith("bash -se"))


if __name__ == "__main__":
    unittest.main()
