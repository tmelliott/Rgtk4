generate_init_c <- function(src_dir = "src", output_file = "src/init.c") {

  cat("Generating init.c for package registration...\n")

  # Find all C files
  c_files <- list.files(src_dir, pattern = "\\.c$", full.names = TRUE)
  c_files <- c_files[!grepl("init\\.c$", c_files)]  # Exclude init.c itself

  cat("Scanning", length(c_files), "C files...\n")

  # Scan per-file so each signature keeps its origin. Functions from
  # GtkSource_autogen.c are compiled only under #ifdef HAVE_GTKSOURCE, so
  # their extern declaration and registration entry must be guarded too -
  # otherwise the build fails to link when GtkSourceView is absent.
  sig_records <- list()
  for (cf in c_files) {
    lines <- readLines(cf)
    func_lines <- grep("^SEXP R_", lines, value = TRUE)
    if (length(func_lines) == 0) next
    sigs <- sub("\\s*\\{.*$", "", func_lines)
    is_gtksource <- grepl("GtkSource_autogen\\.c$", cf)
    for (sig in sigs) {
      sig_records[[length(sig_records) + 1]] <-
        list(sig = sig, gtksource = is_gtksource)
    }
  }

  cat("Found", length(sig_records), "functions to register\n")

  externs <- character()
  entries <- character()

  for (rec in sig_records) {
    sig <- rec$sig

    name <- sub("^SEXP\\s+", "", sig)
    name <- sub("\\(.*$", "", name)

    if (grepl("\\(void\\)|\\(\\)", sig)) {
      nargs <- 0
    } else {
      param_part <- sub("^[^(]*\\(", "", sig)
      param_part <- sub("\\).*$", "", param_part)
      nargs <- length(gregexpr("SEXP", param_part)[[1]])
      if (nargs == -1) nargs <- 0
    }

    extern_line <- paste0("extern ", sig, ";")
    entry_line <- sprintf('    {"%s", (DL_FUNC) &%s, %d},', name, name, nargs)

    if (isTRUE(rec$gtksource)) {
      externs <- c(externs, "#ifdef HAVE_GTKSOURCE", extern_line, "#endif")
      entries <- c(entries, "#ifdef HAVE_GTKSOURCE", entry_line, "#endif")
    } else {
      externs <- c(externs, extern_line)
      entries <- c(entries, entry_line)
    }
  }

  cat("Writing", output_file, "...\n")

  writeLines(c(
    "#include <R.h>",
    "#include <Rinternals.h>",
    "#include <R_ext/Rdynload.h>",
    "",
    "/* Declarations */",
    externs,
    "",
    "static const R_CallMethodDef CallEntries[] = {",
    entries,
    "    {NULL, NULL, 0}",
    "};",
    "",
    "void R_init_Rgtk4(DllInfo *dll) {",
    "    R_registerRoutines(dll, NULL, CallEntries, NULL, NULL);",
    "    R_useDynamicSymbols(dll, FALSE);",
    "}"
  ), output_file)

  cat("Done! Generated", length(readLines(output_file)), "lines in", output_file, "\n")
  cat("Registered", length(sig_records), "functions\n")

  invisible(output_file)
}

args <- commandArgs(trailingOnly = TRUE)
src_dir <- if (length(args) > 0) args[1] else "src"
output_file <- if (length(args) > 1) args[2] else file.path(src_dir, "init.c")

generate_init_c(src_dir, output_file)
