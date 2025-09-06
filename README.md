# Weather Vibes - MATE Panel Weather Applet

[![CI](https://github.com/yourusername/mate-weather/actions/workflows/ci.yml/badge.svg)](https://github.com/yourusername/mate-weather/actions/workflows/ci.yml)

A modern weather applet for the MATE desktop panel with multiple weather providers and emoji support.

## Features

- 🌤️ Multiple weather providers:
  - AnsiWeather (Global, no API key required)
  - Bright Sky (DWD - Germany)
  - OpenWeather (requires free API key)
  - Tomorrow.io (requires free API key)
- 🌡️ Real-time weather updates
- 🌅 Day/night detection using sunrise/sunset data
- 💾 Configuration stored in GSettings
- 🧪 Comprehensive test suite

## Building

### Dependencies

- GTK+ 3.0
- MATE Panel 1.24+
- JSON-GLib
- libsoup 2.4

### Ubuntu/Debian

```bash
sudo apt install build-essential pkg-config libgtk-3-dev \
  libmate-panel-applet-4-dev libjson-glib-dev libsoup2.4-dev

make
sudo make install
```

## Testing

Run the test suite:

```bash
make check
```

Test weather providers:

```bash
make test
./test_providers "Berlin"
```

## Configuration

The applet stores configuration in GSettings under the schema:
`org.mate.panel.applet.weather-vibes`

Available settings:
- `city`: Location for weather data
- `use-celsius`: Temperature unit (true for Celsius, false for Fahrenheit)
- `update-interval-minutes`: Update frequency (1-60 minutes)
- `provider`: Weather provider (0-3)
- API keys for OpenWeather and Tomorrow.io

## Development

### Project Structure

```
src/
├── weather_applet.c         # Main applet entry point
├── weather_provider.c       # Provider abstraction layer
├── provider_*.c            # Individual provider implementations
├── weather_conditions.c    # Weather condition mapping and emojis
├── day_night.c            # Day/night detection logic
├── config_gsettings.c     # GSettings configuration
├── json_helpers.c         # JSON parsing utilities
└── ...

test/
├── test_weather_conditions.c  # Weather conditions tests
├── test_day_night.c           # Day/night logic tests
├── test_json_helpers.c        # JSON helper tests
└── test_providers.c           # Provider integration tests
```

### Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Run tests with `make check`
5. Submit a pull request

## CI/CD Setup

### GitHub Secrets

For CI to test all weather providers, you need to set up GitHub secrets:

1. Go to your repository Settings → Secrets and variables → Actions
2. Add the following repository secrets:
   - `OPENWEATHER_API_KEY`: Your OpenWeather API key
   - `TOMORROW_API_KEY`: Your Tomorrow.io API key

Without these secrets, CI will still run but will skip testing the providers that require API keys.

### Local Testing

To run tests locally with API keys:

```bash
export OPENWEATHER_API_KEY="your-key-here"
export TOMORROW_API_KEY="your-key-here"
make check
```

## Security

⚠️ **NEVER commit API keys to the repository!** Always use environment variables or GitHub secrets.

## License

GPL-3.0