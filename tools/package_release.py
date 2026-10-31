"""Build the release archives: a binary zip and a source tarball.

Run from anywhere: python tools/package_release.py
The version is read from Version.h so the archive names cannot drift from the
binaries they contain.

The source tarball exists because the archives GitHub generates leave
submodules out, and xy-VSSppf64.dll is compiled from one. This one carries the
xy-VSFilter tree inside it.
"""

import argparse
import hashlib
import io
import os
import re
import subprocess
import sys
import tarfile
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SUB = os.path.join(ROOT, "misc", "xy-VSFilter", "src")

BINARIES = ["Spp2Pgs64.exe", "xy-VSSppf64.dll"]
DOCUMENTS = ["README.md", "README.zh.md", "CHANGELOG.md", "CHANGELOG.zh.md"]
# (path in the repository, name inside the archive)
LICENSES = [
    ("License.txt", "License.txt"),
    ("misc/xy-VSFilter/src/License.txt", "xy-VSFilter-license.txt"),
]


def version():
    with open(os.path.join(ROOT, "Version.h"), encoding="ascii") as f:
        text = f.read()
    parts = []
    for key in ("MAJOR", "MINOR", "PATCH"):
        m = re.search(r"SPP2PGS_VERSION_%s (\d+)" % key, text)
        if not m:
            raise SystemExit("Version.h: SPP2PGS_VERSION_%s not found" % key)
        parts.append(m.group(1))
    return ".".join(parts)


def build_zip(ver, out):
    entries = [(os.path.join("bin", name), name) for name in BINARIES]
    entries += [(name, name) for name in DOCUMENTS]
    entries += LICENSES
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for src, name in entries:
            path = os.path.join(ROOT, src)
            if not os.path.exists(path):
                raise SystemExit("missing: " + path)
            z.write(path, name)
    return len(entries)


def git_archive(cwd, prefix):
    if not os.path.isdir(os.path.join(cwd, ".git")) and not os.path.isfile(
        os.path.join(cwd, ".git")
    ):
        raise SystemExit("not a git checkout: " + cwd)
    result = subprocess.run(
        ["git", "archive", "--format=tar", "--prefix=" + prefix, "HEAD"],
        cwd=cwd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if result.returncode != 0:
        raise SystemExit(
            "git archive failed in %s: %s"
            % (cwd, result.stderr.decode("utf-8", "replace").strip())
        )
    return result.stdout


def build_source(ver, out):
    root_name = "Spp2Pgs-%s" % ver
    blobs = [
        git_archive(ROOT, root_name + "/"),
        git_archive(SUB, root_name + "/misc/xy-VSFilter/src/"),
    ]
    count = 0
    with tarfile.open(out, "w:gz", compresslevel=9) as dst:
        for blob in blobs:
            with tarfile.open(fileobj=io.BytesIO(blob)) as src:
                for member in src.getmembers():
                    handle = src.extractfile(member) if member.isreg() else None
                    dst.addfile(member, handle)
                    count += 1
    return count


def report(path, label, count):
    size = os.path.getsize(path)
    digest = hashlib.sha256(open(path, "rb").read()).hexdigest()
    print("%-6s : %s" % (label, os.path.relpath(path, ROOT)))
    print("         {:,} bytes, {} entries".format(size, count))
    print("         sha256 " + digest)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", default=os.path.join(ROOT, "artifacts"))
    parser.add_argument(
        "--expect-tag",
        default=None,
        help="fail unless the tag matches Version.h, e.g. 0.10.0",
    )
    args = parser.parse_args()

    ver = version()
    if args.expect_tag is not None and args.expect_tag != ver:
        raise SystemExit(
            "Version.h says %s but the tag is %s" % (ver, args.expect_tag)
        )

    os.makedirs(args.out, exist_ok=True)
    zip_path = os.path.join(args.out, "Spp2Pgs-%s-x64.zip" % ver)
    src_path = os.path.join(args.out, "Spp2Pgs-%s-src.tar.gz" % ver)

    report(zip_path, "zip", build_zip(ver, zip_path))
    report(src_path, "source", build_source(ver, src_path))
    print("version:", ver)
    return 0


if __name__ == "__main__":
    sys.exit(main())
