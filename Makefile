CC = gcc
CFLAGS = -Wall -Wextra -g

BASE_VERSION := $(shell python3 -c 'import json; d = json.load(open("version.json")); print("%s.%s.%s" % (d["major"], d["minor"], d["patch"]))' 2>/dev/null || echo "0.0.0")
APPLET_VERSION ?= $(BASE_VERSION)-dev

# Only check packages when building (not for clean)
ifneq ($(MAKECMDGOALS),clean)
    # Ensure system pkg-config paths are included (for Homebrew/Linuxbrew users)
    PKGCONFIG_CHECK := $(shell PKG_CONFIG_PATH="/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/share/pkgconfig:$$PKG_CONFIG_PATH" pkg-config --exists gtk+-3.0 libmatepanelapplet-4.0 json-glib-1.0 libsoup-2.4 && echo "ok")
    
    ifeq ($(PKGCONFIG_CHECK),ok)
        PKGCONFIG_CFLAGS = $(shell PKG_CONFIG_PATH="/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/share/pkgconfig:$$PKG_CONFIG_PATH" pkg-config --cflags gtk+-3.0 libmatepanelapplet-4.0 json-glib-1.0 libsoup-2.4)
        PKGCONFIG_LIBS = $(shell PKG_CONFIG_PATH="/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/share/pkgconfig:$$PKG_CONFIG_PATH" pkg-config --libs gtk+-3.0 libmatepanelapplet-4.0 json-glib-1.0 libsoup-2.4)
    else
        $(error Missing required packages. Run: PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig make)
    endif
    
    LDFLAGS = $(PKGCONFIG_LIBS) -lm
else
    # For clean, we don't need any libraries
    PKGCONFIG_CFLAGS =
    PKGCONFIG_LIBS =
    LDFLAGS = -lm
endif

# Panel applet
APPLET_TARGET = weather-vibes
APPLET_SOURCES = src/weather_applet.c src/weather_details_dialog.c src/weather_graph.c src/preferences_dialog.c src/about_dialog.c src/preferences_handler.c src/applet_config.c src/weather_display.c src/weather_update_async.c src/async_fetch.c src/day_night.c src/config_gsettings.c src/json_helpers.c src/weather_fetcher.c src/weather_provider.c src/weather_conditions.c src/network.c src/provider_ansiweather.c src/provider_brightsky.c src/provider_openweather.c src/provider_tomorrow.c src/logger.c src/geocoding.c src/geoip.c src/weather_resume.c
APPLET_OBJECTS = $(patsubst src/%.c,build/%.o,$(APPLET_SOURCES))

# Installation directories
PREFIX = /usr
LIBEXECDIR = $(PREFIX)/lib/mate-panel
APPLETDIR = $(PREFIX)/share/mate-panel/applets
SERVICEDIR = $(PREFIX)/share/dbus-1/services
SCHEMADIR = $(PREFIX)/share/glib-2.0/schemas

SRCDIR = src
BUILDDIR = build

.PHONY: all clean install uninstall test check

all: $(APPLET_TARGET)
	@chmod -R a+rwX $(BUILDDIR) 2>/dev/null || true
	@chmod a+rw $(APPLET_TARGET) 2>/dev/null || true

$(BUILDDIR)/build_info.h: | $(BUILDDIR)
	@COMMIT=$$(git rev-parse --short=12 HEAD 2>/dev/null || echo "unknown"); \
	DIRTY=$$(test -n "$$(git status --porcelain 2>/dev/null)" && echo 1 || echo 0); \
	printf '%s\n' '#ifndef BUILD_INFO_H' '#define BUILD_INFO_H' '' \
	  "#define WEATHER_VIBES_GIT_COMMIT \"$$COMMIT\"" \
	  "#define WEATHER_VIBES_GIT_DIRTY $$DIRTY" '' '#endif // BUILD_INFO_H' > $(BUILDDIR)/build_info.h

$(BUILDDIR)/app_version.h: version.json | $(BUILDDIR)
	@printf '%s\n' '#ifndef APP_VERSION_H' '#define APP_VERSION_H' '' \
	  "#define WEATHER_VIBES_VERSION \"$(APPLET_VERSION)\"" '' '#endif // APP_VERSION_H' > $(BUILDDIR)/app_version.h

# Test program
test: test_providers
	./test_providers

test_providers: test/test_providers.c $(SRCDIR)/json_helpers.c $(SRCDIR)/weather_fetcher.c $(SRCDIR)/config.c $(SRCDIR)/weather_provider.c $(SRCDIR)/weather_conditions.c $(SRCDIR)/network.c $(SRCDIR)/async_fetch.c $(SRCDIR)/provider_ansiweather.c $(SRCDIR)/provider_brightsky.c $(SRCDIR)/provider_openweather.c $(SRCDIR)/provider_tomorrow.c $(SRCDIR)/logger.c $(SRCDIR)/geocoding.c $(SRCDIR)/geoip.c
	$(CC) $(CFLAGS) $(PKGCONFIG_CFLAGS) -o $@ $^ $(LDFLAGS)
	@chmod a+rw $@ 2>/dev/null || true

# Build panel applet
$(APPLET_TARGET): $(APPLET_OBJECTS)
	$(CC) $(APPLET_OBJECTS) -o $@ $(LDFLAGS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c $(BUILDDIR)/build_info.h $(BUILDDIR)/app_version.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -I$(BUILDDIR) $(PKGCONFIG_CFLAGS) -c $< -o $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	@rm -rf $(BUILDDIR) $(APPLET_TARGET) test_providers test_weather_conditions test_day_night test_json_helpers 2>/dev/null || true

# Install panel applet (requires sudo)
install: $(APPLET_TARGET)
	install -D -m 755 $(APPLET_TARGET) $(LIBEXECDIR)/$(APPLET_TARGET)
	install -D -m 644 data/org.mate.applets.WeatherVibes.mate-panel-applet $(APPLETDIR)/org.mate.applets.WeatherVibes.mate-panel-applet
	install -D -m 644 data/org.mate.panel.applet.WeatherVibesFactory.service $(SERVICEDIR)/org.mate.panel.applet.WeatherVibesFactory.service
	install -D -m 644 data/org.mate.panel.applet.weather-vibes.gschema.xml $(SCHEMADIR)/org.mate.panel.applet.weather-vibes.gschema.xml
	glib-compile-schemas $(SCHEMADIR)

uninstall:
	rm -f $(LIBEXECDIR)/$(APPLET_TARGET)
	rm -f $(APPLETDIR)/org.mate.applets.WeatherVibes.mate-panel-applet
	rm -f $(SERVICEDIR)/org.mate.panel.applet.WeatherVibesFactory.service
	rm -f $(SCHEMADIR)/org.mate.panel.applet.weather-vibes.gschema.xml
	glib-compile-schemas $(SCHEMADIR) || true

# Unit tests with GLib test framework
test_weather_conditions: test/test_weather_conditions.c $(SRCDIR)/weather_conditions.c
	$(CC) $(CFLAGS) $(PKGCONFIG_CFLAGS) -o $@ $^ $(LDFLAGS)
	@chmod a+rw $@ 2>/dev/null || true

test_day_night: test/test_day_night.c $(SRCDIR)/day_night.c $(SRCDIR)/weather_conditions.c
	$(CC) $(CFLAGS) $(PKGCONFIG_CFLAGS) -o $@ $^ $(LDFLAGS)
	@chmod a+rw $@ 2>/dev/null || true

test_json_helpers: test/test_json_helpers.c $(SRCDIR)/json_helpers.c
	$(CC) $(CFLAGS) $(PKGCONFIG_CFLAGS) -o $@ $^ $(LDFLAGS)
	@chmod a+rw $@ 2>/dev/null || true

# Run all tests with 'make check'
check: test_weather_conditions test_day_night test_json_helpers test_providers
	@echo "Running unit tests..."
	@echo "===================="
	./test_weather_conditions
	./test_day_night
	./test_json_helpers
	@echo "===================="
	@echo "Running integration tests..."
	@echo "===================="
	./test_providers "Berlin" | tail -20
	@echo "===================="
	@echo "All tests passed!"

# For development - build with proper PKG_CONFIG_PATH
dev:
	PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig:$(PKG_CONFIG_PATH) $(MAKE) all
