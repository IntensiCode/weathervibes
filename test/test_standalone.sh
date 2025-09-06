#!/bin/bash

# Test the weather applet in standalone mode

echo "Weather Applet Standalone Test"
echo "=============================="
echo ""
echo "This will run the weather applet in a test window for debugging."
echo "Check /tmp/weather.log for detailed logging."
echo ""

# Clear the log first
rm -f /tmp/weather.log

# Build the applet if needed
if [ ! -f ./weather-applet ]; then
    echo "Building applet..."
    PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig make weather-applet || exit 1
fi

# Option 1: Use MATE's test container (recommended)
if [ -x /usr/bin/mate-panel-test-applets ]; then
    echo "Starting applet in MATE test container..."
    echo "Usage:"
    echo "  1. Select 'WeatherAppletFactory' from the list"
    echo "  2. Click 'Execute'"
    echo "  3. Right-click the applet icon for preferences"
    echo ""
    
    # Create a temporary .mate-panel-applet file for testing
    cat > /tmp/test-weather.mate-panel-applet << EOF
[Applet Factory]
Id=WeatherAppletFactory
Location=./weather-applet
Name=Weather Applet Test
Description=Test the weather applet

[WeatherApplet]
Name=Weather
Description=Show weather information
Icon=weather-clear
EOF
    
    # Run with our local applet
    MATE_PANEL_APPLET_DIR=/tmp \
    MATE_PANEL_DEBUG=1 \
    G_MESSAGES_DEBUG=all \
    mate-panel-test-applets --iid WeatherApplet ./weather-applet
else
    echo "mate-panel-test-applets not found."
    echo "Running basic test window instead..."
    
    # Option 2: Run directly with debug output
    G_MESSAGES_DEBUG=all ./weather-applet --run-in-window
fi

echo ""
echo "Check the log file for details:"
echo "  tail -f /tmp/weather.log"