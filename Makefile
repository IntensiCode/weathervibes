CC = gcc
CFLAGS = -Wall -Wextra -g

# Only check packages when building (not for clean)
ifneq ($(MAKECMDGOALS),clean)
    # Ensure system pkg-config paths are included (for Homebrew/Linuxbrew users)
    PKGCONFIG_CHECK := $(shell PKG_CONFIG_PATH="/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/share/pkgconfig:$$PKG_CONFIG_PATH" pkg-config --exists gtk+-3.0 libmatepanelapplet-4.0 json-glib-1.0 && echo "ok")
    
    ifeq ($(PKGCONFIG_CHECK),ok)
        PKGCONFIG_CFLAGS = $(shell PKG_CONFIG_PATH="/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/share/pkgconfig:$$PKG_CONFIG_PATH" pkg-config --cflags gtk+-3.0 libmatepanelapplet-4.0 json-glib-1.0)
        PKGCONFIG_LIBS = $(shell PKG_CONFIG_PATH="/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/share/pkgconfig:$$PKG_CONFIG_PATH" pkg-config --libs gtk+-3.0 libmatepanelapplet-4.0 json-glib-1.0)
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
APPLET_SOURCES = src/weather-applet.c src/config.c src/weather_fetcher.c src/weather_provider.c src/provider_ansiweather.c src/provider_brightsky.c src/provider_openweather.c src/provider_tomorrow.c src/logger.c src/geocoding.c src/geoip.c
APPLET_OBJECTS = $(patsubst src/%.c,build/%.o,$(APPLET_SOURCES))

# Installation directories
PREFIX = /usr
LIBEXECDIR = $(PREFIX)/lib/mate-panel
APPLETDIR = $(PREFIX)/share/mate-panel/applets
SERVICEDIR = $(PREFIX)/share/dbus-1/services

SRCDIR = src
BUILDDIR = build

.PHONY: all clean install uninstall test

all: $(APPLET_TARGET)
	@chmod -R a+rwX $(BUILDDIR) 2>/dev/null || true
	@chmod a+rw $(APPLET_TARGET) 2>/dev/null || true

# Test program
test: test_providers
	./test_providers

test_providers: test_providers.c $(SRCDIR)/weather_fetcher.c $(SRCDIR)/config.c $(SRCDIR)/weather_provider.c $(SRCDIR)/provider_ansiweather.c $(SRCDIR)/provider_brightsky.c $(SRCDIR)/provider_openweather.c $(SRCDIR)/provider_tomorrow.c $(SRCDIR)/logger.c $(SRCDIR)/geocoding.c $(SRCDIR)/geoip.c
	$(CC) $(CFLAGS) $(PKGCONFIG_CFLAGS) -o $@ $^ $(LDFLAGS)
	@chmod a+rw $@ 2>/dev/null || true

# Build panel applet
$(APPLET_TARGET): $(APPLET_OBJECTS)
	$(CC) $(APPLET_OBJECTS) -o $@ $(LDFLAGS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $(PKGCONFIG_CFLAGS) -c $< -o $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	@rm -rf $(BUILDDIR) $(APPLET_TARGET) test_providers 2>/dev/null || true

# Install panel applet (requires sudo)
install: $(APPLET_TARGET)
	install -D -m 755 $(APPLET_TARGET) $(LIBEXECDIR)/$(APPLET_TARGET)
	install -D -m 644 data/org.mate.applets.WeatherVibes.mate-panel-applet $(APPLETDIR)/org.mate.applets.WeatherVibes.mate-panel-applet
	install -D -m 644 data/org.mate.panel.applet.WeatherVibesFactory.service $(SERVICEDIR)/org.mate.panel.applet.WeatherVibesFactory.service

uninstall:
	rm -f $(LIBEXECDIR)/$(APPLET_TARGET)
	rm -f $(APPLETDIR)/org.mate.applets.WeatherVibes.mate-panel-applet
	rm -f $(SERVICEDIR)/org.mate.panel.applet.WeatherVibesFactory.service

# For development - build with proper PKG_CONFIG_PATH
dev:
	PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig:$(PKG_CONFIG_PATH) $(MAKE) all