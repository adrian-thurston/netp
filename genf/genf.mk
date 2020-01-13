# This tag indicates this file comes from genf package.
# installed-by-genf

pkglibexec_SCRIPTS = init.d bootstrap

BUILT_SOURCES = genf.h

INSTALL_DIRS = $(logdir) $(piddir) $(pkgdatadir) $(sysconfdir)

EXTRA_DIST = init.d.sh bootstrap.sh ../sedsubst

SUFFIXES = .in .sh

.in:
	$(top_srcdir)/sedsubst $(srcdir)/$< $@ "-w" $(SED_SUBST)
.sh:
	$(top_srcdir)/sedsubst $(srcdir)/$< $@ "-w,+x" $(SED_SUBST)

install-data-local:
	for D in $(INSTALL_DIRS); do bash -c "set -x; $(MKDIR_P) $(DESTDIR)$$D"; done
