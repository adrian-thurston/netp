
pkgdata_DATA = openvpn.conf dnsmasq.conf

pkglibexec_SCRIPTS = \
	shuttle-up \
	shuttle-down \
	vpn-up \
	vpn-down \
	live-up \
	live-down \
	comp-up \
	comp-down

SUFFIXES = .in .sh

.in:
	$(top_srcdir)/sedsubst $(srcdir)/$< $@ "-w" $(SED_SUBST)
.sh:
	$(top_srcdir)/sedsubst $(srcdir)/$< $@ "-w,+x" $(SED_SUBST)

install-data-local:
	for D in $(INSTALL_DIRS); do bash -c "set -x; $(MKDIR_P) $(DESTDIR)$$D"; done
