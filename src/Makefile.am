
pkglibexec_SCRIPTS = updown config-iface

SUFFIXES = .in .sh

INSTALL_DIRS = @piddir@ @logdir@ @sysconfdir@ @pkgstatedir@ @pkgdatadir@

.in:
	$(top_srcdir)/sedsubst $(srcdir)/$< $@ "-w" $(SED_SUBST)
.sh:
	$(top_srcdir)/sedsubst $(srcdir)/$< $@ "-w,+x" $(SED_SUBST)

install-data-local:
	for D in $(INSTALL_DIRS); do bash -c "set -x; $(MKDIR_P) $(DESTDIR)$$D"; done
