#!/usr/bin/env python3
"""Extract our release bundles, rejecting traversal, links and file collisions."""

import argparse
from pathlib import Path, PurePosixPath
import shutil
import tarfile


def extract(archive: Path, output: Path) -> None:
    with tarfile.open(archive) as bundle:
        for member in bundle:
            path = PurePosixPath(member.name)
            if (
                path.is_absolute()
                or ".." in path.parts
                or not path.parts
                or path.parts[0] not in {"debian", "rpm", "srpm", "provenance"}
                or not (member.isfile() or member.isdir())
            ):
                raise ValueError(f"Unsafe bundle member: {member.name}")
            destination = output.joinpath(*path.parts)
            if member.isdir():
                destination.mkdir(parents=True, exist_ok=True)
                continue
            destination.parent.mkdir(parents=True, exist_ok=True)
            if destination.exists():
                raise ValueError(f"Duplicate package path: {member.name}")
            with bundle.extractfile(member) as source, destination.open("xb") as target:
                shutil.copyfileobj(source, target)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("bundles", nargs="+", type=Path)
    args = parser.parse_args()
    for archive in args.bundles:
        extract(archive, args.output)
