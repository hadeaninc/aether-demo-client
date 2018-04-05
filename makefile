all: godot opengl

repclient:
	$(MAKE) -C common/repclient

distclean:
	find . -\( -name obj -or -name bin -\) \
	  -exec rm -rf {} \; \
	  -prune
	rm -f clients/godot-repclient/lib/*.d
	rm -f clients/godot-repclient/lib/*.so

opengl: repclient
	$(MAKE) -C clients/opengl

godot: repclient
	$(MAKE) -C clients/godot-repclient

install-repclient:
	mkdir -p $(DESTDIR)/include/repclient \
	  $(DESTDIR)/lib
	cp common/repclient/obj/librepclient.a $(DESTDIR)/lib
	cp common/repclient/src/*.hh $(DESTDIR)/include/repclient

install-data:
	mkdir -p $(DESTDIR)/opt/aether
	cp -f aether_recording.dump $(DESTDIR)/opt/aether

install-opengl: opengl install-data install-repclient
	mkdir -p $(DESTDIR)/bin
	cp clients/*/bin/* $(DESTDIR)/bin

install-godot: godot install-data install-repclient
	mkdir -p $(DESTDIR)/opt/aether/clients
	cp -r clients/godot clients/godot-repclient clients/godot-common \
	  $(DESTDIR)/opt/aether/clients
	rm -f $(DESTDIR)/opt/aether/clients/godot-repclient/lib/*.d

install: install-opengl install-godot

.PHONY: all install repclient clients distclean opengl-only install-opengl install-godot install-data $(CLIENT_DIRS)
