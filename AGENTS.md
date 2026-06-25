If you have read this file, always type "Read AGENTS.md" in the chat response.

Always use "GitHub MCP Server" tool instead of "gh" when available and type "Using GitHub MCP Server", and if not available type "GitHub MCP Server not available, using gh instead.". Except for PRs, then use only the gh tool.

When running docker build or docker run, always check which CPU architecture the current development environment is using and run the build with that architecture instead of always on linux/amd64.
For example, if running on macOS with Apple Silicon, always run the docker build with `--platform=linux/arm64`.

Always run "./scripts/check_git_clean_after_build.sh" after the build to ensure the git working directory is clean, and if not, fix the problem with unclean git.

Always run "./scripts/format_code.sh" after the build to format the code, before committing.

Always use Conventional Commits for commit messages.

## Debian architecture compatibility

When changing the build system, Debian packaging, or Rust/C FFI code, preserve support for Debian release architectures including `ppc64el`, `riscv64`, and `s390x`.

Important checks:

- Do not restrict Debian binary packages to a narrow architecture list unless there is a confirmed technical reason. Shared library packages such as `libddccontrol0` should normally use `Architecture: any` if the programs that link against them use `Architecture: any`.
- Remember that `s390x` is big-endian. Do not add code that assumes little-endian byte order unless it is explicitly guarded and tested.
- Keep Rust/C FFI layouts portable:
  - Use `#[repr(C)]` for structs shared with C.
  - Use C ABI types such as `c_int`, `c_char`, `c_ushort`, and raw pointers at the boundary.
  - Do not expose Rust-native types such as `String`, `Vec`, `bool`, or Rust enums directly across the C ABI.
  - Allocate and free memory consistently on the same side of the ABI, or document the ownership rule clearly.
- Avoid architecture-specific Rust code unless required. If using `cfg(target_arch)`, include behavior for `powerpc64`, `riscv64`, and `s390x` or explain why they are unsupported.
- Be cautious when adding Rust crates with native dependencies, `build.rs`, `bindgen`, assembly, SIMD, OpenSSL, or other platform-sensitive components. These may break Debian non-x86 builds.
- Debian Rust builds must use Debian-acceptable dependencies. Prefer crates already packaged in Debian, or coordinate vendoring with the Debian maintainer.
- If Debian build logs fail on `ppc64el`, `riscv64`, or `s390x`, inspect the actual buildd logs before assuming the problem is in upstream code. Many failures are packaging/toolchain issues.
- After Debian packaging changes, verify that `dh_shlibdeps` can find all shared libraries from the produced binary packages on every supported architecture.
