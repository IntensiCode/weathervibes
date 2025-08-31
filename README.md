# Weather Vibes

A comprehensive MATE Panel weather applet for Linux with multiple weather providers and geocoding support.

## Features

- **MATE Panel applet** with dynamic weather display (emoji icons + temperature)
- **Automatic GeoIP location detection** on first run (falls back to Berlin if detection fails)
- **Multiple weather providers**:
  - AnsiWeather (Global coverage via OpenWeatherMap)
  - Bright Sky (DWD - Germany, high accuracy for German locations)
  - OpenWeather (Global coverage, requires free API key)
  - Tomorrow.io (Global coverage, requires free API key)
- **Automatic geocoding** using OpenStreetMap Nominatim
- **Configurable settings** via preferences dialog:
  - City/location
  - Update interval (5 minutes, 10 minutes, 15 minutes, 30 minutes, or 1 hour)
  - Temperature unit (Celsius/Fahrenheit)
  - Weather provider selection
  - API key management
- **Detailed weather information** including:
  - Temperature and "feels like"
  - Weather conditions with emoji icons
  - Humidity, wind speed/direction
  - Atmospheric pressure
  - UV index
  - Sunrise/sunset times (where available)
- **Automatic updates** at configurable intervals
- **Right-click menu** for preferences and about
- **Comprehensive logging** to `/tmp/weather.log`

## Dependencies

- GTK3
- MATE Panel development libraries (`libmate-panel-applet-dev`)
- JSON-GLib (`libjson-glib-dev`)
- curl (for API requests)
- ansiweather (for AnsiWeather provider: `sudo apt install ansiweather`)

## Building

Install dependencies:
```bash
sudo apt install libmate-panel-applet-dev libgtk-3-dev libjson-glib-dev ansiweather curl
```

Build the applet:
```bash
PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig make
```

## Installation

```bash
sudo PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig make install
```

The applet will be installed to `/usr/lib/mate-panel/` and can be added to your MATE Panel.

To uninstall:
```bash
sudo make uninstall
```

## Testing

Test all weather providers:
```bash
PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig make test_providers
./test_providers "Your City"
```

## Adding to MATE Panel

1. Right-click on your MATE Panel
2. Select "Add to Panel..."
3. Look for "Weather Applet" in the list
4. Click "Add"
5. Right-click the applet and select "Preferences" to configure

## Configuration

The applet stores its configuration in `~/.config/weather-vibes/config.ini`

Default settings:
- City: Auto-detected via GeoIP (or Berlin if detection fails)
- Update interval: 10 minutes
- Temperature unit: Celsius
- Provider: AnsiWeather

Configuration is managed through the Preferences dialog (right-click → Preferences).

## Weather Providers

### AnsiWeather (Default)
- **Coverage**: Global
- **API Key**: Not required (uses built-in OpenWeatherMap key)
- **Best for**: General use worldwide

### Bright Sky (DWD)
- **Coverage**: Germany only
- **API Key**: Not required
- **Best for**: German locations (most accurate)
- **Data source**: Deutscher Wetterdienst (German Weather Service)

### OpenWeather
- **Coverage**: Global
- **API Key**: Required (free at https://openweathermap.org/api)
- **Best for**: Users wanting more control over their API usage
- **Features**: Sunrise/sunset times, detailed conditions

### Tomorrow.io
- **Coverage**: Global
- **API Key**: Required (free at https://www.tomorrow.io/weather-api/)
- **Best for**: Advanced weather data with machine learning predictions
- **Features**: Detailed atmospheric data, weather codes

## Getting API Keys

### OpenWeather API Key
1. Visit https://openweathermap.org/api
2. Sign up for a free account
3. Go to "API keys" in your account
4. Copy your API key
5. Enter it in the applet's Preferences dialog

### Tomorrow.io API Key
1. Visit https://www.tomorrow.io/weather-api/
2. Sign up for a free account
3. Go to your dashboard
4. Copy your API key
5. Enter it in the applet's Preferences dialog

## Weather Icons

The applet uses emoji weather icons:
- ☀️ Clear/Sunny
- 🌤️ Mostly Clear
- ⛅ Partly Cloudy
- 🌥️ Mostly Cloudy
- ☁️ Cloudy/Overcast
- 🌧️ Rain
- ⛈️ Thunderstorm
- ❄️ Snow
- 🌨️ Sleet/Freezing Rain
- 🌫️ Fog/Mist

## Troubleshooting

### Logs
Check `/tmp/weather.log` for detailed debug information.

### Common Issues

**Weather not updating:**
- Check your internet connection
- Verify the city name is spelled correctly
- Check the log file for API errors

**API key errors:**
- Ensure you've entered the API key correctly
- Check that your API key is active
- For Tomorrow.io, ensure you're within the free tier limits

**Locale issues (wrong coordinates):**
- The applet now uses locale-independent formatting
- If you see wrong locations, restart the applet

## Usage

1. **Left-click** the applet icon to open detailed weather popup with:
   - Full weather conditions
   - Temperature, feels like, humidity, pressure
   - Wind speed and direction
   - UV index and sunrise/sunset times
   - Refresh button for immediate update
2. **Right-click** for menu options:
   - Preferences: Configure city, provider, and API keys
   - About: View applet information
3. Weather updates automatically at your configured interval
4. Hover over the icon for a tooltip with current conditions