use ddccontrol_db_format::{self as format, encode, field, map, text};
use ddccontrol_dbgen::{convert, diagnostic, is_xml_path, read_bounded, source_paths, summary};
use std::{
    env,
    ffi::OsString,
    fs::{self, OpenOptions},
    io::Write,
    path::{Path, PathBuf},
    sync::atomic::{AtomicUsize, Ordering},
};

const HELP: &str = "ddccontrol-dbgen: deterministic offline database tooling\n\nUsage:\n  ddccontrol-dbgen convert DIRECTORY OUTPUT [--revision REV] [--snapshot PATH]\n  ddccontrol-dbgen validate INPUT\n  ddccontrol-dbgen dump INPUT\n  ddccontrol-dbgen rewrite INPUT OUTPUT [--snapshot PATH]\n  ddccontrol-dbgen --version\n\nXML remains the maintained source. Unknown CBOR fields survive rewrite.\n";

fn atomic_write(path: &Path, bytes: &[u8]) -> Result<(), String> {
    static NEXT: AtomicUsize = AtomicUsize::new(0);
    let parent = path
        .parent()
        .filter(|p| !p.as_os_str().is_empty())
        .unwrap_or(Path::new("."));
    let name = path.file_name().ok_or("output path requires filename")?;
    let mut temporary = OsString::from(".");
    temporary.push(name);
    temporary.push(format!(
        ".{}.{}",
        std::process::id(),
        NEXT.fetch_add(1, Ordering::Relaxed)
    ));
    let temporary = parent.join(temporary);
    let mut options = OpenOptions::new();
    options.write(true).create_new(true);
    #[cfg(unix)]
    {
        use std::os::unix::fs::OpenOptionsExt;
        options.mode(0o644);
    }
    let mut file = options
        .open(&temporary)
        .map_err(|e| format!("{}: {e}", temporary.display()))?;
    let result = (|| {
        file.write_all(bytes)?;
        file.sync_all()?;
        drop(file);
        fs::rename(&temporary, path)
    })()
    .map_err(|e| format!("{}: {e}", path.display()));
    let _ = fs::remove_file(temporary);
    result
}

fn resolved(path: &Path) -> Option<PathBuf> {
    fs::canonicalize(path).ok().or_else(|| {
        let parent = path
            .parent()
            .filter(|p| !p.as_os_str().is_empty())
            .unwrap_or(Path::new("."));
        Some(fs::canonicalize(parent).ok()?.join(path.file_name()?))
    })
}
fn same_file(a: &Path, b: &Path) -> bool {
    a == b || resolved(a).zip(resolved(b)).is_some_and(|(a, b)| a == b)
}

fn run() -> Result<(), String> {
    let mut args = env::args_os().skip(1);
    let command = args
        .next()
        .ok_or(HELP)?
        .into_string()
        .map_err(|_| "command must be UTF-8")?;
    if command == "--help" || command == "-h" {
        print!("{HELP}");
        return Ok(());
    }
    if command == "--version" {
        println!("ddccontrol-dbgen {}", env!("CARGO_PKG_VERSION"));
        return Ok(());
    }
    if !["convert", "validate", "dump", "rewrite"].contains(&command.as_str()) {
        return Err(HELP.into());
    }
    let input = PathBuf::from(args.next().ok_or(HELP)?);
    let output = if command == "convert" || command == "rewrite" {
        Some(PathBuf::from(args.next().ok_or(HELP)?))
    } else {
        None
    };
    let mut revision = None;
    let mut snapshot: Option<PathBuf> = None;
    while let Some(option) = args.next() {
        if option == "--revision" && command == "convert" && revision.is_none() {
            revision = Some(
                args.next()
                    .ok_or("--revision needs a value")?
                    .into_string()
                    .map_err(|_| "revision must be UTF-8")?,
            );
        } else if option == "--snapshot" && output.is_some() && snapshot.is_none() {
            snapshot = Some(PathBuf::from(args.next().ok_or("--snapshot needs a path")?));
        } else {
            return Err(format!("unexpected argument: {}", option.to_string_lossy()));
        }
    }
    if let Some(output) = &output {
        if command == "rewrite" && same_file(&input, output) {
            return Err("rewrite requires an output distinct from input".into());
        }
        if snapshot
            .as_ref()
            .is_some_and(|s| same_file(output, s) || (command == "rewrite" && same_file(&input, s)))
        {
            return Err("input, output and snapshot paths must be distinct".into());
        }
        if command == "convert" {
            // Failed generation removes stale artifacts; never apply cleanup to XML sources.
            let inputs: Vec<_> = source_paths(&input)
                .into_iter()
                .filter_map(Result::ok)
                .collect();
            for destination in [Some(output), snapshot.as_ref()].into_iter().flatten() {
                let canonical = resolved(destination);
                let inside_source = canonical.as_ref().is_some_and(|p| {
                    is_xml_path(p)
                        && [&input, &input.join("monitor")]
                            .into_iter()
                            .any(|directory| {
                                fs::canonicalize(directory).is_ok_and(|d| p.starts_with(d))
                            })
                });
                if inside_source || inputs.iter().any(|path| same_file(destination, path)) {
                    return Err("output must not overwrite source XML".into());
                }
            }
        }
    }
    let result = (|| {
        let root = if command == "convert" {
            convert(&input, revision.as_deref())?
        } else {
            format::validate(&read_bounded(&input)?)?
        };
        let bytes = encode(&root)?;
        if let Some(output) = &output {
            if let Some(snapshot) = &snapshot {
                atomic_write(snapshot, &encode(&format::fallback_manifest(&root)?)?)?;
            }
            atomic_write(output, &bytes)?;
        }
        match command.as_str() {
            "convert" => println!("{}", summary(&root, bytes.len())?),
            "dump" => print!("{}", diagnostic(&root)?),
            "validate" => println!(
                "valid candidate-v1: {} profiles, revision {}",
                map(field(&root, 6)?)?.len(),
                text(field(&root, 2)?)?
            ),
            _ => {}
        }
        Ok(())
    })();
    // Conversion must invalidate stale package artifacts. A failed rewrite
    // must not delete preexisting destinations when its input cannot be read.
    if command == "convert" && result.is_err() {
        for path in [output.as_ref(), snapshot.as_ref()].into_iter().flatten() {
            let _ = fs::remove_file(path);
        }
    }
    result
}
fn main() {
    if let Err(error) = run() {
        eprintln!("ddccontrol-dbgen: {error}");
        std::process::exit(1);
    }
}
