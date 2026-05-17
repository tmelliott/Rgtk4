#define R_NO_REMAP
#include <R.h>
#include <Rinternals.h>
#include <gtk/gtk.h>
#ifdef HAVE_GTKSOURCE
#include <gtksourceview/gtksource.h>
#endif
#include <glib.h>
#include <stdint.h>
#include <string.h>
#include "rgtk4_callbacks.h"
#include "rgtk4_autogen_callbacks.h"

/* Suppress pedantic warnings in auto-generated GTK glue code */
#pragma GCC diagnostic ignored "-Wpedantic"

/* Agnostic numeric extraction helper */
static inline double _unbox_numeric(SEXP s) {
  if (TYPEOF(s) == REALSXP) return REAL(s)[0];
  if (TYPEOF(s) == INTSXP)  return (double)INTEGER(s)[0];
  if (TYPEOF(s) == LGLSXP)  return (double)LOGICAL(s)[0];
  return 0.0;
}

/* Bounded numeric extraction. NA and out-of-range values throw. */
static inline gint64 _unbox_int_range(SEXP s, gint64 lo, gint64 hi, const char *func) __attribute__((unused));
static inline gint64 _unbox_int_range(SEXP s, gint64 lo, gint64 hi, const char *func) {
  double v;
  if (TYPEOF(s) == INTSXP) {
    int iv = INTEGER(s)[0];
    if (iv == NA_INTEGER) Rf_error("%s: NA not allowed for integer argument", func);
    v = (double)iv;
  } else if (TYPEOF(s) == REALSXP) {
    v = REAL(s)[0];
    if (!R_finite(v)) Rf_error("%s: NA/Inf not allowed for integer argument", func);
  } else if (TYPEOF(s) == LGLSXP) {
    int lv = LOGICAL(s)[0];
    if (lv == NA_LOGICAL) Rf_error("%s: NA not allowed for integer argument", func);
    v = (double)lv;
  } else {
    Rf_error("%s: expected numeric scalar, got %s", func, Rf_type2char(TYPEOF(s)));
  }
  if (v < (double)lo || v > (double)hi) {
    Rf_error("%s: value %.0f out of range [%lld, %lld]", func, v, (long long)lo, (long long)hi);
  }
  return (gint64)v;
}

static inline double _unbox_real(SEXP s, const char *func) __attribute__((unused));
static inline double _unbox_real(SEXP s, const char *func) {
  if (TYPEOF(s) == REALSXP) {
    double v = REAL(s)[0];
    if (ISNA(v)) Rf_error("%s: NA not allowed for numeric argument", func);
    return v;
  }
  if (TYPEOF(s) == INTSXP) {
    int iv = INTEGER(s)[0];
    if (iv == NA_INTEGER) Rf_error("%s: NA not allowed for numeric argument", func);
    return (double)iv;
  }
  if (TYPEOF(s) == LGLSXP) {
    int lv = LOGICAL(s)[0];
    if (lv == NA_LOGICAL) Rf_error("%s: NA not allowed for numeric argument", func);
    return (double)lv;
  }
  Rf_error("%s: expected numeric scalar, got %s", func, Rf_type2char(TYPEOF(s)));
  return 0.0;  /* unreachable */
}

#define _UNBOX_GINT(s)   ((gint)  _unbox_int_range((s), G_MININT,    G_MAXINT,    __func__))
#define _UNBOX_GUINT(s)  ((guint) _unbox_int_range((s), 0,           G_MAXUINT,   __func__))
#define _UNBOX_GINT8(s)  ((gint8) _unbox_int_range((s), G_MININT8,   G_MAXINT8,   __func__))
#define _UNBOX_GUINT8(s) ((guint8)_unbox_int_range((s), 0,           G_MAXUINT8,  __func__))
#define _UNBOX_GINT16(s) ((gint16)_unbox_int_range((s), G_MININT16,  G_MAXINT16,  __func__))
#define _UNBOX_GUINT16(s)((guint16)_unbox_int_range((s),0,           G_MAXUINT16, __func__))
#define _UNBOX_GINT32(s) ((gint32)_unbox_int_range((s), G_MININT32,  G_MAXINT32,  __func__))
#define _UNBOX_GUINT32(s)((guint32)_unbox_int_range((s),0,           G_MAXUINT32, __func__))
#define _UNBOX_GINT64(s) ((gint64)_unbox_int_range((s), G_MININT64,  G_MAXINT64,  __func__))
#define _UNBOX_GSIZE(s)  ((gsize) _unbox_int_range((s), 0,           G_MAXINT64,  __func__))
#define _UNBOX_GBOOL(s)  ((gboolean)(Rf_asLogical(s) == TRUE))

/* Safe pointer extraction with validation */
static inline void* get_ptr_internal(SEXP s, const char* func) __attribute__((unused));
static inline void* get_ptr_internal(SEXP s, const char* func) {
  if (s == R_NilValue) return NULL;
  if (TYPEOF(s) != EXTPTRSXP) {
    Rf_error("%s: expected external pointer, got %s", func, Rf_type2char(TYPEOF(s)));
  }
  void *addr = R_ExternalPtrAddr(s);
  if (!addr) {
    Rf_error("%s: external pointer is NULL (object may have been destroyed)", func);
  }
  return addr;
}
#define get_ptr(s) get_ptr_internal(s, __func__)

/* GTK init guard. Bindings that touch GTK/GDK call
   RGTK4_REQUIRE_INIT at entry to surface a clean error rather than
   crash on uninitialized state. Uses gtk_is_initialized() directly. */
#define RGTK4_REQUIRE_INIT() do { \
  if (!gtk_is_initialized()) { \
    Rf_error("%s: gtkInit() has not been called — call gtkInit() first", __func__); \
  } \
} while (0)

static void _finalizer_g_free(SEXP s) __attribute__((unused));
static void _finalizer_g_free(SEXP s) {
  void *p = R_ExternalPtrAddr(s);
  if (p) g_free(p);
}

extern SEXP make_gobject_ptr(gpointer obj);
extern SEXP make_boxed_struct(const void *src, size_t size, const char *type_name);

static SEXP _box_GStrv(char **strv) __attribute__((unused));
static SEXP _box_GStrv(char **strv) {
  if (!strv) return R_NilValue;
  int n = g_strv_length(strv);
  SEXP res = PROTECT(Rf_allocVector(STRSXP, n));
  for (int i = 0; i < n; i++) SET_STRING_ELT(res, i, Rf_mkChar(strv[i]));
  UNPROTECT(1);
  return res;
}

static SEXP tag_pointer(SEXP ptr, const char* fallback_name) {
  if (ptr == R_NilValue || TYPEOF(ptr) != EXTPTRSXP) return ptr;
  void *obj = R_ExternalPtrAddr(ptr);
  if ((uintptr_t)obj < 0x1000) {
    R_SetExternalPtrTag(ptr, Rf_mkChar(fallback_name));
    SEXP classes = PROTECT(Rf_allocVector(STRSXP, 3));
    SET_STRING_ELT(classes, 0, Rf_mkChar(fallback_name));
    SET_STRING_ELT(classes, 1, Rf_mkChar("GObject"));
    SET_STRING_ELT(classes, 2, Rf_mkChar("RGtkObject"));
    Rf_setAttrib(ptr, R_ClassSymbol, classes);
    UNPROTECT(1);
    return ptr;
  }
  if (G_IS_OBJECT(obj)) {
    const char *tn = G_OBJECT_TYPE_NAME(obj);
    R_SetExternalPtrTag(ptr, Rf_mkChar(tn ? tn : fallback_name));
    SEXP classes = PROTECT(Rf_allocVector(STRSXP, 3));
    SET_STRING_ELT(classes, 0, Rf_mkChar(tn ? tn : fallback_name));
    SET_STRING_ELT(classes, 1, Rf_mkChar("GObject"));
    SET_STRING_ELT(classes, 2, Rf_mkChar("RGtkObject"));
    Rf_setAttrib(ptr, R_ClassSymbol, classes);
    UNPROTECT(1);
  } else {
    R_SetExternalPtrTag(ptr, Rf_mkChar(fallback_name));
    SEXP classes = PROTECT(Rf_allocVector(STRSXP, 3));
    SET_STRING_ELT(classes, 0, Rf_mkChar(fallback_name));
    SET_STRING_ELT(classes, 1, Rf_mkChar("GObject"));
    SET_STRING_ELT(classes, 2, Rf_mkChar("RGtkObject"));
    Rf_setAttrib(ptr, R_ClassSymbol, classes);
    UNPROTECT(1);
  }
  return ptr;
}

/* Autogenerated for GtkSource */
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
#ifdef __clang__
#pragma GCC diagnostic ignored "-Wimplicit-enum-enum-cast"
#endif

#ifdef HAVE_GTKSOURCE

SEXP R_gtk_source_annotation_new(SEXP s1, SEXP s2, SEXP s3, SEXP s4) {
  RGTK4_REQUIRE_INIT();
  const char* v1 = (s1 != R_NilValue) ? (const char*)(CHAR(STRING_ELT(s1,0))) : NULL; (void)v1;
  GIcon* v2 = (s2 != R_NilValue) ? (GIcon*)(get_ptr(s2)) : NULL; (void)v2;
  gint v3 = (gint)((gint)_unbox_numeric(s3)); (void)v3;
  GtkSourceAnnotationStyle v4 = (GtkSourceAnnotationStyle)((GtkSourceAnnotationStyle)(TYPEOF(s4)==EXTPTRSXP ? (size_t)R_ExternalPtrAddr(s4) : INTEGER(s4)[0])); (void)v4;
  gconstpointer _ret = (gconstpointer)gtk_source_annotation_new(v1, v2, v3, v4);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Annotation"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_annotation_get_description(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceAnnotation* v1 = (GtkSourceAnnotation*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_annotation_get_description(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_annotation_get_icon(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceAnnotation* v1 = (GtkSourceAnnotation*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_annotation_get_icon(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Gio.Icon"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_annotation_get_line(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceAnnotation* v1 = (GtkSourceAnnotation*)(get_ptr(s1)); (void)v1;
  int _ret = (int)gtk_source_annotation_get_line(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gint"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_annotation_get_style(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceAnnotation* v1 = (GtkSourceAnnotation*)(get_ptr(s1)); (void)v1;
  GtkSourceAnnotationStyle _ret = (GtkSourceAnnotationStyle)gtk_source_annotation_get_style(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, tag_pointer(R_MakeExternalPtr((void*)(_ret), R_NilValue, R_NilValue), "AnnotationStyle"));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("AnnotationStyle"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_annotations_add_provider(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceAnnotations* v1 = (GtkSourceAnnotations*)(get_ptr(s1)); (void)v1;
  GtkSourceAnnotationProvider* v2 = (GtkSourceAnnotationProvider*)(get_ptr(s2)); (void)v2;
  gtk_source_annotations_add_provider(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_annotations_remove_provider(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceAnnotations* v1 = (GtkSourceAnnotations*)(get_ptr(s1)); (void)v1;
  GtkSourceAnnotationProvider* v2 = (GtkSourceAnnotationProvider*)(get_ptr(s2)); (void)v2;
  gboolean _ret = (gboolean)gtk_source_annotations_remove_provider(v1, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_buffer_new(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkTextTagTable* v1 = (s1 != R_NilValue) ? (GtkTextTagTable*)(get_ptr(s1)) : NULL; (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_buffer_new(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Buffer"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_buffer_new_with_language(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceLanguage* v1 = (GtkSourceLanguage*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_buffer_new_with_language(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Buffer"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_buffer_backward_iter_to_source_mark(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceBuffer* v1 = (GtkSourceBuffer*)(get_ptr(s1)); (void)v1;
  GtkTextIter _out_iter = {0}; (void)_out_iter;
  const char* v2 = (s2 != R_NilValue) ? (const char*)(CHAR(STRING_ELT(s2,0))) : NULL; (void)v2;
  gboolean _ret = (gboolean)gtk_source_buffer_backward_iter_to_source_mark(v1, &_out_iter, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 2));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 2));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  SET_VECTOR_ELT(_ans, 1, make_boxed_struct(&_out_iter, sizeof(GtkTextIter), "GtkTextIter"));
  if (VECTOR_ELT(_ans, 1) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 1), Rf_install("glib_type"), Rf_mkString("Gtk.TextIter"));
  }
  SET_STRING_ELT(_ans_names, 1, Rf_mkChar("iter"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_buffer_change_case(SEXP s1, SEXP s2, SEXP s3, SEXP s4) {
  RGTK4_REQUIRE_INIT();
  GtkSourceBuffer* v1 = (GtkSourceBuffer*)(get_ptr(s1)); (void)v1;
  GtkSourceChangeCaseType v2 = (GtkSourceChangeCaseType)((GtkSourceChangeCaseType)(TYPEOF(s2)==EXTPTRSXP ? (size_t)R_ExternalPtrAddr(s2) : INTEGER(s2)[0])); (void)v2;
  GtkTextIter* v3 = (GtkTextIter*)(get_ptr(s3)); (void)v3;
  GtkTextIter* v4 = (GtkTextIter*)(get_ptr(s4)); (void)v4;
  gtk_source_buffer_change_case(v1, v2, v3, v4);
  return R_NilValue;
}


SEXP R_gtk_source_buffer_create_source_mark(SEXP s1, SEXP s2, SEXP s3, SEXP s4) {
  RGTK4_REQUIRE_INIT();
  GtkSourceBuffer* v1 = (GtkSourceBuffer*)(get_ptr(s1)); (void)v1;
  const char* v2 = (s2 != R_NilValue) ? (const char*)(CHAR(STRING_ELT(s2,0))) : NULL; (void)v2;
  const char* v3 = (const char*)(CHAR(STRING_ELT(s3,0))); (void)v3;
  const GtkTextIter* v4 = (const GtkTextIter*)(get_ptr(s4)); (void)v4;
  gconstpointer _ret = (gconstpointer)gtk_source_buffer_create_source_mark(v1, v2, v3, v4);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Mark"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_buffer_ensure_highlight(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceBuffer* v1 = (GtkSourceBuffer*)(get_ptr(s1)); (void)v1;
  const GtkTextIter* v2 = (const GtkTextIter*)(get_ptr(s2)); (void)v2;
  const GtkTextIter* v3 = (const GtkTextIter*)(get_ptr(s3)); (void)v3;
  gtk_source_buffer_ensure_highlight(v1, v2, v3);
  return R_NilValue;
}


SEXP R_gtk_source_buffer_forward_iter_to_source_mark(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceBuffer* v1 = (GtkSourceBuffer*)(get_ptr(s1)); (void)v1;
  GtkTextIter _out_iter = {0}; (void)_out_iter;
  const char* v2 = (s2 != R_NilValue) ? (const char*)(CHAR(STRING_ELT(s2,0))) : NULL; (void)v2;
  gboolean _ret = (gboolean)gtk_source_buffer_forward_iter_to_source_mark(v1, &_out_iter, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 2));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 2));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  SET_VECTOR_ELT(_ans, 1, make_boxed_struct(&_out_iter, sizeof(GtkTextIter), "GtkTextIter"));
  if (VECTOR_ELT(_ans, 1) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 1), Rf_install("glib_type"), Rf_mkString("Gtk.TextIter"));
  }
  SET_STRING_ELT(_ans_names, 1, Rf_mkChar("iter"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_buffer_get_context_classes_at_iter(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceBuffer* v1 = (GtkSourceBuffer*)(get_ptr(s1)); (void)v1;
  const GtkTextIter* v2 = (const GtkTextIter*)(get_ptr(s2)); (void)v2;
  gconstpointer _ret = (gconstpointer)gtk_source_buffer_get_context_classes_at_iter(v1, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : tag_pointer(Rf_mkString(_ret ? (const char*)_ret : ""), "utf8"));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_buffer_get_highlight_matching_brackets(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceBuffer* v1 = (GtkSourceBuffer*)(get_ptr(s1)); (void)v1;
  gboolean _ret = (gboolean)gtk_source_buffer_get_highlight_matching_brackets(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_buffer_get_highlight_syntax(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceBuffer* v1 = (GtkSourceBuffer*)(get_ptr(s1)); (void)v1;
  gboolean _ret = (gboolean)gtk_source_buffer_get_highlight_syntax(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_buffer_get_implicit_trailing_newline(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceBuffer* v1 = (GtkSourceBuffer*)(get_ptr(s1)); (void)v1;
  gboolean _ret = (gboolean)gtk_source_buffer_get_implicit_trailing_newline(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_buffer_get_language(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceBuffer* v1 = (GtkSourceBuffer*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_buffer_get_language(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Language"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_buffer_get_loading(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceBuffer* v1 = (GtkSourceBuffer*)(get_ptr(s1)); (void)v1;
  gboolean _ret = (gboolean)gtk_source_buffer_get_loading(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_buffer_get_source_marks_at_iter(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceBuffer* v1 = (GtkSourceBuffer*)(get_ptr(s1)); (void)v1;
  GtkTextIter* v2 = (GtkTextIter*)(get_ptr(s2)); (void)v2;
  const char* v3 = (s3 != R_NilValue) ? (const char*)(CHAR(STRING_ELT(s3,0))) : NULL; (void)v3;
  gconstpointer _ret = (gconstpointer)gtk_source_buffer_get_source_marks_at_iter(v1, v2, v3);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("GLib.SList"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_buffer_get_source_marks_at_line(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceBuffer* v1 = (GtkSourceBuffer*)(get_ptr(s1)); (void)v1;
  gint v2 = (gint)((gint)_unbox_numeric(s2)); (void)v2;
  const char* v3 = (s3 != R_NilValue) ? (const char*)(CHAR(STRING_ELT(s3,0))) : NULL; (void)v3;
  gconstpointer _ret = (gconstpointer)gtk_source_buffer_get_source_marks_at_line(v1, v2, v3);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("GLib.SList"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_buffer_get_style_scheme(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceBuffer* v1 = (GtkSourceBuffer*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_buffer_get_style_scheme(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("StyleScheme"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_buffer_iter_backward_to_context_class_toggle(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceBuffer* v1 = (GtkSourceBuffer*)(get_ptr(s1)); (void)v1;
  GtkTextIter _out_iter = {0}; (void)_out_iter;
  const char* v2 = (const char*)(CHAR(STRING_ELT(s2,0))); (void)v2;
  gboolean _ret = (gboolean)gtk_source_buffer_iter_backward_to_context_class_toggle(v1, &_out_iter, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 2));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 2));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  SET_VECTOR_ELT(_ans, 1, make_boxed_struct(&_out_iter, sizeof(GtkTextIter), "GtkTextIter"));
  if (VECTOR_ELT(_ans, 1) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 1), Rf_install("glib_type"), Rf_mkString("Gtk.TextIter"));
  }
  SET_STRING_ELT(_ans_names, 1, Rf_mkChar("iter"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_buffer_iter_forward_to_context_class_toggle(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceBuffer* v1 = (GtkSourceBuffer*)(get_ptr(s1)); (void)v1;
  GtkTextIter _out_iter = {0}; (void)_out_iter;
  const char* v2 = (const char*)(CHAR(STRING_ELT(s2,0))); (void)v2;
  gboolean _ret = (gboolean)gtk_source_buffer_iter_forward_to_context_class_toggle(v1, &_out_iter, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 2));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 2));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  SET_VECTOR_ELT(_ans, 1, make_boxed_struct(&_out_iter, sizeof(GtkTextIter), "GtkTextIter"));
  if (VECTOR_ELT(_ans, 1) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 1), Rf_install("glib_type"), Rf_mkString("Gtk.TextIter"));
  }
  SET_STRING_ELT(_ans_names, 1, Rf_mkChar("iter"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_buffer_iter_has_context_class(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceBuffer* v1 = (GtkSourceBuffer*)(get_ptr(s1)); (void)v1;
  const GtkTextIter* v2 = (const GtkTextIter*)(get_ptr(s2)); (void)v2;
  const char* v3 = (const char*)(CHAR(STRING_ELT(s3,0))); (void)v3;
  gboolean _ret = (gboolean)gtk_source_buffer_iter_has_context_class(v1, v2, v3);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_buffer_join_lines(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceBuffer* v1 = (GtkSourceBuffer*)(get_ptr(s1)); (void)v1;
  GtkTextIter* v2 = (GtkTextIter*)(get_ptr(s2)); (void)v2;
  GtkTextIter* v3 = (GtkTextIter*)(get_ptr(s3)); (void)v3;
  gtk_source_buffer_join_lines(v1, v2, v3);
  return R_NilValue;
}


SEXP R_gtk_source_buffer_remove_source_marks(SEXP s1, SEXP s2, SEXP s3, SEXP s4) {
  RGTK4_REQUIRE_INIT();
  GtkSourceBuffer* v1 = (GtkSourceBuffer*)(get_ptr(s1)); (void)v1;
  const GtkTextIter* v2 = (const GtkTextIter*)(get_ptr(s2)); (void)v2;
  const GtkTextIter* v3 = (const GtkTextIter*)(get_ptr(s3)); (void)v3;
  const char* v4 = (s4 != R_NilValue) ? (const char*)(CHAR(STRING_ELT(s4,0))) : NULL; (void)v4;
  gtk_source_buffer_remove_source_marks(v1, v2, v3, v4);
  return R_NilValue;
}


SEXP R_gtk_source_buffer_set_highlight_matching_brackets(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceBuffer* v1 = (GtkSourceBuffer*)(get_ptr(s1)); (void)v1;
  gboolean v2 = (gboolean)((gboolean)LOGICAL(s2)[0]); (void)v2;
  gtk_source_buffer_set_highlight_matching_brackets(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_buffer_set_highlight_syntax(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceBuffer* v1 = (GtkSourceBuffer*)(get_ptr(s1)); (void)v1;
  gboolean v2 = (gboolean)((gboolean)LOGICAL(s2)[0]); (void)v2;
  gtk_source_buffer_set_highlight_syntax(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_buffer_set_implicit_trailing_newline(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceBuffer* v1 = (GtkSourceBuffer*)(get_ptr(s1)); (void)v1;
  gboolean v2 = (gboolean)((gboolean)LOGICAL(s2)[0]); (void)v2;
  gtk_source_buffer_set_implicit_trailing_newline(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_buffer_set_language(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceBuffer* v1 = (GtkSourceBuffer*)(get_ptr(s1)); (void)v1;
  GtkSourceLanguage* v2 = (s2 != R_NilValue) ? (GtkSourceLanguage*)(get_ptr(s2)) : NULL; (void)v2;
  gtk_source_buffer_set_language(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_buffer_set_style_scheme(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceBuffer* v1 = (GtkSourceBuffer*)(get_ptr(s1)); (void)v1;
  GtkSourceStyleScheme* v2 = (s2 != R_NilValue) ? (GtkSourceStyleScheme*)(get_ptr(s2)) : NULL; (void)v2;
  gtk_source_buffer_set_style_scheme(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_buffer_sort_lines(SEXP s1, SEXP s2, SEXP s3, SEXP s4, SEXP s5) {
  RGTK4_REQUIRE_INIT();
  GtkSourceBuffer* v1 = (GtkSourceBuffer*)(get_ptr(s1)); (void)v1;
  GtkTextIter* v2 = (GtkTextIter*)(get_ptr(s2)); (void)v2;
  GtkTextIter* v3 = (GtkTextIter*)(get_ptr(s3)); (void)v3;
  GtkSourceSortFlags v4 = (GtkSourceSortFlags)((GtkSourceSortFlags)(TYPEOF(s4)==EXTPTRSXP ? (size_t)R_ExternalPtrAddr(s4) : INTEGER(s4)[0])); (void)v4;
  gint v5 = (gint)((gint)_unbox_numeric(s5)); (void)v5;
  gtk_source_buffer_sort_lines(v1, v2, v3, v4, v5);
  return R_NilValue;
}


SEXP R_gtk_source_completion_fuzzy_highlight(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  const char* v1 = (const char*)(CHAR(STRING_ELT(s1,0))); (void)v1;
  const char* v2 = (const char*)(CHAR(STRING_ELT(s2,0))); (void)v2;
  gconstpointer _ret = (gconstpointer)gtk_source_completion_fuzzy_highlight(v1, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Pango.AttrList"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_completion_fuzzy_match(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  const char* v1 = (s1 != R_NilValue) ? (const char*)(CHAR(STRING_ELT(s1,0))) : NULL; (void)v1;
  const char* v2 = (const char*)(CHAR(STRING_ELT(s2,0))); (void)v2;
  guint _out_priority = 0; (void)_out_priority;
  gboolean _ret = (gboolean)gtk_source_completion_fuzzy_match(v1, v2, &_out_priority);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 2));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 2));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  SET_VECTOR_ELT(_ans, 1, Rf_ScalarInteger((int)(_out_priority)));
  if (VECTOR_ELT(_ans, 1) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 1), Rf_install("glib_type"), Rf_mkString("guint"));
  }
  SET_STRING_ELT(_ans_names, 1, Rf_mkChar("priority"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_completion_add_provider(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletion* v1 = (GtkSourceCompletion*)(get_ptr(s1)); (void)v1;
  GtkSourceCompletionProvider* v2 = (GtkSourceCompletionProvider*)(get_ptr(s2)); (void)v2;
  gtk_source_completion_add_provider(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_completion_block_interactive(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletion* v1 = (GtkSourceCompletion*)(get_ptr(s1)); (void)v1;
  gtk_source_completion_block_interactive(v1);
  return R_NilValue;
}


SEXP R_gtk_source_completion_get_buffer(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletion* v1 = (GtkSourceCompletion*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_completion_get_buffer(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Buffer"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_completion_get_page_size(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletion* v1 = (GtkSourceCompletion*)(get_ptr(s1)); (void)v1;
  guint _ret = (guint)gtk_source_completion_get_page_size(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("guint"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_completion_get_view(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletion* v1 = (GtkSourceCompletion*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_completion_get_view(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("View"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_completion_hide(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletion* v1 = (GtkSourceCompletion*)(get_ptr(s1)); (void)v1;
  gtk_source_completion_hide(v1);
  return R_NilValue;
}


SEXP R_gtk_source_completion_remove_provider(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletion* v1 = (GtkSourceCompletion*)(get_ptr(s1)); (void)v1;
  GtkSourceCompletionProvider* v2 = (GtkSourceCompletionProvider*)(get_ptr(s2)); (void)v2;
  gtk_source_completion_remove_provider(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_completion_set_page_size(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletion* v1 = (GtkSourceCompletion*)(get_ptr(s1)); (void)v1;
  guint v2 = (guint)((guint)_unbox_numeric(s2)); (void)v2;
  gtk_source_completion_set_page_size(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_completion_show(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletion* v1 = (GtkSourceCompletion*)(get_ptr(s1)); (void)v1;
  gtk_source_completion_show(v1);
  return R_NilValue;
}


SEXP R_gtk_source_completion_unblock_interactive(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletion* v1 = (GtkSourceCompletion*)(get_ptr(s1)); (void)v1;
  gtk_source_completion_unblock_interactive(v1);
  return R_NilValue;
}


SEXP R_gtk_source_completion_cell_get_column(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletionCell* v1 = (GtkSourceCompletionCell*)(get_ptr(s1)); (void)v1;
  GtkSourceCompletionColumn _ret = (GtkSourceCompletionColumn)gtk_source_completion_cell_get_column(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, tag_pointer(R_MakeExternalPtr((void*)(_ret), R_NilValue, R_NilValue), "CompletionColumn"));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("CompletionColumn"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_completion_cell_get_widget(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletionCell* v1 = (GtkSourceCompletionCell*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_completion_cell_get_widget(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Gtk.Widget"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_completion_cell_set_gicon(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletionCell* v1 = (GtkSourceCompletionCell*)(get_ptr(s1)); (void)v1;
  GIcon* v2 = (GIcon*)(get_ptr(s2)); (void)v2;
  gtk_source_completion_cell_set_gicon(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_completion_cell_set_icon_name(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletionCell* v1 = (GtkSourceCompletionCell*)(get_ptr(s1)); (void)v1;
  const char* v2 = (const char*)(CHAR(STRING_ELT(s2,0))); (void)v2;
  gtk_source_completion_cell_set_icon_name(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_completion_cell_set_markup(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletionCell* v1 = (GtkSourceCompletionCell*)(get_ptr(s1)); (void)v1;
  const char* v2 = (const char*)(CHAR(STRING_ELT(s2,0))); (void)v2;
  gtk_source_completion_cell_set_markup(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_completion_cell_set_paintable(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletionCell* v1 = (GtkSourceCompletionCell*)(get_ptr(s1)); (void)v1;
  GdkPaintable* v2 = (GdkPaintable*)(get_ptr(s2)); (void)v2;
  gtk_source_completion_cell_set_paintable(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_completion_cell_set_text(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletionCell* v1 = (GtkSourceCompletionCell*)(get_ptr(s1)); (void)v1;
  const char* v2 = (s2 != R_NilValue) ? (const char*)(CHAR(STRING_ELT(s2,0))) : NULL; (void)v2;
  gtk_source_completion_cell_set_text(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_completion_cell_set_text_with_attributes(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletionCell* v1 = (GtkSourceCompletionCell*)(get_ptr(s1)); (void)v1;
  const char* v2 = (const char*)(CHAR(STRING_ELT(s2,0))); (void)v2;
  PangoAttrList* v3 = (PangoAttrList*)(get_ptr(s3)); (void)v3;
  gtk_source_completion_cell_set_text_with_attributes(v1, v2, v3);
  return R_NilValue;
}


SEXP R_gtk_source_completion_cell_set_widget(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletionCell* v1 = (GtkSourceCompletionCell*)(get_ptr(s1)); (void)v1;
  GtkWidget* v2 = (GtkWidget*)(get_ptr(s2)); (void)v2;
  gtk_source_completion_cell_set_widget(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_completion_context_get_activation(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletionContext* v1 = (GtkSourceCompletionContext*)(get_ptr(s1)); (void)v1;
  GtkSourceCompletionActivation _ret = (GtkSourceCompletionActivation)gtk_source_completion_context_get_activation(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, tag_pointer(R_MakeExternalPtr((void*)(_ret), R_NilValue, R_NilValue), "CompletionActivation"));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("CompletionActivation"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_completion_context_get_bounds(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletionContext* v1 = (GtkSourceCompletionContext*)(get_ptr(s1)); (void)v1;
  GtkTextIter _out_begin = {0}; (void)_out_begin;
  GtkTextIter _out_end = {0}; (void)_out_end;
  gboolean _ret = (gboolean)gtk_source_completion_context_get_bounds(v1, &_out_begin, &_out_end);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 3));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 3));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  SET_VECTOR_ELT(_ans, 1, make_boxed_struct(&_out_begin, sizeof(GtkTextIter), "GtkTextIter"));
  if (VECTOR_ELT(_ans, 1) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 1), Rf_install("glib_type"), Rf_mkString("Gtk.TextIter"));
  }
  SET_STRING_ELT(_ans_names, 1, Rf_mkChar("begin"));
  SET_VECTOR_ELT(_ans, 2, make_boxed_struct(&_out_end, sizeof(GtkTextIter), "GtkTextIter"));
  if (VECTOR_ELT(_ans, 2) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 2), Rf_install("glib_type"), Rf_mkString("Gtk.TextIter"));
  }
  SET_STRING_ELT(_ans_names, 2, Rf_mkChar("end"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_completion_context_get_buffer(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletionContext* v1 = (GtkSourceCompletionContext*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_completion_context_get_buffer(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Buffer"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_completion_context_get_busy(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletionContext* v1 = (GtkSourceCompletionContext*)(get_ptr(s1)); (void)v1;
  gboolean _ret = (gboolean)gtk_source_completion_context_get_busy(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_completion_context_get_completion(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletionContext* v1 = (GtkSourceCompletionContext*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_completion_context_get_completion(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Completion"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_completion_context_get_empty(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletionContext* v1 = (GtkSourceCompletionContext*)(get_ptr(s1)); (void)v1;
  gboolean _ret = (gboolean)gtk_source_completion_context_get_empty(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_completion_context_get_language(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletionContext* v1 = (GtkSourceCompletionContext*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_completion_context_get_language(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Language"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_completion_context_get_view(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletionContext* v1 = (GtkSourceCompletionContext*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_completion_context_get_view(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("View"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_completion_context_get_word(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletionContext* v1 = (GtkSourceCompletionContext*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_completion_context_get_word(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_completion_context_set_proposals_for_provider(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletionContext* v1 = (GtkSourceCompletionContext*)(get_ptr(s1)); (void)v1;
  GtkSourceCompletionProvider* v2 = (GtkSourceCompletionProvider*)(get_ptr(s2)); (void)v2;
  GListModel* v3 = (s3 != R_NilValue) ? (GListModel*)(get_ptr(s3)) : NULL; (void)v3;
  gtk_source_completion_context_set_proposals_for_provider(v1, v2, v3);
  return R_NilValue;
}


SEXP R_gtk_source_completion_provider_activate(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletionProvider* v1 = (GtkSourceCompletionProvider*)(get_ptr(s1)); (void)v1;
  GtkSourceCompletionContext* v2 = (GtkSourceCompletionContext*)(get_ptr(s2)); (void)v2;
  GtkSourceCompletionProposal* v3 = (GtkSourceCompletionProposal*)(get_ptr(s3)); (void)v3;
  gtk_source_completion_provider_activate(v1, v2, v3);
  return R_NilValue;
}


SEXP R_gtk_source_completion_provider_display(SEXP s1, SEXP s2, SEXP s3, SEXP s4) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletionProvider* v1 = (GtkSourceCompletionProvider*)(get_ptr(s1)); (void)v1;
  GtkSourceCompletionContext* v2 = (GtkSourceCompletionContext*)(get_ptr(s2)); (void)v2;
  GtkSourceCompletionProposal* v3 = (GtkSourceCompletionProposal*)(get_ptr(s3)); (void)v3;
  GtkSourceCompletionCell* v4 = (GtkSourceCompletionCell*)(get_ptr(s4)); (void)v4;
  gtk_source_completion_provider_display(v1, v2, v3, v4);
  return R_NilValue;
}


SEXP R_gtk_source_completion_provider_get_priority(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletionProvider* v1 = (GtkSourceCompletionProvider*)(get_ptr(s1)); (void)v1;
  GtkSourceCompletionContext* v2 = (GtkSourceCompletionContext*)(get_ptr(s2)); (void)v2;
  int _ret = (int)gtk_source_completion_provider_get_priority(v1, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gint"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_completion_provider_get_title(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletionProvider* v1 = (GtkSourceCompletionProvider*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_completion_provider_get_title(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_completion_provider_is_trigger(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletionProvider* v1 = (GtkSourceCompletionProvider*)(get_ptr(s1)); (void)v1;
  const GtkTextIter* v2 = (const GtkTextIter*)(get_ptr(s2)); (void)v2;
  gunichar v3 = (gunichar)((gunichar)_unbox_numeric(s3)); (void)v3;
  gboolean _ret = (gboolean)gtk_source_completion_provider_is_trigger(v1, v2, v3);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_completion_provider_key_activates(SEXP s1, SEXP s2, SEXP s3, SEXP s4, SEXP s5) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletionProvider* v1 = (GtkSourceCompletionProvider*)(get_ptr(s1)); (void)v1;
  GtkSourceCompletionContext* v2 = (GtkSourceCompletionContext*)(get_ptr(s2)); (void)v2;
  GtkSourceCompletionProposal* v3 = (GtkSourceCompletionProposal*)(get_ptr(s3)); (void)v3;
  guint v4 = (guint)((guint)_unbox_numeric(s4)); (void)v4;
  GdkModifierType v5 = (GdkModifierType)((GdkModifierType)(TYPEOF(s5)==EXTPTRSXP ? (size_t)R_ExternalPtrAddr(s5) : INTEGER(s5)[0])); (void)v5;
  gboolean _ret = (gboolean)gtk_source_completion_provider_key_activates(v1, v2, v3, v4, v5);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_completion_provider_list_alternates(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletionProvider* v1 = (GtkSourceCompletionProvider*)(get_ptr(s1)); (void)v1;
  GtkSourceCompletionContext* v2 = (GtkSourceCompletionContext*)(get_ptr(s2)); (void)v2;
  GtkSourceCompletionProposal* v3 = (GtkSourceCompletionProposal*)(get_ptr(s3)); (void)v3;
  gconstpointer _ret = (gconstpointer)gtk_source_completion_provider_list_alternates(v1, v2, v3);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("CompletionProposal"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_completion_provider_populate_async(SEXP s1, SEXP s2, SEXP s3, SEXP s4) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletionProvider* v1 = (GtkSourceCompletionProvider*)(get_ptr(s1)); (void)v1;
  GtkSourceCompletionContext* v2 = (GtkSourceCompletionContext*)(get_ptr(s2)); (void)v2;
  GCancellable* v3 = (s3 != R_NilValue) ? (GCancellable*)(get_ptr(s3)) : NULL; (void)v3;
  RCallbackClosure *_cb_closure_4 = (s4 == R_NilValue) ? NULL : rgtk4_closure_new(s4); (void)_cb_closure_4;
  gtk_source_completion_provider_populate_async(v1, v2, v3, (GAsyncReadyCallback)(_cb_closure_4 ? _rgtk4_cb_AsyncReadyCallback : NULL), _cb_closure_4);
  return R_NilValue;
}


SEXP R_gtk_source_completion_provider_populate_finish(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletionProvider* v1 = (GtkSourceCompletionProvider*)(get_ptr(s1)); (void)v1;
  GAsyncResult* v2 = (GAsyncResult*)(get_ptr(s2)); (void)v2;
  GError *_err = NULL;
  gconstpointer _ret = (gconstpointer)gtk_source_completion_provider_populate_finish(v1, v2, &_err);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Gio.ListModel"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_completion_provider_refilter(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletionProvider* v1 = (GtkSourceCompletionProvider*)(get_ptr(s1)); (void)v1;
  GtkSourceCompletionContext* v2 = (GtkSourceCompletionContext*)(get_ptr(s2)); (void)v2;
  GListModel* v3 = (GListModel*)(get_ptr(s3)); (void)v3;
  gtk_source_completion_provider_refilter(v1, v2, v3);
  return R_NilValue;
}


SEXP R_gtk_source_completion_snippets_new(void) {
  RGTK4_REQUIRE_INIT();

  gconstpointer _ret = (gconstpointer)gtk_source_completion_snippets_new();
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("CompletionSnippets"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_completion_words_new(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  const char* v1 = (s1 != R_NilValue) ? (const char*)(CHAR(STRING_ELT(s1,0))) : NULL; (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_completion_words_new(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("CompletionWords"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_completion_words_register(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletionWords* v1 = (GtkSourceCompletionWords*)(get_ptr(s1)); (void)v1;
  GtkTextBuffer* v2 = (GtkTextBuffer*)(get_ptr(s2)); (void)v2;
  gtk_source_completion_words_register(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_completion_words_unregister(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceCompletionWords* v1 = (GtkSourceCompletionWords*)(get_ptr(s1)); (void)v1;
  GtkTextBuffer* v2 = (GtkTextBuffer*)(get_ptr(s2)); (void)v2;
  gtk_source_completion_words_unregister(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_encoding_copy(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  const GtkSourceEncoding* v1 = (const GtkSourceEncoding*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_encoding_copy(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Encoding"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_encoding_free(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceEncoding* v1 = (GtkSourceEncoding*)(get_ptr(s1)); (void)v1;
  gtk_source_encoding_free(v1);
  return R_NilValue;
}


SEXP R_gtk_source_encoding_get_charset(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  const GtkSourceEncoding* v1 = (const GtkSourceEncoding*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_encoding_get_charset(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_encoding_get_name(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  const GtkSourceEncoding* v1 = (const GtkSourceEncoding*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_encoding_get_name(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_encoding_to_string(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  const GtkSourceEncoding* v1 = (const GtkSourceEncoding*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_encoding_to_string(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_encoding_get_all(void) {
  RGTK4_REQUIRE_INIT();

  gconstpointer _ret = (gconstpointer)gtk_source_encoding_get_all();
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("GLib.SList"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_encoding_get_current(void) {
  RGTK4_REQUIRE_INIT();

  gconstpointer _ret = (gconstpointer)gtk_source_encoding_get_current();
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Encoding"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_encoding_get_default_candidates(void) {
  RGTK4_REQUIRE_INIT();

  gconstpointer _ret = (gconstpointer)gtk_source_encoding_get_default_candidates();
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("GLib.SList"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_encoding_get_from_charset(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  const char* v1 = (const char*)(CHAR(STRING_ELT(s1,0))); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_encoding_get_from_charset(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Encoding"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_encoding_get_utf8(void) {
  RGTK4_REQUIRE_INIT();

  gconstpointer _ret = (gconstpointer)gtk_source_encoding_get_utf8();
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Encoding"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_file_new(void) {
  RGTK4_REQUIRE_INIT();

  gconstpointer _ret = (gconstpointer)gtk_source_file_new();
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("File"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_file_check_file_on_disk(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceFile* v1 = (GtkSourceFile*)(get_ptr(s1)); (void)v1;
  gtk_source_file_check_file_on_disk(v1);
  return R_NilValue;
}


SEXP R_gtk_source_file_get_compression_type(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceFile* v1 = (GtkSourceFile*)(get_ptr(s1)); (void)v1;
  GtkSourceCompressionType _ret = (GtkSourceCompressionType)gtk_source_file_get_compression_type(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, tag_pointer(R_MakeExternalPtr((void*)(_ret), R_NilValue, R_NilValue), "CompressionType"));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("CompressionType"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_file_get_encoding(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceFile* v1 = (GtkSourceFile*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_file_get_encoding(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Encoding"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_file_get_location(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceFile* v1 = (GtkSourceFile*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_file_get_location(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Gio.File"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_file_get_newline_type(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceFile* v1 = (GtkSourceFile*)(get_ptr(s1)); (void)v1;
  GtkSourceNewlineType _ret = (GtkSourceNewlineType)gtk_source_file_get_newline_type(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, tag_pointer(R_MakeExternalPtr((void*)(_ret), R_NilValue, R_NilValue), "NewlineType"));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("NewlineType"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_file_is_deleted(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceFile* v1 = (GtkSourceFile*)(get_ptr(s1)); (void)v1;
  gboolean _ret = (gboolean)gtk_source_file_is_deleted(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_file_is_externally_modified(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceFile* v1 = (GtkSourceFile*)(get_ptr(s1)); (void)v1;
  gboolean _ret = (gboolean)gtk_source_file_is_externally_modified(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_file_is_local(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceFile* v1 = (GtkSourceFile*)(get_ptr(s1)); (void)v1;
  gboolean _ret = (gboolean)gtk_source_file_is_local(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_file_is_readonly(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceFile* v1 = (GtkSourceFile*)(get_ptr(s1)); (void)v1;
  gboolean _ret = (gboolean)gtk_source_file_is_readonly(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_file_set_location(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceFile* v1 = (GtkSourceFile*)(get_ptr(s1)); (void)v1;
  GFile* v2 = (s2 != R_NilValue) ? (GFile*)(get_ptr(s2)) : NULL; (void)v2;
  gtk_source_file_set_location(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_file_loader_new(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceBuffer* v1 = (GtkSourceBuffer*)(get_ptr(s1)); (void)v1;
  GtkSourceFile* v2 = (GtkSourceFile*)(get_ptr(s2)); (void)v2;
  gconstpointer _ret = (gconstpointer)gtk_source_file_loader_new(v1, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("FileLoader"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_file_loader_new_from_stream(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceBuffer* v1 = (GtkSourceBuffer*)(get_ptr(s1)); (void)v1;
  GtkSourceFile* v2 = (GtkSourceFile*)(get_ptr(s2)); (void)v2;
  GInputStream* v3 = (GInputStream*)(get_ptr(s3)); (void)v3;
  gconstpointer _ret = (gconstpointer)gtk_source_file_loader_new_from_stream(v1, v2, v3);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("FileLoader"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_file_loader_get_buffer(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceFileLoader* v1 = (GtkSourceFileLoader*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_file_loader_get_buffer(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Buffer"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_file_loader_get_compression_type(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceFileLoader* v1 = (GtkSourceFileLoader*)(get_ptr(s1)); (void)v1;
  GtkSourceCompressionType _ret = (GtkSourceCompressionType)gtk_source_file_loader_get_compression_type(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, tag_pointer(R_MakeExternalPtr((void*)(_ret), R_NilValue, R_NilValue), "CompressionType"));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("CompressionType"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_file_loader_get_encoding(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceFileLoader* v1 = (GtkSourceFileLoader*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_file_loader_get_encoding(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Encoding"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_file_loader_get_file(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceFileLoader* v1 = (GtkSourceFileLoader*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_file_loader_get_file(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("File"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_file_loader_get_input_stream(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceFileLoader* v1 = (GtkSourceFileLoader*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_file_loader_get_input_stream(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Gio.InputStream"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_file_loader_get_location(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceFileLoader* v1 = (GtkSourceFileLoader*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_file_loader_get_location(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Gio.File"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_file_loader_get_newline_type(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceFileLoader* v1 = (GtkSourceFileLoader*)(get_ptr(s1)); (void)v1;
  GtkSourceNewlineType _ret = (GtkSourceNewlineType)gtk_source_file_loader_get_newline_type(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, tag_pointer(R_MakeExternalPtr((void*)(_ret), R_NilValue, R_NilValue), "NewlineType"));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("NewlineType"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_file_loader_load_async(SEXP s1, SEXP s2, SEXP s3, SEXP s4, SEXP s5) {
  RGTK4_REQUIRE_INIT();
  GtkSourceFileLoader* v1 = (GtkSourceFileLoader*)(get_ptr(s1)); (void)v1;
  gint v2 = (gint)((gint)_unbox_numeric(s2)); (void)v2;
  GCancellable* v3 = (s3 != R_NilValue) ? (GCancellable*)(get_ptr(s3)) : NULL; (void)v3;
  RCallbackClosure *_cb_closure_4 = (s4 == R_NilValue) ? NULL : rgtk4_closure_new(s4); (void)_cb_closure_4;
  RCallbackClosure *_cb_closure_7 = (s5 == R_NilValue) ? NULL : rgtk4_closure_new(s5); (void)_cb_closure_7;
  gtk_source_file_loader_load_async(v1, v2, v3, (GFileProgressCallback)(_cb_closure_4 ? _rgtk4_cb_FileProgressCallback : NULL), _cb_closure_4, rgtk4_closure_free, (GAsyncReadyCallback)(_cb_closure_7 ? _rgtk4_cb_AsyncReadyCallback : NULL), _cb_closure_7);
  return R_NilValue;
}


SEXP R_gtk_source_file_loader_load_finish(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceFileLoader* v1 = (GtkSourceFileLoader*)(get_ptr(s1)); (void)v1;
  GAsyncResult* v2 = (GAsyncResult*)(get_ptr(s2)); (void)v2;
  GError *_err = NULL;
  gboolean _ret = (gboolean)gtk_source_file_loader_load_finish(v1, v2, &_err);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_file_loader_set_candidate_encodings(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceFileLoader* v1 = (GtkSourceFileLoader*)(get_ptr(s1)); (void)v1;
  GSList* v2 = (GSList*)(get_ptr(s2)); (void)v2;
  gtk_source_file_loader_set_candidate_encodings(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_file_loader_error_quark(void) {
  RGTK4_REQUIRE_INIT();

  GQuark _ret = (GQuark)gtk_source_file_loader_error_quark();
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("GLib.Quark"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_file_saver_new(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceBuffer* v1 = (GtkSourceBuffer*)(get_ptr(s1)); (void)v1;
  GtkSourceFile* v2 = (GtkSourceFile*)(get_ptr(s2)); (void)v2;
  gconstpointer _ret = (gconstpointer)gtk_source_file_saver_new(v1, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("FileSaver"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_file_saver_new_with_target(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceBuffer* v1 = (GtkSourceBuffer*)(get_ptr(s1)); (void)v1;
  GtkSourceFile* v2 = (GtkSourceFile*)(get_ptr(s2)); (void)v2;
  GFile* v3 = (GFile*)(get_ptr(s3)); (void)v3;
  gconstpointer _ret = (gconstpointer)gtk_source_file_saver_new_with_target(v1, v2, v3);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("FileSaver"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_file_saver_get_buffer(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceFileSaver* v1 = (GtkSourceFileSaver*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_file_saver_get_buffer(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Buffer"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_file_saver_get_compression_type(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceFileSaver* v1 = (GtkSourceFileSaver*)(get_ptr(s1)); (void)v1;
  GtkSourceCompressionType _ret = (GtkSourceCompressionType)gtk_source_file_saver_get_compression_type(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, tag_pointer(R_MakeExternalPtr((void*)(_ret), R_NilValue, R_NilValue), "CompressionType"));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("CompressionType"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_file_saver_get_encoding(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceFileSaver* v1 = (GtkSourceFileSaver*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_file_saver_get_encoding(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Encoding"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_file_saver_get_file(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceFileSaver* v1 = (GtkSourceFileSaver*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_file_saver_get_file(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("File"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_file_saver_get_flags(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceFileSaver* v1 = (GtkSourceFileSaver*)(get_ptr(s1)); (void)v1;
  GtkSourceFileSaverFlags _ret = (GtkSourceFileSaverFlags)gtk_source_file_saver_get_flags(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, tag_pointer(R_MakeExternalPtr((void*)(_ret), R_NilValue, R_NilValue), "FileSaverFlags"));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("FileSaverFlags"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_file_saver_get_location(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceFileSaver* v1 = (GtkSourceFileSaver*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_file_saver_get_location(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Gio.File"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_file_saver_get_newline_type(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceFileSaver* v1 = (GtkSourceFileSaver*)(get_ptr(s1)); (void)v1;
  GtkSourceNewlineType _ret = (GtkSourceNewlineType)gtk_source_file_saver_get_newline_type(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, tag_pointer(R_MakeExternalPtr((void*)(_ret), R_NilValue, R_NilValue), "NewlineType"));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("NewlineType"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_file_saver_save_async(SEXP s1, SEXP s2, SEXP s3, SEXP s4, SEXP s5) {
  RGTK4_REQUIRE_INIT();
  GtkSourceFileSaver* v1 = (GtkSourceFileSaver*)(get_ptr(s1)); (void)v1;
  gint v2 = (gint)((gint)_unbox_numeric(s2)); (void)v2;
  GCancellable* v3 = (s3 != R_NilValue) ? (GCancellable*)(get_ptr(s3)) : NULL; (void)v3;
  RCallbackClosure *_cb_closure_4 = (s4 == R_NilValue) ? NULL : rgtk4_closure_new(s4); (void)_cb_closure_4;
  RCallbackClosure *_cb_closure_7 = (s5 == R_NilValue) ? NULL : rgtk4_closure_new(s5); (void)_cb_closure_7;
  gtk_source_file_saver_save_async(v1, v2, v3, (GFileProgressCallback)(_cb_closure_4 ? _rgtk4_cb_FileProgressCallback : NULL), _cb_closure_4, rgtk4_closure_free, (GAsyncReadyCallback)(_cb_closure_7 ? _rgtk4_cb_AsyncReadyCallback : NULL), _cb_closure_7);
  return R_NilValue;
}


SEXP R_gtk_source_file_saver_save_finish(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceFileSaver* v1 = (GtkSourceFileSaver*)(get_ptr(s1)); (void)v1;
  GAsyncResult* v2 = (GAsyncResult*)(get_ptr(s2)); (void)v2;
  GError *_err = NULL;
  gboolean _ret = (gboolean)gtk_source_file_saver_save_finish(v1, v2, &_err);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_file_saver_set_compression_type(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceFileSaver* v1 = (GtkSourceFileSaver*)(get_ptr(s1)); (void)v1;
  GtkSourceCompressionType v2 = (GtkSourceCompressionType)((GtkSourceCompressionType)(TYPEOF(s2)==EXTPTRSXP ? (size_t)R_ExternalPtrAddr(s2) : INTEGER(s2)[0])); (void)v2;
  gtk_source_file_saver_set_compression_type(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_file_saver_set_encoding(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceFileSaver* v1 = (GtkSourceFileSaver*)(get_ptr(s1)); (void)v1;
  const GtkSourceEncoding* v2 = (s2 != R_NilValue) ? (const GtkSourceEncoding*)(get_ptr(s2)) : NULL; (void)v2;
  gtk_source_file_saver_set_encoding(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_file_saver_set_flags(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceFileSaver* v1 = (GtkSourceFileSaver*)(get_ptr(s1)); (void)v1;
  GtkSourceFileSaverFlags v2 = (GtkSourceFileSaverFlags)((GtkSourceFileSaverFlags)(TYPEOF(s2)==EXTPTRSXP ? (size_t)R_ExternalPtrAddr(s2) : INTEGER(s2)[0])); (void)v2;
  gtk_source_file_saver_set_flags(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_file_saver_set_newline_type(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceFileSaver* v1 = (GtkSourceFileSaver*)(get_ptr(s1)); (void)v1;
  GtkSourceNewlineType v2 = (GtkSourceNewlineType)((GtkSourceNewlineType)(TYPEOF(s2)==EXTPTRSXP ? (size_t)R_ExternalPtrAddr(s2) : INTEGER(s2)[0])); (void)v2;
  gtk_source_file_saver_set_newline_type(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_file_saver_error_quark(void) {
  RGTK4_REQUIRE_INIT();

  GQuark _ret = (GQuark)gtk_source_file_saver_error_quark();
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("GLib.Quark"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_gutter_get_view(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutter* v1 = (GtkSourceGutter*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_gutter_get_view(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("View"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_gutter_insert(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutter* v1 = (GtkSourceGutter*)(get_ptr(s1)); (void)v1;
  GtkSourceGutterRenderer* v2 = (GtkSourceGutterRenderer*)(get_ptr(s2)); (void)v2;
  gint v3 = (gint)((gint)_unbox_numeric(s3)); (void)v3;
  gboolean _ret = (gboolean)gtk_source_gutter_insert(v1, v2, v3);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_gutter_remove(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutter* v1 = (GtkSourceGutter*)(get_ptr(s1)); (void)v1;
  GtkSourceGutterRenderer* v2 = (GtkSourceGutterRenderer*)(get_ptr(s2)); (void)v2;
  gtk_source_gutter_remove(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_gutter_reorder(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutter* v1 = (GtkSourceGutter*)(get_ptr(s1)); (void)v1;
  GtkSourceGutterRenderer* v2 = (GtkSourceGutterRenderer*)(get_ptr(s2)); (void)v2;
  gint v3 = (gint)((gint)_unbox_numeric(s3)); (void)v3;
  gtk_source_gutter_reorder(v1, v2, v3);
  return R_NilValue;
}


SEXP R_gtk_source_gutter_lines_add_class(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterLines* v1 = (GtkSourceGutterLines*)(get_ptr(s1)); (void)v1;
  guint v2 = (guint)((guint)_unbox_numeric(s2)); (void)v2;
  const char* v3 = (const char*)(CHAR(STRING_ELT(s3,0))); (void)v3;
  gtk_source_gutter_lines_add_class(v1, v2, v3);
  return R_NilValue;
}


SEXP R_gtk_source_gutter_lines_add_qclass(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterLines* v1 = (GtkSourceGutterLines*)(get_ptr(s1)); (void)v1;
  guint v2 = (guint)((guint)_unbox_numeric(s2)); (void)v2;
  GQuark v3 = (GQuark)((GQuark)(TYPEOF(s3)==EXTPTRSXP ? (size_t)R_ExternalPtrAddr(s3) : INTEGER(s3)[0])); (void)v3;
  gtk_source_gutter_lines_add_qclass(v1, v2, v3);
  return R_NilValue;
}


SEXP R_gtk_source_gutter_lines_get_buffer(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterLines* v1 = (GtkSourceGutterLines*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_gutter_lines_get_buffer(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Gtk.TextBuffer"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_gutter_lines_get_first(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterLines* v1 = (GtkSourceGutterLines*)(get_ptr(s1)); (void)v1;
  guint _ret = (guint)gtk_source_gutter_lines_get_first(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("guint"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_gutter_lines_get_iter_at_line(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterLines* v1 = (GtkSourceGutterLines*)(get_ptr(s1)); (void)v1;
  GtkTextIter _out_iter = {0}; (void)_out_iter;
  guint v2 = (guint)((guint)_unbox_numeric(s2)); (void)v2;
  gtk_source_gutter_lines_get_iter_at_line(v1, &_out_iter, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, make_boxed_struct(&_out_iter, sizeof(GtkTextIter), "GtkTextIter"));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Gtk.TextIter"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("iter"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_gutter_lines_get_last(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterLines* v1 = (GtkSourceGutterLines*)(get_ptr(s1)); (void)v1;
  guint _ret = (guint)gtk_source_gutter_lines_get_last(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("guint"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_gutter_lines_get_line_yrange(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterLines* v1 = (GtkSourceGutterLines*)(get_ptr(s1)); (void)v1;
  guint v2 = (guint)((guint)_unbox_numeric(s2)); (void)v2;
  GtkSourceGutterRendererAlignmentMode v3 = (GtkSourceGutterRendererAlignmentMode)((GtkSourceGutterRendererAlignmentMode)(TYPEOF(s3)==EXTPTRSXP ? (size_t)R_ExternalPtrAddr(s3) : INTEGER(s3)[0])); (void)v3;
  gint _out_y = 0; (void)_out_y;
  gint _out_height = 0; (void)_out_height;
  gtk_source_gutter_lines_get_line_yrange(v1, v2, v3, &_out_y, &_out_height);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 2));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 2));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_out_y)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gint"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("y"));
  SET_VECTOR_ELT(_ans, 1, Rf_ScalarInteger((int)(_out_height)));
  if (VECTOR_ELT(_ans, 1) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 1), Rf_install("glib_type"), Rf_mkString("gint"));
  }
  SET_STRING_ELT(_ans_names, 1, Rf_mkChar("height"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_gutter_lines_get_view(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterLines* v1 = (GtkSourceGutterLines*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_gutter_lines_get_view(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Gtk.TextView"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_gutter_lines_has_class(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterLines* v1 = (GtkSourceGutterLines*)(get_ptr(s1)); (void)v1;
  guint v2 = (guint)((guint)_unbox_numeric(s2)); (void)v2;
  const char* v3 = (const char*)(CHAR(STRING_ELT(s3,0))); (void)v3;
  gboolean _ret = (gboolean)gtk_source_gutter_lines_has_class(v1, v2, v3);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_gutter_lines_has_qclass(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterLines* v1 = (GtkSourceGutterLines*)(get_ptr(s1)); (void)v1;
  guint v2 = (guint)((guint)_unbox_numeric(s2)); (void)v2;
  GQuark v3 = (GQuark)((GQuark)(TYPEOF(s3)==EXTPTRSXP ? (size_t)R_ExternalPtrAddr(s3) : INTEGER(s3)[0])); (void)v3;
  gboolean _ret = (gboolean)gtk_source_gutter_lines_has_qclass(v1, v2, v3);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_gutter_lines_is_cursor(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterLines* v1 = (GtkSourceGutterLines*)(get_ptr(s1)); (void)v1;
  guint v2 = (guint)((guint)_unbox_numeric(s2)); (void)v2;
  gboolean _ret = (gboolean)gtk_source_gutter_lines_is_cursor(v1, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_gutter_lines_is_prelit(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterLines* v1 = (GtkSourceGutterLines*)(get_ptr(s1)); (void)v1;
  guint v2 = (guint)((guint)_unbox_numeric(s2)); (void)v2;
  gboolean _ret = (gboolean)gtk_source_gutter_lines_is_prelit(v1, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_gutter_lines_is_selected(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterLines* v1 = (GtkSourceGutterLines*)(get_ptr(s1)); (void)v1;
  guint v2 = (guint)((guint)_unbox_numeric(s2)); (void)v2;
  gboolean _ret = (gboolean)gtk_source_gutter_lines_is_selected(v1, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_gutter_lines_remove_class(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterLines* v1 = (GtkSourceGutterLines*)(get_ptr(s1)); (void)v1;
  guint v2 = (guint)((guint)_unbox_numeric(s2)); (void)v2;
  const char* v3 = (const char*)(CHAR(STRING_ELT(s3,0))); (void)v3;
  gtk_source_gutter_lines_remove_class(v1, v2, v3);
  return R_NilValue;
}


SEXP R_gtk_source_gutter_lines_remove_qclass(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterLines* v1 = (GtkSourceGutterLines*)(get_ptr(s1)); (void)v1;
  guint v2 = (guint)((guint)_unbox_numeric(s2)); (void)v2;
  GQuark v3 = (GQuark)((GQuark)(TYPEOF(s3)==EXTPTRSXP ? (size_t)R_ExternalPtrAddr(s3) : INTEGER(s3)[0])); (void)v3;
  gtk_source_gutter_lines_remove_qclass(v1, v2, v3);
  return R_NilValue;
}


SEXP R_gtk_source_gutter_renderer_activate(SEXP s1, SEXP s2, SEXP s3, SEXP s4, SEXP s5, SEXP s6) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterRenderer* v1 = (GtkSourceGutterRenderer*)(get_ptr(s1)); (void)v1;
  const GtkTextIter* v2 = (const GtkTextIter*)(get_ptr(s2)); (void)v2;
  const GdkRectangle* v3 = (const GdkRectangle*)(get_ptr(s3)); (void)v3;
  guint v4 = (guint)((guint)_unbox_numeric(s4)); (void)v4;
  GdkModifierType v5 = (GdkModifierType)((GdkModifierType)(TYPEOF(s5)==EXTPTRSXP ? (size_t)R_ExternalPtrAddr(s5) : INTEGER(s5)[0])); (void)v5;
  gint v6 = (gint)((gint)_unbox_numeric(s6)); (void)v6;
  gtk_source_gutter_renderer_activate(v1, v2, v3, v4, v5, v6);
  return R_NilValue;
}


SEXP R_gtk_source_gutter_renderer_align_cell(SEXP s1, SEXP s2, SEXP s3, SEXP s4) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterRenderer* v1 = (GtkSourceGutterRenderer*)(get_ptr(s1)); (void)v1;
  guint v2 = (guint)((guint)_unbox_numeric(s2)); (void)v2;
  gfloat v3 = (gfloat)((gfloat)_unbox_numeric(s3)); (void)v3;
  gfloat v4 = (gfloat)((gfloat)_unbox_numeric(s4)); (void)v4;
  gfloat _out_x = 0; (void)_out_x;
  gfloat _out_y = 0; (void)_out_y;
  gtk_source_gutter_renderer_align_cell(v1, v2, v3, v4, &_out_x, &_out_y);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 2));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 2));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarReal((double)(_out_x)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gfloat"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("x"));
  SET_VECTOR_ELT(_ans, 1, Rf_ScalarReal((double)(_out_y)));
  if (VECTOR_ELT(_ans, 1) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 1), Rf_install("glib_type"), Rf_mkString("gfloat"));
  }
  SET_STRING_ELT(_ans_names, 1, Rf_mkChar("y"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_gutter_renderer_get_alignment_mode(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterRenderer* v1 = (GtkSourceGutterRenderer*)(get_ptr(s1)); (void)v1;
  GtkSourceGutterRendererAlignmentMode _ret = (GtkSourceGutterRendererAlignmentMode)gtk_source_gutter_renderer_get_alignment_mode(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, tag_pointer(R_MakeExternalPtr((void*)(_ret), R_NilValue, R_NilValue), "GutterRendererAlignmentMode"));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("GutterRendererAlignmentMode"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_gutter_renderer_get_buffer(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterRenderer* v1 = (GtkSourceGutterRenderer*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_gutter_renderer_get_buffer(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Buffer"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_gutter_renderer_get_view(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterRenderer* v1 = (GtkSourceGutterRenderer*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_gutter_renderer_get_view(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("View"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_gutter_renderer_get_xalign(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterRenderer* v1 = (GtkSourceGutterRenderer*)(get_ptr(s1)); (void)v1;
  gfloat _ret = (gfloat)gtk_source_gutter_renderer_get_xalign(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarReal((double)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gfloat"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_gutter_renderer_get_xpad(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterRenderer* v1 = (GtkSourceGutterRenderer*)(get_ptr(s1)); (void)v1;
  gint _ret = (gint)gtk_source_gutter_renderer_get_xpad(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gint"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_gutter_renderer_get_yalign(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterRenderer* v1 = (GtkSourceGutterRenderer*)(get_ptr(s1)); (void)v1;
  gfloat _ret = (gfloat)gtk_source_gutter_renderer_get_yalign(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarReal((double)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gfloat"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_gutter_renderer_get_ypad(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterRenderer* v1 = (GtkSourceGutterRenderer*)(get_ptr(s1)); (void)v1;
  gint _ret = (gint)gtk_source_gutter_renderer_get_ypad(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gint"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_gutter_renderer_query_activatable(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterRenderer* v1 = (GtkSourceGutterRenderer*)(get_ptr(s1)); (void)v1;
  const GtkTextIter* v2 = (const GtkTextIter*)(get_ptr(s2)); (void)v2;
  const GdkRectangle* v3 = (const GdkRectangle*)(get_ptr(s3)); (void)v3;
  gboolean _ret = (gboolean)gtk_source_gutter_renderer_query_activatable(v1, v2, v3);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_gutter_renderer_set_alignment_mode(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterRenderer* v1 = (GtkSourceGutterRenderer*)(get_ptr(s1)); (void)v1;
  GtkSourceGutterRendererAlignmentMode v2 = (GtkSourceGutterRendererAlignmentMode)((GtkSourceGutterRendererAlignmentMode)(TYPEOF(s2)==EXTPTRSXP ? (size_t)R_ExternalPtrAddr(s2) : INTEGER(s2)[0])); (void)v2;
  gtk_source_gutter_renderer_set_alignment_mode(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_gutter_renderer_set_xalign(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterRenderer* v1 = (GtkSourceGutterRenderer*)(get_ptr(s1)); (void)v1;
  gfloat v2 = (gfloat)((gfloat)_unbox_numeric(s2)); (void)v2;
  gtk_source_gutter_renderer_set_xalign(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_gutter_renderer_set_xpad(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterRenderer* v1 = (GtkSourceGutterRenderer*)(get_ptr(s1)); (void)v1;
  gint v2 = (gint)((gint)_unbox_numeric(s2)); (void)v2;
  gtk_source_gutter_renderer_set_xpad(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_gutter_renderer_set_yalign(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterRenderer* v1 = (GtkSourceGutterRenderer*)(get_ptr(s1)); (void)v1;
  gfloat v2 = (gfloat)((gfloat)_unbox_numeric(s2)); (void)v2;
  gtk_source_gutter_renderer_set_yalign(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_gutter_renderer_set_ypad(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterRenderer* v1 = (GtkSourceGutterRenderer*)(get_ptr(s1)); (void)v1;
  gint v2 = (gint)((gint)_unbox_numeric(s2)); (void)v2;
  gtk_source_gutter_renderer_set_ypad(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_gutter_renderer_pixbuf_new(void) {
  RGTK4_REQUIRE_INIT();

  gconstpointer _ret = (gconstpointer)gtk_source_gutter_renderer_pixbuf_new();
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("GutterRenderer"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_gutter_renderer_pixbuf_get_gicon(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterRendererPixbuf* v1 = (GtkSourceGutterRendererPixbuf*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_gutter_renderer_pixbuf_get_gicon(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Gio.Icon"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_gutter_renderer_pixbuf_get_icon_name(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterRendererPixbuf* v1 = (GtkSourceGutterRendererPixbuf*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_gutter_renderer_pixbuf_get_icon_name(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_gutter_renderer_pixbuf_get_paintable(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterRendererPixbuf* v1 = (GtkSourceGutterRendererPixbuf*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_gutter_renderer_pixbuf_get_paintable(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Gdk.Paintable"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_gutter_renderer_pixbuf_get_pixbuf(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterRendererPixbuf* v1 = (GtkSourceGutterRendererPixbuf*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_gutter_renderer_pixbuf_get_pixbuf(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("GdkPixbuf.Pixbuf"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_gutter_renderer_pixbuf_overlay_paintable(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterRendererPixbuf* v1 = (GtkSourceGutterRendererPixbuf*)(get_ptr(s1)); (void)v1;
  GdkPaintable* v2 = (GdkPaintable*)(get_ptr(s2)); (void)v2;
  gtk_source_gutter_renderer_pixbuf_overlay_paintable(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_gutter_renderer_pixbuf_set_gicon(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterRendererPixbuf* v1 = (GtkSourceGutterRendererPixbuf*)(get_ptr(s1)); (void)v1;
  GIcon* v2 = (s2 != R_NilValue) ? (GIcon*)(get_ptr(s2)) : NULL; (void)v2;
  gtk_source_gutter_renderer_pixbuf_set_gicon(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_gutter_renderer_pixbuf_set_icon_name(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterRendererPixbuf* v1 = (GtkSourceGutterRendererPixbuf*)(get_ptr(s1)); (void)v1;
  const char* v2 = (s2 != R_NilValue) ? (const char*)(CHAR(STRING_ELT(s2,0))) : NULL; (void)v2;
  gtk_source_gutter_renderer_pixbuf_set_icon_name(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_gutter_renderer_pixbuf_set_paintable(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterRendererPixbuf* v1 = (GtkSourceGutterRendererPixbuf*)(get_ptr(s1)); (void)v1;
  GdkPaintable* v2 = (s2 != R_NilValue) ? (GdkPaintable*)(get_ptr(s2)) : NULL; (void)v2;
  gtk_source_gutter_renderer_pixbuf_set_paintable(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_gutter_renderer_pixbuf_set_pixbuf(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterRendererPixbuf* v1 = (GtkSourceGutterRendererPixbuf*)(get_ptr(s1)); (void)v1;
  GdkPixbuf* v2 = (s2 != R_NilValue) ? (GdkPixbuf*)(get_ptr(s2)) : NULL; (void)v2;
  gtk_source_gutter_renderer_pixbuf_set_pixbuf(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_gutter_renderer_text_new(void) {
  RGTK4_REQUIRE_INIT();

  gconstpointer _ret = (gconstpointer)gtk_source_gutter_renderer_text_new();
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("GutterRenderer"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_gutter_renderer_text_measure(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterRendererText* v1 = (GtkSourceGutterRendererText*)(get_ptr(s1)); (void)v1;
  const char* v2 = (const char*)(CHAR(STRING_ELT(s2,0))); (void)v2;
  gint _out_width = 0; (void)_out_width;
  gint _out_height = 0; (void)_out_height;
  gtk_source_gutter_renderer_text_measure(v1, v2, &_out_width, &_out_height);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 2));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 2));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_out_width)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gint"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("width"));
  SET_VECTOR_ELT(_ans, 1, Rf_ScalarInteger((int)(_out_height)));
  if (VECTOR_ELT(_ans, 1) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 1), Rf_install("glib_type"), Rf_mkString("gint"));
  }
  SET_STRING_ELT(_ans_names, 1, Rf_mkChar("height"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_gutter_renderer_text_measure_markup(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterRendererText* v1 = (GtkSourceGutterRendererText*)(get_ptr(s1)); (void)v1;
  const char* v2 = (const char*)(CHAR(STRING_ELT(s2,0))); (void)v2;
  gint _out_width = 0; (void)_out_width;
  gint _out_height = 0; (void)_out_height;
  gtk_source_gutter_renderer_text_measure_markup(v1, v2, &_out_width, &_out_height);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 2));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 2));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_out_width)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gint"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("width"));
  SET_VECTOR_ELT(_ans, 1, Rf_ScalarInteger((int)(_out_height)));
  if (VECTOR_ELT(_ans, 1) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 1), Rf_install("glib_type"), Rf_mkString("gint"));
  }
  SET_STRING_ELT(_ans_names, 1, Rf_mkChar("height"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_gutter_renderer_text_set_markup(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterRendererText* v1 = (GtkSourceGutterRendererText*)(get_ptr(s1)); (void)v1;
  const char* v2 = (const char*)(CHAR(STRING_ELT(s2,0))); (void)v2;
  gint v3 = (gint)((gint)_unbox_numeric(s3)); (void)v3;
  gtk_source_gutter_renderer_text_set_markup(v1, v2, v3);
  return R_NilValue;
}


SEXP R_gtk_source_gutter_renderer_text_set_text(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceGutterRendererText* v1 = (GtkSourceGutterRendererText*)(get_ptr(s1)); (void)v1;
  const char* v2 = (const char*)(CHAR(STRING_ELT(s2,0))); (void)v2;
  gint v3 = (gint)((gint)_unbox_numeric(s3)); (void)v3;
  gtk_source_gutter_renderer_text_set_text(v1, v2, v3);
  return R_NilValue;
}


SEXP R_gtk_source_hover_add_provider(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceHover* v1 = (GtkSourceHover*)(get_ptr(s1)); (void)v1;
  GtkSourceHoverProvider* v2 = (GtkSourceHoverProvider*)(get_ptr(s2)); (void)v2;
  gtk_source_hover_add_provider(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_hover_remove_provider(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceHover* v1 = (GtkSourceHover*)(get_ptr(s1)); (void)v1;
  GtkSourceHoverProvider* v2 = (GtkSourceHoverProvider*)(get_ptr(s2)); (void)v2;
  gtk_source_hover_remove_provider(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_hover_context_get_bounds(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceHoverContext* v1 = (GtkSourceHoverContext*)(get_ptr(s1)); (void)v1;
  GtkTextIter _out_begin = {0}; (void)_out_begin;
  GtkTextIter _out_end = {0}; (void)_out_end;
  gboolean _ret = (gboolean)gtk_source_hover_context_get_bounds(v1, &_out_begin, &_out_end);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 3));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 3));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  SET_VECTOR_ELT(_ans, 1, make_boxed_struct(&_out_begin, sizeof(GtkTextIter), "GtkTextIter"));
  if (VECTOR_ELT(_ans, 1) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 1), Rf_install("glib_type"), Rf_mkString("Gtk.TextIter"));
  }
  SET_STRING_ELT(_ans_names, 1, Rf_mkChar("begin"));
  SET_VECTOR_ELT(_ans, 2, make_boxed_struct(&_out_end, sizeof(GtkTextIter), "GtkTextIter"));
  if (VECTOR_ELT(_ans, 2) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 2), Rf_install("glib_type"), Rf_mkString("Gtk.TextIter"));
  }
  SET_STRING_ELT(_ans_names, 2, Rf_mkChar("end"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_hover_context_get_buffer(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceHoverContext* v1 = (GtkSourceHoverContext*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_hover_context_get_buffer(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Buffer"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_hover_context_get_iter(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceHoverContext* v1 = (GtkSourceHoverContext*)(get_ptr(s1)); (void)v1;
  GtkTextIter _out_iter = {0}; (void)_out_iter;
  gboolean _ret = (gboolean)gtk_source_hover_context_get_iter(v1, &_out_iter);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 2));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 2));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  SET_VECTOR_ELT(_ans, 1, make_boxed_struct(&_out_iter, sizeof(GtkTextIter), "GtkTextIter"));
  if (VECTOR_ELT(_ans, 1) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 1), Rf_install("glib_type"), Rf_mkString("Gtk.TextIter"));
  }
  SET_STRING_ELT(_ans_names, 1, Rf_mkChar("iter"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_hover_context_get_view(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceHoverContext* v1 = (GtkSourceHoverContext*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_hover_context_get_view(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("View"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_hover_display_append(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceHoverDisplay* v1 = (GtkSourceHoverDisplay*)(get_ptr(s1)); (void)v1;
  GtkWidget* v2 = (GtkWidget*)(get_ptr(s2)); (void)v2;
  gtk_source_hover_display_append(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_hover_display_insert_after(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceHoverDisplay* v1 = (GtkSourceHoverDisplay*)(get_ptr(s1)); (void)v1;
  GtkWidget* v2 = (GtkWidget*)(get_ptr(s2)); (void)v2;
  GtkWidget* v3 = (GtkWidget*)(get_ptr(s3)); (void)v3;
  gtk_source_hover_display_insert_after(v1, v2, v3);
  return R_NilValue;
}


SEXP R_gtk_source_hover_display_prepend(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceHoverDisplay* v1 = (GtkSourceHoverDisplay*)(get_ptr(s1)); (void)v1;
  GtkWidget* v2 = (GtkWidget*)(get_ptr(s2)); (void)v2;
  gtk_source_hover_display_prepend(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_hover_display_remove(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceHoverDisplay* v1 = (GtkSourceHoverDisplay*)(get_ptr(s1)); (void)v1;
  GtkWidget* v2 = (GtkWidget*)(get_ptr(s2)); (void)v2;
  gtk_source_hover_display_remove(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_hover_provider_populate_async(SEXP s1, SEXP s2, SEXP s3, SEXP s4, SEXP s5) {
  RGTK4_REQUIRE_INIT();
  GtkSourceHoverProvider* v1 = (GtkSourceHoverProvider*)(get_ptr(s1)); (void)v1;
  GtkSourceHoverContext* v2 = (GtkSourceHoverContext*)(get_ptr(s2)); (void)v2;
  GtkSourceHoverDisplay* v3 = (GtkSourceHoverDisplay*)(get_ptr(s3)); (void)v3;
  GCancellable* v4 = (s4 != R_NilValue) ? (GCancellable*)(get_ptr(s4)) : NULL; (void)v4;
  RCallbackClosure *_cb_closure_5 = (s5 == R_NilValue) ? NULL : rgtk4_closure_new(s5); (void)_cb_closure_5;
  gtk_source_hover_provider_populate_async(v1, v2, v3, v4, (GAsyncReadyCallback)(_cb_closure_5 ? _rgtk4_cb_AsyncReadyCallback : NULL), _cb_closure_5);
  return R_NilValue;
}


SEXP R_gtk_source_hover_provider_populate_finish(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceHoverProvider* v1 = (GtkSourceHoverProvider*)(get_ptr(s1)); (void)v1;
  GAsyncResult* v2 = (GAsyncResult*)(get_ptr(s2)); (void)v2;
  GError *_err = NULL;
  gboolean _ret = (gboolean)gtk_source_hover_provider_populate_finish(v1, v2, &_err);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_indenter_indent(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceIndenter* v1 = (GtkSourceIndenter*)(get_ptr(s1)); (void)v1;
  GtkSourceView* v2 = (GtkSourceView*)(get_ptr(s2)); (void)v2;
  GtkTextIter _out_iter = {0}; (void)_out_iter;
  gtk_source_indenter_indent(v1, v2, &_out_iter);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, make_boxed_struct(&_out_iter, sizeof(GtkTextIter), "GtkTextIter"));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Gtk.TextIter"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("iter"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_indenter_is_trigger(SEXP s1, SEXP s2, SEXP s3, SEXP s4, SEXP s5) {
  RGTK4_REQUIRE_INIT();
  GtkSourceIndenter* v1 = (GtkSourceIndenter*)(get_ptr(s1)); (void)v1;
  GtkSourceView* v2 = (GtkSourceView*)(get_ptr(s2)); (void)v2;
  const GtkTextIter* v3 = (const GtkTextIter*)(get_ptr(s3)); (void)v3;
  GdkModifierType v4 = (GdkModifierType)((GdkModifierType)(TYPEOF(s4)==EXTPTRSXP ? (size_t)R_ExternalPtrAddr(s4) : INTEGER(s4)[0])); (void)v4;
  guint v5 = (guint)((guint)_unbox_numeric(s5)); (void)v5;
  gboolean _ret = (gboolean)gtk_source_indenter_is_trigger(v1, v2, v3, v4, v5);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_language_get_globs(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceLanguage* v1 = (GtkSourceLanguage*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_language_get_globs(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : tag_pointer(Rf_mkString(_ret ? (const char*)_ret : ""), "utf8"));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_language_get_hidden(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceLanguage* v1 = (GtkSourceLanguage*)(get_ptr(s1)); (void)v1;
  gboolean _ret = (gboolean)gtk_source_language_get_hidden(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_language_get_id(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceLanguage* v1 = (GtkSourceLanguage*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_language_get_id(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_language_get_metadata(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceLanguage* v1 = (GtkSourceLanguage*)(get_ptr(s1)); (void)v1;
  const char* v2 = (const char*)(CHAR(STRING_ELT(s2,0))); (void)v2;
  gconstpointer _ret = (gconstpointer)gtk_source_language_get_metadata(v1, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_language_get_mime_types(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceLanguage* v1 = (GtkSourceLanguage*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_language_get_mime_types(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : tag_pointer(Rf_mkString(_ret ? (const char*)_ret : ""), "utf8"));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_language_get_name(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceLanguage* v1 = (GtkSourceLanguage*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_language_get_name(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_language_get_section(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceLanguage* v1 = (GtkSourceLanguage*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_language_get_section(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_language_get_style_fallback(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceLanguage* v1 = (GtkSourceLanguage*)(get_ptr(s1)); (void)v1;
  const char* v2 = (const char*)(CHAR(STRING_ELT(s2,0))); (void)v2;
  gconstpointer _ret = (gconstpointer)gtk_source_language_get_style_fallback(v1, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_language_get_style_ids(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceLanguage* v1 = (GtkSourceLanguage*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_language_get_style_ids(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : tag_pointer(Rf_mkString(_ret ? (const char*)_ret : ""), "utf8"));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_language_get_style_name(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceLanguage* v1 = (GtkSourceLanguage*)(get_ptr(s1)); (void)v1;
  const char* v2 = (const char*)(CHAR(STRING_ELT(s2,0))); (void)v2;
  gconstpointer _ret = (gconstpointer)gtk_source_language_get_style_name(v1, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_language_manager_new(void) {
  RGTK4_REQUIRE_INIT();

  gconstpointer _ret = (gconstpointer)gtk_source_language_manager_new();
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("LanguageManager"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_language_manager_get_default(void) {
  RGTK4_REQUIRE_INIT();

  gconstpointer _ret = (gconstpointer)gtk_source_language_manager_get_default();
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("LanguageManager"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_language_manager_get_language(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceLanguageManager* v1 = (GtkSourceLanguageManager*)(get_ptr(s1)); (void)v1;
  const char* v2 = (const char*)(CHAR(STRING_ELT(s2,0))); (void)v2;
  gconstpointer _ret = (gconstpointer)gtk_source_language_manager_get_language(v1, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Language"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_language_manager_get_language_ids(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceLanguageManager* v1 = (GtkSourceLanguageManager*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_language_manager_get_language_ids(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_language_manager_get_search_path(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceLanguageManager* v1 = (GtkSourceLanguageManager*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_language_manager_get_search_path(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_language_manager_guess_language(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceLanguageManager* v1 = (GtkSourceLanguageManager*)(get_ptr(s1)); (void)v1;
  const char* v2 = (s2 != R_NilValue) ? (const char*)(CHAR(STRING_ELT(s2,0))) : NULL; (void)v2;
  const char* v3 = (s3 != R_NilValue) ? (const char*)(CHAR(STRING_ELT(s3,0))) : NULL; (void)v3;
  gconstpointer _ret = (gconstpointer)gtk_source_language_manager_guess_language(v1, v2, v3);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Language"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_language_manager_set_search_path(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceLanguageManager* v1 = (GtkSourceLanguageManager*)(get_ptr(s1)); (void)v1;
  const gchar* const* v2 = (s2 != R_NilValue) ? (const gchar* const*)(get_ptr(s2)) : NULL; (void)v2;
  gtk_source_language_manager_set_search_path(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_map_new(void) {
  RGTK4_REQUIRE_INIT();

  gconstpointer _ret = (gconstpointer)gtk_source_map_new();
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Gtk.Widget"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_map_get_view(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceMap* v1 = (GtkSourceMap*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_map_get_view(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("View"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_map_set_view(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceMap* v1 = (GtkSourceMap*)(get_ptr(s1)); (void)v1;
  GtkSourceView* v2 = (GtkSourceView*)(get_ptr(s2)); (void)v2;
  gtk_source_map_set_view(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_mark_new(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  const char* v1 = (s1 != R_NilValue) ? (const char*)(CHAR(STRING_ELT(s1,0))) : NULL; (void)v1;
  const char* v2 = (const char*)(CHAR(STRING_ELT(s2,0))); (void)v2;
  gconstpointer _ret = (gconstpointer)gtk_source_mark_new(v1, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Mark"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_mark_get_category(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceMark* v1 = (GtkSourceMark*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_mark_get_category(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_mark_next(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceMark* v1 = (GtkSourceMark*)(get_ptr(s1)); (void)v1;
  const char* v2 = (s2 != R_NilValue) ? (const char*)(CHAR(STRING_ELT(s2,0))) : NULL; (void)v2;
  gconstpointer _ret = (gconstpointer)gtk_source_mark_next(v1, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Mark"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_mark_prev(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceMark* v1 = (GtkSourceMark*)(get_ptr(s1)); (void)v1;
  const char* v2 = (s2 != R_NilValue) ? (const char*)(CHAR(STRING_ELT(s2,0))) : NULL; (void)v2;
  gconstpointer _ret = (gconstpointer)gtk_source_mark_prev(v1, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Mark"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_mark_attributes_new(void) {
  RGTK4_REQUIRE_INIT();

  gconstpointer _ret = (gconstpointer)gtk_source_mark_attributes_new();
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("MarkAttributes"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_mark_attributes_get_background(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceMarkAttributes* v1 = (GtkSourceMarkAttributes*)(get_ptr(s1)); (void)v1;
  GdkRGBA _out_background = {0}; (void)_out_background;
  gboolean _ret = (gboolean)gtk_source_mark_attributes_get_background(v1, &_out_background);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 2));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 2));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  SET_VECTOR_ELT(_ans, 1, make_boxed_struct(&_out_background, sizeof(GdkRGBA), "GdkRGBA"));
  if (VECTOR_ELT(_ans, 1) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 1), Rf_install("glib_type"), Rf_mkString("Gdk.RGBA"));
  }
  SET_STRING_ELT(_ans_names, 1, Rf_mkChar("background"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_mark_attributes_get_gicon(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceMarkAttributes* v1 = (GtkSourceMarkAttributes*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_mark_attributes_get_gicon(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Gio.Icon"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_mark_attributes_get_icon_name(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceMarkAttributes* v1 = (GtkSourceMarkAttributes*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_mark_attributes_get_icon_name(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_mark_attributes_get_pixbuf(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceMarkAttributes* v1 = (GtkSourceMarkAttributes*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_mark_attributes_get_pixbuf(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("GdkPixbuf.Pixbuf"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_mark_attributes_get_tooltip_markup(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceMarkAttributes* v1 = (GtkSourceMarkAttributes*)(get_ptr(s1)); (void)v1;
  GtkSourceMark* v2 = (GtkSourceMark*)(get_ptr(s2)); (void)v2;
  gconstpointer _ret = (gconstpointer)gtk_source_mark_attributes_get_tooltip_markup(v1, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_mark_attributes_get_tooltip_text(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceMarkAttributes* v1 = (GtkSourceMarkAttributes*)(get_ptr(s1)); (void)v1;
  GtkSourceMark* v2 = (GtkSourceMark*)(get_ptr(s2)); (void)v2;
  gconstpointer _ret = (gconstpointer)gtk_source_mark_attributes_get_tooltip_text(v1, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_mark_attributes_render_icon(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceMarkAttributes* v1 = (GtkSourceMarkAttributes*)(get_ptr(s1)); (void)v1;
  GtkWidget* v2 = (GtkWidget*)(get_ptr(s2)); (void)v2;
  gint v3 = (gint)((gint)_unbox_numeric(s3)); (void)v3;
  gconstpointer _ret = (gconstpointer)gtk_source_mark_attributes_render_icon(v1, v2, v3);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Gdk.Paintable"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_mark_attributes_set_background(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceMarkAttributes* v1 = (GtkSourceMarkAttributes*)(get_ptr(s1)); (void)v1;
  const GdkRGBA* v2 = (const GdkRGBA*)(get_ptr(s2)); (void)v2;
  gtk_source_mark_attributes_set_background(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_mark_attributes_set_gicon(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceMarkAttributes* v1 = (GtkSourceMarkAttributes*)(get_ptr(s1)); (void)v1;
  GIcon* v2 = (GIcon*)(get_ptr(s2)); (void)v2;
  gtk_source_mark_attributes_set_gicon(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_mark_attributes_set_icon_name(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceMarkAttributes* v1 = (GtkSourceMarkAttributes*)(get_ptr(s1)); (void)v1;
  const char* v2 = (const char*)(CHAR(STRING_ELT(s2,0))); (void)v2;
  gtk_source_mark_attributes_set_icon_name(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_mark_attributes_set_pixbuf(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceMarkAttributes* v1 = (GtkSourceMarkAttributes*)(get_ptr(s1)); (void)v1;
  const GdkPixbuf* v2 = (const GdkPixbuf*)(get_ptr(s2)); (void)v2;
  gtk_source_mark_attributes_set_pixbuf(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_print_compositor_new(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceBuffer* v1 = (GtkSourceBuffer*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_print_compositor_new(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("PrintCompositor"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_print_compositor_new_from_view(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_print_compositor_new_from_view(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("PrintCompositor"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_print_compositor_draw_page(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourcePrintCompositor* v1 = (GtkSourcePrintCompositor*)(get_ptr(s1)); (void)v1;
  GtkPrintContext* v2 = (GtkPrintContext*)(get_ptr(s2)); (void)v2;
  gint v3 = (gint)((gint)_unbox_numeric(s3)); (void)v3;
  gtk_source_print_compositor_draw_page(v1, v2, v3);
  return R_NilValue;
}


SEXP R_gtk_source_print_compositor_get_body_font_name(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourcePrintCompositor* v1 = (GtkSourcePrintCompositor*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_print_compositor_get_body_font_name(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_print_compositor_get_bottom_margin(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourcePrintCompositor* v1 = (GtkSourcePrintCompositor*)(get_ptr(s1)); (void)v1;
  GtkUnit v2 = (GtkUnit)((GtkUnit)(TYPEOF(s2)==EXTPTRSXP ? (size_t)R_ExternalPtrAddr(s2) : INTEGER(s2)[0])); (void)v2;
  gdouble _ret = (gdouble)gtk_source_print_compositor_get_bottom_margin(v1, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarReal((double)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gdouble"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_print_compositor_get_buffer(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourcePrintCompositor* v1 = (GtkSourcePrintCompositor*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_print_compositor_get_buffer(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Buffer"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_print_compositor_get_footer_font_name(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourcePrintCompositor* v1 = (GtkSourcePrintCompositor*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_print_compositor_get_footer_font_name(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_print_compositor_get_header_font_name(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourcePrintCompositor* v1 = (GtkSourcePrintCompositor*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_print_compositor_get_header_font_name(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_print_compositor_get_highlight_syntax(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourcePrintCompositor* v1 = (GtkSourcePrintCompositor*)(get_ptr(s1)); (void)v1;
  gboolean _ret = (gboolean)gtk_source_print_compositor_get_highlight_syntax(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_print_compositor_get_left_margin(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourcePrintCompositor* v1 = (GtkSourcePrintCompositor*)(get_ptr(s1)); (void)v1;
  GtkUnit v2 = (GtkUnit)((GtkUnit)(TYPEOF(s2)==EXTPTRSXP ? (size_t)R_ExternalPtrAddr(s2) : INTEGER(s2)[0])); (void)v2;
  gdouble _ret = (gdouble)gtk_source_print_compositor_get_left_margin(v1, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarReal((double)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gdouble"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_print_compositor_get_line_numbers_font_name(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourcePrintCompositor* v1 = (GtkSourcePrintCompositor*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_print_compositor_get_line_numbers_font_name(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_print_compositor_get_n_pages(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourcePrintCompositor* v1 = (GtkSourcePrintCompositor*)(get_ptr(s1)); (void)v1;
  gint _ret = (gint)gtk_source_print_compositor_get_n_pages(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gint"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_print_compositor_get_pagination_progress(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourcePrintCompositor* v1 = (GtkSourcePrintCompositor*)(get_ptr(s1)); (void)v1;
  gdouble _ret = (gdouble)gtk_source_print_compositor_get_pagination_progress(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarReal((double)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gdouble"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_print_compositor_get_print_footer(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourcePrintCompositor* v1 = (GtkSourcePrintCompositor*)(get_ptr(s1)); (void)v1;
  gboolean _ret = (gboolean)gtk_source_print_compositor_get_print_footer(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_print_compositor_get_print_header(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourcePrintCompositor* v1 = (GtkSourcePrintCompositor*)(get_ptr(s1)); (void)v1;
  gboolean _ret = (gboolean)gtk_source_print_compositor_get_print_header(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_print_compositor_get_print_line_numbers(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourcePrintCompositor* v1 = (GtkSourcePrintCompositor*)(get_ptr(s1)); (void)v1;
  guint _ret = (guint)gtk_source_print_compositor_get_print_line_numbers(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("guint"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_print_compositor_get_right_margin(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourcePrintCompositor* v1 = (GtkSourcePrintCompositor*)(get_ptr(s1)); (void)v1;
  GtkUnit v2 = (GtkUnit)((GtkUnit)(TYPEOF(s2)==EXTPTRSXP ? (size_t)R_ExternalPtrAddr(s2) : INTEGER(s2)[0])); (void)v2;
  gdouble _ret = (gdouble)gtk_source_print_compositor_get_right_margin(v1, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarReal((double)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gdouble"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_print_compositor_get_tab_width(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourcePrintCompositor* v1 = (GtkSourcePrintCompositor*)(get_ptr(s1)); (void)v1;
  guint _ret = (guint)gtk_source_print_compositor_get_tab_width(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("guint"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_print_compositor_get_top_margin(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourcePrintCompositor* v1 = (GtkSourcePrintCompositor*)(get_ptr(s1)); (void)v1;
  GtkUnit v2 = (GtkUnit)((GtkUnit)(TYPEOF(s2)==EXTPTRSXP ? (size_t)R_ExternalPtrAddr(s2) : INTEGER(s2)[0])); (void)v2;
  gdouble _ret = (gdouble)gtk_source_print_compositor_get_top_margin(v1, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarReal((double)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gdouble"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_print_compositor_get_wrap_mode(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourcePrintCompositor* v1 = (GtkSourcePrintCompositor*)(get_ptr(s1)); (void)v1;
  GtkWrapMode _ret = (GtkWrapMode)gtk_source_print_compositor_get_wrap_mode(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, tag_pointer(R_MakeExternalPtr((void*)(_ret), R_NilValue, R_NilValue), "Gtk.WrapMode"));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Gtk.WrapMode"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_print_compositor_paginate(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourcePrintCompositor* v1 = (GtkSourcePrintCompositor*)(get_ptr(s1)); (void)v1;
  GtkPrintContext* v2 = (GtkPrintContext*)(get_ptr(s2)); (void)v2;
  gboolean _ret = (gboolean)gtk_source_print_compositor_paginate(v1, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_print_compositor_set_body_font_name(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourcePrintCompositor* v1 = (GtkSourcePrintCompositor*)(get_ptr(s1)); (void)v1;
  const char* v2 = (const char*)(CHAR(STRING_ELT(s2,0))); (void)v2;
  gtk_source_print_compositor_set_body_font_name(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_print_compositor_set_bottom_margin(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourcePrintCompositor* v1 = (GtkSourcePrintCompositor*)(get_ptr(s1)); (void)v1;
  gdouble v2 = (gdouble)((gdouble)_unbox_numeric(s2)); (void)v2;
  GtkUnit v3 = (GtkUnit)((GtkUnit)(TYPEOF(s3)==EXTPTRSXP ? (size_t)R_ExternalPtrAddr(s3) : INTEGER(s3)[0])); (void)v3;
  gtk_source_print_compositor_set_bottom_margin(v1, v2, v3);
  return R_NilValue;
}


SEXP R_gtk_source_print_compositor_set_footer_font_name(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourcePrintCompositor* v1 = (GtkSourcePrintCompositor*)(get_ptr(s1)); (void)v1;
  const char* v2 = (s2 != R_NilValue) ? (const char*)(CHAR(STRING_ELT(s2,0))) : NULL; (void)v2;
  gtk_source_print_compositor_set_footer_font_name(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_print_compositor_set_footer_format(SEXP s1, SEXP s2, SEXP s3, SEXP s4, SEXP s5) {
  RGTK4_REQUIRE_INIT();
  GtkSourcePrintCompositor* v1 = (GtkSourcePrintCompositor*)(get_ptr(s1)); (void)v1;
  gboolean v2 = (gboolean)((gboolean)LOGICAL(s2)[0]); (void)v2;
  const char* v3 = (s3 != R_NilValue) ? (const char*)(CHAR(STRING_ELT(s3,0))) : NULL; (void)v3;
  const char* v4 = (s4 != R_NilValue) ? (const char*)(CHAR(STRING_ELT(s4,0))) : NULL; (void)v4;
  const char* v5 = (s5 != R_NilValue) ? (const char*)(CHAR(STRING_ELT(s5,0))) : NULL; (void)v5;
  gtk_source_print_compositor_set_footer_format(v1, v2, v3, v4, v5);
  return R_NilValue;
}


SEXP R_gtk_source_print_compositor_set_header_font_name(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourcePrintCompositor* v1 = (GtkSourcePrintCompositor*)(get_ptr(s1)); (void)v1;
  const char* v2 = (s2 != R_NilValue) ? (const char*)(CHAR(STRING_ELT(s2,0))) : NULL; (void)v2;
  gtk_source_print_compositor_set_header_font_name(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_print_compositor_set_header_format(SEXP s1, SEXP s2, SEXP s3, SEXP s4, SEXP s5) {
  RGTK4_REQUIRE_INIT();
  GtkSourcePrintCompositor* v1 = (GtkSourcePrintCompositor*)(get_ptr(s1)); (void)v1;
  gboolean v2 = (gboolean)((gboolean)LOGICAL(s2)[0]); (void)v2;
  const char* v3 = (s3 != R_NilValue) ? (const char*)(CHAR(STRING_ELT(s3,0))) : NULL; (void)v3;
  const char* v4 = (s4 != R_NilValue) ? (const char*)(CHAR(STRING_ELT(s4,0))) : NULL; (void)v4;
  const char* v5 = (s5 != R_NilValue) ? (const char*)(CHAR(STRING_ELT(s5,0))) : NULL; (void)v5;
  gtk_source_print_compositor_set_header_format(v1, v2, v3, v4, v5);
  return R_NilValue;
}


SEXP R_gtk_source_print_compositor_set_highlight_syntax(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourcePrintCompositor* v1 = (GtkSourcePrintCompositor*)(get_ptr(s1)); (void)v1;
  gboolean v2 = (gboolean)((gboolean)LOGICAL(s2)[0]); (void)v2;
  gtk_source_print_compositor_set_highlight_syntax(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_print_compositor_set_left_margin(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourcePrintCompositor* v1 = (GtkSourcePrintCompositor*)(get_ptr(s1)); (void)v1;
  gdouble v2 = (gdouble)((gdouble)_unbox_numeric(s2)); (void)v2;
  GtkUnit v3 = (GtkUnit)((GtkUnit)(TYPEOF(s3)==EXTPTRSXP ? (size_t)R_ExternalPtrAddr(s3) : INTEGER(s3)[0])); (void)v3;
  gtk_source_print_compositor_set_left_margin(v1, v2, v3);
  return R_NilValue;
}


SEXP R_gtk_source_print_compositor_set_line_numbers_font_name(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourcePrintCompositor* v1 = (GtkSourcePrintCompositor*)(get_ptr(s1)); (void)v1;
  const char* v2 = (s2 != R_NilValue) ? (const char*)(CHAR(STRING_ELT(s2,0))) : NULL; (void)v2;
  gtk_source_print_compositor_set_line_numbers_font_name(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_print_compositor_set_print_footer(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourcePrintCompositor* v1 = (GtkSourcePrintCompositor*)(get_ptr(s1)); (void)v1;
  gboolean v2 = (gboolean)((gboolean)LOGICAL(s2)[0]); (void)v2;
  gtk_source_print_compositor_set_print_footer(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_print_compositor_set_print_header(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourcePrintCompositor* v1 = (GtkSourcePrintCompositor*)(get_ptr(s1)); (void)v1;
  gboolean v2 = (gboolean)((gboolean)LOGICAL(s2)[0]); (void)v2;
  gtk_source_print_compositor_set_print_header(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_print_compositor_set_print_line_numbers(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourcePrintCompositor* v1 = (GtkSourcePrintCompositor*)(get_ptr(s1)); (void)v1;
  guint v2 = (guint)((guint)_unbox_numeric(s2)); (void)v2;
  gtk_source_print_compositor_set_print_line_numbers(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_print_compositor_set_right_margin(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourcePrintCompositor* v1 = (GtkSourcePrintCompositor*)(get_ptr(s1)); (void)v1;
  gdouble v2 = (gdouble)((gdouble)_unbox_numeric(s2)); (void)v2;
  GtkUnit v3 = (GtkUnit)((GtkUnit)(TYPEOF(s3)==EXTPTRSXP ? (size_t)R_ExternalPtrAddr(s3) : INTEGER(s3)[0])); (void)v3;
  gtk_source_print_compositor_set_right_margin(v1, v2, v3);
  return R_NilValue;
}


SEXP R_gtk_source_print_compositor_set_tab_width(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourcePrintCompositor* v1 = (GtkSourcePrintCompositor*)(get_ptr(s1)); (void)v1;
  guint v2 = (guint)((guint)_unbox_numeric(s2)); (void)v2;
  gtk_source_print_compositor_set_tab_width(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_print_compositor_set_top_margin(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourcePrintCompositor* v1 = (GtkSourcePrintCompositor*)(get_ptr(s1)); (void)v1;
  gdouble v2 = (gdouble)((gdouble)_unbox_numeric(s2)); (void)v2;
  GtkUnit v3 = (GtkUnit)((GtkUnit)(TYPEOF(s3)==EXTPTRSXP ? (size_t)R_ExternalPtrAddr(s3) : INTEGER(s3)[0])); (void)v3;
  gtk_source_print_compositor_set_top_margin(v1, v2, v3);
  return R_NilValue;
}


SEXP R_gtk_source_print_compositor_set_wrap_mode(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourcePrintCompositor* v1 = (GtkSourcePrintCompositor*)(get_ptr(s1)); (void)v1;
  GtkWrapMode v2 = (GtkWrapMode)((GtkWrapMode)(TYPEOF(s2)==EXTPTRSXP ? (size_t)R_ExternalPtrAddr(s2) : INTEGER(s2)[0])); (void)v2;
  gtk_source_print_compositor_set_wrap_mode(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_region_new(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkTextBuffer* v1 = (GtkTextBuffer*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_region_new(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Region"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_region_add_region(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceRegion* v1 = (GtkSourceRegion*)(get_ptr(s1)); (void)v1;
  GtkSourceRegion* v2 = (s2 != R_NilValue) ? (GtkSourceRegion*)(get_ptr(s2)) : NULL; (void)v2;
  gtk_source_region_add_region(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_region_add_subregion(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceRegion* v1 = (GtkSourceRegion*)(get_ptr(s1)); (void)v1;
  const GtkTextIter* v2 = (const GtkTextIter*)(get_ptr(s2)); (void)v2;
  const GtkTextIter* v3 = (const GtkTextIter*)(get_ptr(s3)); (void)v3;
  gtk_source_region_add_subregion(v1, v2, v3);
  return R_NilValue;
}


SEXP R_gtk_source_region_get_bounds(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceRegion* v1 = (GtkSourceRegion*)(get_ptr(s1)); (void)v1;
  GtkTextIter _out_start = {0}; (void)_out_start;
  GtkTextIter _out_end = {0}; (void)_out_end;
  gboolean _ret = (gboolean)gtk_source_region_get_bounds(v1, &_out_start, &_out_end);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 3));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 3));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  SET_VECTOR_ELT(_ans, 1, make_boxed_struct(&_out_start, sizeof(GtkTextIter), "GtkTextIter"));
  if (VECTOR_ELT(_ans, 1) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 1), Rf_install("glib_type"), Rf_mkString("Gtk.TextIter"));
  }
  SET_STRING_ELT(_ans_names, 1, Rf_mkChar("start"));
  SET_VECTOR_ELT(_ans, 2, make_boxed_struct(&_out_end, sizeof(GtkTextIter), "GtkTextIter"));
  if (VECTOR_ELT(_ans, 2) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 2), Rf_install("glib_type"), Rf_mkString("Gtk.TextIter"));
  }
  SET_STRING_ELT(_ans_names, 2, Rf_mkChar("end"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_region_get_buffer(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceRegion* v1 = (GtkSourceRegion*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_region_get_buffer(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Gtk.TextBuffer"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_region_get_start_region_iter(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceRegion* v1 = (GtkSourceRegion*)(get_ptr(s1)); (void)v1;
  GtkSourceRegionIter _out_iter = {0}; (void)_out_iter;
  gtk_source_region_get_start_region_iter(v1, &_out_iter);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, tag_pointer(R_MakeExternalPtr((void*)(&_out_iter), R_NilValue, R_NilValue), "RegionIter"));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("RegionIter"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("iter"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_region_intersect_region(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceRegion* v1 = (s1 != R_NilValue) ? (GtkSourceRegion*)(get_ptr(s1)) : NULL; (void)v1;
  GtkSourceRegion* v2 = (s2 != R_NilValue) ? (GtkSourceRegion*)(get_ptr(s2)) : NULL; (void)v2;
  gconstpointer _ret = (gconstpointer)gtk_source_region_intersect_region(v1, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Region"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_region_intersect_subregion(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceRegion* v1 = (GtkSourceRegion*)(get_ptr(s1)); (void)v1;
  const GtkTextIter* v2 = (const GtkTextIter*)(get_ptr(s2)); (void)v2;
  const GtkTextIter* v3 = (const GtkTextIter*)(get_ptr(s3)); (void)v3;
  gconstpointer _ret = (gconstpointer)gtk_source_region_intersect_subregion(v1, v2, v3);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Region"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_region_is_empty(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceRegion* v1 = (s1 != R_NilValue) ? (GtkSourceRegion*)(get_ptr(s1)) : NULL; (void)v1;
  gboolean _ret = (gboolean)gtk_source_region_is_empty(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_region_subtract_region(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceRegion* v1 = (GtkSourceRegion*)(get_ptr(s1)); (void)v1;
  GtkSourceRegion* v2 = (s2 != R_NilValue) ? (GtkSourceRegion*)(get_ptr(s2)) : NULL; (void)v2;
  gtk_source_region_subtract_region(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_region_subtract_subregion(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceRegion* v1 = (GtkSourceRegion*)(get_ptr(s1)); (void)v1;
  const GtkTextIter* v2 = (const GtkTextIter*)(get_ptr(s2)); (void)v2;
  const GtkTextIter* v3 = (const GtkTextIter*)(get_ptr(s3)); (void)v3;
  gtk_source_region_subtract_subregion(v1, v2, v3);
  return R_NilValue;
}


SEXP R_gtk_source_region_to_string(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceRegion* v1 = (GtkSourceRegion*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_region_to_string(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_region_iter_get_subregion(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceRegionIter* v1 = (GtkSourceRegionIter*)(get_ptr(s1)); (void)v1;
  GtkTextIter _out_start = {0}; (void)_out_start;
  GtkTextIter _out_end = {0}; (void)_out_end;
  gboolean _ret = (gboolean)gtk_source_region_iter_get_subregion(v1, &_out_start, &_out_end);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 3));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 3));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  SET_VECTOR_ELT(_ans, 1, make_boxed_struct(&_out_start, sizeof(GtkTextIter), "GtkTextIter"));
  if (VECTOR_ELT(_ans, 1) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 1), Rf_install("glib_type"), Rf_mkString("Gtk.TextIter"));
  }
  SET_STRING_ELT(_ans_names, 1, Rf_mkChar("start"));
  SET_VECTOR_ELT(_ans, 2, make_boxed_struct(&_out_end, sizeof(GtkTextIter), "GtkTextIter"));
  if (VECTOR_ELT(_ans, 2) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 2), Rf_install("glib_type"), Rf_mkString("Gtk.TextIter"));
  }
  SET_STRING_ELT(_ans_names, 2, Rf_mkChar("end"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_region_iter_is_end(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceRegionIter* v1 = (GtkSourceRegionIter*)(get_ptr(s1)); (void)v1;
  gboolean _ret = (gboolean)gtk_source_region_iter_is_end(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_region_iter_next(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceRegionIter* v1 = (GtkSourceRegionIter*)(get_ptr(s1)); (void)v1;
  gboolean _ret = (gboolean)gtk_source_region_iter_next(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_search_context_new(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceBuffer* v1 = (GtkSourceBuffer*)(get_ptr(s1)); (void)v1;
  GtkSourceSearchSettings* v2 = (s2 != R_NilValue) ? (GtkSourceSearchSettings*)(get_ptr(s2)) : NULL; (void)v2;
  gconstpointer _ret = (gconstpointer)gtk_source_search_context_new(v1, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("SearchContext"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_search_context_backward(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSearchContext* v1 = (GtkSourceSearchContext*)(get_ptr(s1)); (void)v1;
  const GtkTextIter* v2 = (const GtkTextIter*)(get_ptr(s2)); (void)v2;
  GtkTextIter _out_match_start = {0}; (void)_out_match_start;
  GtkTextIter _out_match_end = {0}; (void)_out_match_end;
  gboolean _out_has_wrapped_around = 0; (void)_out_has_wrapped_around;
  gboolean _ret = (gboolean)gtk_source_search_context_backward(v1, v2, &_out_match_start, &_out_match_end, &_out_has_wrapped_around);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 4));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 4));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  SET_VECTOR_ELT(_ans, 1, make_boxed_struct(&_out_match_start, sizeof(GtkTextIter), "GtkTextIter"));
  if (VECTOR_ELT(_ans, 1) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 1), Rf_install("glib_type"), Rf_mkString("Gtk.TextIter"));
  }
  SET_STRING_ELT(_ans_names, 1, Rf_mkChar("match_start"));
  SET_VECTOR_ELT(_ans, 2, make_boxed_struct(&_out_match_end, sizeof(GtkTextIter), "GtkTextIter"));
  if (VECTOR_ELT(_ans, 2) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 2), Rf_install("glib_type"), Rf_mkString("Gtk.TextIter"));
  }
  SET_STRING_ELT(_ans_names, 2, Rf_mkChar("match_end"));
  SET_VECTOR_ELT(_ans, 3, Rf_ScalarInteger((int)(_out_has_wrapped_around)));
  if (VECTOR_ELT(_ans, 3) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 3), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 3, Rf_mkChar("has_wrapped_around"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_search_context_backward_async(SEXP s1, SEXP s2, SEXP s3, SEXP s4) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSearchContext* v1 = (GtkSourceSearchContext*)(get_ptr(s1)); (void)v1;
  const GtkTextIter* v2 = (const GtkTextIter*)(get_ptr(s2)); (void)v2;
  GCancellable* v3 = (s3 != R_NilValue) ? (GCancellable*)(get_ptr(s3)) : NULL; (void)v3;
  RCallbackClosure *_cb_closure_4 = (s4 == R_NilValue) ? NULL : rgtk4_closure_new(s4); (void)_cb_closure_4;
  gtk_source_search_context_backward_async(v1, v2, v3, (GAsyncReadyCallback)(_cb_closure_4 ? _rgtk4_cb_AsyncReadyCallback : NULL), _cb_closure_4);
  return R_NilValue;
}


SEXP R_gtk_source_search_context_backward_finish(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSearchContext* v1 = (GtkSourceSearchContext*)(get_ptr(s1)); (void)v1;
  GAsyncResult* v2 = (GAsyncResult*)(get_ptr(s2)); (void)v2;
  GtkTextIter _out_match_start = {0}; (void)_out_match_start;
  GtkTextIter _out_match_end = {0}; (void)_out_match_end;
  gboolean _out_has_wrapped_around = 0; (void)_out_has_wrapped_around;
  GError *_err = NULL;
  gboolean _ret = (gboolean)gtk_source_search_context_backward_finish(v1, v2, &_out_match_start, &_out_match_end, &_out_has_wrapped_around, &_err);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 4));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 4));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  SET_VECTOR_ELT(_ans, 1, make_boxed_struct(&_out_match_start, sizeof(GtkTextIter), "GtkTextIter"));
  if (VECTOR_ELT(_ans, 1) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 1), Rf_install("glib_type"), Rf_mkString("Gtk.TextIter"));
  }
  SET_STRING_ELT(_ans_names, 1, Rf_mkChar("match_start"));
  SET_VECTOR_ELT(_ans, 2, make_boxed_struct(&_out_match_end, sizeof(GtkTextIter), "GtkTextIter"));
  if (VECTOR_ELT(_ans, 2) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 2), Rf_install("glib_type"), Rf_mkString("Gtk.TextIter"));
  }
  SET_STRING_ELT(_ans_names, 2, Rf_mkChar("match_end"));
  SET_VECTOR_ELT(_ans, 3, Rf_ScalarInteger((int)(_out_has_wrapped_around)));
  if (VECTOR_ELT(_ans, 3) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 3), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 3, Rf_mkChar("has_wrapped_around"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_search_context_forward(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSearchContext* v1 = (GtkSourceSearchContext*)(get_ptr(s1)); (void)v1;
  const GtkTextIter* v2 = (const GtkTextIter*)(get_ptr(s2)); (void)v2;
  GtkTextIter _out_match_start = {0}; (void)_out_match_start;
  GtkTextIter _out_match_end = {0}; (void)_out_match_end;
  gboolean _out_has_wrapped_around = 0; (void)_out_has_wrapped_around;
  gboolean _ret = (gboolean)gtk_source_search_context_forward(v1, v2, &_out_match_start, &_out_match_end, &_out_has_wrapped_around);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 4));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 4));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  SET_VECTOR_ELT(_ans, 1, make_boxed_struct(&_out_match_start, sizeof(GtkTextIter), "GtkTextIter"));
  if (VECTOR_ELT(_ans, 1) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 1), Rf_install("glib_type"), Rf_mkString("Gtk.TextIter"));
  }
  SET_STRING_ELT(_ans_names, 1, Rf_mkChar("match_start"));
  SET_VECTOR_ELT(_ans, 2, make_boxed_struct(&_out_match_end, sizeof(GtkTextIter), "GtkTextIter"));
  if (VECTOR_ELT(_ans, 2) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 2), Rf_install("glib_type"), Rf_mkString("Gtk.TextIter"));
  }
  SET_STRING_ELT(_ans_names, 2, Rf_mkChar("match_end"));
  SET_VECTOR_ELT(_ans, 3, Rf_ScalarInteger((int)(_out_has_wrapped_around)));
  if (VECTOR_ELT(_ans, 3) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 3), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 3, Rf_mkChar("has_wrapped_around"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_search_context_forward_async(SEXP s1, SEXP s2, SEXP s3, SEXP s4) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSearchContext* v1 = (GtkSourceSearchContext*)(get_ptr(s1)); (void)v1;
  const GtkTextIter* v2 = (const GtkTextIter*)(get_ptr(s2)); (void)v2;
  GCancellable* v3 = (s3 != R_NilValue) ? (GCancellable*)(get_ptr(s3)) : NULL; (void)v3;
  RCallbackClosure *_cb_closure_4 = (s4 == R_NilValue) ? NULL : rgtk4_closure_new(s4); (void)_cb_closure_4;
  gtk_source_search_context_forward_async(v1, v2, v3, (GAsyncReadyCallback)(_cb_closure_4 ? _rgtk4_cb_AsyncReadyCallback : NULL), _cb_closure_4);
  return R_NilValue;
}


SEXP R_gtk_source_search_context_forward_finish(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSearchContext* v1 = (GtkSourceSearchContext*)(get_ptr(s1)); (void)v1;
  GAsyncResult* v2 = (GAsyncResult*)(get_ptr(s2)); (void)v2;
  GtkTextIter _out_match_start = {0}; (void)_out_match_start;
  GtkTextIter _out_match_end = {0}; (void)_out_match_end;
  gboolean _out_has_wrapped_around = 0; (void)_out_has_wrapped_around;
  GError *_err = NULL;
  gboolean _ret = (gboolean)gtk_source_search_context_forward_finish(v1, v2, &_out_match_start, &_out_match_end, &_out_has_wrapped_around, &_err);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 4));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 4));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  SET_VECTOR_ELT(_ans, 1, make_boxed_struct(&_out_match_start, sizeof(GtkTextIter), "GtkTextIter"));
  if (VECTOR_ELT(_ans, 1) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 1), Rf_install("glib_type"), Rf_mkString("Gtk.TextIter"));
  }
  SET_STRING_ELT(_ans_names, 1, Rf_mkChar("match_start"));
  SET_VECTOR_ELT(_ans, 2, make_boxed_struct(&_out_match_end, sizeof(GtkTextIter), "GtkTextIter"));
  if (VECTOR_ELT(_ans, 2) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 2), Rf_install("glib_type"), Rf_mkString("Gtk.TextIter"));
  }
  SET_STRING_ELT(_ans_names, 2, Rf_mkChar("match_end"));
  SET_VECTOR_ELT(_ans, 3, Rf_ScalarInteger((int)(_out_has_wrapped_around)));
  if (VECTOR_ELT(_ans, 3) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 3), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 3, Rf_mkChar("has_wrapped_around"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_search_context_get_buffer(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSearchContext* v1 = (GtkSourceSearchContext*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_search_context_get_buffer(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Buffer"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_search_context_get_highlight(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSearchContext* v1 = (GtkSourceSearchContext*)(get_ptr(s1)); (void)v1;
  gboolean _ret = (gboolean)gtk_source_search_context_get_highlight(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_search_context_get_match_style(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSearchContext* v1 = (GtkSourceSearchContext*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_search_context_get_match_style(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Style"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_search_context_get_occurrence_position(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSearchContext* v1 = (GtkSourceSearchContext*)(get_ptr(s1)); (void)v1;
  const GtkTextIter* v2 = (const GtkTextIter*)(get_ptr(s2)); (void)v2;
  const GtkTextIter* v3 = (const GtkTextIter*)(get_ptr(s3)); (void)v3;
  gint _ret = (gint)gtk_source_search_context_get_occurrence_position(v1, v2, v3);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gint"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_search_context_get_occurrences_count(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSearchContext* v1 = (GtkSourceSearchContext*)(get_ptr(s1)); (void)v1;
  gint _ret = (gint)gtk_source_search_context_get_occurrences_count(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gint"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_search_context_get_regex_error(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSearchContext* v1 = (GtkSourceSearchContext*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_search_context_get_regex_error(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("GLib.Error"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_search_context_get_settings(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSearchContext* v1 = (GtkSourceSearchContext*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_search_context_get_settings(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("SearchSettings"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_search_context_replace(SEXP s1, SEXP s2, SEXP s3, SEXP s4, SEXP s5) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSearchContext* v1 = (GtkSourceSearchContext*)(get_ptr(s1)); (void)v1;
  GtkTextIter* v2 = (GtkTextIter*)(get_ptr(s2)); (void)v2;
  GtkTextIter* v3 = (GtkTextIter*)(get_ptr(s3)); (void)v3;
  const char* v4 = (const char*)(CHAR(STRING_ELT(s4,0))); (void)v4;
  gint v5 = (gint)((gint)_unbox_numeric(s5)); (void)v5;
  GError *_err = NULL;
  gboolean _ret = (gboolean)gtk_source_search_context_replace(v1, v2, v3, v4, v5, &_err);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_search_context_replace_all(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSearchContext* v1 = (GtkSourceSearchContext*)(get_ptr(s1)); (void)v1;
  const char* v2 = (const char*)(CHAR(STRING_ELT(s2,0))); (void)v2;
  gint v3 = (gint)((gint)_unbox_numeric(s3)); (void)v3;
  GError *_err = NULL;
  guint _ret = (guint)gtk_source_search_context_replace_all(v1, v2, v3, &_err);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("guint"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_search_context_set_highlight(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSearchContext* v1 = (GtkSourceSearchContext*)(get_ptr(s1)); (void)v1;
  gboolean v2 = (gboolean)((gboolean)LOGICAL(s2)[0]); (void)v2;
  gtk_source_search_context_set_highlight(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_search_context_set_match_style(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSearchContext* v1 = (GtkSourceSearchContext*)(get_ptr(s1)); (void)v1;
  GtkSourceStyle* v2 = (s2 != R_NilValue) ? (GtkSourceStyle*)(get_ptr(s2)) : NULL; (void)v2;
  gtk_source_search_context_set_match_style(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_search_settings_new(void) {
  RGTK4_REQUIRE_INIT();

  gconstpointer _ret = (gconstpointer)gtk_source_search_settings_new();
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("SearchSettings"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_search_settings_get_at_word_boundaries(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSearchSettings* v1 = (GtkSourceSearchSettings*)(get_ptr(s1)); (void)v1;
  gboolean _ret = (gboolean)gtk_source_search_settings_get_at_word_boundaries(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_search_settings_get_case_sensitive(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSearchSettings* v1 = (GtkSourceSearchSettings*)(get_ptr(s1)); (void)v1;
  gboolean _ret = (gboolean)gtk_source_search_settings_get_case_sensitive(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_search_settings_get_regex_enabled(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSearchSettings* v1 = (GtkSourceSearchSettings*)(get_ptr(s1)); (void)v1;
  gboolean _ret = (gboolean)gtk_source_search_settings_get_regex_enabled(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_search_settings_get_search_text(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSearchSettings* v1 = (GtkSourceSearchSettings*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_search_settings_get_search_text(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_search_settings_get_wrap_around(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSearchSettings* v1 = (GtkSourceSearchSettings*)(get_ptr(s1)); (void)v1;
  gboolean _ret = (gboolean)gtk_source_search_settings_get_wrap_around(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_search_settings_set_at_word_boundaries(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSearchSettings* v1 = (GtkSourceSearchSettings*)(get_ptr(s1)); (void)v1;
  gboolean v2 = (gboolean)((gboolean)LOGICAL(s2)[0]); (void)v2;
  gtk_source_search_settings_set_at_word_boundaries(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_search_settings_set_case_sensitive(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSearchSettings* v1 = (GtkSourceSearchSettings*)(get_ptr(s1)); (void)v1;
  gboolean v2 = (gboolean)((gboolean)LOGICAL(s2)[0]); (void)v2;
  gtk_source_search_settings_set_case_sensitive(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_search_settings_set_regex_enabled(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSearchSettings* v1 = (GtkSourceSearchSettings*)(get_ptr(s1)); (void)v1;
  gboolean v2 = (gboolean)((gboolean)LOGICAL(s2)[0]); (void)v2;
  gtk_source_search_settings_set_regex_enabled(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_search_settings_set_search_text(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSearchSettings* v1 = (GtkSourceSearchSettings*)(get_ptr(s1)); (void)v1;
  const char* v2 = (s2 != R_NilValue) ? (const char*)(CHAR(STRING_ELT(s2,0))) : NULL; (void)v2;
  gtk_source_search_settings_set_search_text(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_search_settings_set_wrap_around(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSearchSettings* v1 = (GtkSourceSearchSettings*)(get_ptr(s1)); (void)v1;
  gboolean v2 = (gboolean)((gboolean)LOGICAL(s2)[0]); (void)v2;
  gtk_source_search_settings_set_wrap_around(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_snippet_new(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  const char* v1 = (s1 != R_NilValue) ? (const char*)(CHAR(STRING_ELT(s1,0))) : NULL; (void)v1;
  const char* v2 = (s2 != R_NilValue) ? (const char*)(CHAR(STRING_ELT(s2,0))) : NULL; (void)v2;
  gconstpointer _ret = (gconstpointer)gtk_source_snippet_new(v1, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Snippet"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_snippet_add_chunk(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippet* v1 = (GtkSourceSnippet*)(get_ptr(s1)); (void)v1;
  GtkSourceSnippetChunk* v2 = (GtkSourceSnippetChunk*)(get_ptr(s2)); (void)v2;
  gtk_source_snippet_add_chunk(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_snippet_copy(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippet* v1 = (GtkSourceSnippet*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_snippet_copy(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Snippet"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_snippet_get_context(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippet* v1 = (GtkSourceSnippet*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_snippet_get_context(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("SnippetContext"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_snippet_get_description(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippet* v1 = (GtkSourceSnippet*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_snippet_get_description(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_snippet_get_focus_position(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippet* v1 = (GtkSourceSnippet*)(get_ptr(s1)); (void)v1;
  gint _ret = (gint)gtk_source_snippet_get_focus_position(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gint"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_snippet_get_language_id(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippet* v1 = (GtkSourceSnippet*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_snippet_get_language_id(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_snippet_get_n_chunks(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippet* v1 = (GtkSourceSnippet*)(get_ptr(s1)); (void)v1;
  guint _ret = (guint)gtk_source_snippet_get_n_chunks(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("guint"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_snippet_get_name(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippet* v1 = (GtkSourceSnippet*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_snippet_get_name(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_snippet_get_nth_chunk(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippet* v1 = (GtkSourceSnippet*)(get_ptr(s1)); (void)v1;
  guint v2 = (guint)((guint)_unbox_numeric(s2)); (void)v2;
  gconstpointer _ret = (gconstpointer)gtk_source_snippet_get_nth_chunk(v1, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("SnippetChunk"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_snippet_get_trigger(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippet* v1 = (GtkSourceSnippet*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_snippet_get_trigger(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_snippet_set_description(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippet* v1 = (GtkSourceSnippet*)(get_ptr(s1)); (void)v1;
  const char* v2 = (const char*)(CHAR(STRING_ELT(s2,0))); (void)v2;
  gtk_source_snippet_set_description(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_snippet_set_language_id(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippet* v1 = (GtkSourceSnippet*)(get_ptr(s1)); (void)v1;
  const char* v2 = (const char*)(CHAR(STRING_ELT(s2,0))); (void)v2;
  gtk_source_snippet_set_language_id(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_snippet_set_name(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippet* v1 = (GtkSourceSnippet*)(get_ptr(s1)); (void)v1;
  const char* v2 = (const char*)(CHAR(STRING_ELT(s2,0))); (void)v2;
  gtk_source_snippet_set_name(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_snippet_set_trigger(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippet* v1 = (GtkSourceSnippet*)(get_ptr(s1)); (void)v1;
  const char* v2 = (const char*)(CHAR(STRING_ELT(s2,0))); (void)v2;
  gtk_source_snippet_set_trigger(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_snippet_chunk_new(void) {
  RGTK4_REQUIRE_INIT();

  gconstpointer _ret = (gconstpointer)gtk_source_snippet_chunk_new();
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("SnippetChunk"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_snippet_chunk_copy(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippetChunk* v1 = (GtkSourceSnippetChunk*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_snippet_chunk_copy(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("SnippetChunk"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_snippet_chunk_get_context(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippetChunk* v1 = (GtkSourceSnippetChunk*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_snippet_chunk_get_context(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("SnippetContext"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_snippet_chunk_get_focus_position(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippetChunk* v1 = (GtkSourceSnippetChunk*)(get_ptr(s1)); (void)v1;
  gint _ret = (gint)gtk_source_snippet_chunk_get_focus_position(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gint"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_snippet_chunk_get_spec(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippetChunk* v1 = (GtkSourceSnippetChunk*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_snippet_chunk_get_spec(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_snippet_chunk_get_text(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippetChunk* v1 = (GtkSourceSnippetChunk*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_snippet_chunk_get_text(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_snippet_chunk_get_text_set(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippetChunk* v1 = (GtkSourceSnippetChunk*)(get_ptr(s1)); (void)v1;
  gboolean _ret = (gboolean)gtk_source_snippet_chunk_get_text_set(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_snippet_chunk_get_tooltip_text(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippetChunk* v1 = (GtkSourceSnippetChunk*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_snippet_chunk_get_tooltip_text(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_snippet_chunk_set_context(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippetChunk* v1 = (GtkSourceSnippetChunk*)(get_ptr(s1)); (void)v1;
  GtkSourceSnippetContext* v2 = (GtkSourceSnippetContext*)(get_ptr(s2)); (void)v2;
  gtk_source_snippet_chunk_set_context(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_snippet_chunk_set_focus_position(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippetChunk* v1 = (GtkSourceSnippetChunk*)(get_ptr(s1)); (void)v1;
  gint v2 = (gint)((gint)_unbox_numeric(s2)); (void)v2;
  gtk_source_snippet_chunk_set_focus_position(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_snippet_chunk_set_spec(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippetChunk* v1 = (GtkSourceSnippetChunk*)(get_ptr(s1)); (void)v1;
  const char* v2 = (const char*)(CHAR(STRING_ELT(s2,0))); (void)v2;
  gtk_source_snippet_chunk_set_spec(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_snippet_chunk_set_text(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippetChunk* v1 = (GtkSourceSnippetChunk*)(get_ptr(s1)); (void)v1;
  const char* v2 = (const char*)(CHAR(STRING_ELT(s2,0))); (void)v2;
  gtk_source_snippet_chunk_set_text(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_snippet_chunk_set_text_set(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippetChunk* v1 = (GtkSourceSnippetChunk*)(get_ptr(s1)); (void)v1;
  gboolean v2 = (gboolean)((gboolean)LOGICAL(s2)[0]); (void)v2;
  gtk_source_snippet_chunk_set_text_set(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_snippet_chunk_set_tooltip_text(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippetChunk* v1 = (GtkSourceSnippetChunk*)(get_ptr(s1)); (void)v1;
  const char* v2 = (const char*)(CHAR(STRING_ELT(s2,0))); (void)v2;
  gtk_source_snippet_chunk_set_tooltip_text(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_snippet_context_new(void) {
  RGTK4_REQUIRE_INIT();

  gconstpointer _ret = (gconstpointer)gtk_source_snippet_context_new();
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("SnippetContext"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_snippet_context_clear_variables(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippetContext* v1 = (GtkSourceSnippetContext*)(get_ptr(s1)); (void)v1;
  gtk_source_snippet_context_clear_variables(v1);
  return R_NilValue;
}


SEXP R_gtk_source_snippet_context_expand(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippetContext* v1 = (GtkSourceSnippetContext*)(get_ptr(s1)); (void)v1;
  const char* v2 = (const char*)(CHAR(STRING_ELT(s2,0))); (void)v2;
  gconstpointer _ret = (gconstpointer)gtk_source_snippet_context_expand(v1, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_snippet_context_get_variable(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippetContext* v1 = (GtkSourceSnippetContext*)(get_ptr(s1)); (void)v1;
  const char* v2 = (const char*)(CHAR(STRING_ELT(s2,0))); (void)v2;
  gconstpointer _ret = (gconstpointer)gtk_source_snippet_context_get_variable(v1, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_snippet_context_set_constant(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippetContext* v1 = (GtkSourceSnippetContext*)(get_ptr(s1)); (void)v1;
  const char* v2 = (const char*)(CHAR(STRING_ELT(s2,0))); (void)v2;
  const char* v3 = (const char*)(CHAR(STRING_ELT(s3,0))); (void)v3;
  gtk_source_snippet_context_set_constant(v1, v2, v3);
  return R_NilValue;
}


SEXP R_gtk_source_snippet_context_set_line_prefix(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippetContext* v1 = (GtkSourceSnippetContext*)(get_ptr(s1)); (void)v1;
  const char* v2 = (const char*)(CHAR(STRING_ELT(s2,0))); (void)v2;
  gtk_source_snippet_context_set_line_prefix(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_snippet_context_set_tab_width(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippetContext* v1 = (GtkSourceSnippetContext*)(get_ptr(s1)); (void)v1;
  gint v2 = (gint)((gint)_unbox_numeric(s2)); (void)v2;
  gtk_source_snippet_context_set_tab_width(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_snippet_context_set_use_spaces(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippetContext* v1 = (GtkSourceSnippetContext*)(get_ptr(s1)); (void)v1;
  gboolean v2 = (gboolean)((gboolean)LOGICAL(s2)[0]); (void)v2;
  gtk_source_snippet_context_set_use_spaces(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_snippet_context_set_variable(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippetContext* v1 = (GtkSourceSnippetContext*)(get_ptr(s1)); (void)v1;
  const char* v2 = (const char*)(CHAR(STRING_ELT(s2,0))); (void)v2;
  const char* v3 = (const char*)(CHAR(STRING_ELT(s3,0))); (void)v3;
  gtk_source_snippet_context_set_variable(v1, v2, v3);
  return R_NilValue;
}


SEXP R_gtk_source_snippet_manager_get_default(void) {
  RGTK4_REQUIRE_INIT();

  gconstpointer _ret = (gconstpointer)gtk_source_snippet_manager_get_default();
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("SnippetManager"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_snippet_manager_get_search_path(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippetManager* v1 = (GtkSourceSnippetManager*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_snippet_manager_get_search_path(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_snippet_manager_get_snippet(SEXP s1, SEXP s2, SEXP s3, SEXP s4) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippetManager* v1 = (GtkSourceSnippetManager*)(get_ptr(s1)); (void)v1;
  const char* v2 = (s2 != R_NilValue) ? (const char*)(CHAR(STRING_ELT(s2,0))) : NULL; (void)v2;
  const char* v3 = (s3 != R_NilValue) ? (const char*)(CHAR(STRING_ELT(s3,0))) : NULL; (void)v3;
  const char* v4 = (const char*)(CHAR(STRING_ELT(s4,0))); (void)v4;
  gconstpointer _ret = (gconstpointer)gtk_source_snippet_manager_get_snippet(v1, v2, v3, v4);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Snippet"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_snippet_manager_list_groups(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippetManager* v1 = (GtkSourceSnippetManager*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_snippet_manager_list_groups(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : tag_pointer(Rf_mkString(_ret ? (const char*)_ret : ""), "utf8"));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_snippet_manager_list_matching(SEXP s1, SEXP s2, SEXP s3, SEXP s4) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippetManager* v1 = (GtkSourceSnippetManager*)(get_ptr(s1)); (void)v1;
  const char* v2 = (s2 != R_NilValue) ? (const char*)(CHAR(STRING_ELT(s2,0))) : NULL; (void)v2;
  const char* v3 = (s3 != R_NilValue) ? (const char*)(CHAR(STRING_ELT(s3,0))) : NULL; (void)v3;
  const char* v4 = (s4 != R_NilValue) ? (const char*)(CHAR(STRING_ELT(s4,0))) : NULL; (void)v4;
  gconstpointer _ret = (gconstpointer)gtk_source_snippet_manager_list_matching(v1, v2, v3, v4);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Gio.ListModel"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_snippet_manager_set_search_path(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSnippetManager* v1 = (GtkSourceSnippetManager*)(get_ptr(s1)); (void)v1;
  const gchar* const* v2 = (s2 != R_NilValue) ? (const gchar* const*)(get_ptr(s2)) : NULL; (void)v2;
  gtk_source_snippet_manager_set_search_path(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_space_drawer_new(void) {
  RGTK4_REQUIRE_INIT();

  gconstpointer _ret = (gconstpointer)gtk_source_space_drawer_new();
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("SpaceDrawer"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_space_drawer_bind_matrix_setting(SEXP s1, SEXP s2, SEXP s3, SEXP s4) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSpaceDrawer* v1 = (GtkSourceSpaceDrawer*)(get_ptr(s1)); (void)v1;
  GSettings* v2 = (GSettings*)(get_ptr(s2)); (void)v2;
  const char* v3 = (const char*)(CHAR(STRING_ELT(s3,0))); (void)v3;
  GSettingsBindFlags v4 = (GSettingsBindFlags)((GSettingsBindFlags)(TYPEOF(s4)==EXTPTRSXP ? (size_t)R_ExternalPtrAddr(s4) : INTEGER(s4)[0])); (void)v4;
  gtk_source_space_drawer_bind_matrix_setting(v1, v2, v3, v4);
  return R_NilValue;
}


SEXP R_gtk_source_space_drawer_get_enable_matrix(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSpaceDrawer* v1 = (GtkSourceSpaceDrawer*)(get_ptr(s1)); (void)v1;
  gboolean _ret = (gboolean)gtk_source_space_drawer_get_enable_matrix(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_space_drawer_get_matrix(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSpaceDrawer* v1 = (GtkSourceSpaceDrawer*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_space_drawer_get_matrix(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("GLib.Variant"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_space_drawer_get_types_for_locations(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSpaceDrawer* v1 = (GtkSourceSpaceDrawer*)(get_ptr(s1)); (void)v1;
  GtkSourceSpaceLocationFlags v2 = (GtkSourceSpaceLocationFlags)((GtkSourceSpaceLocationFlags)(TYPEOF(s2)==EXTPTRSXP ? (size_t)R_ExternalPtrAddr(s2) : INTEGER(s2)[0])); (void)v2;
  GtkSourceSpaceTypeFlags _ret = (GtkSourceSpaceTypeFlags)gtk_source_space_drawer_get_types_for_locations(v1, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, tag_pointer(R_MakeExternalPtr((void*)(_ret), R_NilValue, R_NilValue), "SpaceTypeFlags"));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("SpaceTypeFlags"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_space_drawer_set_enable_matrix(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSpaceDrawer* v1 = (GtkSourceSpaceDrawer*)(get_ptr(s1)); (void)v1;
  gboolean v2 = (gboolean)((gboolean)LOGICAL(s2)[0]); (void)v2;
  gtk_source_space_drawer_set_enable_matrix(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_space_drawer_set_matrix(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSpaceDrawer* v1 = (GtkSourceSpaceDrawer*)(get_ptr(s1)); (void)v1;
  GVariant* v2 = (s2 != R_NilValue) ? (GVariant*)(get_ptr(s2)) : NULL; (void)v2;
  gtk_source_space_drawer_set_matrix(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_space_drawer_set_types_for_locations(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceSpaceDrawer* v1 = (GtkSourceSpaceDrawer*)(get_ptr(s1)); (void)v1;
  GtkSourceSpaceLocationFlags v2 = (GtkSourceSpaceLocationFlags)((GtkSourceSpaceLocationFlags)(TYPEOF(s2)==EXTPTRSXP ? (size_t)R_ExternalPtrAddr(s2) : INTEGER(s2)[0])); (void)v2;
  GtkSourceSpaceTypeFlags v3 = (GtkSourceSpaceTypeFlags)((GtkSourceSpaceTypeFlags)(TYPEOF(s3)==EXTPTRSXP ? (size_t)R_ExternalPtrAddr(s3) : INTEGER(s3)[0])); (void)v3;
  gtk_source_space_drawer_set_types_for_locations(v1, v2, v3);
  return R_NilValue;
}


SEXP R_gtk_source_style_apply(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  const GtkSourceStyle* v1 = (s1 != R_NilValue) ? (const GtkSourceStyle*)(get_ptr(s1)) : NULL; (void)v1;
  GtkTextTag* v2 = (GtkTextTag*)(get_ptr(s2)); (void)v2;
  gtk_source_style_apply(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_style_copy(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  const GtkSourceStyle* v1 = (const GtkSourceStyle*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_style_copy(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Style"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_style_scheme_get_authors(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceStyleScheme* v1 = (GtkSourceStyleScheme*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_style_scheme_get_authors(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_style_scheme_get_description(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceStyleScheme* v1 = (GtkSourceStyleScheme*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_style_scheme_get_description(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_style_scheme_get_filename(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceStyleScheme* v1 = (GtkSourceStyleScheme*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_style_scheme_get_filename(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_style_scheme_get_id(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceStyleScheme* v1 = (GtkSourceStyleScheme*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_style_scheme_get_id(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_style_scheme_get_name(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceStyleScheme* v1 = (GtkSourceStyleScheme*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_style_scheme_get_name(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_style_scheme_get_style(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceStyleScheme* v1 = (GtkSourceStyleScheme*)(get_ptr(s1)); (void)v1;
  const char* v2 = (const char*)(CHAR(STRING_ELT(s2,0))); (void)v2;
  gconstpointer _ret = (gconstpointer)gtk_source_style_scheme_get_style(v1, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Style"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_style_scheme_chooser_get_style_scheme(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceStyleSchemeChooser* v1 = (GtkSourceStyleSchemeChooser*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_style_scheme_chooser_get_style_scheme(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("StyleScheme"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_style_scheme_chooser_set_style_scheme(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceStyleSchemeChooser* v1 = (GtkSourceStyleSchemeChooser*)(get_ptr(s1)); (void)v1;
  GtkSourceStyleScheme* v2 = (GtkSourceStyleScheme*)(get_ptr(s2)); (void)v2;
  gtk_source_style_scheme_chooser_set_style_scheme(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_style_scheme_chooser_button_new(void) {
  RGTK4_REQUIRE_INIT();

  gconstpointer _ret = (gconstpointer)gtk_source_style_scheme_chooser_button_new();
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Gtk.Widget"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_style_scheme_chooser_widget_new(void) {
  RGTK4_REQUIRE_INIT();

  gconstpointer _ret = (gconstpointer)gtk_source_style_scheme_chooser_widget_new();
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Gtk.Widget"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_style_scheme_manager_new(void) {
  RGTK4_REQUIRE_INIT();

  gconstpointer _ret = (gconstpointer)gtk_source_style_scheme_manager_new();
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("StyleSchemeManager"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_style_scheme_manager_get_default(void) {
  RGTK4_REQUIRE_INIT();

  gconstpointer _ret = (gconstpointer)gtk_source_style_scheme_manager_get_default();
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("StyleSchemeManager"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_style_scheme_manager_append_search_path(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceStyleSchemeManager* v1 = (GtkSourceStyleSchemeManager*)(get_ptr(s1)); (void)v1;
  const char* v2 = (const char*)(CHAR(STRING_ELT(s2,0))); (void)v2;
  gtk_source_style_scheme_manager_append_search_path(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_style_scheme_manager_force_rescan(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceStyleSchemeManager* v1 = (GtkSourceStyleSchemeManager*)(get_ptr(s1)); (void)v1;
  gtk_source_style_scheme_manager_force_rescan(v1);
  return R_NilValue;
}


SEXP R_gtk_source_style_scheme_manager_get_scheme(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceStyleSchemeManager* v1 = (GtkSourceStyleSchemeManager*)(get_ptr(s1)); (void)v1;
  const char* v2 = (const char*)(CHAR(STRING_ELT(s2,0))); (void)v2;
  gconstpointer _ret = (gconstpointer)gtk_source_style_scheme_manager_get_scheme(v1, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("StyleScheme"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_style_scheme_manager_get_scheme_ids(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceStyleSchemeManager* v1 = (GtkSourceStyleSchemeManager*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_style_scheme_manager_get_scheme_ids(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_style_scheme_manager_get_search_path(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceStyleSchemeManager* v1 = (GtkSourceStyleSchemeManager*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_style_scheme_manager_get_search_path(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_style_scheme_manager_prepend_search_path(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceStyleSchemeManager* v1 = (GtkSourceStyleSchemeManager*)(get_ptr(s1)); (void)v1;
  const char* v2 = (const char*)(CHAR(STRING_ELT(s2,0))); (void)v2;
  gtk_source_style_scheme_manager_prepend_search_path(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_style_scheme_manager_set_search_path(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceStyleSchemeManager* v1 = (GtkSourceStyleSchemeManager*)(get_ptr(s1)); (void)v1;
  const gchar* const* v2 = (s2 != R_NilValue) ? (const gchar* const*)(get_ptr(s2)) : NULL; (void)v2;
  gtk_source_style_scheme_manager_set_search_path(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_style_scheme_preview_get_selected(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceStyleSchemePreview* v1 = (GtkSourceStyleSchemePreview*)(get_ptr(s1)); (void)v1;
  gboolean _ret = (gboolean)gtk_source_style_scheme_preview_get_selected(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_style_scheme_preview_set_selected(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceStyleSchemePreview* v1 = (GtkSourceStyleSchemePreview*)(get_ptr(s1)); (void)v1;
  gboolean v2 = (gboolean)((gboolean)LOGICAL(s2)[0]); (void)v2;
  gtk_source_style_scheme_preview_set_selected(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_tag_new(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  const char* v1 = (s1 != R_NilValue) ? (const char*)(CHAR(STRING_ELT(s1,0))) : NULL; (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_tag_new(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Gtk.TextTag"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_view_new(void) {
  RGTK4_REQUIRE_INIT();

  gconstpointer _ret = (gconstpointer)gtk_source_view_new();
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Gtk.Widget"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_view_new_with_buffer(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceBuffer* v1 = (GtkSourceBuffer*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_view_new_with_buffer(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Gtk.Widget"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_view_get_auto_indent(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  gboolean _ret = (gboolean)gtk_source_view_get_auto_indent(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_view_get_background_pattern(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  GtkSourceBackgroundPatternType _ret = (GtkSourceBackgroundPatternType)gtk_source_view_get_background_pattern(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, tag_pointer(R_MakeExternalPtr((void*)(_ret), R_NilValue, R_NilValue), "BackgroundPatternType"));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("BackgroundPatternType"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_view_get_completion(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_view_get_completion(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Completion"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_view_get_enable_snippets(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  gboolean _ret = (gboolean)gtk_source_view_get_enable_snippets(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_view_get_gutter(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  GtkTextWindowType v2 = (GtkTextWindowType)((GtkTextWindowType)(TYPEOF(s2)==EXTPTRSXP ? (size_t)R_ExternalPtrAddr(s2) : INTEGER(s2)[0])); (void)v2;
  gconstpointer _ret = (gconstpointer)gtk_source_view_get_gutter(v1, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Gutter"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_view_get_highlight_current_line(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  gboolean _ret = (gboolean)gtk_source_view_get_highlight_current_line(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_view_get_hover(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_view_get_hover(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Hover"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_view_get_indent_on_tab(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  gboolean _ret = (gboolean)gtk_source_view_get_indent_on_tab(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_view_get_indent_width(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  gint _ret = (gint)gtk_source_view_get_indent_width(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gint"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_view_get_indenter(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_view_get_indenter(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Indenter"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_view_get_insert_spaces_instead_of_tabs(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  gboolean _ret = (gboolean)gtk_source_view_get_insert_spaces_instead_of_tabs(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_view_get_mark_attributes(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  const char* v2 = (const char*)(CHAR(STRING_ELT(s2,0))); (void)v2;
  gint* v3 = (gint*)((gint*)INTEGER(s3)); (void)v3;
  gconstpointer _ret = (gconstpointer)gtk_source_view_get_mark_attributes(v1, v2, v3);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("MarkAttributes"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_view_get_right_margin_position(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  guint _ret = (guint)gtk_source_view_get_right_margin_position(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("guint"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_view_get_show_line_marks(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  gboolean _ret = (gboolean)gtk_source_view_get_show_line_marks(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_view_get_show_line_numbers(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  gboolean _ret = (gboolean)gtk_source_view_get_show_line_numbers(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_view_get_show_right_margin(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  gboolean _ret = (gboolean)gtk_source_view_get_show_right_margin(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_view_get_smart_backspace(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  gboolean _ret = (gboolean)gtk_source_view_get_smart_backspace(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_view_get_smart_home_end(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  GtkSourceSmartHomeEndType _ret = (GtkSourceSmartHomeEndType)gtk_source_view_get_smart_home_end(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, tag_pointer(R_MakeExternalPtr((void*)(_ret), R_NilValue, R_NilValue), "SmartHomeEndType"));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("SmartHomeEndType"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_view_get_space_drawer(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_view_get_space_drawer(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("SpaceDrawer"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_view_get_tab_width(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  guint _ret = (guint)gtk_source_view_get_tab_width(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("guint"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_view_get_visual_column(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  const GtkTextIter* v2 = (const GtkTextIter*)(get_ptr(s2)); (void)v2;
  guint _ret = (guint)gtk_source_view_get_visual_column(v1, v2);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("guint"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_view_indent_lines(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  GtkTextIter* v2 = (GtkTextIter*)(get_ptr(s2)); (void)v2;
  GtkTextIter* v3 = (GtkTextIter*)(get_ptr(s3)); (void)v3;
  gtk_source_view_indent_lines(v1, v2, v3);
  return R_NilValue;
}


SEXP R_gtk_source_view_push_snippet(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  GtkSourceSnippet* v2 = (GtkSourceSnippet*)(get_ptr(s2)); (void)v2;
  GtkTextIter* v3 = (s3 != R_NilValue) ? (GtkTextIter*)(get_ptr(s3)) : NULL; (void)v3;
  gtk_source_view_push_snippet(v1, v2, v3);
  return R_NilValue;
}


SEXP R_gtk_source_view_set_auto_indent(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  gboolean v2 = (gboolean)((gboolean)LOGICAL(s2)[0]); (void)v2;
  gtk_source_view_set_auto_indent(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_view_set_background_pattern(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  GtkSourceBackgroundPatternType v2 = (GtkSourceBackgroundPatternType)((GtkSourceBackgroundPatternType)(TYPEOF(s2)==EXTPTRSXP ? (size_t)R_ExternalPtrAddr(s2) : INTEGER(s2)[0])); (void)v2;
  gtk_source_view_set_background_pattern(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_view_set_enable_snippets(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  gboolean v2 = (gboolean)((gboolean)LOGICAL(s2)[0]); (void)v2;
  gtk_source_view_set_enable_snippets(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_view_set_highlight_current_line(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  gboolean v2 = (gboolean)((gboolean)LOGICAL(s2)[0]); (void)v2;
  gtk_source_view_set_highlight_current_line(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_view_set_indent_on_tab(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  gboolean v2 = (gboolean)((gboolean)LOGICAL(s2)[0]); (void)v2;
  gtk_source_view_set_indent_on_tab(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_view_set_indent_width(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  gint v2 = (gint)((gint)_unbox_numeric(s2)); (void)v2;
  gtk_source_view_set_indent_width(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_view_set_indenter(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  GtkSourceIndenter* v2 = (s2 != R_NilValue) ? (GtkSourceIndenter*)(get_ptr(s2)) : NULL; (void)v2;
  gtk_source_view_set_indenter(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_view_set_insert_spaces_instead_of_tabs(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  gboolean v2 = (gboolean)((gboolean)LOGICAL(s2)[0]); (void)v2;
  gtk_source_view_set_insert_spaces_instead_of_tabs(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_view_set_mark_attributes(SEXP s1, SEXP s2, SEXP s3, SEXP s4) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  const char* v2 = (const char*)(CHAR(STRING_ELT(s2,0))); (void)v2;
  GtkSourceMarkAttributes* v3 = (GtkSourceMarkAttributes*)(get_ptr(s3)); (void)v3;
  gint v4 = (gint)((gint)_unbox_numeric(s4)); (void)v4;
  gtk_source_view_set_mark_attributes(v1, v2, v3, v4);
  return R_NilValue;
}


SEXP R_gtk_source_view_set_right_margin_position(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  guint v2 = (guint)((guint)_unbox_numeric(s2)); (void)v2;
  gtk_source_view_set_right_margin_position(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_view_set_show_line_marks(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  gboolean v2 = (gboolean)((gboolean)LOGICAL(s2)[0]); (void)v2;
  gtk_source_view_set_show_line_marks(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_view_set_show_line_numbers(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  gboolean v2 = (gboolean)((gboolean)LOGICAL(s2)[0]); (void)v2;
  gtk_source_view_set_show_line_numbers(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_view_set_show_right_margin(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  gboolean v2 = (gboolean)((gboolean)LOGICAL(s2)[0]); (void)v2;
  gtk_source_view_set_show_right_margin(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_view_set_smart_backspace(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  gboolean v2 = (gboolean)((gboolean)LOGICAL(s2)[0]); (void)v2;
  gtk_source_view_set_smart_backspace(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_view_set_smart_home_end(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  GtkSourceSmartHomeEndType v2 = (GtkSourceSmartHomeEndType)((GtkSourceSmartHomeEndType)(TYPEOF(s2)==EXTPTRSXP ? (size_t)R_ExternalPtrAddr(s2) : INTEGER(s2)[0])); (void)v2;
  gtk_source_view_set_smart_home_end(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_view_set_tab_width(SEXP s1, SEXP s2) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  guint v2 = (guint)((guint)_unbox_numeric(s2)); (void)v2;
  gtk_source_view_set_tab_width(v1, v2);
  return R_NilValue;
}


SEXP R_gtk_source_view_unindent_lines(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  GtkSourceView* v1 = (GtkSourceView*)(get_ptr(s1)); (void)v1;
  GtkTextIter* v2 = (GtkTextIter*)(get_ptr(s2)); (void)v2;
  GtkTextIter* v3 = (GtkTextIter*)(get_ptr(s3)); (void)v3;
  gtk_source_view_unindent_lines(v1, v2, v3);
  return R_NilValue;
}


SEXP R_gtk_source_vim_im_context_new(void) {
  RGTK4_REQUIRE_INIT();

  gconstpointer _ret = (gconstpointer)gtk_source_vim_im_context_new();
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : make_gobject_ptr((gpointer)_ret));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("Gtk.IMContext"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_check_version(SEXP s1, SEXP s2, SEXP s3) {
  RGTK4_REQUIRE_INIT();
  guint v1 = (guint)((guint)_unbox_numeric(s1)); (void)v1;
  guint v2 = (guint)((guint)_unbox_numeric(s2)); (void)v2;
  guint v3 = (guint)((guint)_unbox_numeric(s3)); (void)v3;
  gboolean _ret = (gboolean)gtk_source_check_version(v1, v2, v3);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("gboolean"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_finalize(void) {
  RGTK4_REQUIRE_INIT();

  gtk_source_finalize();
  return R_NilValue;
}


SEXP R_gtk_source_get_major_version(void) {
  RGTK4_REQUIRE_INIT();

  guint _ret = (guint)gtk_source_get_major_version();
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("guint"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_get_micro_version(void) {
  RGTK4_REQUIRE_INIT();

  guint _ret = (guint)gtk_source_get_micro_version();
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("guint"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_get_minor_version(void) {
  RGTK4_REQUIRE_INIT();

  guint _ret = (guint)gtk_source_get_minor_version();
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, Rf_ScalarInteger((int)(_ret)));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("guint"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_init(void) {
  RGTK4_REQUIRE_INIT();

  gtk_source_init();
  return R_NilValue;
}


SEXP R_gtk_source_utils_escape_search_text(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  const char* v1 = (const char*)(CHAR(STRING_ELT(s1,0))); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_utils_escape_search_text(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}


SEXP R_gtk_source_utils_unescape_search_text(SEXP s1) {
  RGTK4_REQUIRE_INIT();
  const char* v1 = (const char*)(CHAR(STRING_ELT(s1,0))); (void)v1;
  gconstpointer _ret = (gconstpointer)gtk_source_utils_unescape_search_text(v1);
  SEXP _ans = PROTECT(Rf_allocVector(VECSXP, 1));
  SEXP _ans_names = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_VECTOR_ELT(_ans, 0, (_ret == NULL) ? R_NilValue : Rf_mkString(_ret ? (const char*)_ret : ""));
  if (VECTOR_ELT(_ans, 0) != R_NilValue) {
    Rf_setAttrib(VECTOR_ELT(_ans, 0), Rf_install("glib_type"), Rf_mkString("utf8"));
  }
  SET_STRING_ELT(_ans_names, 0, Rf_mkChar("result"));
  Rf_setAttrib(_ans, R_NamesSymbol, _ans_names);
  UNPROTECT(2);
  return _ans;
}

#endif /* HAVE_GTKSOURCE */
