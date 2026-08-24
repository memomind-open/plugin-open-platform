fn main() {
    let previewer = "../../previewer/src";
    cc::Build::new()
        .cpp(true)
        .std("c++17")
        .include(previewer)
        .file(format!("{previewer}/previewer.cpp"))
        .file(format!("{previewer}/previewer_c_api.cpp"))
        .file(format!("{previewer}/rv32.cpp"))
        .warnings(true)
        .compile("gmplugin_previewer_core");

    println!("cargo:rerun-if-changed={previewer}/previewer.cpp");
    println!("cargo:rerun-if-changed={previewer}/previewer.hpp");
    println!("cargo:rerun-if-changed={previewer}/previewer_c_api.cpp");
    println!("cargo:rerun-if-changed={previewer}/previewer_c_api.h");
    println!("cargo:rerun-if-changed={previewer}/rv32.cpp");
    println!("cargo:rerun-if-changed={previewer}/rv32.hpp");
    tauri_build::build();
}
