# Distribution package versions

Snapshot checked on **2026-10-28**. This is a survey of the program
`ddccontrol` and the separate monitor database `ddccontrol-db`. The latter is
not `ddccontrol-dbgen`, the database build tool. Versions can change without a
project release; follow the linked package sources before making installation
or packaging decisions.

The table covers major distribution families and other repositories where a
package was found. It is not a claim to cover every Linux distribution or every
third-party repository. Entries are package versions, including distribution
revisions when available. Unless noted otherwise, Debian and Ubuntu entries are
for the main release repositories and amd64; the database package is
architecture-independent. Binary availability and revisions can differ by
architecture, repository mirror, enabled components, or update pocket.

The release year is the initial release of the named distribution version or
repository branch, not the date of a package update. `—` denotes a rolling
repository, a development branch without a final release, or an unversioned
recipe collection. Fedora 45 is listed as beta as of this snapshot.

| Distribution and repository | Release year | `ddccontrol` | `ddccontrol-db` | Source and notes |
| --- | --- | --- | --- | --- |
| Debian 11 bullseye | 2021 | `0.4.4-1.2` | `20190826-1` | [Debian package search](https://packages.debian.org/search?keywords=ddccontrol) |
| Debian 12 bookworm | 2023 | `0.6.1-1` | `20230223-1` | [Debian package search](https://packages.debian.org/search?keywords=ddccontrol) |
| Debian 13 trixie | 2025 | `1.0.3-1+b1` | `20250504-1` | [Debian package search](https://packages.debian.org/search?keywords=ddccontrol) |
| Debian testing (forky) | — | `3.4.0-1` | `20261015-1` | [Debian package search](https://packages.debian.org/search?keywords=ddccontrol) |
| Debian unstable (sid) | — | `3.4.0-1` | `20261015-1` | [Debian package search](https://packages.debian.org/search?keywords=ddccontrol); some Debian Ports architectures have older program versions |
| Raspberry Pi OS 13, 32-bit (Raspbian trixie) | 2025 | `1.0.3-1+b1` | `20250504-1` | [Raspbian armhf package index](https://archive.raspbian.org/raspbian/dists/trixie/main/binary-armhf/Packages.xz) |
| Kali Linux, kali-rolling | — | `3.4.0-1` | `20261015-1` | [Kali amd64 package index](https://kali.download/kali/dists/kali-rolling/main/binary-amd64/Packages.gz) |
| Ubuntu 22.04 LTS jammy, universe | 2022 | `0.6.0-8` | `20220216-1` | [Ubuntu amd64 package index](https://archive.ubuntu.com/ubuntu/dists/jammy/universe/binary-amd64/Packages.xz) |
| Ubuntu 24.04 LTS noble, universe | 2024 | `1.0.3-1build2` | `20240209-1` | [Ubuntu amd64 package index](https://archive.ubuntu.com/ubuntu/dists/noble/universe/binary-amd64/Packages.xz) |
| Ubuntu 26.04 LTS resolute, universe | 2026 | `1.0.3-1build4` | `20260120-1` | [Ubuntu amd64 package index](https://archive.ubuntu.com/ubuntu/dists/resolute/universe/binary-amd64/Packages.xz) |
| Ubuntu 26.10 stonking, universe | 2026 | `3.2.0-5` | `20260813-1` | [Ubuntu amd64 package index](https://archive.ubuntu.com/ubuntu/dists/stonking/universe/binary-amd64/Packages.xz) |
| Fedora 43 | 2025 | `1.0.3-5.fc43` | `20250504-2.fc43` | [Program](https://packages.fedoraproject.org/pkgs/ddccontrol/ddccontrol/) and [database](https://packages.fedoraproject.org/pkgs/ddccontrol-db/ddccontrol-db/), stable column |
| Fedora 44 | 2026 | `1.0.3-7.fc44` | `20260120-1.fc44` | Same Fedora sources, stable column |
| Fedora 45 | 2026 (beta) | `3.3.0-2.fc45` | `20260731-1.fc45` | Same Fedora sources, stable column |
| Fedora Rawhide | — | `3.4.1-1.fc46` | `20261026-1.fc46` | Same Fedora sources; development repository |
| Arch Linux, AUR | — | `3.4.1-2` | `20261026-1` | [Program](https://aur.archlinux.org/packages/ddccontrol), [database](https://aur.archlinux.org/packages/ddccontrol-db); community-maintained build recipes, not official binary packages |
| Gentoo | — | `1.0.3-r1` | `20240920` | [Program](https://packages.gentoo.org/packages/app-misc/ddccontrol), [database](https://packages.gentoo.org/packages/app-misc/ddccontrol-db); keyworded unstable on amd64, x86 and ppc |
| NixOS, nixos-unstable | — | `3.4.0` | `20261015` | Nixpkgs recipes: [program](https://github.com/NixOS/nixpkgs/blob/nixos-unstable/pkgs/by-name/dd/ddccontrol/package.nix), [database](https://github.com/NixOS/nixpkgs/blob/nixos-unstable/pkgs/by-name/dd/ddccontrol-db/package.nix); a recipe is not proof of a successful build on every architecture |
| ALT Linux p11 | 2024 | `0.6.0-alt1.20211005git8be298f` | `20231004-alt1_1` | [p11 source package index](https://download.basealt.ru/pub/distributions/ALTLinux/p11/branch/files/SRPMS/); source packages, binary availability not checked |
| T2 SDE | — | `3.4.0` | `20261015` | [Program](https://www.t2linux.com/packages/ddccontrol), [database](https://www.t2linux.com/packages/ddccontrol-db); build recipes, not a guarantee of prebuilt binaries |
| SUSE Package Hub 15 SP5 | 2023 | `0.4.2+20140105+git9d89d8c-bp155.3.14` | `20120905+git130da80-bp155.2.9` | [Program](https://packagehub.suse.com/packages/ddccontrol/), [database](https://packagehub.suse.com/packages/ddccontrol-db/); historical GA entries |
| openSUSE Tumbleweed, community repository `home:siegel` | — | Not verified | `20220629-7.ss1599.2` | [Community repository index](https://download.opensuse.org/repositories/home:/siegel/openSUSE_Tumbleweed/noarch/); not an official distribution package |

Release-year sources: [Debian releases](https://www.debian.org/releases/),
[Raspberry Pi OS Trixie](https://www.raspberrypi.com/news/tag/raspberry-pi-os/),
[Ubuntu releases](https://wiki.ubuntu.com/Releases),
[Fedora releases](https://docs.fedoraproject.org/en-US/releases/),
[Fedora 45 beta announcement](https://fedoramagazine.org/announcing-fedora-linux-45-beta/),
[ALT Linux p11 announcement](https://lists.altlinux.org/pipermail/altlinux-announce-en/2024/000039.html),
and [SUSE lifecycle](https://www.suse.com/lifecycle/).

## Interpretation and gaps

- Fedora's GTK interface is packaged separately as `ddccontrol-gtk`; Debian and
  Ubuntu call it `gddccontrol`. Those names are not database versions.
- Debian's package search shows architecture-specific differences: for example,
  trixie's program package is listed for amd64, armel, armhf and i386, while
  forky also lists ppc64el, riscv64 and s390x. Check the source for the actual
  release and architecture instead of extrapolating from amd64.
- The [Arch official package search](https://archlinux.org/packages/?q=ddccontrol)
  returned no matching packages when checked; the AUR results above are separate.
  The [Solus Polaris package index](https://cdn.getsol.us/repo/polaris/eopkg-index.xml.xz)
  contained neither package name. These observations apply only to the
  repositories checked.
- The [openSUSE package page](https://software.opensuse.org/package/ddccontrol)
  reports no official package for Leap 16.0. Current Tumbleweed and Leap database
  versions were not independently confirmed; the community repository and older
  SUSE Package Hub entries above do not establish current official availability.
- Linux Mint, elementary OS, Pop!_OS, Alpine, Void,
  Mageia, openMandriva, Slackware and other derivatives or independent
  distributions need separate repository and release checks. Do not assume that
  a derivative has the exact version of its parent; pinning, updates and enabled
  repositories can differ. No package version is asserted for them here.
- Ubuntu rows were checked against the live amd64 universe package indexes;
  updates and other architectures may have different versions.

The project also publishes separate upstream package repositories; see
[package repository installation and release setup](releasing-packages.md).
Those packages are not counted as distribution-published packages in this table.
