# Upstream files, kept for reference

These came with Aleph One 0.12.0 (2001) and BERO's 2002 Dreamcast port. None of
them is used to build this project, which uses `Makefile.dc` and `build.sh` at
the top level.

They are here rather than deleted because they are the historical record of what
this was before, and because `README` in particular is the upstream project's own
description of itself, which is worth keeping intact rather than editing.

| | |
|---|---|
| `README` | Aleph One/SDL, 2001. Describes BeOS, Windows 9x/NT and Unix, and links to a university page that is long gone. |
| `INSTALL.BeOS`, `INSTALL.Unix` | build instructions for those platforms |
| `Makefile.BeOS`, `Makefile.am`, `Makefile.in` | the BeOS and autotools builds |
| `configure`, `config.*`, `aclocal.m4`, `install-sh`, `missing`, `mkinstalldirs`, `stamp-h.in` | generated autotools machinery |
| `AlephOne.spec`, `AlephOne.spec.in` | RPM packaging |

Nothing in the source tree needs `config.h`: the two files that include it,
`Source_Files/Expat/xmldef.h` and `Source_Files/CSeries/sdl_cseries.h`, guard it
with `#ifdef HAVE_CONFIG_H`, which this build does not define.
