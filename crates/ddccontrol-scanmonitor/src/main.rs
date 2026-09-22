// Copyright(c) 2004-2026 DDCcontrol authors and contributors (see AUTHORS and CONTRIBUTORS)

mod backend;
mod xml;

use std::collections::{BTreeMap, BTreeSet};
use std::env;
use std::ffi::OsString;
use std::fs::{self, OpenOptions};
use std::io::{self, IsTerminal, Write};
use std::path::{Path, PathBuf};
use std::process::ExitCode;

use backend::{Backend, Monitor};
use ddccontrol_caps::Caps;

const HELP: &str = "ddccontrol-scanmonitor - create a monitor database XML file

Usage: ddccontrol-scanmonitor [OPTIONS] [dev:/dev/i2c-N]

With no device, finds connected monitors and asks you to choose if needed.
Writes PNPID.xml in the current directory, ready to edit and test.

Options:
  -o, --output FILE   Write to FILE instead (never overwrites existing files)
  -b, --db-path DIR   Read control definitions from DIR/options.xml
      --list          List available monitors and exit
  -h, --help          Show this help
  -V, --version       Show version

Requires the ddccontrol system D-Bus service and ddccontrol-db.
Reads capabilities and control values; does not set control values.
Review the XML comments, test your controls, then submit the XML to
https://github.com/ddccontrol/ddccontrol-db.
";

#[derive(Debug, Default, PartialEq)]
struct Args {
    device: Option<String>,
    output: Option<PathBuf>,
    db_path: Option<PathBuf>,
    list: bool,
    help: bool,
    version: bool,
}

fn parse_args(args: impl IntoIterator<Item = OsString>) -> Result<Args, String> {
    let mut parsed = Args::default();
    let mut args = args.into_iter();
    while let Some(arg) = args.next() {
        match arg.to_str() {
            Some("-h" | "--help") => parsed.help = true,
            Some("-V" | "--version") => parsed.version = true,
            Some("--list") => parsed.list = true,
            Some("-o" | "--output" | "-b" | "--db-path") => {
                let value = args
                    .next()
                    .filter(|v| !v.is_empty())
                    .ok_or_else(|| format!("{} requires a path", arg.to_string_lossy()))?;
                let slot = if arg == "-o" || arg == "--output" {
                    &mut parsed.output
                } else {
                    &mut parsed.db_path
                };
                if slot.replace(value.into()).is_some() {
                    return Err(format!(
                        "{} specified more than once",
                        arg.to_string_lossy()
                    ));
                }
            }
            Some(device) if device.starts_with("dev:/dev/") => {
                if parsed.device.replace(device.to_string()).is_some() {
                    return Err("select only one monitor at a time".into());
                }
            }
            _ => {
                return Err(format!(
                    "unknown argument: {}. Try --help",
                    arg.to_string_lossy()
                ))
            }
        }
    }
    if parsed.list && (parsed.device.is_some() || parsed.output.is_some()) {
        return Err("--list cannot be combined with a device or --output".into());
    }
    Ok(parsed)
}

fn database_path(explicit: Option<PathBuf>) -> Result<PathBuf, String> {
    if let Some(path) = explicit {
        return Ok(path);
    }
    if let Some(path) = option_env!("DDCONTROL_DATADIR") {
        return Ok(path.into());
    }
    ["/usr/share/ddccontrol-db", "/usr/local/share/ddccontrol-db"]
        .into_iter()
        .map(PathBuf::from)
        .find(|path| path.join("options.xml").is_file())
        .ok_or_else(|| "cannot find ddccontrol-db; install it or use --db-path DIR".into())
}

fn print_monitors(monitors: &[Monitor], output: &mut impl Write) -> io::Result<()> {
    for (index, monitor) in monitors.iter().enumerate() {
        writeln!(
            output,
            "  {}. {} ({})",
            index + 1,
            monitor.name,
            monitor.device
        )?;
    }
    Ok(())
}

fn select_monitor(monitors: &[Monitor], choice: &str) -> Result<usize, String> {
    choice
        .trim()
        .parse::<usize>()
        .ok()
        .filter(|index| *index > 0 && *index <= monitors.len())
        .map(|index| index - 1)
        .ok_or_else(|| format!("enter a monitor number from 1 to {}", monitors.len()))
}

fn choose_device(backend: &Backend) -> Result<String, String> {
    eprintln!("Looking for monitors...");
    let monitors = backend.list()?;
    match monitors.as_slice() {
        [] => Err("no DDC/CI monitors found; enable DDC/CI in the monitor menu and check the connection and i2c-dev module".into()),
        [monitor] => Ok(monitor.device.clone()),
        _ => {
            print_monitors(&monitors, &mut io::stderr()).map_err(|e| e.to_string())?;
            if !io::stdin().is_terminal() {
                return Err("more than one monitor found; pass a device from the list, for example ddccontrol-scanmonitor dev:/dev/i2c-4".into());
            }
            loop {
                eprint!("Monitor number: ");
                io::stderr().flush().map_err(|e| e.to_string())?;
                let mut choice = String::new();
                if io::stdin().read_line(&mut choice).map_err(|e| e.to_string())? == 0 {
                    return Err("no monitor selected".into());
                }
                match select_monitor(&monitors, &choice) {
                    Ok(index) => return Ok(monitors[index].device.clone()),
                    Err(error) => eprintln!("{error}"),
                }
            }
        }
    }
}

fn write_new(path: &Path, contents: &str) -> Result<(), String> {
    let mut file = OpenOptions::new()
        .write(true)
        .create_new(true)
        .open(path)
        .map_err(|e| {
            if e.kind() == io::ErrorKind::AlreadyExists {
                format!(
                    "{} already exists; choose another --output file",
                    path.display()
                )
            } else {
                format!("cannot create {}: {e}", path.display())
            }
        })?;
    if let Err(error) = file
        .write_all(contents.as_bytes())
        .and_then(|_| file.sync_all())
    {
        // This invocation created the file; remove incomplete output on failure.
        drop(file);
        let _ = fs::remove_file(path);
        return Err(format!("cannot write {}: {error}", path.display()));
    }
    Ok(())
}

fn run(args: Args) -> Result<(), String> {
    if args.help {
        print!("{HELP}");
        return Ok(());
    }
    if args.version {
        println!("ddccontrol-scanmonitor {}", env!("CARGO_PKG_VERSION"));
        return Ok(());
    }
    if args.list {
        let monitors = Backend::connect()?.list()?;
        if monitors.is_empty() {
            return Err("no DDC/CI monitors found".into());
        }
        return print_monitors(&monitors, &mut io::stdout()).map_err(|e| e.to_string());
    }
    let custom_database = args.db_path.is_some();
    let db_path = database_path(args.db_path)?;
    let database = ddccontrol_db::options::load(&db_path)?;
    let options = xml::index_options(&database)?;
    let candidates: Vec<u8> = options.keys().copied().collect();
    let backend = Backend::connect()?;
    let device = match args.device {
        Some(device) => device,
        None => choose_device(&backend)?,
    };
    eprintln!("Reading monitor identification and capabilities from {device}...");
    let opened = backend.open(&device)?;
    let output = args
        .output
        .unwrap_or_else(|| PathBuf::from(format!("{}.xml", opened.pnp_id)));
    if output.symlink_metadata().is_ok() {
        return Err(format!(
            "{} already exists; choose another --output file",
            output.display()
        ));
    }
    let caps = match Caps::parse(&opened.capabilities) {
        Ok(caps) => caps,
        Err(error) => {
            eprintln!("Could not parse capabilities ({error}); trying known control addresses.");
            Caps::default()
        }
    };
    let codes: BTreeSet<u8> = caps
        .vcp_codes()
        .chain(candidates)
        .filter(|code| *code < 0xe0)
        .collect();
    let mut readings = BTreeMap::new();
    eprintln!(
        "Reading {} control addresses. This can take a few minutes...",
        codes.len()
    );
    for (index, code) in codes.iter().copied().enumerate() {
        eprint!(
            "\rReading control {}/{} (0x{code:02x})...",
            index + 1,
            codes.len()
        );
        match backend.read(&device, code) {
            Ok(Some(reading)) => {
                readings.insert(code, reading);
            }
            Ok(None) => {}
            Err(error) => eprintln!("\nControl 0x{code:02x}: {error}"),
        }
    }
    eprintln!();
    if readings.is_empty() {
        return Err(
            "no readable controls found; no XML written. Check that DDC/CI is enabled and retry"
                .into(),
        );
    }
    let scan = xml::Scan {
        pnp_id: opened.pnp_id,
        name: opened
            .name
            .filter(|name| !name.trim().is_empty())
            .unwrap_or_else(|| "Unknown monitor (edit this name)".into()),
        caps,
        readings,
    };
    let document = xml::generate(&scan, &options);
    write_new(&output, &document)?;
    println!(
        "Created {} for {} ({}).",
        output.display(),
        scan.name,
        scan.pnp_id
    );
    println!("Review the XML comments and test the enabled controls.");
    println!("Preview with DDCCONTROL_NO_DAEMON=1 gddccontrol --monitor-file FILE");
    println!(
        "or DDCCONTROL_NO_DAEMON=1 ddccontrol --monitor-file FILE (requires I2C permissions)."
    );
    println!("Keep the filename {}.xml; relaunch after editing. No service restart is needed for preview.", scan.pnp_id);
    println!(
        "For permanent installation, copy it to {}.",
        db_path
            .join("monitor")
            .join(format!("{}.xml", scan.pnp_id))
            .display()
    );
    println!("Restart ddccontrol.service after changing the installed database.");
    if custom_database {
        println!("--db-path selects the XML definitions only; use the same database with the preview application's -b option.");
    }
    println!("Once tested, submit the XML to https://github.com/ddccontrol/ddccontrol-db.");
    Ok(())
}

fn main() -> ExitCode {
    match parse_args(env::args_os().skip(1)).and_then(run) {
        Ok(()) => ExitCode::SUCCESS,
        Err(error) => {
            eprintln!("ddccontrol-scanmonitor: {error}");
            ExitCode::FAILURE
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn args(values: &[&str]) -> Result<Args, String> {
        parse_args(values.iter().map(OsString::from))
    }

    #[test]
    fn arguments_and_missing_values() {
        assert_eq!(args(&[]).unwrap(), Args::default());
        let parsed = args(&[
            "--output",
            "profile.xml",
            "--db-path",
            "/tmp/db",
            "dev:/dev/i2c-4",
        ])
        .unwrap();
        assert_eq!(parsed.output, Some("profile.xml".into()));
        assert_eq!(parsed.db_path, Some("/tmp/db".into()));
        assert_eq!(parsed.device.as_deref(), Some("dev:/dev/i2c-4"));
        for input in [
            &["--output"][..],
            &["--db-path"],
            &["--unknown"],
            &["--list", "--output", "foo"],
            &["dev:/dev/i2c-1", "dev:/dev/i2c-2"],
        ] {
            assert!(args(input).is_err(), "{input:?}");
        }
    }

    #[test]
    fn selection_requires_valid_one_based_number() {
        let monitors = vec![Monitor {
            device: "dev:/dev/i2c-4".into(),
            name: "Display".into(),
        }];
        assert_eq!(select_monitor(&monitors, "1\n").unwrap(), 0);
        for choice in ["0", "2", "-1", "", "x"] {
            assert!(select_monitor(&monitors, choice).is_err());
        }
    }
}
