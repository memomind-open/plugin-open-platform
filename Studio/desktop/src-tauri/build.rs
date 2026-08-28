use std::fs;
use std::path::{Path, PathBuf};

fn collect_c_sources(directory: &Path, output: &mut Vec<PathBuf>) {
    for entry in fs::read_dir(directory).expect("could not enumerate bundled LVGL sources") {
        let path = entry.expect("could not read bundled LVGL entry").path();
        if path.is_dir() {
            collect_c_sources(&path, output);
        } else if path.extension().and_then(|value| value.to_str()) == Some("c") {
            output.push(path);
        }
    }
}

fn emit_header_dependencies(directory: &Path) {
    for entry in fs::read_dir(directory).expect("could not enumerate bundled LVGL headers") {
        let path = entry
            .expect("could not read bundled LVGL header entry")
            .path();
        if path.is_dir() {
            emit_header_dependencies(&path);
        } else if path.extension().and_then(|value| value.to_str()) == Some("h") {
            println!("cargo:rerun-if-changed={}", path.display());
        }
    }
}

fn emit_file_dependencies(directory: &Path) {
    for entry in fs::read_dir(directory).expect("could not enumerate frontend files") {
        let path = entry.expect("could not read frontend entry").path();
        if path.is_dir() {
            emit_file_dependencies(&path);
        } else {
            println!("cargo:rerun-if-changed={}", path.display());
        }
    }
}

fn main() {
    let previewer_core = Path::new("../../previewer/core");
    let previewer_include = Path::new("../../previewer/include");
    let lvgl = Path::new("../../previewer/third_party/lvgl");
    let mut lvgl_sources = Vec::new();
    collect_c_sources(&lvgl.join("src"), &mut lvgl_sources);
    lvgl_sources.sort();

    cc::Build::new()
        .cpp(true)
        .std("c++17")
        .static_crt(true)
        .include(previewer_include)
        .include(lvgl)
        .file(previewer_core.join("lvgl_host.cpp"))
        .file(previewer_core.join("previewer.cpp"))
        .file(previewer_core.join("previewer_c_api.cpp"))
        .file(previewer_core.join("rv32.cpp"))
        .flag_if_supported("/utf-8")
        .flag_if_supported("/EHsc")
        .warnings(true)
        .compile("gmplugin_previewer_core");

    let mut lvgl_build = cc::Build::new();
    lvgl_build
        .include(lvgl)
        .static_crt(true)
        .define("LV_CONF_INCLUDE_SIMPLE", "1")
        .define("CONFIG_XGIMI_PATCH", "1")
        .flag_if_supported("/utf-8")
        .warnings(true);
    for source in &lvgl_sources {
        lvgl_build.file(source);
        println!("cargo:rerun-if-changed={}", source.display());
    }
    lvgl_build.compile("gm_device_lvgl");
    emit_header_dependencies(lvgl);

    for source in [
        "lvgl_host.cpp",
        "previewer.cpp",
        "previewer_c_api.cpp",
        "rv32.cpp",
    ] {
        println!(
            "cargo:rerun-if-changed={}",
            previewer_core.join(source).display()
        );
    }
    for header in [
        "lvgl_host.hpp",
        "previewer.hpp",
        "previewer_c_api.h",
        "rv32.hpp",
    ] {
        println!(
            "cargo:rerun-if-changed={}",
            previewer_include.join(header).display()
        );
    }
    println!(
        "cargo:rerun-if-changed={}",
        lvgl.join("lv_conf.h").display()
    );
    println!(
        "cargo:rerun-if-changed={}",
        lvgl.join("fonts/lv_font_xgimi_17.bin").display()
    );
    println!(
        "cargo:rerun-if-changed={}",
        lvgl.join("fonts/lv_font_xgimi_20.bin").display()
    );
    emit_file_dependencies(Path::new("../ui"));
    tauri_build::build();
}
