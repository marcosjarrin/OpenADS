# X# WinForms + OpenADS example

A small X# WinForms app: open an Advantage table via X#'s `AXDBFCDX`
RDD, bind it to a `DataGridView`, navigate. Linked against OpenADS'
`ace64.dll` / `ace32.dll`.

> **Not in CI.** Opens a window — manual showcase only. For the
> headless equivalent see `tests/smoke/xsharp/`.

## Prerequisites

- The **X# compiler + runtime + IDE** — from <https://www.xsharp.eu/>.
  Not vendored here.
- A built OpenADS `ace64.dll` (x64) / `ace32.dll` (x86), e.g.
  `build/default/src/Release/ace64.dll`. Must be the OpenADS DLL, not
  SAP's — make sure it resolves ahead of any other `ace64.dll`
  (put its directory on `PATH`, or copy it next to the produced exe).

## Layout (to be added)

- `OpenADSWinFormsDemo.xsproj` — WinForms exe, references
  `XSharp.RDD` / `XSharp.RT` / `XSharp.Core` + `System.Windows.Forms`.
- `MainForm.prg` — form with a `DataGridView`; on load: `RddSetDefault`,
  `DbUseArea` a sample table, pull rows into a `DataTable`, bind.

Rules: first-party source only — never check in the X# runtime
assemblies or SAP `ace*.dll`/`ace*.lib`.
