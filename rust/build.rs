fn main() {
    let mut build = cc::Build::new();
    build
        .file("../cur.c")
        .include("..")
        .warnings(true);
    if cfg!(target_env = "msvc") {
        build.flag("/utf-8");
    }
    build.compile("curlib");
}
