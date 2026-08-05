# Tests for fixes applied to Rgtk4:
#   - gObjectSetEnum  (issue #1)
#   - gtkListBoxGetSelectedRows unpacking (issue #3)
#   - notify::property signal connections (issue #9)
#   - gtkWidgetTranslateCoordinates returns doubles (issue #6)
#   - scalar out-param boxing regression (gboolean, gdouble)

# --- #1: gObjectSetEnum ---------------------------------------------------

test_that("gObjectSetEnum sets GtkJustification enum on label", {
  skip_if_no_gtk()
  label <- gtkLabelNew("test")
  # GTK_JUSTIFY_CENTER = 2
  expect_no_error(gObjectSetEnum(label, "justify", 2L))
})

test_that("gObjectSetEnum sets PangoWeight (gint property) on GtkTextTag", {
  skip_if_no_gtk()
  buf   <- gtkTextBufferNew(NULL)
  table <- gtkTextBufferGetTagTable(buf)
  tag   <- gtkTextTagNew("bold")
  gtkTextTagTableAdd(table, tag)
  # PANGO_WEIGHT_BOLD = 700
  expect_no_error(gObjectSetEnum(tag, "weight", 700L))
})

test_that("gObjectSetEnum sets PangoStyle (enum property) on GtkTextTag", {
  skip_if_no_gtk()
  buf   <- gtkTextBufferNew(NULL)
  table <- gtkTextBufferGetTagTable(buf)
  tag   <- gtkTextTagNew("italic")
  gtkTextTagTableAdd(table, tag)
  # PANGO_STYLE_ITALIC = 2
  expect_no_error(gObjectSetEnum(tag, "style", 2L))
})

test_that("gObjectSetEnum errors on unknown property", {
  skip_if_no_gtk()
  label <- gtkLabelNew("test")
  expect_error(gObjectSetEnum(label, "no_such_property", 1L), "no property")
})

test_that("gObjectSetEnum errors on string-typed property", {
  skip_if_no_gtk()
  label <- gtkLabelNew("test")
  expect_error(gObjectSetEnum(label, "label", 1L))
})

test_that("gObjectSetEnum rejects NA", {
  skip_if_no_gtk()
  label <- gtkLabelNew("test")
  expect_error(gObjectSetEnum(label, "justify", NA_integer_), "NA")
})

# --- #3: gtkListBoxGetSelectedRows returns an R list ----------------------

test_that("gtkListBoxGetSelectedRows returns empty list when nothing selected", {
  skip_if_no_gtk()
  lb <- gtkListBoxNew()
  gtkListBoxAppend(lb, gtkLabelNew("a"))
  gtkListBoxAppend(lb, gtkLabelNew("b"))
  gtkListBoxUnselectAll(lb)
  rows <- gtkListBoxGetSelectedRows(lb)
  expect_type(rows, "list")
  expect_length(rows, 0L)
})

test_that("gtkListBoxGetSelectedRows returns list of extptrs in single-select", {
  skip_if_no_gtk()
  gtkStartEventLoop()
  win <- gtkWindowNew()
  gtkWindowSetDefaultSize(win, 300L, 200L)
  lb  <- gtkListBoxNew()
  gtkListBoxAppend(lb, gtkLabelNew("first"))
  gtkListBoxAppend(lb, gtkLabelNew("second"))
  gtkWindowSetChild(win, lb)
  gtkWindowPresent(win)

  deadline <- Sys.time() + 2
  while (Sys.time() < deadline) gtkMainIteration()

  row0 <- gtkListBoxGetRowAtIndex(lb, 0L)
  gtkListBoxSelectRow(lb, row0)

  rows <- gtkListBoxGetSelectedRows(lb)
  expect_type(rows, "list")
  expect_length(rows, 1L)
  expect_identical(typeof(rows[[1L]]), "externalptr")

  gtkWindowDestroy(win)
})

test_that("gtkListBoxGetSelectedRows returns multiple rows in multi-select", {
  skip_if_no_gtk()
  gtkStartEventLoop()
  win <- gtkWindowNew()
  gtkWindowSetDefaultSize(win, 300L, 200L)
  lb  <- gtkListBoxNew()
  # GTK_SELECTION_MULTIPLE = 3
  gObjectSetEnum(lb, "selection-mode", 3L)
  gtkListBoxAppend(lb, gtkLabelNew("a"))
  gtkListBoxAppend(lb, gtkLabelNew("b"))
  gtkListBoxAppend(lb, gtkLabelNew("c"))
  gtkWindowSetChild(win, lb)
  gtkWindowPresent(win)

  deadline <- Sys.time() + 2
  while (Sys.time() < deadline) gtkMainIteration()

  gtkListBoxSelectAll(lb)
  rows <- gtkListBoxGetSelectedRows(lb)
  expect_type(rows, "list")
  expect_length(rows, 3L)
  for (r in rows) expect_identical(typeof(r), "externalptr")

  gtkWindowDestroy(win)
})

# --- #9: notify::property signal connections ------------------------------

test_that("gSignalConnectR accepts notify::property detail syntax", {
  skip_if_no_gtk()
  label <- gtkLabelNew("hello")
  expect_no_error(
    gSignalConnectR(label, "notify::label", function(obj, pspec) invisible(NULL))
  )
})

test_that("notify::property signal fires when property changes", {
  skip_if_no_gtk()
  gtkStartEventLoop()
  label <- gtkLabelNew("initial")
  fired <- FALSE

  gSignalConnectR(label, "notify::label", function(obj, pspec) {
    fired <<- TRUE
  })

  gtkLabelSetText(label, "changed")

  deadline <- Sys.time() + 1
  while (!fired && Sys.time() < deadline) gtkMainIteration()

  expect_true(fired)
})

test_that("gSignalConnectR still works for plain signals (regression)", {
  skip_if_no_gtk()
  button <- gtkButtonNew()
  expect_no_error(
    gSignalConnectR(button, "clicked", function(w) invisible(NULL))
  )
})

test_that("gSignalConnectR rejects genuinely missing signals", {
  skip_if_no_gtk()
  label <- gtkLabelNew("test")
  expect_error(
    gSignalConnectR(label, "no_such_signal_xyz", function() {}),
    "not found"
  )
})

# --- #6 + scalar boxing regression ----------------------------------------

test_that("gtkWidgetTranslateCoordinates dest_x/dest_y are doubles not integers", {
  skip_if_no_gtk()
  gtkStartEventLoop()
  win <- gtkWindowNew()
  gtkWindowSetDefaultSize(win, 300L, 200L)
  box <- gtkBoxNew(1L, 0L)
  src <- gtkLabelNew("src")
  dst <- gtkLabelNew("dst")
  gtkBoxAppend(box, src)
  gtkBoxAppend(box, dst)
  gtkWindowSetChild(win, box)
  gtkWindowPresent(win)

  deadline <- Sys.time() + 2
  while (Sys.time() < deadline) gtkMainIteration()

  result <- gtkWidgetTranslateCoordinates(src, dst, 1.7, 2.3)
  expect_type(result$dest_x, "double")
  expect_type(result$dest_y, "double")

  gtkWindowDestroy(win)
})

test_that("gtkLevelBarGetOffsetValue: gboolean->logical, gdouble->double (scalar boxing regression)", {
  skip_if_no_gtk()
  lb     <- gtkLevelBarNew()
  result <- gtkLevelBarGetOffsetValue(lb, "low")
  # gboolean out-param must still be logical
  expect_type(result$result, "integer")
  # gdouble out-param must be double, not integer (the bug this fixes)
  expect_type(result$value, "double")
})

# --- gtkFileChooserDialogRun: titlebar/delete must not CRITICAL --------------

.close_file_chooser_by_title <- function(title, response_id = -6L, destroy = FALSE) {
  gTimeoutAdd(200L, function() {
    tops <- gListToRList(gtkWindowListToplevels(), free_list = TRUE)
    for (w in tops) {
      tit <- tryCatch(as.character(gtkWindowGetTitle(w)), error = function(e) NA_character_)
      if (identical(tit, title)) {
        gtkDialogResponse(w, as.integer(response_id))
        if (isTRUE(destroy))
          try(gtkWindowDestroy(w), silent = TRUE)
      }
    }
    FALSE
  })
}

test_that("gtkFileChooserDialogRun cancel via response returns cleanly", {
  skip_if_no_gtk()
  ## Do not call gtkStartEventLoop() here: nested g_main_loop_run in the
  ## helper is enough, and the background loop makes automation flaky.
  parent <- gtkWindowNew()
  gtkWindowSetTitle(parent, "file-chooser-parent")
  gtkWidgetSetVisible(parent, FALSE)
  .close_file_chooser_by_title("file-chooser-cancel", response_id = -6L)
  res <- gtkFileChooserDialogRun(parent = parent, title = "file-chooser-cancel", action = 0L)
  expect_type(res, "list")
  expect_equal(res$response, -6L)
  expect_null(res$file)
  gtkWindowDestroy(parent)
})

test_that("gtkFileChooserDialogRun DELETE_EVENT + destroy does not CRITICAL", {
  skip_if_no_gtk()
  parent <- gtkWindowNew()
  gtkWindowSetTitle(parent, "file-chooser-parent-2")
  gtkWidgetSetVisible(parent, FALSE)
  ## Mimic titlebar close: emit delete-event then destroy while the nested
  ## loop is still unwinding (the path that used to CRITICAL).
  .close_file_chooser_by_title("file-chooser-delete", response_id = -4L, destroy = TRUE)
  expect_no_error({
    res <- gtkFileChooserDialogRun(
      parent = parent,
      title = "file-chooser-delete",
      action = 0L
    )
  })
  expect_equal(res$response, -4L)
  expect_null(res$file)
  gtkWindowDestroy(parent)
})
