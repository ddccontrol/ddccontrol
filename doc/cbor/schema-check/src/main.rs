use std::{env, fs, process};

fn main() {
    let arguments: Vec<_> = env::args().collect();
    if arguments.len() < 4 {
        eprintln!("usage: schema-check SCHEMA ENTRY-TYPE CBOR-FILE...");
        process::exit(2);
    }
    let schema = fs::read_to_string(&arguments[1]).expect("read schema");
    for path in &arguments[3..] {
        let bytes = fs::read(path).expect("read CBOR input");
        if let Err(error) = cddl_cat::validate_cbor_bytes(&arguments[2], &schema, &bytes) {
            eprintln!("invalid {path}: {error:?}");
            process::exit(1);
        }
        println!("valid {path}");
    }
}
