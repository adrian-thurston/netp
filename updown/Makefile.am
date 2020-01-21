
pkglibexec_SCRIPTS = bootstrap

bin_SCRIPTS = updown

EXTRA_DIST = bootstrap.sh

SUFFIXES = .in .sh

INSTALL_DIRS = @piddir@ @logdir@ @sysconfdir@ @modulesdir@ @pkgstatedir@ @pkgdatadir@ @pkgconfdir@

.in:
	$(top_srcdir)/sedsubst $(srcdir)/$< $@ "-w" $(SED_SUBST)
.sh:
	$(top_srcdir)/sedsubst $(srcdir)/$< $@ "-w,+x" $(SED_SUBST)

install-data-local:
	for D in $(INSTALL_DIRS); do bash -c "set -x; $(MKDIR_P) $(DESTDIR)$$D"; done
